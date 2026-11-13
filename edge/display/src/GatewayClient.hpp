#pragma once

#include "catalog.hpp"

#include <QObject>
#include <QTimer>
#include <QVariant>

class QNetworkAccessManager;

class GatewayClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY changed)
    Q_PROPERTY(QString link READ link NOTIFY changed)

public:
    explicit GatewayClient(QString baseUrl, QObject* parent = nullptr);
    QVariantList devices() const { return devices_; }
    QVariantList sources() const { return sources_; }
    QString link() const { return link_; }
    Q_INVOKABLE void setSourceActive(const QString& id, bool active);

signals:
    void changed();

private:
    void refresh();
    QVariantList to_variants(const QVector<DeviceRow>& rows) const;
    QVariantList to_variants(const QVector<SourceRow>& rows) const;

    QString baseUrl_;
    QNetworkAccessManager* network_ = nullptr;
    QTimer timer_;
    QVariantList devices_;
    QVariantList sources_;
    QString link_ = "connecting";
};
