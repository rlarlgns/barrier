/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2024 Gemini CLI
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "net/SerialSocket.h"
#include "net/NetworkAddress.h"
#include "net/SocketMultiplexer.h"
#include "net/TSocketMultiplexerMethodJob.h"
#include "base/Log.h"
#include "base/IEventQueue.h"
#include "base/TMethodEventJob.h"
#include "base/XBase.h"
#include "arch/Arch.h"
#include "mt/Lock.h"

#include <cstring>

//
// SerialSocket
//

SerialSocket::SerialSocket(IEventQueue* events, SocketMultiplexer* socketMultiplexer,
                           const std::string& port, int baudRate, bool isServer,
                           std::shared_ptr<bool> activeConnection) :
    IDataSocket(events),
    m_events(events),
    m_socketMultiplexer(socketMultiplexer),
    m_portName(port),
    m_baudRate(baudRate),
    m_serialPort(NULL),
    m_connected(false),
    m_readable(false),
    m_writable(false),
    m_isServer(isServer),
    m_synced(false),
    m_skippingWakeup(isServer),
    m_wakeupTimer(NULL),
    m_activeConnection(activeConnection)
{
}

SerialSocket::~SerialSocket()
{
    close();
}

void
SerialSocket::bind(const NetworkAddress&)
{
    // Serial ports don't bind in the network sense
}

void
SerialSocket::close()
{
    setJob(nullptr);

    Lock lock(&m_mutex);
    if (m_connected) {
        m_events->addEvent(Event(m_events->forISocket().disconnected(), getEventTarget()));
    }
    onDisconnected();

    if (m_serialPort != NULL) {
        ARCH->closeSerial(m_serialPort);
        m_serialPort = NULL;
    }

    if (m_wakeupTimer != NULL) {
        m_events->removeHandler(Event::kTimer, m_wakeupTimer);
        m_events->deleteTimer(m_wakeupTimer);
        m_wakeupTimer = NULL;
    }
}

void*
SerialSocket::getEventTarget() const
{
    return const_cast<void*>(static_cast<const void*>(this));
}

UInt32
SerialSocket::read(void* buffer, UInt32 n)
{
    Lock lock(&m_mutex);
    UInt32 size = m_inputBuffer.getSize();
    if (n > size) n = size;
    if (buffer != NULL && n != 0) {
        memcpy(buffer, m_inputBuffer.peek(n), n);
    }
    m_inputBuffer.pop(n);
    return n;
}

void
SerialSocket::write(const void* buffer, UInt32 n)
{
    bool wasEmpty;
    {
        Lock lock(&m_mutex);
        if (!m_writable) return;
        if (n == 0) return;

        wasEmpty = (m_outputBuffer.getSize() == 0);
        m_outputBuffer.write(buffer, n);
    }

    if (wasEmpty) {
        setJob(newJob());
    }
}

void
SerialSocket::flush()
{
    // For serial, we'll just assume it flushes quickly
}

void
SerialSocket::shutdownInput()
{
    Lock lock(&m_mutex);
    m_readable = false;
}

void
SerialSocket::shutdownOutput()
{
    Lock lock(&m_mutex);
    m_writable = false;
}

bool
SerialSocket::isReady() const
{
    Lock lock(&m_mutex);
    return (m_inputBuffer.getSize() > 0);
}

bool
SerialSocket::isFatal() const
{
    return false;
}

UInt32
SerialSocket::getSize() const
{
    Lock lock(&m_mutex);
    return m_inputBuffer.getSize();
}

void
SerialSocket::connect(const NetworkAddress&)
{
    Lock lock(&m_mutex);
    if (m_connected) return;

    try {
        m_serialPort = ARCH->openSerial(m_portName, m_baudRate);
        onConnected();

        if (!m_isServer) {
            // Client: flood 256 bytes of 0xAA before any real Barrier data.
            //
            // Purpose: if the server still holds a zombie connection from our
            //   previous session, the 256-byte blob arrives as an absurdly large
            //   packet-length prefix (0xAAAAAAAA ≈ 2.8 GB).  Barrier's own
            //   PacketStreamFilter checks every incoming length against
            //   PROTOCOL_MAX_MESSAGE_LENGTH (4 MB) and fires inputFormatError,
            //   which closes the zombie socket immediately.
            //   A fresh server ignores these bytes via m_skippingWakeup.
            //
            // 256 bytes guarantees the length prefix (first 4 bytes) is all 0xAA
            // regardless of UART FIFO timing — the server always reads at least 4.
            UInt8 wakeup[256];
            memset(wakeup, 0xAA, sizeof(wakeup));
            ARCH->writeSerial(m_serialPort, wakeup, sizeof(wakeup));

            // start periodic ping (keeps the server's wakeup logic alive)
            m_wakeupTimer = m_events->newOneShotTimer(0.2, NULL);
            m_events->adoptHandler(Event::kTimer, m_wakeupTimer,
                new TMethodEventJob<SerialSocket>(this, &SerialSocket::handleWakeupTimer));
        }

        m_events->addEvent(Event(m_events->forIDataSocket().connected(), getEventTarget()));
        setJob(newJob());
    }
    catch (std::exception& e) {
        LOG((CLOG_ERR "failed to open serial port: %s", e.what()));
        throw XBase(e.what());
    }
}

