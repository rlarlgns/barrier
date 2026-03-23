/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2024 Gemini CLI
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include "arch/IArchSerial.h"

#define ARCH_SERIAL ArchSerialUnix

class ArchSerialImpl {
public:
    int m_fd;
    int m_refCount;
};

//! Unix implementation of IArchSerial
class ArchSerialUnix : public IArchSerial {
public:
    ArchSerialUnix();
    virtual ~ArchSerialUnix();

    // IArchSerial overrides
    virtual ArchSerialPort openSerial(const std::string& port, int baudRate);
    virtual void           closeSerial(ArchSerialPort p);
    virtual size_t         readSerial(ArchSerialPort p, void* buf, size_t len);
    virtual size_t         writeSerial(ArchSerialPort p, const void* buf, size_t len);
    virtual bool           pollSerial(ArchSerialPort p, double timeout);
};
