#pragma once

#include "app/batch_operation_preflight.h"

#include <QObject>
#include <QVector>
#include <functional>

class AdapterRegistry;

// Executes one approved target at a time. The injected handler is the only
// place where a real protocol operation may be performed.
class BatchOperationRunner final : public QObject
{
    Q_OBJECT
public:
    using Handler = std::function<bool(const DeviceRecord &, DeviceOperation, QString &)>;

    explicit BatchOperationRunner(const AdapterRegistry &registry,
                                  QObject *parent = nullptr);

    bool isRunning() const;
    int completedCount() const;
    int totalCount() const;
    QVector<BatchTargetResult> results() const;

    bool start(const QVector<DeviceRecord> &devices,
               DeviceOperation operation,
               Handler handler);
    void cancel();

signals:
    void progressChanged(int completed, int total);
    void targetFinished(const BatchTargetResult &result);
    void finished(const QVector<BatchTargetResult> &results);

private slots:
    void processNext();

private:
    const AdapterRegistry &registry_;
    QVector<DeviceRecord> devices_;
    QVector<BatchTargetResult> results_;
    DeviceOperation operation_{DeviceOperation::ReadDeviceInformation};
    Handler handler_;
    int nextIndex_{0};
    int completedCount_{0};
    bool running_{false};
    bool cancelRequested_{false};
};
