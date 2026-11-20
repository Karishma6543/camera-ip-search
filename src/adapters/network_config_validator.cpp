#include "network_config_validator.h"

#include <QHostAddress>

namespace {
bool parseUnicastIpv4(const QString &text, quint32 *value)
{
    QHostAddress address;
    if (!address.setAddress(text.trimmed()) ||
        address.protocol() != QAbstractSocket::IPv4Protocol ||
        address.isNull() || address.isLoopback() || address.isMulticast())
        return false;
    if (value) *value = address.toIPv4Address();
    return true;
}

bool fail(QString *error, const QString &message)
{
    if (error) *error = message;
    return false;
}
}

bool NetworkConfigValidator::validateStaticIpv4(const QString &address,
                                                const QString &subnetMask,
                                                const QString &gateway,
                                                const QString &dnsServer,
                                                QString *error)
{
    quint32 ip = 0;
    quint32 mask = 0;
    if (!parseUnicastIpv4(address, &ip))
        return fail(error, QStringLiteral("Enter a valid unicast IPv4 address."));
    if (!parseUnicastIpv4(subnetMask, &mask) || mask == 0 || mask == 0xffffffffU)
        return fail(error, QStringLiteral("Enter a contiguous IPv4 subnet mask."));
    const quint32 inverse = ~mask;
    if ((inverse & (inverse + 1U)) != 0)
        return fail(error, QStringLiteral("Subnet mask bits must be contiguous."));
    const quint32 network = ip & mask;
    const quint32 broadcast = network | inverse;
    if (ip == network || ip == broadcast)
        return fail(error, QStringLiteral("The device address cannot be the network or broadcast address."));

    if (!gateway.trimmed().isEmpty()) {
        quint32 gatewayIp = 0;
        if (!parseUnicastIpv4(gateway, &gatewayIp))
            return fail(error, QStringLiteral("Enter a valid unicast IPv4 gateway."));
        if ((gatewayIp & mask) != network || gatewayIp == network || gatewayIp == broadcast)
            return fail(error, QStringLiteral("Gateway must be a usable address in the same subnet."));
        if (gatewayIp == ip)
            return fail(error, QStringLiteral("Gateway and device address must be different."));
    }
    if (!dnsServer.trimmed().isEmpty()) {
        quint32 dns = 0;
        if (!parseUnicastIpv4(dnsServer, &dns))
            return fail(error, QStringLiteral("Enter a valid unicast IPv4 DNS server."));
    }
    if (error) error->clear();
    return true;
}
