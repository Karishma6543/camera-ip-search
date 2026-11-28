#include "device_identity.h"


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


bool conflictingMacs(const QString &left, const QString &right)
{
    const QString normalizedLeft = normalizedMac(left);
    const QString normalizedRight = normalizedMac(right);
    return !normalizedLeft.isEmpty() && !normalizedRight.isEmpty() &&
           normalizedLeft != normalizedRight;
}

bool isKnownValue(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    return !normalized.isEmpty() &&
           normalized != QStringLiteral("n/a") &&
           !normalized.contains(QStringLiteral("unknown")) &&
           !normalized.contains(QStringLiteral("unverified"));
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
            if (sameNonEmpty(endpoint, it.value().onvifEndpoint) &&
                !conflictingMacs(mac, it.value().macAddress))
                return it.key();
    }
    if (!serial.isEmpty() && isKnownValue(manufacturer)) {
        for (auto it = devices.cbegin(); it != devices.cend(); ++it) {
            const DeviceRecord &known = it.value();
            if (sameNonEmpty(serial, known.serialNumber) &&
                sameNonEmpty(manufacturer, known.manufacturer) &&
                !conflictingMacs(mac, known.macAddress))
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

bool hasStrongMatch(const DeviceRecord &expected, const DeviceRecord &observation)
{
    const QString expectedMac = normalizedMac(expected.macAddress);
    const QString observedMac = normalizedMac(observation.macAddress);
    if (!expectedMac.isEmpty() && !observedMac.isEmpty() && expectedMac == observedMac)
        return true;
    if (conflictingMacs(expected.macAddress, observation.macAddress))
        return false;
    if (sameNonEmpty(expected.onvifEndpoint, observation.onvifEndpoint))
        return true;
    return sameNonEmpty(expected.serialNumber, observation.serialNumber) &&
           sameNonEmpty(expected.manufacturer, observation.manufacturer);
}

DeviceRecord mergeObservations(const DeviceRecord &existing,
                               const DeviceRecord &observation)
{
    DeviceRecord merged = existing;
    if (merged.id.isEmpty()) merged.id = observation.id;
    if (merged.userLabel.isEmpty()) merged.userLabel = observation.userLabel;

    const bool hasProtocolEvidence =
        observation.discoverySources.contains(QStringLiteral("ONVIF WS-Discovery")) ||
        observation.discoverySources.contains(QStringLiteral("ONVIF GetDeviceInformation")) ||
        observation.discoverySources.contains(QStringLiteral("Project UDP 9988")) ||
        observation.discoverySources.contains(QStringLiteral("ONVIF GetNetworkInterfaces"));
    const bool verifiedObservation = !observation.isCandidate && hasProtocolEvidence;
    if (verifiedObservation && !observation.ipAddress.trimmed().isEmpty())
        merged.ipAddress = observation.ipAddress;
    if (merged.macAddress.isEmpty()) merged.macAddress = observation.macAddress;
    if (!isKnownValue(merged.manufacturer) && isKnownValue(observation.manufacturer))
        merged.manufacturer = observation.manufacturer;
    if (!isKnownValue(merged.model) && isKnownValue(observation.model))
        merged.model = observation.model;
    if (verifiedObservation && !observation.firmwareVersion.isEmpty())
        merged.firmwareVersion = observation.firmwareVersion;
    if (merged.hardwareId.isEmpty()) merged.hardwareId = observation.hardwareId;
    if (merged.serialNumber.isEmpty()) merged.serialNumber = observation.serialNumber;
    if (verifiedObservation && !observation.deviceServiceUrl.isEmpty())
        merged.deviceServiceUrl = observation.deviceServiceUrl;
    const bool networkInterfacesUpdated =
        observation.discoverySources.contains(QStringLiteral("ONVIF GetNetworkInterfaces"));
    if (networkInterfacesUpdated) {
        merged.networkSummary = observation.networkSummary;
        // An empty token is meaningful: the ONVIF response did not identify
        // the interface owning the discovered IP, so stale write access must
        // be revoked rather than retained from an earlier observation.
        merged.ipv4ConfigurationKnown = observation.ipv4ConfigurationKnown;
        if (observation.ipv4ConfigurationKnown) {
            merged.networkInterfaceToken = observation.networkInterfaceToken;
            merged.ipv4Address = observation.ipv4Address;
            merged.ipv4SubnetMask = observation.ipv4SubnetMask;
            merged.ipv4Gateway = observation.ipv4Gateway;
            merged.ipv4DhcpEnabled = observation.ipv4DhcpEnabled;
        } else {
            merged.networkInterfaceToken.clear();
            merged.ipv4Address.clear();
            merged.ipv4SubnetMask.clear();
            merged.ipv4Gateway.clear();
            merged.ipv4DhcpEnabled = false;
        }
    } else if (observation.discoverySources.contains(QStringLiteral("ONVIF GetDNS"))) {
        merged.ipv4DnsServer = observation.ipv4DnsServer;
    } else {
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
    }
    if (verifiedObservation && !observation.onvifEndpoint.isEmpty())
        merged.onvifEndpoint = observation.onvifEndpoint;
    if (verifiedObservation && !observation.interfaceName.isEmpty())
        merged.interfaceName = observation.interfaceName;
    if (!observation.lastSeenAt.isEmpty()) {
        // A timestamp supplied by the current observation supersedes the
        // historical/stale reachability state loaded from persistent inventory.
        merged.lastSeenAt = observation.lastSeenAt;
        if (!observation.reachabilityStatus.isEmpty())
            merged.reachabilityStatus = observation.reachabilityStatus;
        if (!observation.reachabilityCheckedAt.isEmpty())
            merged.reachabilityCheckedAt = observation.reachabilityCheckedAt;
        if (!observation.lastSuccessfulResponseAt.isEmpty())
            merged.lastSuccessfulResponseAt = observation.lastSuccessfulResponseAt;
    } else {
        if (merged.reachabilityStatus == QStringLiteral("Not checked") &&
            observation.reachabilityStatus != QStringLiteral("Not checked"))
            merged.reachabilityStatus = observation.reachabilityStatus;
        if (merged.reachabilityCheckedAt.isEmpty())
            merged.reachabilityCheckedAt = observation.reachabilityCheckedAt;
        if (merged.lastSuccessfulResponseAt.isEmpty())
            merged.lastSuccessfulResponseAt = observation.lastSuccessfulResponseAt;
    }

    appendUnique(merged.discoverySources, observation.discoverySources);
    if (observation.discoverySources.contains(QStringLiteral("ONVIF GetCapabilities")))
        merged.capabilities = observation.capabilities;
    else
        appendUnique(merged.capabilities, observation.capabilities);
    appendUnique(merged.openPorts, observation.openPorts);

    // A confirmed protocol observation upgrades a TCP-only candidate.
    if (verifiedObservation)
        merged.isCandidate = false;
    // ONVIF does not define activation state. Only a trusted source that
    // actually reports a known state may replace the inventory value.
    if (verifiedObservation && isKnownValue(observation.activationState))
        merged.activationState = observation.activationState;
    return merged;
}
}
