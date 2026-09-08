#ifndef DEVICE_H
#define DEVICE_H

#include <QString>
#include <QVector>

struct Signal {
    QString suffix; // Приставка после тега объекта
    QString ioType; // Тип сигнала
    QString description; // Описание сигнала
};

// Класс для всех устройств
class Device {
public:
    virtual ~Device() = default;

    virtual QString tag() const = 0;
    virtual QString description() const = 0;
    virtual QString type() const = 0;
    virtual QVector<Signal> signalList() const = 0;
};

#endif
