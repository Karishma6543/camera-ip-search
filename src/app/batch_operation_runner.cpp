#include "batch_operation_runner.h"

#include "adapters/adapter_registry.h"

#include <QMetaObject>
#include <utility>

BatchOperationRunner::BatchOperationRunner(const AdapterRegistry &registry,
                                           QObject *parent)
    : QObject(parent), registry_(registry)
{
}

bool BatchOperationRunner::isRunning() const { return running_; }
int BatchOperationRunner::completedCount() const { return completedCount_; }
int BatchOperationRunner::totalCount() const { return results_.size(); }
QVector<BatchTargetResult> BatchOperationRunner::results() const { return results_; }

bool BatchOperationRunner::start(const QVector<DeviceRecord> &devices,
                                 DeviceOperation operation,
                                 Handler handler)
{
    if (running_ || !handler || devices.isEmpty())
        return false;

    devices_ = devices;
    operation_ = operation;
    handler_ = std::move(handler);
    results_ = BatchOperationPreflight::build(devices_, registry_, operation_);
    nextIndex_ = 0;
    completedCount_ = 0;
    cancelRequested_ = false;
    running_ = true;

    QMetaObject::invokeMethod(this, &BatchOperationRunner::processNext,
                              Qt::QueuedConnection);
    return true;
}

void BatchOperationRunner::cancel()
{
    if (running_)
        cancelRequested_ = true;
}

void BatchOperationRunner::processNext()
{
    if (!running_)
        return;

    if (cancelRequested_ || nextIndex_ >= devices_.size()) {
        if (cancelRequested_) {
            for (int i = nextIndex_; i < results_.size(); ++i) {
                if (results_[i].state == BatchTargetState::Ready) {
                    results_[i].state = BatchTargetState::Cancelled;
                    results_[i].message = QStringLiteral("Cancelled before execution");
                }
                ++completedCount_;
                emit targetFinished(results_[i]);
            }
        }
        running_ = false;
        handler_ = {};
        emit progressChanged(completedCount_, results_.size());
        emit finished(results_);
        return;
    }

    BatchTargetResult &result = results_[nextIndex_];
    const DeviceRecord &device = devices_[nextIndex_];
    ++nextIndex_;

    if (result.state == BatchTargetState::Ready) {
        QString detail;
        const bool succeeded = handler_(device, operation_, detail);
        result.state = succeeded ? BatchTargetState::Succeeded
                                 : BatchTargetState::ExecutionFailed;
        result.message = detail.trimmed().isEmpty()
            ? (succeeded ? QStringLiteral("Operation completed")
                         : QStringLiteral("Operation failed without details"))
            : detail.trimmed();
    }

    ++completedCount_;
    emit targetFinished(result);
    emit progressChanged(completedCount_, results_.size());
    QMetaObject::invokeMethod(this, &BatchOperationRunner::processNext,
                              Qt::QueuedConnection);
}
