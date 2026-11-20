#pragma once

#include <QByteArray>
#include <QString>

namespace OnvifNetworkConfig {
bool parseSetNetworkInterfacesResponse(const QByteArray &xml, bool *rebootNeeded,
                                        QString *error = nullptr);
bool verifyInterfaceIpv4Address(const QByteArray &xml, const QString &interfaceToken,
                                const QString &address, QString *error = nullptr);
bool parseSoapOperationResponse(const QByteArray &xml, const QString &expectedResponse,
                                QString *error = nullptr);
QByteArray buildSetNetworkDefaultGatewayRequest(const QString &gateway);
}
