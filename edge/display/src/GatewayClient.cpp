#include "GatewayClient.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

GatewayClient::GatewayClient(QString baseUrl, DisplaySettings settings, QObject* parent)
    : QObject(parent), baseUrl_(std::move(baseUrl)), settings_(std::move(settings)) {
    network_ = new QNetworkAccessManager(this);
    connect(&timer_, &QTimer::timeout, this, &GatewayClient::refresh);
    timer_.start(1000);
    refresh();
}

void GatewayClient::refresh() {
    auto* devices = network_->get(QNetworkRequest(QUrl(baseUrl_ + QStringLiteral("/api/v1/devices"))));
    connect(devices, &QNetworkReply::finished, this, [this, devices] {
        devices->deleteLater();
        if (devices->error() != QNetworkReply::NoError) {
            link_ = QStringLiteral("offline");
            emit changed();
            return;
        }
        devicesBody_ = devices->readAll();
        link_ = QStringLiteral("live");
        rebuild();
    });
    auto* support = network_->get(QNetworkRequest(QUrl(baseUrl_ + QStringLiteral("/api/v1/support"))));
    connect(support, &QNetworkReply::finished, this, [this, support] {
        support->deleteLater();
        if (support->error() != QNetworkReply::NoError) {
            return;
        }
        supportBody_ = support->readAll();
        rebuild();
    });
}

void GatewayClient::rebuild() {
    const auto view = catalog_from_json(devicesBody_, supportBody_, settings_);
    sensors_.clear();
    for (const auto& row : view.sensors) {
        sensors_.push_back(QVariantMap{{QStringLiteral("id"), row.id},
                                       {QStringLiteral("name"), row.name},
                                       {QStringLiteral("availability"), row.availability},
                                       {QStringLiteral("canToggle"), row.canToggle},
                                       {QStringLiteral("detail"), row.detail}});
    }
    dashboard_.clear();
    for (const auto& card : view.dashboard) {
        dashboard_.push_back(QVariantMap{{QStringLiteral("id"), card.id},
                                         {QStringLiteral("name"), card.name},
                                         {QStringLiteral("status"), card.status},
                                         {QStringLiteral("values"), card.values},
                                         {QStringLiteral("commandId"), card.commandId},
                                         {QStringLiteral("commandName"), card.commandName}});
    }
    emit changed();
}

void GatewayClient::setSourceActive(const QString& id, bool active) {
    QNetworkRequest request(QUrl(baseUrl_ + QStringLiteral("/api/v1/support/sources/") + id));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QJsonObject body{{QStringLiteral("state"), active ? QStringLiteral("active") : QStringLiteral("inactive")},
                     {QStringLiteral("settings"), QJsonObject{}}};
    auto* reply = network_->put(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        refresh();
    });
}

void GatewayClient::sendCommand(const QString& deviceId, const QString& capabilityId, double value) {
    QNetworkRequest request(QUrl(baseUrl_ + QStringLiteral("/api/v1/devices/") + deviceId + QStringLiteral("/commands/") +
                                 capabilityId));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QJsonObject body{{QStringLiteral("value"), value}};
    auto* reply = network_->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}
