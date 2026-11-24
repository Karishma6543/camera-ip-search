#include "ip_assignment_preflight.h"

#include <QHostAddress>
#include <QSet>

namespace {
bool isIpv4(const QString &value)
{
    QHostAddress address;
    return address.setAddress(value) &&
           address.protocol() == QAbstractSocket::IPv4Protocol;
}
}

QVector<IpAssignmentPreflightResult> IpAssignmentPreflight::evaluate(
    const QList<IpAssignmentTarget> &plan,
    const QSet<QString> &inventoryAddresses,
    const QSet<QString> &localReservedAddresses,
    const Probe &probe)
{
    QVector<IpAssignmentPreflightResult> results;
    results.reserve(plan.size());

    QSet<QString> deviceIds;
    QSet<QString> proposedAddresses;
    QSet<QString> occupied;

    auto normalizeSet = [&occupied](const QSet<QString> &values) {
        for (const QString &value : values)
            occupied.insert(value.trimmed());
    };
    normalizeSet(inventoryAddresses);
    normalizeSet(localReservedAddresses);

    bool structurallyValid = !plan.isEmpty();
    for (const IpAssignmentTarget &target : plan) {
        const QString id = target.deviceId.trimmed();
        const QString address = target.proposedAddress.trimmed();
        if (id.isEmpty() || deviceIds.contains(id) ||
            !isIpv4(address) || proposedAddresses.contains(address)) {
            structurallyValid = false;
        }
        deviceIds.insert(id);
        proposedAddresses.insert(address);
    }

    for (const IpAssignmentTarget &target : plan) {
        IpAssignmentPreflightResult result;
        result.deviceId = target.deviceId.trimmed();
        result.currentAddress = target.currentAddress.trimmed();
        result.proposedAddress = target.proposedAddress.trimmed();

        if (!structurallyValid) {
            result.state = IpAssignmentPreflightState::InvalidPlan;
            result.message = QStringLiteral("Plan contains a missing/duplicate device ID, invalid IPv4 address, duplicate target address, or no targets.");
        } else if (occupied.contains(result.proposedAddress)) {
            result.state = IpAssignmentPreflightState::AddressConflict;
            result.message = QStringLiteral("Proposed address is already reserved by inventory or local network configuration.");
        } else if (!probe) {
            result.state = IpAssignmentPreflightState::ProbeUnavailable;
            result.message = QStringLiteral("No live address-conflict probe is configured; assignment is blocked.");
        } else {
            const IpAddressProbeResult probeResult = probe(result.proposedAddress);
            if (probeResult.state == IpAddressProbeState::Conflict) {
                result.state = IpAssignmentPreflightState::AddressConflict;
                result.message = probeResult.evidence.trimmed().isEmpty()
                    ? QStringLiteral("A live probe detected an address conflict.")
                    : probeResult.evidence.trimmed();
            } else if (probeResult.state == IpAddressProbeState::Clear) {
                result.state = IpAssignmentPreflightState::Ready;
                result.message = probeResult.evidence.trimmed().isEmpty()
                    ? QStringLiteral("No conflict detected by the configured probe.")
                    : probeResult.evidence.trimmed();
            } else {
                result.state = IpAssignmentPreflightState::ProbeUnavailable;
                result.message = probeResult.evidence.trimmed().isEmpty()
                    ? QStringLiteral("The address could not be verified; assignment is blocked.")
                    : probeResult.evidence.trimmed();
            }
        }
        results.push_back(result);
    }
    return results;
}
