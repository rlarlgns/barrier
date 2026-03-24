/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2024 Gemini CLI
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include "net/IDataSocket.h"
#include "net/ISocketMultiplexerJob.h"
#include "io/StreamBuffer.h"
#include "mt/Mutex.h"
#include "arch/IArchSerial.h"
#include <memory>

class IEventQueue;
class SocketMultiplexer;
class EventQueueTimer;

//! Serial data socket
class SerialSocket : public IDataSocket {
public:
    SerialSocket(IEventQueue* events, SocketMultiplexer* socketMultiplexer, const std::string& port, int baudRate, bool isServer = false, std::shared_ptr<bool> activeConnection = nullptr);
    virtual ~SerialSocket();

    // ISocket overrides
    virtual void        bind(const NetworkAddress&);
    virtual void        close();
    virtual void*        getEventTarget() const;

    // IStream overrides
    virtual UInt32        read(void* buffer, UInt32 n);
    virtual void        write(const void* buffer, UInt32 n);
    virtual void        flush();
    virtual void        shutdownInput();
    virtual void        shutdownOutput();
    virtual bool        isReady() const;
    virtual bool        isFatal() const;
    virtual UInt32        getSize() const;

    // IDataSocket overrides
    virtual void        connect(const NetworkAddress&);

    virtual std::unique_ptr<ISocketMultiplexerJob> newJob();

protected:
    enum EJobResult {
        kBreak = -1,
        kRetry,
        kNew
    };

    virtual EJobResult    doRead();
    virtual EJobResult    doWrite();

    void setJob(std::unique_ptr<ISocketMultiplexerJob>&& job);

private:
    void                onConnected();
    void                onDisconnected();
    MultiplexerJobStatus serviceConnected(ISocketMultiplexerJob*, bool, bool, bool);
    void                handleWakeupTimer(const Event&, void*);
    void                handleImmunityTimer(const Event&, void*);

private:
    IEventQueue*        m_events;
    SocketMultiplexer*    m_socketMultiplexer;
    std::string         m_portName;
    int                 m_baudRate;
    ArchSerialPort      m_serialPort;
    bool                m_connected;
    bool                m_readable;
    bool                m_writable;
    StreamBuffer        m_inputBuffer;
    StreamBuffer        m_outputBuffer;
    Mutex               m_mutex;
    bool                m_isServer;
    bool                m_synced;
    bool                m_skippingWakeup;
    EventQueueTimer*    m_wakeupTimer;
    size_t              m_resetMatchIdx;
    bool                m_ignoreResets;
    EventQueueTimer*    m_immunityTimer;
    std::shared_ptr<bool> m_activeConnection;
};
