#include "device_identity.h"

#include <QSet>

namespace {
QString normalizedMac(const QString &value)
{
    QString result = value.trimmed().toUpper();
    result.replace('-', ':');
    return result;
}

bool sameNonEmpty(const QString &left, const QString &right)
{
    return !left.trimmed().isEmpty() &&
           left.trimmed().compare(right.trimmed(), Qt::CaseInsensitive) == 0;
}

bool isKnownValue(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    return !normalized.isEmpty() &&
           normalized != QStringLiteral("unknown") &&
           normalized != QStringLiteral("model unknown") &&
           normalized != QStringLiteral("n/a");
}

void appendUnique(QStringList &target, const QStringList &incoming)
{
    for (const QString &value : incoming) {
        const QString cleaned = value.trimmed();
        if (!cleaned.isEmpty() && !target.contains(cleaned))
            target.append(cleaned);
    }
}
}

namespace DeviceIdentity {
QString findMatchingKey(const QHash<QString, DeviceRecord> &devices,
                        const DeviceRecord &observation)
{
    const QString mac = normalizedMac(observation.macAddress);
    const QString endpoint = observation.onvifEndpoint.trimmed();
    const QString serial = observation.serialNumber.trimmed();
    const QString manufacturer = observation.manufacturer.trimmed();
    const QString ip = observation.ipAddress.trimmed();

    // Strong identifiers take precedence over the transient network address.
    for (auto it = devices.cbegin(); it != devices.cend(); ++it) {
        const DeviceRecord &known = it.value();
        if (!mac.isEmpty() && mac == normalizedMac(known.macAddress))
            return it.key();
    }
    if (!endpoint.isEmpty()) {
        for (auto it = devices.cbegin(); it != devices.cend(); ++it)
            if (sameNonEmpty(endpoint, it.value().onvifEndpoint))
                return it.key();
    }
    if (!serial.isEmpty() && isKnownValue(manufacturer)) {
        for (auto it = devices.cbegin(); it != devices.cend(); ++it) {
            const DeviceRecord &known = it.value();
            if (sameNonEmpty(serial, known.serialNumber) &&
                sameNonEmpty(manufacturer, known.manufacturer))
                return it.key();
        }
    }

    if (ip.isEmpty())
        return {};
    for (auto it = devices.cbegin(); it != devices.cend(); ++it) {
        const DeviceRecord &known = it.value();
        if (!sameNonEmpty(ip, known.ipAddress))
            continue;
        const QString knownMac = normalizedMac(known.macAddress);
        // Never merge two explicitly different MAC identities just because a
        // DHCP reassignment or stale inventory row shares an IPv4 address.
        if (!mac.isEmpty() && !knownMac.isEmpty() && mac != knownMac)
            continue;
        return it.key();
    }
    return {};
}

DeviceRecord mergeObservations(const DeviceRecord &existing,
                               const DeviceRecord &observation)
{
    DeviceRecord merged = existing;
    if (merged.id.isEmpty()) merged.id = observation.id;
    if (merged.userLabel.isEmpty()) merged.userLabel = observation.userLabel;

    const bool verifiedObservation = !observation.isCandidate;
    if (verifiedObservation && !observation.ipAddress.trimmed().isEmpty())
        merged.ipAddress = observation.ipAddress;
    if (merged.macAddress.isEmpty()) merged.macAddress = observation.macAddress;
    if (!isKnownValue(merged.manufacturer) && isKnownValue(observation.manufacturer))
        merged.manufacturer = observation.manufacturer;
    if (!isKnownValue(merged.model) && isKnownValue(observation.model))
        merged.model = observation.model;
    if (merged.firmwareVersion.isEmpty()) merged.firmwareVersion = observation.firmwareVersion;
    if (merged.hardwareId.isEmpty()) merged.hardwareId = observation.hardwareId;
    if (merged.serialNumber.isEmpty()) merged.serialNumber = observation.serialNumber;
    if (merged.deviceServiceUrl.isEmpty()) merged.deviceServiceUrl = observation.deviceServiceUrl;
    if (merged.networkSummary.isEmpty()) merged.networkSummary = observation.networkSummary;
    if (merged.networkInterfaceToken.isEmpty()) merged.networkInterfaceToken = observation.networkInterfaceToken;
    if (merged.ipv4Address.isEmpty()) merged.ipv4Address = observation.ipv4Address;
    if (merged.ipv4SubnetMask.isEmpty()) merged.ipv4SubnetMask = observation.ipv4SubnetMask;
    if (merged.ipv4Gateway.isEmpty()) merged.ipv4Gateway = observation.ipv4Gateway;
    if (merged.ipv4DnsServer.isEmpty()) merged.ipv4DnsServer = observation.ipv4DnsServer;
    if (!merged.ipv4ConfigurationKnown && observation.ipv4ConfigurationKnown) {
        merged.ipv4ConfigurationKnown = true;
        merged.ipv4DhcpEnabled = observation.ipv4DhcpEnabled;
    }
    if (merged.onvifEndpoint.isEmpty()) merged.onvifEndpoint = observation.onvifEndpoint;
    if (merged.interfaceName.isEmpty()) merged.interfaceName = observation.interfaceName;
    if (merged.reachabilityStatus == QStringLiteral("Not checked") &&
        observation.reachabilityStatus != QStringLiteral("Not checked"))
        merged.reachabilityStatus = observation.reachabilityStatus;
    if (merged.reachabilityCheckedAt.isEmpty())
        merged.reachabilityCheckedAt = observation.reachabilityCheckedAt;
    if (merged.lastSuccessfulResponseAt.isEmpty())
        merged.lastSuccessfulResponseAt = observation.lastSuccessfulResponseAt;

    appendUnique(merged.discoverySources, observation.discoverySources);
    appendUnique(merged.capabilities, observation.capabilities);
    appendUnique(merged.openPorts, observation.openPorts);

    // A confirmed protocol observation upgrades a TCP-only candidate.
    if (verifiedObservation)
        merged.isCandidate = false;
    if (merged.activationState == QStringLiteral("Unverified") &&
        observation.activationState != QStringLiteral("Unverified"))
        merged.activationState = observation.activationState;
    return merged;
}
}
