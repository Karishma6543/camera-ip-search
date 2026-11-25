#pragma once

#include "core/device_record.h"

#include <QObject>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <QPointer>
#include <QHash>
#include <QTimer>
#include <QTcpSocket>
#include <QVector>

class SubnetScanner final : public QObject
{
    Q_OBJECT
public:
    explicit SubnetScanner(QObject *parent = nullptr);
    // maxHosts=256 keeps large scans bounded; maxHosts=0 requests a full subnet scan (up to 4096 hosts).
    bool start(const QNetworkInterface &interface, const QNetworkAddressEntry &entry,
               quint32 maxHosts = 256);
    static bool isUsableSubnet(const QNetworkAddressEntry &entry);
    // Adds a discovered TCP port once; useful when several probes identify one host.
    static bool appendOpenPort(DeviceRecord &device, quint16 port);
    void cancel();
    bool isRunning() const;

signals:
    void candidateFound(const DeviceRecord &device);
    void statusChanged(const QString &message);
    void progressChanged(int completedProbes, int totalProbes, int candidateCount);
    void finished(int candidateCount);
    void cancelled(int candidateCount);

private:
    struct Probe {
        QPointer<QTcpSocket> socket;
        QPointer<QTimer> timeout;
        QString ip;
        quint16 port{0};
    };
    void schedule();
    void launchProbe(const QString &ip, quint16 port);
    void completeProbe(QTcpSocket *socket, bool connected);
    void finishIfDone();

    QVector<Probe> active_;
    QVector<QPair<QString, quint16>> pending_;
    QHash<QString, DeviceRecord> discoveredCandidates_;
    QTimer scheduler_;
    bool running_{false};
    int candidates_{0};
    int completedProbes_{0};
    int totalProbes_{0};
    int maxConcurrent_{48};
};
