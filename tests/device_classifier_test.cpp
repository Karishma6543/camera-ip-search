#include "core/device_classifier.h"

#include <QtTest>

class DeviceClassifierTest final : public QObject
{
    Q_OBJECT
private slots:
    void identifiesOnvifCameraAsVerified();
    void identifiesNvrFromExplicitModel();
    void identifiesPrinterFromMultipleServices();
    void identifiesRouterFromGatewayFingerprintAsProbable();
    void identifiesUpnpGatewayAsHighConfidenceRouter();
    void identifiesUpnpPrinterAsHighConfidencePrinter();
    void identifiesSwitchFromExplicitModel();
    void identifiesComputerFromNetworkServicesAsProbable();
    void leavesAmbiguousHostUnknown();
    void doesNotTreatSingleWebPortAsCamera();
};

void DeviceClassifierTest::identifiesOnvifCameraAsVerified()
{
    DeviceRecord device;
    device.discoverySources << QStringLiteral("ONVIF WS-Discovery");
    const auto result = DeviceClassifier::classify(device);
    QCOMPARE(result.key(), QStringLiteral("camera"));
    QCOMPARE(result.confidence, DeviceClassifier::Confidence::Verified);
}

void DeviceClassifierTest::identifiesNvrFromExplicitModel()
{
    DeviceRecord device;
    device.manufacturer = QStringLiteral("Example");
    device.model = QStringLiteral("NVR-8208-PoE");
    const auto result = DeviceClassifier::classify(device);
    QCOMPARE(result.key(), QStringLiteral("nvr"));
    QCOMPARE(result.confidence, DeviceClassifier::Confidence::High);
}

void DeviceClassifierTest::identifiesPrinterFromMultipleServices()
{
    DeviceRecord device;
    device.isCandidate = true;
    device.openPorts = {QStringLiteral("9100"), QStringLiteral("631")};
    const auto result = DeviceClassifier::classify(device);
    QCOMPARE(result.key(), QStringLiteral("printer"));
    QCOMPARE(result.confidence, DeviceClassifier::Confidence::Probable);
}

void DeviceClassifierTest::identifiesRouterFromGatewayFingerprintAsProbable()
{
    DeviceRecord device;
    device.isCandidate = true;
    device.openPorts = {QStringLiteral("53"), QStringLiteral("443")};
    const auto result = DeviceClassifier::classify(device);
    QCOMPARE(result.key(), QStringLiteral("router"));
    QCOMPARE(result.confidence, DeviceClassifier::Confidence::Probable);
}

void DeviceClassifierTest::identifiesUpnpGatewayAsHighConfidenceRouter()
{
    DeviceRecord device;
    device.discoverySources << QStringLiteral("SSDP/UPnP");
    device.model = QStringLiteral("urn:schemas-upnp-org:device:InternetGatewayDevice:1");
    const auto result = DeviceClassifier::classify(device);
    QCOMPARE(result.key(), QStringLiteral("router"));
    QCOMPARE(result.confidence, DeviceClassifier::Confidence::High);
}

void DeviceClassifierTest::identifiesUpnpPrinterAsHighConfidencePrinter()
{
    DeviceRecord device;
    device.discoverySources << QStringLiteral("SSDP/UPnP");
    device.model = QStringLiteral("urn:schemas-upnp-org:device:Printer:1");
    const auto result = DeviceClassifier::classify(device);
    QCOMPARE(result.key(), QStringLiteral("printer"));
    QCOMPARE(result.confidence, DeviceClassifier::Confidence::High);
}

void DeviceClassifierTest::identifiesSwitchFromExplicitModel()
{
    DeviceRecord device;
    device.model = QStringLiteral("PoE Switch 16-Port");
    const auto result = DeviceClassifier::classify(device);
    QCOMPARE(result.key(), QStringLiteral("switch"));
    QCOMPARE(result.confidence, DeviceClassifier::Confidence::High);
}

void DeviceClassifierTest::identifiesComputerFromNetworkServicesAsProbable()
{
    DeviceRecord device;
    device.isCandidate = true;
    device.openPorts = {QStringLiteral("445"), QStringLiteral("3389")};
    const auto result = DeviceClassifier::classify(device);
    QCOMPARE(result.key(), QStringLiteral("computer"));
    QCOMPARE(result.confidence, DeviceClassifier::Confidence::Probable);
}

void DeviceClassifierTest::leavesAmbiguousHostUnknown()
{
    DeviceRecord device;
    device.isCandidate = true;
    device.openPorts = {QStringLiteral("8080")};
    const auto result = DeviceClassifier::classify(device);
    QCOMPARE(result.key(), QStringLiteral("unknown"));
    QCOMPARE(result.confidence, DeviceClassifier::Confidence::Unverified);
}

void DeviceClassifierTest::doesNotTreatSingleWebPortAsCamera()
{
    DeviceRecord device;
    device.isCandidate = true;
    device.openPorts = {QStringLiteral("80")};
    const auto result = DeviceClassifier::classify(device);
    QCOMPARE(result.key(), QStringLiteral("unknown"));
}

QTEST_GUILESS_MAIN(DeviceClassifierTest)
#include "device_classifier_test.moc"
