/*
 * Copyright (c) 2020, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Debug.h>
#include <LibCore/EventLoop.h>
#include <LibGemini/GeminiJob.h>
#include <LibGemini/GeminiResponse.h>
#include <LibTLS/TLSv12.h>
#include <stdio.h>
#include <unistd.h>

namespace Gemini {

void GeminiJob::start()
{
    VERIFY(!m_socket);
    m_socket = TLS::TLSv12::construct(this);
    m_socket->set_root_certificates(m_override_ca_certificates ? *m_override_ca_certificates : DefaultRootCACertificates::the().certificates());
    m_socket->on_tls_connected = [this] {
        dbgln_if(GEMINIJOB_DEBUG, "GeminiJob: on_connected callback");
        on_socket_connected();
    };
    m_socket->on_tls_error = [this](TLS::AlertDescription error) {
        if (error == TLS::AlertDescription::HandshakeFailure) {
            deferred_invoke([this] {
                return did_fail(Core::NetworkJob::Error::ProtocolFailed);
            });
        } else if (error == TLS::AlertDescription::DecryptError) {
            deferred_invoke([this] {
                return did_fail(Core::NetworkJob::Error::ConnectionFailed);
            });
        } else {
            deferred_invoke([this] {
                return did_fail(Core::NetworkJob::Error::TransmissionFailed);
            });
        }
    };
    m_socket->on_tls_finished = [this] {
        finish_up();
    };
    m_socket->on_tls_certificate_request = [this](auto&) {
        if (on_certificate_requested)
            on_certificate_requested(*this);
    };
    bool success = ((TLS::TLSv12&)*m_socket).connect(m_request.url().host(), m_request.url().port());
    if (!success) {
        deferred_invoke([this] {
            return did_fail(Core::NetworkJob::Error::ConnectionFailed);
        });
    }
}

void GeminiJob::shutdown()
{
    if (!m_socket)
        return;
    m_socket->on_tls_ready_to_read = nullptr;
    m_socket->on_tls_connected = nullptr;
    remove_child(*m_socket);
    m_socket = nullptr;
}

void GeminiJob::read_while_data_available(Function<IterationDecision()> read)
{
    while (m_socket->can_read()) {
        if (read() == IterationDecision::Break)
            break;
    }
}

void GeminiJob::set_certificate(String certificate, String private_key)
{
    if (!m_socket->add_client_key(certificate.bytes(), private_key.bytes())) {
        dbgln("LibGemini: Failed to set a client certificate");
        // FIXME: Do something about this failure
        VERIFY_NOT_REACHED();
    }
}

void GeminiJob::register_on_ready_to_read(Function<void()> callback)
{
    m_socket->on_tls_ready_to_read = [callback = move(callback)](auto&) {
        callback();
    };
}

void GeminiJob::register_on_ready_to_write(Function<void()> callback)
{
    m_socket->on_tls_ready_to_write = [callback = move(callback)](auto&) {
        callback();
    };
}

bool GeminiJob::can_read_line() const
{
    return m_socket->can_read_line();
}

String GeminiJob::read_line(size_t size)
{
    return m_socket->read_line(size);
}

ByteBuffer GeminiJob::receive(size_t size)
{
    return m_socket->read(size);
}

bool GeminiJob::can_read() const
{
    return m_socket->can_read();
}

bool GeminiJob::eof() const
{
    return m_socket->eof();
}

bool GeminiJob::write(ReadonlyBytes bytes)
{
    return m_socket->write(bytes);
}

}
