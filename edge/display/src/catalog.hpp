#pragma once

#include "display_config.hpp"

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

// settings supplies the names and the enabled flags from display.toml.
CatalogView catalog_from_json(const QByteArray& devices_body, const QByteArray& support_body,
                              const DisplaySettings& settings = {});
