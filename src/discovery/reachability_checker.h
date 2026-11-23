#pragma once
#include "core/device_record.h"
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QTcpSocket>
#include <QVector>

class ReachabilityChecker final : public QObject
{
    Q_OBJECT
public:
    explicit ReachabilityChecker(QObject *parent = nullptr);
    bool isRunning() const;
    void start(const QVector<DeviceRecord> &devices);
    void cancel();
signals:
    void deviceChecked(const QString &ipAddress, const QString &status,
                       const QString &checkedAt, const QString &lastSuccessfulResponseAt);
    void finished(int checkedCount);
    void cancelled(int checkedCount);
    void progressChanged(int checkedCount, int totalCount, const QString &currentIp);
private:
    void checkNextDevice();
    void tryNextPort();
    void completeCurrent(const QString &status);
    QVector<DeviceRecord> devices_;
    QStringList ports_;
    int deviceIndex_{-1};
    int portIndex_{0};
    int checkedCount_{0};
    bool running_{false};
    bool completed_{false};
    bool suppressSocketError_{false};
    QTcpSocket *socket_{nullptr};
    QTimer timeout_;
};