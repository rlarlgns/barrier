/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2024 Gemini CLI
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include "net/ISocketFactory.h"
#include <string>

class IEventQueue;
class SocketMultiplexer;

//! Socket factory for serial sockets
class SerialSocketFactory : public ISocketFactory {
public:
    SerialSocketFactory(IEventQueue* events, SocketMultiplexer* socketMultiplexer, const std::string& port, int baudRate);
    virtual ~SerialSocketFactory();

    // ISocketFactory overrides
    virtual IDataSocket* create(IArchNetwork::EAddressFamily family,
                                ConnectionSecurityLevel security_level) const;

    virtual IListenSocket* createListen(IArchNetwork::EAddressFamily family,
                                        ConnectionSecurityLevel security_level) const;

private:
    IEventQueue*        m_events;
    SocketMultiplexer*    m_socketMultiplexer;
    std::string         m_portName;
    int                 m_baudRate;
};
