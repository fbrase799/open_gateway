#pragma once

#include <QString>
#include <QVector>

struct DeviceRow {
    QString id;
    QString name;
    QString status;
};

struct SourceRow {
    QString id;
    QString name;
    QString state;
    QString error;
};

QVector<DeviceRow> devices_from_json(const QByteArray& body);
QVector<SourceRow> sources_from_json(const QByteArray& body);
