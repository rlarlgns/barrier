/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2024 Gemini CLI
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include "common/IInterface.h"
#include "common/stdstring.h"
#include "arch/IArchNetwork.h" // For ArchThread

/*!
\class ArchSerialImpl
\brief Internal serial port data.
An architecture dependent type holding the necessary data for a serial port.
*/
class ArchSerialImpl;

/*!
\var ArchSerialPort
\brief Opaque serial port type.
An opaque type representing a serial port.
*/
typedef ArchSerialImpl* ArchSerialPort;

//! Interface for architecture dependent serial communication
class IArchSerial : public IInterface {
public:
    //! @name manipulators
    //@{

    //! Open a serial port
    virtual ArchSerialPort openSerial(const std::string& port, int baudRate) = 0;

    //! Close a serial port
    virtual void           closeSerial(ArchSerialPort p) = 0;

    //! Read data from serial port
    virtual size_t         readSerial(ArchSerialPort p, void* buf, size_t len) = 0;

    //! Write data to serial port
    virtual size_t         writeSerial(ArchSerialPort p, const void* buf, size_t len) = 0;

    //! Wait for data or timeout
    virtual bool           pollSerial(ArchSerialPort p, double timeout) = 0;

    //@}
};
