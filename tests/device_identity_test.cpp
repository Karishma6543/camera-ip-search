#include "core/device_identity.h"

#include <QtTest>

class DeviceIdentityTest final : public QObject
{
    Q_OBJECT
private slots:
    void mergesScannerCandidateWithProtocolObservation();
    void matchesMacAfterAddressChanges();
    void doesNotMergeDifferentMacsAtSameAddress();
    void matchesStableOnvifEndpoint();
    void preservesVerifiedFieldsAndCombinesSourcesAndPorts();
};

void DeviceIdentityTest::mergesScannerCandidateWithProtocolObservation()
{
    DeviceRecord candidate;
    candidate.ipAddress = QStringLiteral("192.168.1.20");
    candidate.isCandidate = true;
    candidate.model = QStringLiteral("Unverified network device");
    candidate.discoverySources << QStringLiteral("Local subnet TCP scan");
    candidate.openPorts << QStringLiteral("80");

    QHash<QString, DeviceRecord> inventory;
    inventory.insert(candidate.ipAddress, candidate);

    DeviceRecord onvif;
    onvif.ipAddress = candidate.ipAddress;
    onvif.macAddress = QStringLiteral("00:11:22:33:44:55");
    onvif.manufacturer = QStringLiteral("Example");
    onvif.model = QStringLiteral("Camera A");
    onvif.discoverySources << QStringLiteral("ONVIF");

    const QString key = DeviceIdentity::findMatchingKey(inventory, onvif);
    QCOMPARE(key, candidate.ipAddress);
    const DeviceRecord merged = DeviceIdentity::mergeObservations(inventory.value(key), onvif);
    QVERIFY(!merged.isCandidate);
    QCOMPARE(merged.macAddress, onvif.macAddress);
    QCOMPARE(merged.model, onvif.model);
    QCOMPARE(merged.discoverySources.size(), 2);
    QCOMPARE(merged.openPorts, QStringList{QStringLiteral("80")});
}

void DeviceIdentityTest::matchesMacAfterAddressChanges()
{
    DeviceRecord previous;
    previous.ipAddress = QStringLiteral("192.168.1.20");
    previous.macAddress = QStringLiteral("00:11:22:33:44:55");
    QHash<QString, DeviceRecord> inventory;
    inventory.insert(previous.ipAddress, previous);

    DeviceRecord current;
    current.ipAddress = QStringLiteral("192.168.1.45");
    current.macAddress = previous.macAddress;
    QCOMPARE(DeviceIdentity::findMatchingKey(inventory, current), previous.ipAddress);
    const DeviceRecord merged = DeviceIdentity::mergeObservations(previous, current);
    QCOMPARE(merged.ipAddress, current.ipAddress);
}

void DeviceIdentityTest::doesNotMergeDifferentMacsAtSameAddress()
{
    DeviceRecord previous;
    previous.ipAddress = QStringLiteral("192.168.1.20");
    previous.macAddress = QStringLiteral("00:11:22:33:44:55");
    QHash<QString, DeviceRecord> inventory;
    inventory.insert(previous.macAddress, previous);

    DeviceRecord current;
    current.ipAddress = previous.ipAddress;
    current.macAddress = QStringLiteral("00:11:22:33:44:66");
    QVERIFY(DeviceIdentity::findMatchingKey(inventory, current).isEmpty());
}

void DeviceIdentityTest::matchesStableOnvifEndpoint()
{
    DeviceRecord previous;
    previous.ipAddress = QStringLiteral("192.168.1.20");
    previous.onvifEndpoint = QStringLiteral("urn:uuid:camera-identity");
    QHash<QString, DeviceRecord> inventory;
    inventory.insert(previous.ipAddress, previous);

    DeviceRecord current;
    current.ipAddress = QStringLiteral("192.168.1.50");
    current.onvifEndpoint = previous.onvifEndpoint;
    QCOMPARE(DeviceIdentity::findMatchingKey(inventory, current), previous.ipAddress);
}

void DeviceIdentityTest::preservesVerifiedFieldsAndCombinesSourcesAndPorts()
{
    DeviceRecord known;
    known.manufacturer = QStringLiteral("Verified manufacturer");
    known.model = QStringLiteral("Verified model");
    known.openPorts << QStringLiteral("80");
    known.discoverySources << QStringLiteral("ONVIF");

    DeviceRecord candidate;
    candidate.isCandidate = true;
    candidate.manufacturer = QStringLiteral("Unknown");
    candidate.model = QStringLiteral("Unverified network device");
    candidate.openPorts << QStringLiteral("554");
    candidate.discoverySources << QStringLiteral("Local subnet TCP scan");

    const DeviceRecord merged = DeviceIdentity::mergeObservations(known, candidate);
    QCOMPARE(merged.manufacturer, known.manufacturer);
    QCOMPARE(merged.model, known.model);
    QCOMPARE(merged.openPorts.size(), 2);
    QCOMPARE(merged.discoverySources.size(), 2);
    QVERIFY(!merged.isCandidate);
}

QTEST_GUILESS_MAIN(DeviceIdentityTest)
#include "device_identity_test.moc"
