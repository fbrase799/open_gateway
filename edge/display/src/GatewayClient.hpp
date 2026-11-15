#pragma once

#include "catalog.hpp"

#include <QByteArray>
#include <QObject>
#include <QTimer>
#include <QVariant>

class QNetworkAccessManager;

class GatewayClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList sensors READ sensors NOTIFY changed)
    Q_PROPERTY(QVariantList dashboard READ dashboard NOTIFY changed)
    Q_PROPERTY(QString link READ link NOTIFY changed)

public:
    explicit GatewayClient(QString baseUrl, QObject* parent = nullptr);
    QVariantList sensors() const { return sensors_; }
    QVariantList dashboard() const { return dashboard_; }
    QString link() const { return link_; }
    Q_INVOKABLE void setSourceActive(const QString& id, bool active);
    Q_INVOKABLE void sendCommand(const QString& deviceId, const QString& capabilityId, double value);

signals:
    void changed();

private:
    void refresh();
    void rebuild();

    QString baseUrl_;
    QNetworkAccessManager* network_ = nullptr;
    QTimer timer_;
    QByteArray devicesBody_;
    QByteArray supportBody_;
    QVariantList sensors_;
    QVariantList dashboard_;
    QString link_ = QStringLiteral("connecting");
};
