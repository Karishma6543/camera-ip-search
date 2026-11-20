#include "adapters/adapter_registry.h"
#include "adapters/onvif_adapter.h"

#include <QtTest>

namespace {
class TestVendorAdapter final : public DeviceAdapter
{
public:
    QString id() const override { return QStringLiteral("test-vendor"); }
    QString displayName() const override { return QStringLiteral("Test vendor"); }
    bool canHandle(const DeviceRecord &device) const override
    {
        return device.manufacturer.compare(QStringLiteral("Acme"), Qt::CaseInsensitive) == 0;
    }
    QStringList verifiedReadOperations() const override
    {
        return {QStringLiteral("GetDeviceInformation")};
    }
};
}

class AdapterRegistryTest final : public QObject
{
    Q_OBJECT
private slots:
    void recognizesOnvifDeviceService();
    void rejectsEndpointForDifferentHost();
    void exposesOnlyImplementedReadOperations();
    void doesNotInferManagementSupportFromDiscovery();
    void registersAdditionalVendorAdapters();
    void rejectsNullAndDuplicateAdapterIds();
};

void AdapterRegistryTest::recognizesOnvifDeviceService()
{
    DeviceRecord device;
    device.ipAddress = QStringLiteral("192.168.1.20");
    device.deviceServiceUrl = QStringLiteral("http://192.168.1.20/onvif/device_service");
    device.discoverySources << QStringLiteral("ONVIF");
    AdapterRegistry registry;
    const DeviceAdapter *adapter = registry.adapterFor(device);
    QVERIFY(adapter);
    QCOMPARE(adapter->id(), QStringLiteral("onvif"));
}

void AdapterRegistryTest::rejectsEndpointForDifferentHost()
{
    DeviceRecord device;
    device.ipAddress = QStringLiteral("192.168.1.20");
    device.deviceServiceUrl = QStringLiteral("http://192.168.1.99/onvif/device_service");
    device.discoverySources << QStringLiteral("ONVIF");
    AdapterRegistry registry;
    QVERIFY(!registry.adapterFor(device));
}

void AdapterRegistryTest::exposesOnlyImplementedReadOperations()
{
    DeviceRecord device;
    device.ipAddress = QStringLiteral("192.168.1.20");
    device.deviceServiceUrl = QStringLiteral("http://192.168.1.20/onvif/device_service");
    device.discoverySources << QStringLiteral("ONVIF");
    AdapterRegistry registry;
    const QStringList operations = registry.supportedReadOperations(device);
    QCOMPARE(operations.size(), 3);
    QVERIFY(operations.contains(QStringLiteral("GetDeviceInformation")));
    QVERIFY(operations.contains(QStringLiteral("GetCapabilities")));
    QVERIFY(operations.contains(QStringLiteral("GetNetworkInterfaces")));
    QVERIFY(!operations.contains(QStringLiteral("SetNetworkInterfaces")));
}

void AdapterRegistryTest::doesNotInferManagementSupportFromDiscovery()
{
    DeviceRecord device;
    device.ipAddress = QStringLiteral("192.168.1.20");
    device.deviceServiceUrl = QStringLiteral("http://192.168.1.20/onvif/device_service");
    device.discoverySources << QStringLiteral("ONVIF");
    AdapterRegistry registry;
    const DeviceAdapter *adapter = registry.adapterFor(device);
    QVERIFY(adapter);
    QVERIFY(adapter->supports(DeviceOperation::ReadDeviceInformation));
    QVERIFY(adapter->supports(DeviceOperation::ReadCapabilities));
    QVERIFY(adapter->supports(DeviceOperation::ReadNetworkInterfaces));
    QVERIFY(!adapter->supports(DeviceOperation::ConfigureStaticIpv4));
    QVERIFY(!adapter->supports(DeviceOperation::PreviewStream));
    QVERIFY(!adapter->supports(DeviceOperation::CaptureSnapshot));
    QVERIFY(!adapter->supports(DeviceOperation::Reboot));
    QVERIFY(!adapter->supports(DeviceOperation::ChangePassword));
    QVERIFY(!adapter->supports(DeviceOperation::UpdateFirmware));
    QVERIFY(!adapter->supports(DeviceOperation::ConfigureImageSettings));
    QVERIFY(!adapter->supports(DeviceOperation::ManageNvrChannels));
}

void AdapterRegistryTest::registersAdditionalVendorAdapters()
{
    AdapterRegistry registry;
    QVERIFY(registry.registerAdapter(std::make_unique<TestVendorAdapter>()));

    DeviceRecord device;
    device.ipAddress = QStringLiteral("192.168.1.40");
    device.manufacturer = QStringLiteral("ACME");
    const DeviceAdapter *adapter = registry.adapterFor(device);
    QVERIFY(adapter);
    QCOMPARE(adapter->id(), QStringLiteral("test-vendor"));
    QVERIFY(adapter->supports(DeviceOperation::ReadDeviceInformation));
    QVERIFY(!adapter->supports(DeviceOperation::Reboot));
}

void AdapterRegistryTest::rejectsNullAndDuplicateAdapterIds()
{
    AdapterRegistry registry;
    QVERIFY(!registry.registerAdapter(std::unique_ptr<DeviceAdapter>{}));
    QVERIFY(!registry.registerAdapter(std::make_unique<TestVendorAdapter>()));
    QVERIFY(!registry.registerAdapter(std::make_unique<TestVendorAdapter>()));
}

QTEST_MAIN(AdapterRegistryTest)
#include "adapter_registry_test.moc"
