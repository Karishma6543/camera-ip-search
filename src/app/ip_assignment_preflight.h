#pragma once

#include "discovery/ip_assignment_planner.h"

#include <QSet>
#include <QString>
#include <QVector>
#include <functional>

enum class IpAddressProbeState {
    Clear,
    Conflict,
    Unknown
};

struct IpAddressProbeResult
{
    IpAddressProbeState state{IpAddressProbeState::Unknown};
    QString evidence;
};

enum class IpAssignmentPreflightState {
    Ready,
    AddressConflict,
    ProbeUnavailable,
    InvalidPlan
};

struct IpAssignmentPreflightResult
{
    QString deviceId;
    QString currentAddress;
    QString proposedAddress;
    IpAssignmentPreflightState state{IpAssignmentPreflightState::InvalidPlan};
    QString message;
};

class IpAssignmentPreflight final
{
public:
    using Probe = std::function<IpAddressProbeResult(const QString &address)>;

    // inventoryAddresses and localReservedAddresses must include every known
    // occupied address (including selected devices' current addresses),
    // interface addresses, gateways and other infrastructure reservations.
    // A missing probe or an Unknown probe result is never treated as Clear.
    static QVector<IpAssignmentPreflightResult> evaluate(
        const QList<IpAssignmentTarget> &plan,
        const QSet<QString> &inventoryAddresses,
        const QSet<QString> &localReservedAddresses,
        const Probe &probe);
};
