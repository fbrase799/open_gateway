#pragma once

#include <QString>
#include <QVector>

struct SensorRow {
    QString id;
    QString name;
    QString availability;
    bool canToggle = false;
    QString detail;
};

struct DashboardCard {
    QString id;
    QString name;
    QString status;
    QString values;
    QString commandId;
    QString commandName;
};

struct CatalogView {
    QVector<SensorRow> sensors;
    QVector<DashboardCard> dashboard;
};

CatalogView catalog_from_json(const QByteArray& devices_body, const QByteArray& support_body);
