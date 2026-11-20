#include "app/batch_operation_preflight.h"

#include "adapters/adapter_registry.h"

#include <QtTest>

class BatchOperationPreflightTest final : public QObject
{
    Q_OBJECT
private slots:
    void marksVerifiedReadAsReady();
    void keepsManagementOperationsDisabled();
    void reportsMissingAndDuplicateIds();
    void reportsDevicesWithoutAdapter();
};

static DeviceRecord onvifDevice(const QString &id, const QString &ip)
{
    DeviceRecord device;
    device.id = id;
    device.ipAddress = ip;
    device.deviceServiceUrl = QStringLiteral("http://%1/onvif/device_service").arg(ip);
    device.discoverySources << QStringLiteral("ONVIF");
    return device;
}

void BatchOperationPreflightTest::marksVerifiedReadAsReady()
{
    AdapterRegistry registry;
    const auto results = BatchOperationPreflight::build(
        {onvifDevice(QStringLiteral("cam-1"), QStringLiteral("192.168.1.10"))},
        registry, DeviceOperation::ReadDeviceInformation);
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().state, BatchTargetState::Ready);
    QCOMPARE(results.first().adapterId, QStringLiteral("onvif"));
}

void BatchOperationPreflightTest::keepsManagementOperationsDisabled()
{
    AdapterRegistry registry;
    const auto results = BatchOperationPreflight::build(
        {onvifDevice(QStringLiteral("cam-1"), QStringLiteral("192.168.1.10"))},
        registry, DeviceOperation::Reboot);
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().state, BatchTargetState::UnsupportedOperation);
}

void BatchOperationPreflightTest::reportsMissingAndDuplicateIds()
{
    AdapterRegistry registry;
    const auto results = BatchOperationPreflight::build(
        {onvifDevice({}, QStringLiteral("192.168.1.10")),
         onvifDevice(QStringLiteral("cam-1"), QStringLiteral("192.168.1.11")),
         onvifDevice(QStringLiteral("cam-1"), QStringLiteral("192.168.1.12"))},
        registry, DeviceOperation::ReadCapabilities);
    QCOMPARE(results.size(), 3);
    QCOMPARE(results.at(0).state, BatchTargetState::MissingIdentity);
    QCOMPARE(results.at(1).state, BatchTargetState::Ready);
    QCOMPARE(results.at(2).state, BatchTargetState::DuplicateTarget);
}

void BatchOperationPreflightTest::reportsDevicesWithoutAdapter()
{
    AdapterRegistry registry;
    DeviceRecord unknown;
    unknown.id = QStringLiteral("unknown-1");
    unknown.ipAddress = QStringLiteral("192.168.1.50");
    const auto results = BatchOperationPreflight::build(
        {unknown}, registry, DeviceOperation::ReadDeviceInformation);
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().state, BatchTargetState::NoCompatibleAdapter);
}

QTEST_MAIN(BatchOperationPreflightTest)
#include "batch_operation_preflight_test.moc"
