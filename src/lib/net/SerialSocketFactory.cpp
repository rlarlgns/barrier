/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2024 Gemini CLI
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "net/SerialSocketFactory.h"
#include "net/SerialSocket.h"
#include "net/SerialListenSocket.h"

SerialSocketFactory::SerialSocketFactory(IEventQueue* events, SocketMultiplexer* socketMultiplexer, const std::string& port, int baudRate) :
    m_events(events),
    m_socketMultiplexer(socketMultiplexer),
    m_portName(port),
    m_baudRate(baudRate)
{
}

SerialSocketFactory::~SerialSocketFactory()
{
}

IDataSocket*
SerialSocketFactory::create(IArchNetwork::EAddressFamily family,
                            ConnectionSecurityLevel security_level) const
{
    // For serial, we ignore security_level for now as requested.
    return new SerialSocket(m_events, m_socketMultiplexer, m_portName, m_baudRate, false);
}

IListenSocket*
SerialSocketFactory::createListen(IArchNetwork::EAddressFamily family,
                                  ConnectionSecurityLevel security_level) const
{
    return new SerialListenSocket(m_events, m_socketMultiplexer, m_portName, m_baudRate);
}
