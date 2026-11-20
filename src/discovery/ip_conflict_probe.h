#pragma once

#include "app/ip_assignment_preflight.h"

#include <QString>

class IpConflictProbe final
{
public:
    // Linux-only IPv4 duplicate-address-detection ARP probe on the explicitly
    // selected interface. Requires iputils arping and raw-socket privileges.
    // Windows/macOS deliberately return Unknown until native backends are
    // separately implemented and validated. Only a documented zero-response
    // DAD result is Clear; every ambiguous result fails closed.
    static IpAddressProbeResult probe(const QString &targetAddress,
                                      const QString &interfaceName,
                                      const QString &arpingProgram = QStringLiteral("arping"));
};
