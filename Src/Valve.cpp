#include "Valve.h"

Valve::Valve(const QString& number, const QString& index, const QString& prefix)
    : m_prefix(prefix), m_number(number.trimmed()), m_index(index.trimmed())
{
}

void Valve::setPrefix(const QString& prefix)
{
    m_prefix = prefix;
}

void Valve::setNumber(int number)
{
    m_number = QString::number(number);
}

void Valve::setNumber(const QString& number)
{
    m_number = number.trimmed();
}

QString Valve::number() const
{
    return m_number;
}

void Valve::setIndex(const QString& index)
{
    m_index = index.trimmed();
}

QString Valve::tag() const {
    QString result;
    if (!m_prefix.isEmpty()) {
        result = m_prefix;
    }
    if (!m_number.isEmpty()) {
        if (!result.isEmpty()) {
            result += "_";
        }
        result += m_number;
    }
    if (!m_index.isEmpty()) {
        if (!result.isEmpty()) {
            result += "_";
        }
        result += m_index;
    }
    return result;
}

QString Valve::description() const {
    QString result = "Задвижка";
    if (!m_number.isEmpty()) {
        result += " " + m_number;
    }
    if (!m_index.isEmpty()) {
        result += "_" + m_index;
    }
    return result;
}

QString Valve::type() const {
    return "Valve";
}

QVector<Signal> Valve::signalList() const {
    return {
        {"_OPN", "DI", "Открыта"},
        {"_CLS", "DI", "Закрыта"},
        {"_OPN_CMD", "DO", "Команда открыть"},
        {"_CLS_CMD", "DO", "Команда закрыть"}
    };
}