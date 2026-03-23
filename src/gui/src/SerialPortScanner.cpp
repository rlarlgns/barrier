#include "SerialPortScanner.h"
#include <QtSerialPort/QSerialPortInfo>

QStringList SerialPortScanner::availablePorts()
{
    QStringList ports;
    const auto availablePorts = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : availablePorts) {
        ports << info.portName();
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
        // On Unix, full path might be needed for our backend
        if (!info.systemLocation().isEmpty()) {
            // Check if it's already in the list
            if (!ports.contains(info.systemLocation())) {
                ports << info.systemLocation();
            }
        }
#endif
    }
    ports.removeDuplicates();
    ports.sort();
    return ports;
}
