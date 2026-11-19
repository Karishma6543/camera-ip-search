#pragma once

#include "core/device_record.h"

#include <QObject>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QTcpSocket>
#include <QVector>

class SubnetScanner final : public QObject
{
    Q_OBJECT
public:
    explicit SubnetScanner(QObject *parent = nullptr);
    bool start(const QNetworkInterface &interface, const QNetworkAddressEntry &entry);
    void cancel();
    bool isRunning() const;

signals:
    void candidateFound(const DeviceRecord &device);
    void statusChanged(const QString &message);
    void progressChanged(int completedProbes, int totalProbes, int candidateCount);
    void finished(int candidateCount);

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
    QSet<QString> emitted_;
    QTimer scheduler_;
    bool running_{false};
    int candidates_{0};
    int completedProbes_{0};
    int totalProbes_{0};
    int maxConcurrent_{48};
};
