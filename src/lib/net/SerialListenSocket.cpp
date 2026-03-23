/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2024 Gemini CLI
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "net/SerialListenSocket.h"
#include "net/SerialSocket.h"
#include "net/NetworkAddress.h"
#include "base/IEventQueue.h"
#include "base/TMethodEventJob.h"
#include "base/Event.h"
#include "base/Log.h"

SerialListenSocket::SerialListenSocket(IEventQueue* events, SocketMultiplexer* socketMultiplexer, const std::string& port, int baudRate) :
    m_events(events),
    m_socketMultiplexer(socketMultiplexer),
    m_portName(port),
    m_baudRate(baudRate),
    m_activeConnection(),
    m_timer(NULL)
{
}

SerialListenSocket::~SerialListenSocket()
{
    close();
}

void
SerialListenSocket::bind(const NetworkAddress&)
{
    // Start a 1 second timer to continually check if the active connection has died.
    if (m_timer == NULL) {
        m_timer = m_events->newOneShotTimer(1.0, NULL);
        m_events->adoptHandler(Event::kTimer, m_timer,
            new TMethodEventJob<SerialListenSocket>(this, &SerialListenSocket::handleTimer));
    }
    
    // For serial, we "bind" by being ready to accept.
    m_events->addEvent(Event(m_events->forIListenSocket().connecting(), getEventTarget()));
}

void
SerialListenSocket::handleTimer(const Event&, void*)
{
    if (m_activeConnection.expired()) {
        m_events->addEvent(Event(m_events->forIListenSocket().connecting(), getEventTarget()));
    }
    
    // Reschedule the timer
    m_events->deleteTimer(m_timer);
    m_timer = m_events->newOneShotTimer(1.0, NULL);
    m_events->adoptHandler(Event::kTimer, m_timer,
        new TMethodEventJob<SerialListenSocket>(this, &SerialListenSocket::handleTimer));
}

void
SerialListenSocket::close()
{
    if (m_timer != NULL) {
        m_events->removeHandler(Event::kTimer, m_timer);
        m_events->deleteTimer(m_timer);
        m_timer = NULL;
    }
}

void*
SerialListenSocket::getEventTarget() const
{
    return const_cast<void*>(static_cast<const void*>(this));
}

IDataSocket*
SerialListenSocket::accept()
{
    if (!m_activeConnection.expired()) return NULL;

    auto active = std::make_shared<bool>(true);
    m_activeConnection = active;
    
    SerialSocket* socket = new SerialSocket(m_events, m_socketMultiplexer, m_portName, m_baudRate, true, active);
    socket->connect(NetworkAddress()); // This opens the port
    return socket;
}
