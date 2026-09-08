#include "DeviceFactory.h"
#include "Valve.h"

std::shared_ptr<Device> DeviceFactory::createDevice(const QString& type,
                                                    const QString& number,
                                                    const QString& index,
                                                    const QString& prefix)
{
    if (type == "Valve") {
        return std::make_shared<Valve>(number, index, prefix);
    }
    // Для будущих типов добавить сюда
    return nullptr;
}

std::shared_ptr<Device> DeviceFactory::createDevice(const QString& type,
                                                    int number,
                                                    const QString& index,
                                                    const QString& prefix)
{
    return createDevice(type, QString::number(number), index, prefix);
}

QStringList DeviceFactory::availableTypes() {
    return {"Valve"};
}