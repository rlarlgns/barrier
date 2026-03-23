#ifndef SERIALPORTSCANNER_H
#define SERIALPORTSCANNER_H

#include <QStringList>

class SerialPortScanner
{
public:
    static QStringList availablePorts();
};

#endif // SERIALPORTSCANNER_H
