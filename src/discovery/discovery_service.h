#pragma once

#include "core/device_record.h"

#include <QObject>
#include <QHash>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QPointer>
#include <QTimer>
#include <QUdpSocket>
#include <QVector>

class DiscoveryService final : public QObject
{
    Q_OBJECT
public:
    explicit DiscoveryService(QObject *parent = nullptr);
    void start();
    void cancel();
    bool isRunning() const;

signals:
    void deviceFound(const DeviceRecord &device);
    void statusChanged(const QString &message);
    void finished(int deviceCount);

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
    void processDatagram(QUdpSocket *socket);
    void mergeAndEmit(DeviceRecord device);

    QVector<InterfaceSocket> sockets_;
    QHash<QString, DeviceRecord> devices_;
    QTimer timeout_;
    bool running_{false};
    quint32 sequence_{0};
};
