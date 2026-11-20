#include "batch_operation_preflight.h"

#include "adapters/adapter_registry.h"

#include <QHash>
#include <QHostAddress>

namespace {
QString canonicalIpv4(const QString &value)
{
    QHostAddress address;
    if (!address.setAddress(value.trimmed()) ||
        address.protocol() != QAbstractSocket::IPv4Protocol)
        return {};
    return address.toString();
}
}

QVector<BatchTargetResult> BatchOperationPreflight::build(
    const QVector<DeviceRecord> &devices,
    const AdapterRegistry &registry,
    DeviceOperation operation)
{
    QVector<BatchTargetResult> results;
    results.reserve(devices.size());

    QHash<QString, int> idCounts;
    QHash<QString, int> addressCounts;
    for (const DeviceRecord &device : devices) {
        const QString id = device.id.trimmed();
        if (!id.isEmpty())
            ++idCounts[id];
        const QString address = canonicalIpv4(device.ipAddress);
        if (!address.isEmpty())
            ++addressCounts[address];
    }

    for (const DeviceRecord &device : devices) {
        BatchTargetResult result;
        result.deviceId = device.id.trimmed();
        result.address = device.ipAddress.trimmed();
        const QString normalizedAddress = canonicalIpv4(device.ipAddress);

        if (result.deviceId.isEmpty()) {
            result.state = BatchTargetState::MissingIdentity;
            result.message = QStringLiteral("Device has no stable inventory ID");
        } else if (idCounts.value(result.deviceId) > 1) {
            // Mark every occurrence, not just the later duplicate, as unsafe.
            result.state = BatchTargetState::DuplicateTarget;
            result.message = QStringLiteral("Device ID occurs more than once in batch");
        } else if (!normalizedAddress.isEmpty() &&
                   addressCounts.value(normalizedAddress) > 1) {
            // Two inventory records resolving to one IPv4 endpoint must not
            // receive duplicate batch actions, even if their IDs differ.
            result.state = BatchTargetState::DuplicateAddress;
            result.message = QStringLiteral("IPv4 address occurs more than once in batch");
        } else {
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
