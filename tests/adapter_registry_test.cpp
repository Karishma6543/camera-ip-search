#include "adapters/adapter_registry.h"
#include "adapters/onvif_adapter.h"

#include <QtTest>

class AdapterRegistryTest final : public QObject
{
    Q_OBJECT
private slots:
    void recognizesOnvifDeviceService();
    void rejectsEndpointForDifferentHost();
    void rejectsEndpointWithEmbeddedCredentials();
    void exposesOnlyImplementedReadOperations();
    void exposesImplementedOnvifWriteOperations();
    void exposesFactoryResetOnlyForOnvifAdapter();
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


void AdapterRegistryTest::rejectsEndpointWithEmbeddedCredentials()
{
    DeviceRecord device;
    device.ipAddress = QStringLiteral("192.168.1.20");
    device.deviceServiceUrl = QStringLiteral("http://operator:secret@192.168.1.20/onvif/device_service");
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


void AdapterRegistryTest::exposesImplementedOnvifWriteOperations()
{
    DeviceRecord device;
    device.ipAddress = QStringLiteral("192.168.1.20");
    device.deviceServiceUrl = QStringLiteral("http://192.168.1.20/onvif/device_service");
    device.discoverySources << QStringLiteral("ONVIF");
    AdapterRegistry registry;
    const QStringList operations = registry.implementedWriteOperations(device);
    QVERIFY(operations.contains(QStringLiteral("SetNetworkInterfaces")));
    QVERIFY(operations.contains(QStringLiteral("SetNetworkDefaultGateway")));
}

void AdapterRegistryTest::exposesFactoryResetOnlyForOnvifAdapter()
{
    DeviceRecord device;
    device.ipAddress = QStringLiteral("192.168.1.20");
    device.deviceServiceUrl = QStringLiteral("http://192.168.1.20/onvif/device_service");
    device.discoverySources << QStringLiteral("ONVIF");
    AdapterRegistry registry;
    const DeviceAdapter *adapter = registry.adapterFor(device);
    QVERIFY(adapter);
    QVERIFY(adapter->supports(DeviceOperation::FactoryReset));
    QVERIFY(registry.implementedWriteOperations(device).contains(QStringLiteral("SetSystemFactoryDefault")));
}

QTEST_MAIN(AdapterRegistryTest)
#include "adapter_registry_test.moc"
