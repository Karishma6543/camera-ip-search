#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

// Validated ONVIF Device-service SOAP request builders. These functions only
// construct protocol payloads; callers must still authenticate, enforce TLS
// policy, verify the camera's advertised capabilities and verify responses.
namespace OnvifMaintenanceOperations {
QByteArray buildSetDhcpRequest(const QString &interfaceToken, bool enabled);
QByteArray buildSetDnsRequest(bool fromDhcp, const QStringList &ipv4Servers);
QByteArray buildSystemRebootRequest();
}
