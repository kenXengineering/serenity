/*
 * Copyright (c) 2020, Liav A. <liavalb@hotmail.co.il>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>
#include <Kernel/FileSystem/FileSystem.h>
#include <Kernel/FileSystem/Inode.h>
#include <Kernel/FileSystem/InodeMetadata.h>
#include <Kernel/TTY/SlavePTY.h>

namespace Kernel {

class DevTmpFS final : public FileSystem {
    friend class DevTmpFSInode;
    friend class DevTmpFSRootDirectoryInode;

public:
    virtual ~DevTmpFS() override;
    static KResultOr<NonnullRefPtr<DevTmpFS>> try_create();

    virtual KResult initialize() override;
    virtual StringView class_name() const override { return "DevTmpFS"sv; }
    virtual Inode& root_inode() override;

private:
    DevTmpFS();
    size_t allocate_inode_index();

    RefPtr<DevTmpFSRootDirectoryInode> m_root_inode;
    InodeIndex m_next_inode_index { 0 };
};

class DevTmpFSInode : public Inode {
    friend class DevTmpFS;
    friend class DevTmpFSRootDirectoryInode;
    friend class DevTmpFSDirectoryInode;

public:
    virtual StringView name() const = 0;

    DevTmpFS& fs() { return static_cast<DevTmpFS&>(Inode::fs()); }
    DevTmpFS const& fs() const { return static_cast<DevTmpFS const&>(Inode::fs()); }

protected:
    explicit DevTmpFSInode(DevTmpFS&);
    DevTmpFSInode(DevTmpFS&, unsigned, unsigned);
    virtual KResultOr<size_t> read_bytes(off_t, size_t, UserOrKernelBuffer& buffer, OpenFileDescription*) const override;
    virtual KResult traverse_as_directory(Function<bool(FileSystem::DirectoryEntryView const&)>) const override;
    virtual KResultOr<NonnullRefPtr<Inode>> lookup(StringView name) override;
    virtual void flush_metadata() override;
    virtual InodeMetadata metadata() const override final;
    virtual KResultOr<size_t> write_bytes(off_t, size_t, const UserOrKernelBuffer& buffer, OpenFileDescription*) override;
    virtual KResultOr<NonnullRefPtr<Inode>> create_child(StringView name, mode_t, dev_t, UserID, GroupID) override;
    virtual KResult add_child(Inode&, const StringView& name, mode_t) override;
    virtual KResult remove_child(const StringView& name) override;
    virtual KResult chmod(mode_t) override;
    virtual KResult chown(UserID, GroupID) override;
    virtual KResult truncate(u64) override;

    mode_t m_mode { 0600 };
    UserID m_uid { 0 };
    GroupID m_gid { 0 };
    const unsigned m_major_number { 0 };
    const unsigned m_minor_number { 0 };

protected:
    enum class Type {
        BlockDevice,
        CharacterDevice,
        Directory,
        RootDirectory,
        Link
    };
    virtual Type node_type() const = 0;

private:
    IntrusiveListNode<DevTmpFSInode, RefPtr<DevTmpFSInode>> m_list_node;
};

class DevTmpFSDeviceInode final : public DevTmpFSInode {
    friend class DevTmpFS;
    friend class DevTmpFSRootDirectoryInode;
    friend class DevTmpFSDirectoryInode;

public:
    virtual StringView name() const override;
    virtual ~DevTmpFSDeviceInode() override;

private:
    DevTmpFSDeviceInode(DevTmpFS&, unsigned, unsigned, bool, NonnullOwnPtr<KString> name);
    // ^DevTmpFSInode
    virtual Type node_type() const override { return m_block_device ? Type::BlockDevice : Type::CharacterDevice; }

    // ^Inode
    virtual KResultOr<size_t> read_bytes(off_t, size_t, UserOrKernelBuffer& buffer, OpenFileDescription*) const override;
    virtual KResultOr<size_t> write_bytes(off_t, size_t, const UserOrKernelBuffer& buffer, OpenFileDescription*) override;

    NonnullOwnPtr<KString> m_name;
    const bool m_block_device;
};

class DevTmpFSLinkInode final : public DevTmpFSInode {
    friend class DevTmpFS;
    friend class DevTmpFSDirectoryInode;

public:
    virtual StringView name() const override;
    virtual ~DevTmpFSLinkInode() override;

protected:
    DevTmpFSLinkInode(DevTmpFS&, NonnullOwnPtr<KString>);
    // ^DevTmpFSInode
    virtual Type node_type() const override { return Type::Link; }

    // ^Inode
    virtual KResultOr<size_t> read_bytes(off_t, size_t, UserOrKernelBuffer& buffer, OpenFileDescription*) const override;
    virtual KResultOr<size_t> write_bytes(off_t, size_t, const UserOrKernelBuffer& buffer, OpenFileDescription*) override;

    NonnullOwnPtr<KString> m_name;
    OwnPtr<KString> m_link;
};

class DevTmpFSDirectoryInode : public DevTmpFSInode {
    friend class DevTmpFS;
    friend class DevTmpFSRootDirectoryInode;

public:
    virtual ~DevTmpFSDirectoryInode() override;
    virtual StringView name() const override { return m_name->view(); }

protected:
    // ^DevTmpFSInode
    virtual Type node_type() const override { return Type::Directory; }

    virtual KResultOr<NonnullRefPtr<Inode>> create_child(StringView name, mode_t, dev_t, UserID, GroupID) override;
    virtual KResult remove_child(const StringView& name) override;
    virtual KResult traverse_as_directory(Function<bool(FileSystem::DirectoryEntryView const&)>) const override;
    virtual KResultOr<NonnullRefPtr<Inode>> lookup(StringView name) override;
    DevTmpFSDirectoryInode(DevTmpFS&, NonnullOwnPtr<KString> name);
    // ^Inode
    OwnPtr<KString> m_name;
    IntrusiveList<DevTmpFSInode, NonnullRefPtr<DevTmpFSInode>, &DevTmpFSInode::m_list_node> m_nodes;

private:
    explicit DevTmpFSDirectoryInode(DevTmpFS&);
};

class DevTmpFSRootDirectoryInode final : public DevTmpFSDirectoryInode {
    friend class DevTmpFS;

public:
    virtual ~DevTmpFSRootDirectoryInode() override;
    virtual StringView name() const override { return "."; }

private:
    // ^DevTmpFSInode
    virtual Type node_type() const override { return Type::Directory; }

    explicit DevTmpFSRootDirectoryInode(DevTmpFS&);
    virtual KResult chmod(mode_t) override;
    virtual KResult chown(UserID, GroupID) override;
};

}
