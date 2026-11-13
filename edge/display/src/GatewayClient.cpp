#include "GatewayClient.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

GatewayClient::GatewayClient(QString baseUrl, QObject* parent) : QObject(parent), baseUrl_(std::move(baseUrl)) {
    network_ = new QNetworkAccessManager(this);
    connect(&timer_, &QTimer::timeout, this, &GatewayClient::refresh);
    timer_.start(1000);
    refresh();
}

void GatewayClient::refresh() {
    auto* devices = network_->get(QNetworkRequest(QUrl(baseUrl_ + "/api/v1/devices")));
    connect(devices, &QNetworkReply::finished, this, [this, devices] {
        devices->deleteLater();
        if (devices->error() != QNetworkReply::NoError) {
            link_ = "offline";
            emit changed();
            return;
        }
        devices_ = to_variants(devices_from_json(devices->readAll()));
        link_ = "live";
        emit changed();
    });
    auto* support = network_->get(QNetworkRequest(QUrl(baseUrl_ + "/api/v1/support")));
    connect(support, &QNetworkReply::finished, this, [this, support] {
        support->deleteLater();
        if (support->error() != QNetworkReply::NoError) {
            return;
        }
        sources_ = to_variants(sources_from_json(support->readAll()));
        emit changed();
    });
}

void GatewayClient::setSourceActive(const QString& id, bool active) {
    QNetworkRequest request(QUrl(baseUrl_ + "/api/v1/support/sources/" + id));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body{{"state", active ? "active" : "inactive"}, {"settings", QJsonObject{}}};
    auto* reply = network_->put(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        refresh();
    });
}

QVariantList GatewayClient::to_variants(const QVector<DeviceRow>& rows) const {
    QVariantList list;
    for (const auto& row : rows) {
        list.push_back(QVariantMap{{"id", row.id}, {"name", row.name}, {"status", row.status}});
    }
    return list;
}

QVariantList GatewayClient::to_variants(const QVector<SourceRow>& rows) const {
    QVariantList list;
    for (const auto& row : rows) {
        list.push_back(QVariantMap{{"id", row.id}, {"name", row.name}, {"state", row.state}, {"error", row.error}});
    }
    return list;
}
