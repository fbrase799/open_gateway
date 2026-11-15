#include "catalog.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

namespace {

QString value_text(const QJsonValue& value) {
    if (value.isDouble()) {
        return QString::number(value.toDouble(), 'g', 6);
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isString()) {
        return value.toString();
    }
    return {};
}

bool simulated_source(const QJsonObject& source) {
    if (source.contains(QStringLiteral("simulated"))) {
        return source.value(QStringLiteral("simulated")).toBool();
    }
    return source.value(QStringLiteral("id")).toString().endsWith(QStringLiteral("-sim"));
}

QString simulator_list_name(QString name) {
    if (name.contains(QLatin1Char('\n'))) {
        return name;
    }
    const auto suffix = QStringLiteral(" Simulator");
    if (name.endsWith(suffix)) {
        name.chop(suffix.size());
    }
    return name + QLatin1Char('\n') + QStringLiteral("Simulator");
}

const DeviceEntry* configured_entry(const DisplaySettings& settings, bool simulated, const QString& id) {
    const auto& table = simulated ? settings.simulators : settings.devices;
    const auto found = table.constFind(id);
    if (found == table.cend()) {
        return nullptr;
    }
    return &found.value();
}

QString first_line(const QString& name) {
    const auto split = name.indexOf(QLatin1Char('\n'));
    return split < 0 ? name : name.left(split);
}

}  // namespace

CatalogView catalog_from_json(const QByteArray& devices_body, const QByteArray& support_body,
                              const DisplaySettings& settings) {
    const auto device_items = QJsonDocument::fromJson(devices_body).object().value(QStringLiteral("devices")).toArray();
    const auto source_items = QJsonDocument::fromJson(support_body).object().value(QStringLiteral("sources")).toArray();
    CatalogView view;
    for (const auto& source_value : source_items) {
        const auto source = source_value.toObject();
        SensorRow row;
        row.id = source.value(QStringLiteral("id")).toString();
        if (row.id.isEmpty()) {
            continue;
        }
        const bool simulated = simulated_source(source);
        const auto* entry = configured_entry(settings, simulated, row.id);
        if (entry != nullptr && !entry->enabled) {
            continue;
        }
        row.name = source.value(QStringLiteral("name")).toString(row.id);
        row.detail = source.value(QStringLiteral("error")).toString();
        if (entry != nullptr && !entry->name.isEmpty()) {
            row.name = entry->name;
        }
        if (simulated) {
            row.name = simulator_list_name(row.name);
        }
        const bool active = source.value(QStringLiteral("state")).toString() == QStringLiteral("active");
        bool real_live = false;
        QVector<QJsonObject> owned;
        for (const auto& device_value : device_items) {
            const auto device = device_value.toObject();
            if (device.value(QStringLiteral("pluginId")).toString() != row.id) {
                continue;
            }
            owned.push_back(device);
            if (device.value(QStringLiteral("status")).toString() == QStringLiteral("connected")) {
                real_live = true;
            }
        }
        const bool enabled = active && (simulated || real_live);
        const bool greyed = !simulated && !enabled && row.detail.isEmpty();
        if (greyed) {
            row.availability = QStringLiteral("greyed");
            row.canToggle = false;
        } else if (enabled) {
            row.availability = QStringLiteral("enabled");
            row.canToggle = true;
        } else {
            row.availability = QStringLiteral("disabled");
            row.canToggle = simulated;
        }
        view.sensors.push_back(row);
        if (!enabled) {
            continue;
        }
        for (const auto& device : owned) {
            DashboardCard card;
            card.id = device.value(QStringLiteral("deviceId")).toString();
            if (card.id.isEmpty()) {
                continue;
            }
            card.name = device.value(QStringLiteral("displayName")).toString(card.id);
            if (entry != nullptr && !entry->name.isEmpty()) {
                card.name = first_line(entry->name);
            }
            card.status = device.value(QStringLiteral("status")).toString();
            QStringList parts;
            for (const auto& capability_value : device.value(QStringLiteral("capabilities")).toArray()) {
                const auto capability = capability_value.toObject();
                const auto kind = capability.value(QStringLiteral("kind")).toString();
                if (kind == QStringLiteral("command") && card.commandId.isEmpty()) {
                    card.commandId = capability.value(QStringLiteral("id")).toString();
                    card.commandName = capability.value(QStringLiteral("displayName")).toString(card.commandId);
                    continue;
                }
                if (kind != QStringLiteral("measurement")) {
                    continue;
                }
                const auto current = capability.value(QStringLiteral("current")).toObject();
                const auto text = value_text(current.value(QStringLiteral("value")));
                if (text.isEmpty()) {
                    continue;
                }
                const auto label = capability.value(QStringLiteral("displayName")).toString(capability.value(QStringLiteral("id")).toString());
                const auto unit = capability.value(QStringLiteral("unit")).toString();
                parts.push_back(unit.isEmpty() ? label + QStringLiteral(" ") + text : label + QStringLiteral(" ") + text + QStringLiteral(" ") + unit);
            }
            card.values = parts.join(QStringLiteral("  ·  "));
            view.dashboard.push_back(card);
        }
    }
    return view;
}
