#pragma once

#include "app/ip_assignment_preflight.h"

#include <QString>

class IpConflictProbe final
{
public:
    // Runs an IPv4 duplicate-address-detection ARP probe on the explicitly
    // selected interface. Requires an iputils-compatible "arping" executable.
    // Only the documented zero-response DAD result is Clear; every ambiguous
    // result (including missing utility, permissions, timeout or bad output)
    // is Unknown and must block assignment.
    static IpAddressProbeResult probe(const QString &targetAddress,
                                      const QString &interfaceName,
                                      const QString &arpingProgram = QStringLiteral("arping"));
};
