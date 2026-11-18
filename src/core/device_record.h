#pragma once

#include <QString>
#include <QStringList>

struct DeviceRecord
{
    QString id;
    QString ipAddress;
    QString macAddress;
    QString manufacturer;
    QString model;
    QString firmwareVersion;
    QString hardwareId;
    QString serialNumber;
    QString deviceServiceUrl;
    QString networkSummary;
    QString activationState{QStringLiteral("Unknown")};
    QStringList discoverySources;
    QString interfaceName;
    QString onvifEndpoint;
    QStringList openPorts;
    bool isCandidate{false};
};
