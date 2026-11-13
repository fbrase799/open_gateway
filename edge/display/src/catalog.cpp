#include "catalog.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QVector<DeviceRow> devices_from_json(const QByteArray& body) {
    QVector<DeviceRow> rows;
    const auto document = QJsonDocument::fromJson(body);
    const auto devices = document.object().value("devices").toArray();
    for (const auto& item : devices) {
        const auto object = item.toObject();
        DeviceRow row;
        row.id = object.value("deviceId").toString();
        row.name = object.value("displayName").toString(row.id);
        row.status = object.value("status").toString();
        if (!row.id.isEmpty()) {
            rows.push_back(row);
        }
    }
    return rows;
}

QVector<SourceRow> sources_from_json(const QByteArray& body) {
    QVector<SourceRow> rows;
    const auto document = QJsonDocument::fromJson(body);
    const auto sources = document.object().value("sources").toArray();
    for (const auto& item : sources) {
        const auto object = item.toObject();
        SourceRow row;
        row.id = object.value("id").toString();
        row.name = object.value("name").toString(row.id);
        row.state = object.value("state").toString();
        row.error = object.value("error").toString();
        if (!row.id.isEmpty()) {
            rows.push_back(row);
        }
    }
    return rows;
}
