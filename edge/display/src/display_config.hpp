#pragma once

#include <QHash>
#include <QString>

// One device the display may show. enabled false removes it from the sensor
// list and the dashboard. An empty name keeps the name reported by the gateway.
struct DeviceEntry {
    bool enabled = true;
    QString name;
};

struct DisplaySettings {
    QString title;
    QHash<QString, DeviceEntry> simulators;
    QHash<QString, DeviceEntry> devices;
};

// A missing file or a missing title falls back to "Open Gateway".
// A device that is absent from these tables stays visible under its gateway name.
DisplaySettings display_settings_from_toml(const QString& text);
DisplaySettings load_display_settings(const QString& path);
QString display_title_from_toml(const QString& text);