std::unique_ptr<ISocketMultiplexerJob>
SerialSocket::newJob()
{
    if (m_serialPort == NULL || !m_connected) {
        return {};
    }

    ArchSocket pseudoSocket = reinterpret_cast<ArchSocket>(m_serialPort);

    auto writable = m_writable && (m_outputBuffer.getSize() > 0);
    if (!(m_readable || writable)) {
        return {};
    }

    return std::make_unique<TSocketMultiplexerMethodJob>(
        [this](auto j, auto r, auto w, auto e) { return serviceConnected(j, r, w, e); },
        pseudoSocket, m_readable, writable);
}

SerialSocket::EJobResult
SerialSocket::doRead()
{
    UInt8 buffer[1024];
    size_t n = ARCH->readSerial(m_serialPort, buffer, sizeof(buffer));
    if (n > 0) {
        // Debug hex dump
        std::string hexStr;
        char hex[8];
        for (size_t i = 0; i < n; ++i) {
            snprintf(hex, sizeof(hex), "%02X ", buffer[i]);
            hexStr += hex;
        }
        LOG((CLOG_PRINT "Serial incoming %d bytes: %s", (int)n, hexStr.c_str()));

        // Client: first data received confirms server is alive → cancel wakeup timer
        if (!m_isServer && m_wakeupTimer != NULL) {
            m_events->removeHandler(Event::kTimer, m_wakeupTimer);
            m_events->deleteTimer(m_wakeupTimer);
            m_wakeupTimer = NULL;
        }

        // Server: first data received → allow doWrite() to proceed
        if (m_isServer && !m_synced) {
            m_synced = true;
        }

        // Server: skip all leading 0xAA wakeup bytes (from the 256-byte flood)
        size_t offset = 0;
        if (m_isServer && m_skippingWakeup) {
            while (offset < n && buffer[offset] == 0xAA) {
                offset++;
            }
            if (offset < n) {
                // First non-0xAA byte found – stop skipping
                m_skippingWakeup = false;
            }
        }

        bool wasEmpty = (m_inputBuffer.getSize() == 0);
        if (offset < n) {
            m_inputBuffer.write(buffer + offset, (UInt32)(n - offset));
        }

        if (wasEmpty && m_inputBuffer.getSize() > 0) {
            m_events->addEvent(Event(m_events->forIStream().inputReady(), getEventTarget()));
        }
        return kRetry;
    }
    return kBreak;
}

SerialSocket::EJobResult
SerialSocket::doWrite()
{
    // Block server tx until we have received the client's wakeup flood.
    // This prevents the Barrier Hello from being lost before the client
    // is ready to read.
    if (m_isServer && !m_synced) {
        return kRetry;
    }

    UInt32 size = m_outputBuffer.getSize();
    if (size == 0) return kNew;

    const void* buffer = m_outputBuffer.peek(size);
    size_t n = ARCH->writeSerial(m_serialPort, buffer, size);
    if (n > 0) {
        LOG((CLOG_PRINT "Serial dispatched %d bytes to hardware", (int)n));
        m_outputBuffer.pop((UInt32)n);
        if (m_outputBuffer.getSize() == 0) {
            m_events->addEvent(Event(m_events->forIStream().outputFlushed(), getEventTarget()));
        }
        return kNew;
    }
    return kRetry;
}

void
SerialSocket::setJob(std::unique_ptr<ISocketMultiplexerJob>&& job)
{
    if (job.get() == nullptr) {
        m_socketMultiplexer->removeSocket(this);
    } else {
        m_socketMultiplexer->addSocket(this, std::move(job));
    }
}

void
SerialSocket::onConnected()
{
    m_connected = true;
    m_readable  = true;
    m_writable  = true;
}

void
SerialSocket::onDisconnected()
{
    m_connected = false;
    m_readable  = false;
    m_writable  = false;
}

MultiplexerJobStatus
SerialSocket::serviceConnected(ISocketMultiplexerJob* job, bool read, bool write, bool error)
{
    Lock lock(&m_mutex);
    if (error) {
        m_events->addEvent(Event(m_events->forISocket().disconnected(), getEventTarget()));
        onDisconnected();
        return {false, {}};
    }

    if (read)  doRead();
    if (write) doWrite();

    auto new_job = newJob();
    if (new_job) return {true, std::move(new_job)};
    return {false, {}};
}

void
SerialSocket::handleWakeupTimer(const Event&, void*)
{
    // Prevent zombie timers
    if (!m_connected || m_isServer || m_wakeupTimer == NULL) return;

    // Re-send a single 0xAA to continuously wake up the server's serial FIFO
    UInt8 wakeup[1] = {0xAA};
    ARCH->writeSerial(m_serialPort, wakeup, 1);

    // Reschedule
    m_events->deleteTimer(m_wakeupTimer);
    m_wakeupTimer = m_events->newOneShotTimer(0.2, NULL);
    m_events->adoptHandler(Event::kTimer, m_wakeupTimer,
        new TMethodEventJob<SerialSocket>(this, &SerialSocket::handleWakeupTimer));
}
