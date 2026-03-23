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

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define ARCH_SERIAL ArchSerialWin32

class ArchSerialImpl {
public:
    HANDLE m_handle;
    int    m_refCount;
};

//! Win32 implementation of IArchSerial
class ArchSerialWin32 : public IArchSerial {
public:
    ArchSerialWin32();
    virtual ~ArchSerialWin32();

    // IArchSerial overrides
    virtual ArchSerialPort openSerial(const std::string& port, int baudRate);
    virtual void           closeSerial(ArchSerialPort p);
    virtual size_t         readSerial(ArchSerialPort p, void* buf, size_t len);
    virtual size_t         writeSerial(ArchSerialPort p, const void* buf, size_t len);
    virtual bool           pollSerial(ArchSerialPort p, double timeout);
};
