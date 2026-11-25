#pragma once

#include <QString>
#include <QStringList>

struct DeviceRecord
{
    QString id;
    QString userLabel;
    QString ipAddress;
    QString macAddress;
    QString manufacturer;
    QString model;
    QString firmwareVersion;
    QString hardwareId;
    QString serialNumber;
    QString deviceServiceUrl;
    QString networkSummary;
    QString networkInterfaceToken;
    QString ipv4Address;
    QString ipv4SubnetMask;
    QString ipv4Gateway;
    QString ipv4DnsServer;
    bool ipv4DhcpEnabled{false};
    bool ipv4ConfigurationKnown{false};
    QString activationState{QStringLiteral("Unknown")};
    QStringList capabilities;
    QStringList discoverySources;
    QString interfaceName;
    QString onvifEndpoint;
    QStringList openPorts;
    bool isCandidate{false};
    QString reachabilityStatus{QStringLiteral("Not checked")};
    QString reachabilityCheckedAt;
    QString lastSuccessfulResponseAt;
    QString lastSeenAt;
};
