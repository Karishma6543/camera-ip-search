#pragma once

#include "app/ip_assignment_preflight.h"

#include <QString>

class IpConflictProbe final
{
public:
    // Interface-scoped IPv4 conflict probe. Linux uses iputils arping -D and can
    // report Clear only from an explicit zero-response DAD result. Windows uses
    // SendARP only for positive conflict evidence; no reply remains Unknown.
    // macOS remains Unknown until a native DAD backend is validated. Ambiguous
    // results always fail closed.
    static IpAddressProbeResult probe(const QString &targetAddress,
                                      const QString &interfaceName,
                                      const QString &arpingProgram = QStringLiteral("arping"));
};
