#ifndef VALVE_H
#define VALVE_H

#include "Device.h"

class Valve : public Device {
public:
    Valve(const QString& number = QString(), const QString& index = QString(), const QString& prefix = "VLV");

    void setPrefix(const QString& prefix);
    void setNumber(int number);
    void setNumber(const QString& number);
    QString number() const;
    void setIndex(const QString& index);

    QString tag() const override;
    QString description() const override;
    QString type() const override;
    QVector<Signal> signalList() const override;

private:
    QString m_prefix;
    QString m_number;
    QString m_index;
};

#endif
