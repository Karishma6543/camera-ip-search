#include "core/operation_compatibility.h"

#include <QtTest>

class OperationCompatibilityTest final : public QObject
{
    Q_OBJECT
private slots:
    void distinguishesApplicationImplementationFromDeviceEvidence();
    void reportsDeviceResponseOnlyForObservedOperations();
    void doesNotTreatAdvertisedMediaServiceAsImplementedStreaming();
};

void OperationCompatibilityTest::distinguishesApplicationImplementationFromDeviceEvidence()
{
    DeviceRecord device;
    device.ipAddress = QStringLiteral("192.0.2.10");
    device.deviceServiceUrl = QStringLiteral("http://192.0.2.10/onvif/device_service");
    device.discoverySources << QStringLiteral("ONVIF");
    const auto entries = OperationCompatibility::evaluate(device);
    QVERIFY(entries.size() >= 10);
    QCOMPARE(entries.at(0).status, OperationCompatibility::Status::ImplementedNotTested);
    QCOMPARE(entries.at(4).status, OperationCompatibility::Status::ServiceNotReported);
    QCOMPARE(entries.at(9).status, OperationCompatibility::Status::NotImplemented);
}

void OperationCompatibilityTest::reportsDeviceResponseOnlyForObservedOperations()
{
    DeviceRecord device;
    device.ipAddress = QStringLiteral("192.0.2.10");
    device.deviceServiceUrl = QStringLiteral("http://192.0.2.10/onvif/device_service");
    device.discoverySources << QStringLiteral("ONVIF")
                            << QStringLiteral("ONVIF GetDeviceInformation");
    const auto entries = OperationCompatibility::evaluate(device);
    QCOMPARE(entries.at(0).status, OperationCompatibility::Status::DeviceResponded);
    QCOMPARE(entries.at(1).status, OperationCompatibility::Status::ImplementedNotTested);
    QCOMPARE(entries.at(2).status, OperationCompatibility::Status::ImplementedNotTested);
}

void OperationCompatibilityTest::doesNotTreatAdvertisedMediaServiceAsImplementedStreaming()
{
    DeviceRecord device;
    device.ipAddress = QStringLiteral("192.0.2.10");
    device.deviceServiceUrl = QStringLiteral("http://192.0.2.10/onvif/device_service");
    device.discoverySources << QStringLiteral("ONVIF");
    device.capabilities << QStringLiteral("Service: Media") << QStringLiteral("Service: Device");
    const auto entries = OperationCompatibility::evaluate(device);
    QCOMPARE(entries.at(9).status, OperationCompatibility::Status::ServiceAdvertisedNotVerified);
    QVERIFY(entries.at(9).evidence.contains(QStringLiteral("not implemented")));
    QCOMPARE(entries.at(4).status, OperationCompatibility::Status::AuthenticationNotVerified);
}
QTEST_GUILESS_MAIN(OperationCompatibilityTest)
#include "operation_compatibility_test.moc"
