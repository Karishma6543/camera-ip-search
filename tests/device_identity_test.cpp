#include "core/device_identity.h"

#include <QtTest>

class DeviceIdentityTest final : public QObject
{
    Q_OBJECT
private slots:
    void mergesScannerCandidateWithProtocolObservation();
    void matchesMacAfterAddressChanges();
    void doesNotMergeDifferentMacsAtSameAddress();
    void doesNotMergeDifferentMacsWithSameEndpointOrSerial();
    void matchesStableOnvifEndpoint();
    void strongIdentityMatchesAfterAddressChange();
    void strongIdentityRejectsIpOnlyAndConflictingMacs();
    void preservesVerifiedFieldsAndCombinesSourcesAndPorts();
    void refreshesOnvifNetworkStateAfterAddressChange();
    void revokesStaleOnvifInterfaceToken();
    void refreshesDnsFromOnvifObservation();
    void manualEntryDoesNotUpgradeUnverifiedCandidate();
    void replacesOnvifUnknownManufacturerPlaceholder();
    void freshObservationReplacesStaleInventoryStatus();
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
    onvif.discoverySources << QStringLiteral("ONVIF WS-Discovery");

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
    current.discoverySources << QStringLiteral("ONVIF WS-Discovery");
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


void DeviceIdentityTest::doesNotMergeDifferentMacsWithSameEndpointOrSerial()
{
    DeviceRecord previous;
    previous.ipAddress = QStringLiteral("192.168.1.20");
    previous.macAddress = QStringLiteral("00:11:22:33:44:55");
    previous.onvifEndpoint = QStringLiteral("urn:uuid:cloned-endpoint");
    previous.serialNumber = QStringLiteral("SERIAL-123");
    previous.manufacturer = QStringLiteral("Example");
    QHash<QString, DeviceRecord> inventory;
    inventory.insert(previous.macAddress, previous);

    DeviceRecord current = previous;
    current.macAddress = QStringLiteral("00:11:22:33:44:66");
    current.ipAddress = QStringLiteral("192.168.1.45");
    QVERIFY(DeviceIdentity::findMatchingKey(inventory, current).isEmpty());

    current.onvifEndpoint.clear();
    QCOMPARE(current.serialNumber, previous.serialNumber);
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

void DeviceIdentityTest::strongIdentityMatchesAfterAddressChange()
{
    DeviceRecord expected;
    expected.ipAddress = QStringLiteral("192.168.1.20");
    expected.macAddress = QStringLiteral("00:11:22:33:44:55");
    DeviceRecord observed;
    observed.ipAddress = QStringLiteral("192.168.1.45");
    observed.macAddress = expected.macAddress;
    QVERIFY(DeviceIdentity::hasStrongMatch(expected, observed));

    expected.macAddress.clear();
    expected.serialNumber = QStringLiteral("SERIAL-123");
    expected.manufacturer = QStringLiteral("Example");
    observed.macAddress.clear();
    observed.serialNumber = expected.serialNumber;
    observed.manufacturer = expected.manufacturer;
    QVERIFY(DeviceIdentity::hasStrongMatch(expected, observed));

    expected.serialNumber.clear();
    expected.manufacturer.clear();
    expected.onvifEndpoint = QStringLiteral("urn:uuid:camera-identity");
    observed.serialNumber.clear();
    observed.manufacturer.clear();
    observed.onvifEndpoint = expected.onvifEndpoint;
    QVERIFY(DeviceIdentity::hasStrongMatch(expected, observed));
}

void DeviceIdentityTest::strongIdentityRejectsIpOnlyAndConflictingMacs()
{
    DeviceRecord expected;
    expected.ipAddress = QStringLiteral("192.168.1.20");
    DeviceRecord observed;
    observed.ipAddress = expected.ipAddress;
    QVERIFY(!DeviceIdentity::hasStrongMatch(expected, observed));

    expected.macAddress = QStringLiteral("00:11:22:33:44:55");
    expected.serialNumber = QStringLiteral("SERIAL-123");
    expected.manufacturer = QStringLiteral("Example");
    expected.onvifEndpoint = QStringLiteral("urn:uuid:camera-identity");
    observed.macAddress = QStringLiteral("00:11:22:33:44:66");
    observed.serialNumber = expected.serialNumber;
    observed.manufacturer = expected.manufacturer;
    observed.onvifEndpoint = expected.onvifEndpoint;
    QVERIFY(!DeviceIdentity::hasStrongMatch(expected, observed));
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


void DeviceIdentityTest::manualEntryDoesNotUpgradeUnverifiedCandidate()
{
    DeviceRecord candidate;
    candidate.ipAddress = QStringLiteral("192.168.1.20");
    candidate.isCandidate = true;
    candidate.discoverySources << QStringLiteral("Local subnet TCP scan");

    DeviceRecord manual;
    manual.ipAddress = candidate.ipAddress;
    manual.discoverySources << QStringLiteral("Manual");

    const DeviceRecord merged = DeviceIdentity::mergeObservations(candidate, manual);
    QVERIFY(merged.isCandidate);
}

void DeviceIdentityTest::replacesOnvifUnknownManufacturerPlaceholder()
{
    DeviceRecord discovered;
    discovered.manufacturer = QStringLiteral("ONVIF device (manufacturer unknown)");
    discovered.model = QStringLiteral("Model unknown");
    discovered.discoverySources << QStringLiteral("ONVIF WS-Discovery");

    DeviceRecord enriched;
    enriched.manufacturer = QStringLiteral("Example Camera Co.");
    enriched.model = QStringLiteral("IPC-500");
    enriched.discoverySources << QStringLiteral("ONVIF GetDeviceInformation");

    const DeviceRecord merged = DeviceIdentity::mergeObservations(discovered, enriched);
    QCOMPARE(merged.manufacturer, enriched.manufacturer);
    QCOMPARE(merged.model, enriched.model);
}


void DeviceIdentityTest::refreshesOnvifNetworkStateAfterAddressChange()
{
    DeviceRecord existing;
    existing.ipAddress = QStringLiteral("192.168.1.20");
    existing.deviceServiceUrl = QStringLiteral("http://192.168.1.20/onvif/device_service");
    existing.networkInterfaceToken = QStringLiteral("old-token");
    existing.ipv4Address = QStringLiteral("192.168.1.20");
    existing.ipv4SubnetMask = QStringLiteral("255.255.255.0");
    existing.ipv4ConfigurationKnown = true;

    DeviceRecord updated;
    updated.ipAddress = QStringLiteral("192.168.1.45");
    updated.deviceServiceUrl = QStringLiteral("http://192.168.1.45/onvif/device_service");
    updated.networkInterfaceToken = QStringLiteral("new-token");
    updated.ipv4Address = QStringLiteral("192.168.1.45");
    updated.ipv4SubnetMask = QStringLiteral("255.255.0.0");
    updated.ipv4Gateway = QStringLiteral("192.168.0.1");
    updated.ipv4DhcpEnabled = false;
    updated.ipv4ConfigurationKnown = true;
    updated.discoverySources << QStringLiteral("ONVIF GetNetworkInterfaces");

    const DeviceRecord merged = DeviceIdentity::mergeObservations(existing, updated);
    QCOMPARE(merged.ipAddress, updated.ipAddress);
    QCOMPARE(merged.deviceServiceUrl, updated.deviceServiceUrl);
    QCOMPARE(merged.networkInterfaceToken, QStringLiteral("new-token"));
    QCOMPARE(merged.ipv4Address, updated.ipv4Address);
    QCOMPARE(merged.ipv4SubnetMask, updated.ipv4SubnetMask);
    QCOMPARE(merged.ipv4Gateway, updated.ipv4Gateway);
    QVERIFY(merged.ipv4ConfigurationKnown);
}

void DeviceIdentityTest::revokesStaleOnvifInterfaceToken()
{
    DeviceRecord existing;
    existing.ipAddress = QStringLiteral("192.168.1.20");
    existing.networkInterfaceToken = QStringLiteral("stale-token");
    existing.ipv4ConfigurationKnown = true;

    DeviceRecord unmatched;
    unmatched.ipAddress = existing.ipAddress;
    unmatched.networkInterfaceToken = QStringLiteral("incomplete-token");
    unmatched.discoverySources << QStringLiteral("ONVIF GetNetworkInterfaces");
    unmatched.ipv4ConfigurationKnown = false;

    const DeviceRecord merged = DeviceIdentity::mergeObservations(existing, unmatched);
    QVERIFY(merged.networkInterfaceToken.isEmpty());
    QVERIFY(!merged.ipv4ConfigurationKnown);
    QVERIFY(merged.ipv4Address.isEmpty());
    QVERIFY(merged.ipv4SubnetMask.isEmpty());
    QVERIFY(merged.ipv4Gateway.isEmpty());
}

void DeviceIdentityTest::refreshesDnsFromOnvifObservation()
{
    DeviceRecord existing;
    existing.ipv4DnsServer = QStringLiteral("1.1.1.1");

    DeviceRecord updated;
    updated.ipv4DnsServer = QStringLiteral("8.8.8.8");
    updated.discoverySources << QStringLiteral("ONVIF GetDNS");

    const DeviceRecord merged = DeviceIdentity::mergeObservations(existing, updated);
    QCOMPARE(merged.ipv4DnsServer, QStringLiteral("8.8.8.8"));
}

void DeviceIdentityTest::freshObservationReplacesStaleInventoryStatus()
{
    DeviceRecord saved;
    saved.ipAddress = QStringLiteral("192.168.1.20");
    saved.reachabilityStatus = QStringLiteral("Stale");
    saved.lastSeenAt = QStringLiteral("2026-11-24T10:00:00Z");

    DeviceRecord live;
    live.ipAddress = saved.ipAddress;
    live.discoverySources << QStringLiteral("ONVIF WS-Discovery");
    live.reachabilityStatus = QStringLiteral("Reachable");
    live.lastSeenAt = QStringLiteral("2026-11-25T10:00:00Z");
    live.reachabilityCheckedAt = live.lastSeenAt;
    live.lastSuccessfulResponseAt = live.lastSeenAt;

    const DeviceRecord merged = DeviceIdentity::mergeObservations(saved, live);
    QCOMPARE(merged.reachabilityStatus, QStringLiteral("Reachable"));
    QCOMPARE(merged.lastSeenAt, live.lastSeenAt);
    QCOMPARE(merged.reachabilityCheckedAt, live.reachabilityCheckedAt);
    QCOMPARE(merged.lastSuccessfulResponseAt, live.lastSuccessfulResponseAt);
}

QTEST_GUILESS_MAIN(DeviceIdentityTest)
#include "device_identity_test.moc"
