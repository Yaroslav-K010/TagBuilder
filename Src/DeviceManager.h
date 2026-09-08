#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QVector>
#include <memory>
#include "Device.h"

class DeviceManager {
public:
    static QString storageFilePath();

    void addDevice(std::shared_ptr<Device> device);
    void clear();
    const QVector<std::shared_ptr<Device>>& devices() const;
    bool containsTag(const QString& tag) const;
    bool removeDeviceAt(int index);
    bool loadFromFile(const QString& filePath);
    bool importFromCsv(const QString& filePath);
    bool saveToFile(const QString& filePath) const;
    QString exportToCsv() const;

private:
    QVector<std::shared_ptr<Device>> m_devices;
};

#endif // DEVICEMANAGER_H