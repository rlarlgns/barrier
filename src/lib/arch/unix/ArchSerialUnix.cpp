/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2024 Gemini CLI
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "arch/unix/ArchSerialUnix.h"
#include "arch/unix/XArchUnix.h"
#include "arch/Arch.h"
#include "base/Log.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <cstring>

ArchSerialUnix::ArchSerialUnix()
{
}

ArchSerialUnix::~ArchSerialUnix()
{
}

ArchSerialPort
ArchSerialUnix::openSerial(const std::string& port, int baudRate)
{
    int fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        throw XArchNetworkIO("Could not open serial port: " + port + " (" + strerror(errno) + ")");
    }

    struct termios options;
    tcgetattr(fd, &options);

    // Force raw binary mode, disabling all line disciplines and text translations
    cfmakeraw(&options);

    speed_t speed;
    switch (baudRate) {
        case 9600:   speed = B9600;   break;
        case 19200:  speed = B19200;  break;
        case 38400:  speed = B38400;  break;
        case 57600:  speed = B57600;  break;
        case 115200: speed = B115200; break;
        case 230400: speed = B230400; break;
        default:     speed = B115200; break;
    }

    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    
    // Disable hardware flow control, preventing write() blocks on 3-wire cables
#ifdef CRTSCTS
    options.c_cflag &= ~CRTSCTS;
#elif defined(CNEW_RTSCTS)
    options.c_cflag &= ~CNEW_RTSCTS;
#endif

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;

    tcflush(fd, TCIOFLUSH);
    tcsetattr(fd, TCSAFLUSH, &options);

    ArchSerialImpl* p = new ArchSerialImpl;
    p->m_fd = fd;
    p->m_refCount = 2; // Keep alive indefinitely vs SocketMultiplexer pseudo-ref
    return p;
}

void
ArchSerialUnix::closeSerial(ArchSerialPort p)
{
    if (p != NULL) {
        close(p->m_fd);
        delete p;
    }
}

size_t
ArchSerialUnix::readSerial(ArchSerialPort p, void* buf, size_t len)
{
    ssize_t n = read(p->m_fd, buf, len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EINTR) {
            return 0;
        }
        throw XArchNetworkIO("Read from serial port failed");
    }
    return static_cast<size_t>(n);
}

size_t
ArchSerialUnix::writeSerial(ArchSerialPort p, const void* buf, size_t len)
{
    ssize_t n = write(p->m_fd, buf, len);
    if (n < 0) {
        throw XArchNetworkIO("Write to serial port failed");
    }
    return static_cast<size_t>(n);
}

bool
ArchSerialUnix::pollSerial(ArchSerialPort p, double timeout)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(p->m_fd, &rfds);

    struct timeval tv;
    tv.tv_sec = static_cast<int>(timeout);
    tv.tv_usec = static_cast<int>((timeout - tv.tv_sec) * 1000000);

    int retval = select(p->m_fd + 1, &rfds, NULL, NULL, (timeout < 0 ? NULL : &tv));
    if (retval == -1) {
        if (errno == EINTR) return false;
        throw XArchNetworkIO("Poll serial port failed");
    }
    return (retval > 0);
}
