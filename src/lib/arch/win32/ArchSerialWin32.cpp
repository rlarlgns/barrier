/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2024 Gemini CLI
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "arch/win32/ArchSerialWin32.h"
#include "arch/XArch.h"
#include "base/Log.h"

ArchSerialWin32::ArchSerialWin32()
{
}

ArchSerialWin32::~ArchSerialWin32()
{
}

ArchSerialPort
ArchSerialWin32::openSerial(const std::string& port, int baudRate)
{
    std::string portName = "\\\\.\\" + port;
    HANDLE hSerial = CreateFileA(portName.c_str(),
                                GENERIC_READ | GENERIC_WRITE,
                                0,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);

    if (hSerial == INVALID_HANDLE_VALUE) {
        throw XArchNetworkIO("Could not open serial port: " + port);
    }

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        CloseHandle(hSerial);
        throw XArchNetworkIO("GetCommState failed");
    }

    dcbSerialParams.BaudRate = baudRate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity   = NOPARITY;
    dcbSerialParams.fBinary  = TRUE;
    dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;
    dcbSerialParams.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        CloseHandle(hSerial);
        throw XArchNetworkIO("SetCommState failed");
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout         = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant    = 0;
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    timeouts.WriteTotalTimeoutConstant   = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hSerial, &timeouts)) {
        CloseHandle(hSerial);
        throw XArchNetworkIO("SetCommTimeouts failed");
    }

    ArchSerialImpl* p = new ArchSerialImpl;
    p->m_handle = hSerial;
    p->m_refCount = 2; // Prevent multiplexer job cleanup from destroying this object
    return p;
}

void
ArchSerialWin32::closeSerial(ArchSerialPort p)
{
    if (p != NULL) {
        CloseHandle(p->m_handle);
        delete p;
    }
}

size_t
ArchSerialWin32::readSerial(ArchSerialPort p, void* buf, size_t len)
{
    DWORD bytesRead;
    if (!ReadFile(p->m_handle, buf, (DWORD)len, &bytesRead, NULL)) {
        throw XArchNetworkIO("ReadFile failed");
    }
    return (size_t)bytesRead;
}

size_t
ArchSerialWin32::writeSerial(ArchSerialPort p, const void* buf, size_t len)
{
    DWORD bytesWritten;
    if (!WriteFile(p->m_handle, buf, (DWORD)len, &bytesWritten, NULL)) {
        throw XArchNetworkIO("WriteFile failed");
    }
    return (size_t)bytesWritten;
}

bool
ArchSerialWin32::pollSerial(ArchSerialPort p, double timeout)
{
    // Simplified polling for Win32
    COMSTAT comStat;
    DWORD errors;
    if (ClearCommError(p->m_handle, &errors, &comStat)) {
        if (comStat.cbInQue > 0) return true;
    }
    
    if (timeout > 0) {
        Sleep((DWORD)(timeout * 1000));
        if (ClearCommError(p->m_handle, &errors, &comStat)) {
            return (comStat.cbInQue > 0);
        }
    }
    return false;
}
