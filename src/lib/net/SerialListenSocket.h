/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2024 Gemini CLI
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include "net/IListenSocket.h"
#include <string>
#include <memory>

class IEventQueue;
class SocketMultiplexer;
class SerialSocket;
class EventQueueTimer;

//! Serial listen socket
class SerialListenSocket : public IListenSocket {
public:
    SerialListenSocket(IEventQueue* events, SocketMultiplexer* socketMultiplexer, const std::string& port, int baudRate);
    virtual ~SerialListenSocket();

    // ISocket overrides
    virtual void        bind(const NetworkAddress&);
    virtual void        close();
    virtual void*        getEventTarget() const;

    // IListenSocket overrides
    virtual IDataSocket* accept();

private:
    void                handleTimer(const Event&, void*);

    IEventQueue*        m_events;
    SocketMultiplexer*    m_socketMultiplexer;
    std::string         m_portName;
    int                 m_baudRate;
    std::weak_ptr<bool> m_activeConnection;
    EventQueueTimer*    m_timer;
};
