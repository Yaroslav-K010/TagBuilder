#ifndef DEVICEFACTORY_H
#define DEVICEFACTORY_H

#include <QString>
#include <QStringList>
#include <memory>
#include "Device.h"

class DeviceFactory {
public:
    // Возвращает nullptr, если тип неизвестен
    static std::shared_ptr<Device> createDevice(const QString& type,
                                                const QString& number,
                                                const QString& index = QString(),
                                                const QString& prefix = "VLV");
    static std::shared_ptr<Device> createDevice(const QString& type,
                                                int number,
                                                const QString& index = QString(),
                                                const QString& prefix = "VLV");
    static QStringList availableTypes();
};

#endif // DEVICEFACTORY_H
