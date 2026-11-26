#pragma once

#include "core/device_record.h"
#include "adapters/adapter_registry.h"

#include <QObject>
#include <QHash>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QNetworkAccessManager>
#include <QAuthenticator>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <QUdpSocket>
#include <QVector>
#include <QList>

class DiscoveryService final : public QObject
{
    Q_OBJECT
public:
    explicit DiscoveryService(QObject *parent = nullptr);
    // Empty selects every eligible interface; a non-empty name scopes discovery to one interface.
    void start(const QString &interfaceName = QString());
    void cancel();
    bool isRunning() const;
    void refreshDeviceInfo(const DeviceRecord &device);
    void factoryReset(const DeviceRecord &device, const QString &factoryDefaultType);
    void applyStaticIpv4(const DeviceRecord &device, const QString &address,
                         const QString &subnetMask, const QString &gateway);
    void configureDhcp(const DeviceRecord &device, bool enabled);
    void configureDns(const DeviceRecord &device, bool fromDhcp, const QStringList &ipv4Servers);
    void rebootDevice(const DeviceRecord &device);

signals:
    void deviceFound(const DeviceRecord &device);
    void statusChanged(const QString &message);
    void finished(int deviceCount);
    void cancelled(int deviceCount);
    void authenticationRequested(QAuthenticator *authenticator);
    void networkConfigurationFinished(const DeviceRecord &device, bool verified,
                                      const QString &message);
    void maintenanceOperationFinished(const DeviceRecord &device, const QString &operation,
                                       bool succeeded, const QString &message);

private:
    struct InterfaceSocket {
        QPointer<QUdpSocket> socket;
        QNetworkInterface interface;
        QHostAddress localAddress;
        quint32 requestSequence{0};
    };
    void addInterfaceSocket(const QNetworkInterface &interface, const QHostAddress &localAddress);
    void sendOnvifProbe(InterfaceSocket &entry);
    void sendProjectProbe(InterfaceSocket &entry);
    void sendSsdpProbe(InterfaceSocket &entry);
    void processDatagram(QUdpSocket *socket);
    void mergeAndEmit(DeviceRecord device);
    QString storageKeyForDevice(const DeviceRecord &device) const;
    void cancelEnrichmentRequests();
    void guardOnvifReply(QNetworkReply *reply);
    void trackEnrichmentReply(QNetworkReply *reply);
    void requestOnvifDeviceInformation(const DeviceRecord &device);
    void handleOnvifDeviceInformation(QNetworkReply *reply, const QString &deviceKey);
    void requestOnvifCapabilities(const DeviceRecord &device);
    void handleOnvifCapabilities(QNetworkReply *reply, const QString &deviceKey);
    void requestOnvifNetworkInterfaces(const DeviceRecord &device);
    void handleOnvifNetworkInterfaces(QNetworkReply *reply, const QString &deviceKey);
    void requestOnvifDns(const DeviceRecord &device);
    void handleOnvifDns(QNetworkReply *reply, const QString &deviceKey);

    QVector<InterfaceSocket> sockets_;
    QList<QPointer<QNetworkReply>> enrichmentReplies_;
    QHash<QString, DeviceRecord> devices_;
    QTimer timeout_;
    QNetworkAccessManager networkManager_;
    AdapterRegistry adapterRegistry_;
    bool running_{false};
    quint32 sequence_{0};
    quint64 enrichmentGeneration_{0};
};
