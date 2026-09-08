#include "DeviceManager.h"
#include "DeviceFactory.h"
#include "Valve.h"
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include <QStandardPaths>
#include <QStringConverter>
#include <limits>

namespace {
QString decodeTextFile(const QByteArray &data)
{
    if (data.isEmpty()) {
        return {};
    }

    if (data.startsWith("\xEF\xBB\xBF")) {
        return QString::fromUtf8(data.mid(3));
    }
    if (data.startsWith("\xFF\xFE")) {
        return QString::fromUtf16(reinterpret_cast<const char16_t*>(data.constData() + 2), (data.size() - 2) / 2);
    }
    if (data.startsWith("\xFE\xFF")) {
        const QByteArray swapped = data.mid(2);
        const int length = swapped.size() / 2;
        QString text;
        text.resize(length);
        auto *out = reinterpret_cast<char16_t*>(text.data());
        const auto *in = reinterpret_cast<const char16_t*>(swapped.constData());
        for (int i = 0; i < length; ++i) {
            const quint16 value = in[i];
            out[i] = static_cast<char16_t>((value >> 8) | ((value & 0xFF) << 8));
        }
        return text;
    }

    bool looksLikeUtf16 = false;
    int nullCount = 0;
    for (int i = 0; i < data.size() - 1; i += 2) {
        if (data[i] == '\0' && data[i + 1] != '\0') {
            ++nullCount;
        }
        if (data[i] != '\0' && data[i + 1] == '\0') {
            ++nullCount;
        }
    }
    looksLikeUtf16 = (data.size() >= 4 && (nullCount * 2) > data.size() / 3);

    if (looksLikeUtf16) {
        return QString::fromUtf16(reinterpret_cast<const char16_t*>(data.constData()), data.size() / 2);
    }

    const QString utf8Text = QString::fromUtf8(data);
    if (!utf8Text.contains(QChar(0xFFFD))) {
        return utf8Text;
    }

    const QString cp1251Text = QString::fromLocal8Bit(data);
    const QString latin1Text = QString::fromLatin1(data);
    if (cp1251Text.contains(QRegularExpression("[A-Za-zА-Яа-яЁё0-9_./\\-]"))) {
        return cp1251Text;
    }

    return latin1Text;
}

QStringList splitCsvCells(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    QStringList parts = trimmed.split(';', Qt::KeepEmptyParts);
    if (parts.size() <= 1) {
        parts = trimmed.split(',', Qt::KeepEmptyParts);
    }

    for (QString &part : parts) {
        part = part.trimmed();
        if (part.startsWith('"') && part.endsWith('"') && part.size() >= 2) {
            part = part.mid(1, part.size() - 2);
        }
        part.replace("\"\"", "\"");
    }
    return parts;
}
}

QString DeviceManager::storageFilePath() {
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(appData);
    if (!dir.exists()) {
        dir.mkpath(appData);
    }
    return QDir(appData).filePath("tag_constructor_devices.csv");
}

void DeviceManager::addDevice(std::shared_ptr<Device> device) {
    if (device) {
        m_devices.append(device);
    }
}

void DeviceManager::clear() {
    m_devices.clear();
}

const QVector<std::shared_ptr<Device>>& DeviceManager::devices() const {
    return m_devices;
}

bool DeviceManager::containsTag(const QString& tag) const {
    for (const auto& device : m_devices) {
        if (device && device->tag() == tag) {
            return true;
        }
    }
    return false;
}

bool DeviceManager::removeDeviceAt(int index) {
    if (index < 0 || index >= m_devices.size()) {
        return false;
    }
    m_devices.removeAt(index);
    return true;
}

bool DeviceManager::loadFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.exists()) {
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray rawData = file.readAll();
    file.close();
    const QString csvText = decodeTextFile(rawData);
    const QStringList lines = csvText.split('\n', Qt::KeepEmptyParts);

    m_devices.clear();
    for (const QString &line : lines) {
        const QStringList parts = splitCsvCells(line);
        if (parts.size() < 3) {
            continue;
        }

        const QString tag = parts.at(0).trimmed();
        const QString type = parts.at(1).trimmed();

        if (type == "Valve") {
            QString cleanedTag = tag;
            if (cleanedTag.startsWith("VLV_")) {
                cleanedTag.remove(0, 4);
            }

            QString numberStr = cleanedTag;
            QString index;
            const int underscoreIndex = cleanedTag.lastIndexOf('_');
            if (underscoreIndex != -1) {
                numberStr = cleanedTag.left(underscoreIndex);
                index = cleanedTag.mid(underscoreIndex + 1);
            }

            if (numberStr.isEmpty()) {
                continue;
            }

            auto device = DeviceFactory::createDevice(type, numberStr, index, "VLV");
            if (device) {
                m_devices.append(device);
            }
        }
    }

    return true;
}

bool DeviceManager::importFromCsv(const QString& filePath) {
    QFile file(filePath);
    if (!file.exists()) {
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray rawData = file.readAll();
    file.close();
    const QString csvText = decodeTextFile(rawData);
    const QStringList lines = csvText.split('\n', Qt::KeepEmptyParts);

    for (const QString &line : lines) {
        const QStringList parts = splitCsvCells(line);
        if (parts.size() < 3) {
            continue;
        }

        const QString tag = parts.at(0).trimmed();
        const QString type = parts.at(1).trimmed();
        const QString description = parts.at(2).trimmed();
        Q_UNUSED(description);

        QString prefix, numberText, indexText;
        if (!type.isEmpty() && type == "Valve") {
            prefix = "VLV";
            const int firstSep = tag.indexOf('_');
            if (firstSep >= 0) {
                prefix = tag.left(firstSep);
                const QString tail = tag.mid(firstSep + 1);
                const int secondSep = tail.indexOf('_');
                if (secondSep >= 0) {
                    numberText = tail.left(secondSep);
                    indexText = tail.mid(secondSep + 1);
                } else {
                    numberText = tail;
                }
            }

            if (numberText.isEmpty()) {
                continue;
            }

            auto device = DeviceFactory::createDevice(type, numberText, indexText, prefix);
            if (device && !containsTag(device->tag())) {
                m_devices.append(device);
            }
        }
    }

    return !m_devices.isEmpty();
}

bool DeviceManager::saveToFile(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << exportToCsv();
    file.close();
    return true;
}

QString DeviceManager::exportToCsv() const {
    QString csv;
    csv += "ObjectTag;ObjectType;ObjectDescription\n";

    for (const auto& device : m_devices) {
        csv += device->tag() + ";" + device->type() + ";" + device->description() + "\n";
    }
    return csv;
}