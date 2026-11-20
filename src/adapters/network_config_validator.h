#pragma once

#include <QString>

// Validates a proposed static IPv4 configuration before any device command is sent.
class NetworkConfigValidator final
{
public:
    static bool validateStaticIpv4(const QString &address,
                                   const QString &subnetMask,
                                   const QString &gateway,
                                   const QString &dnsServer,
                                   QString *error = nullptr);
};
