#include "batch_operation_preflight.h"

#include "adapters/adapter_registry.h"

#include <QSet>

QVector<BatchTargetResult> BatchOperationPreflight::build(
    const QVector<DeviceRecord> &devices,
    const AdapterRegistry &registry,
    DeviceOperation operation)
{
    QVector<BatchTargetResult> results;
    results.reserve(devices.size());

    QSet<QString> seenIds;
    for (const DeviceRecord &device : devices) {
        BatchTargetResult result;
        result.deviceId = device.id.trimmed();
        result.address = device.ipAddress.trimmed();

        if (result.deviceId.isEmpty()) {
            result.state = BatchTargetState::MissingIdentity;
            result.message = QStringLiteral("Device has no stable inventory ID");
        } else if (seenIds.contains(result.deviceId)) {
            result.state = BatchTargetState::DuplicateTarget;
            result.message = QStringLiteral("Duplicate device ID in batch");
        } else {
            seenIds.insert(result.deviceId);
            const DeviceAdapter *adapter = registry.adapterFor(device);
            if (!adapter) {
                result.state = BatchTargetState::NoCompatibleAdapter;
                result.message = QStringLiteral("No compatible verified adapter");
            } else {
                result.adapterId = adapter->id();
                if (adapter->supports(operation)) {
                    result.state = BatchTargetState::Ready;
                    result.message = QStringLiteral("Adapter advertises this operation");
                } else {
                    result.state = BatchTargetState::UnsupportedOperation;
                    result.message = QStringLiteral("Adapter does not support this operation");
                }
            }
        }
        results.push_back(result);
    }
    return results;
}
