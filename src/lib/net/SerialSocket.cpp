/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2024 Gemini CLI
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "net/SerialSocket.h"
#include "net/SocketMultiplexer.h"
#include "net/TSocketMultiplexerMethodJob.h"
#include "mt/Lock.h"
#include "arch/Arch.h"
#include "base/Log.h"
#include "base/IEventQueue.h"
#include "base/XBase.h"
#include "base/TMethodEventJob.h"
#include "base/Event.h"
#include "arch/XArch.h"
#include <cstring>

static const std::size_t MAX_INPUT_BUFFER_SIZE = 1024 * 1024;

SerialSocket::SerialSocket(IEventQueue* events, SocketMultiplexer* socketMultiplexer, const std::string& port, int baudRate, bool isServer, std::shared_ptr<bool> activeConnection) :
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
    m_resetMatchIdx(0),
    m_ignoreResets(true),
    m_immunityTimer(NULL),
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
    // Serial ports don't really bind in the network sense
}

void
SerialSocket::close()
{
    setJob(NULL);

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

    if (m_immunityTimer != NULL) {
        m_events->removeHandler(Event::kTimer, m_immunityTimer);
        m_events->deleteTimer(m_immunityTimer);
        m_immunityTimer = NULL;
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
    if (n > size) {
        n = size;
    }
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
    // For serial, we'll just assume it flushes quickly or we don't block
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
            // Client: write a dummy "wake up" burst of bytes BEFORE the reset sequence.
            // The server will discard these before processing real Barrier greetings.
            UInt8 wakeup[1] = {0xAA};
            ARCH->writeSerial(m_serialPort, wakeup, 1);
            
            // start periodic ping
            m_wakeupTimer = m_events->newOneShotTimer(0.2, NULL);
            m_events->adoptHandler(Event::kTimer, m_wakeupTimer, new TMethodEventJob<SerialSocket>(this, &SerialSocket::handleWakeupTimer));
        }

        // BOTH Client and Server: write a 16-byte In-Band Reset sequence to kill any hanging connections on the other side
        const UInt8 resetSeq[] = "BARRIER_RESET_RX";
        ARCH->writeSerial(m_serialPort, resetSeq, 16);
        
        // Start a 5.0s immunity window so we don't kill ourselves with the other side's reset burst
        m_ignoreResets = true;
        m_immunityTimer = m_events->newOneShotTimer(5.0, NULL);
        m_events->adoptHandler(Event::kTimer, m_immunityTimer, new TMethodEventJob<SerialSocket>(this, &SerialSocket::handleImmunityTimer));

        m_events->addEvent(Event(m_events->forIDataSocket().connected(), getEventTarget()));
        
        // Use a small trick: since SerialPort is not an ArchSocket, 
        // and SocketMultiplexer expects ArchSocket, we might need a workaround.
        // For now, let's hope we can cast the fd to ArchSocket on Unix.
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

    // This is the risky part: casting ArchSerialPort to ArchSocket.
    // On Unix, both are based on file descriptors.
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
        bool wasEmpty = (m_inputBuffer.getSize() == 0); // Capture empty state BEFORE any writes

        // [DEBUG LOGGING] Dump incoming exact hex bytes to the user interface
        std::string hexStr = "";
        char hex[8];
        for (size_t i = 0; i < n; ++i) {
            snprintf(hex, sizeof(hex), "%02X ", buffer[i]);
            hexStr += hex;
        }
        LOG((CLOG_PRINT "Serial incoming %d bytes: %s", (int)n, hexStr.c_str()));
        // If client, any received data means the server is awake and talking.
        if (!m_isServer && m_wakeupTimer != NULL) {
            m_events->removeHandler(Event::kTimer, m_wakeupTimer);
            m_events->deleteTimer(m_wakeupTimer);
            m_wakeupTimer = NULL;
        }

        if (m_isServer && !m_synced) {
            m_synced = true;
        }

        // --- IN-BAND RESET SEQUENCE SCANNER ---
        const UInt8 resetSeq[] = "BARRIER_RESET_RX";
        const size_t resetSeqLen = 16;
        bool disconnected = false;
        
        for (size_t i = 0; i < n; ++i) {
            UInt8 b = buffer[i];

            if (m_isServer && m_skippingWakeup) {
                if (b == 0xAA) {
                    continue;
                }
                m_skippingWakeup = false;
            }

            if (b == resetSeq[m_resetMatchIdx]) {
                m_resetMatchIdx++;
                if (m_resetMatchIdx == resetSeqLen) {
                    m_resetMatchIdx = 0;
                    if (!m_ignoreResets) {
                        LOG((CLOG_NOTE "Serial In-Band Reset detected! Killing stuck old connection."));
                        disconnected = true;
                        break;
                    } else {
                        LOG((CLOG_DEBUG1 "Serial In-Band Reset strictly ignored due to immunity window."));
                    }
                }
            } else {
                if (m_resetMatchIdx > 0) {
                    m_inputBuffer.write(resetSeq, (UInt32)m_resetMatchIdx);
                    m_resetMatchIdx = 0;
                    if (b == resetSeq[0]) {
                        m_resetMatchIdx = 1;
                        continue;
                    }
                }
                m_inputBuffer.write(&b, 1);
            }
        }

        if (disconnected) {
            m_events->addEvent(Event(m_events->forISocket().disconnected(), getEventTarget()));
            onDisconnected();
            return kBreak;
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
    if (m_isServer && !m_synced) {
        // Block writing until we receive the wakeup trigger from the client.
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

    if (read) doRead();
    if (write) doWrite();

    auto new_job = newJob();
    if (new_job) return {true, std::move(new_job)};
    return {false, {}};
}

void
SerialSocket::handleWakeupTimer(const Event&, void*)
{
    // Prevent zombie timers from queued events that ran after timer cancellation
    if (!m_connected || m_isServer || m_wakeupTimer == NULL) return;
    
    // Resend a single 0xAA byte to ensure the server wakes up
    UInt8 wakeup[1] = {0xAA};
    ARCH->writeSerial(m_serialPort, wakeup, 1);
    
    // Reschedule the timer
    m_events->deleteTimer(m_wakeupTimer);
    m_wakeupTimer = m_events->newOneShotTimer(0.2, NULL);
    m_events->adoptHandler(Event::kTimer, m_wakeupTimer, new TMethodEventJob<SerialSocket>(this, &SerialSocket::handleWakeupTimer));
}

void
SerialSocket::handleImmunityTimer(const Event&, void*)
{
    m_ignoreResets = false;
    if (m_immunityTimer != NULL) {
        m_events->deleteTimer(m_immunityTimer);
        m_immunityTimer = NULL;
    }
}
