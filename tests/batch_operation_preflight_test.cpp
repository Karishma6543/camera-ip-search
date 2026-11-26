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
    void reportsDuplicateIpv4Addresses();
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
    const auto serviceResults = BatchOperationPreflight::build(
        {onvifDevice(QStringLiteral("cam-2"), QStringLiteral("192.168.1.11"))},
        registry, DeviceOperation::ReadServices);
    QCOMPARE(serviceResults.first().state, BatchTargetState::Ready);
}

void BatchOperationPreflightTest::keepsManagementOperationsDisabled()
{
    AdapterRegistry registry;
    const DeviceRecord device = onvifDevice(
        QStringLiteral("cam-1"), QStringLiteral("192.168.1.10"));
    const QVector<DeviceOperation> blockedOperations = {
        DeviceOperation::ConfigureStaticIpv4,
        DeviceOperation::ConfigureDhcp,
        DeviceOperation::ConfigureDns,
        DeviceOperation::Reboot,
        DeviceOperation::ChangePassword,
        DeviceOperation::UpdateFirmware,
        DeviceOperation::ConfigureImageSettings,
        DeviceOperation::ManageNvrChannels,
        DeviceOperation::FactoryReset
    };

    for (DeviceOperation operation : blockedOperations) {
        const auto results = BatchOperationPreflight::build({device}, registry, operation);
        QCOMPARE(results.size(), 1);
        QCOMPARE(results.first().state, BatchTargetState::UnsupportedOperation);
    }
}

void BatchOperationPreflightTest::reportsMissingAndDuplicateIds()
{
    AdapterRegistry registry;
    const auto results = BatchOperationPreflight::build(
        {onvifDevice({}, QStringLiteral("192.168.1.10")),
         onvifDevice(QStringLiteral("cam-ok"), QStringLiteral("192.168.1.11")),
         onvifDevice(QStringLiteral("cam-dup"), QStringLiteral("192.168.1.12")),
         onvifDevice(QStringLiteral("cam-dup"), QStringLiteral("192.168.1.13"))},
        registry, DeviceOperation::ReadCapabilities);
    QCOMPARE(results.size(), 4);
    QCOMPARE(results.at(0).state, BatchTargetState::MissingIdentity);
    QCOMPARE(results.at(1).state, BatchTargetState::Ready);
    QCOMPARE(results.at(2).state, BatchTargetState::DuplicateTarget);
    QCOMPARE(results.at(3).state, BatchTargetState::DuplicateTarget);
}

void BatchOperationPreflightTest::reportsDuplicateIpv4Addresses()
{
    AdapterRegistry registry;
    const auto results = BatchOperationPreflight::build(
        {onvifDevice(QStringLiteral("cam-a"), QStringLiteral("192.168.1.20")),
         onvifDevice(QStringLiteral("cam-b"), QStringLiteral("192.168.1.20")),
         onvifDevice(QStringLiteral("cam-c"), QStringLiteral("192.168.1.21"))},
        registry, DeviceOperation::ReadNetworkInterfaces);
    QCOMPARE(results.size(), 3);
    QCOMPARE(results.at(0).state, BatchTargetState::DuplicateAddress);
    QCOMPARE(results.at(1).state, BatchTargetState::DuplicateAddress);
    QCOMPARE(results.at(2).state, BatchTargetState::Ready);
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
