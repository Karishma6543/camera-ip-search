#pragma once

#include "adapters/device_adapter.h"
#include "core/device_record.h"

#include <QVector>

// This is a preflight report only. A Ready result means that a registered
// adapter advertises the operation; it does not execute or authorize it.
enum class BatchTargetState {
    Ready,
    MissingIdentity,
    DuplicateTarget,
    NoCompatibleAdapter,
    UnsupportedOperation
};

struct BatchTargetResult
{
    QString deviceId;
    QString address;
    BatchTargetState state{BatchTargetState::NoCompatibleAdapter};
    QString adapterId;
    QString message;
};

class AdapterRegistry;

class BatchOperationPreflight final
{
public:
    static QVector<BatchTargetResult> build(
        const QVector<DeviceRecord> &devices,
        const AdapterRegistry &registry,
        DeviceOperation operation);
};

Q_DECLARE_METATYPE(BatchTargetResult)
Q_DECLARE_METATYPE(QVector<BatchTargetResult>)
