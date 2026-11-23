#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace OnvifNetworkConfig {
struct InterfaceIpv4Info {
    QString token;
    QString name;
    QString mac;
    QString address;
    QString prefix;
    QString dhcp;
    QString gateway;
};

bool parseNetworkInterfaces(const QByteArray &xml, const QString &preferredIpv4,
                            InterfaceIpv4Info *selected, QStringList *summaries,
                            QString *error = nullptr);
bool parseSetNetworkInterfacesResponse(const QByteArray &xml, bool *rebootNeeded,
                                        QString *error = nullptr);
bool verifyInterfaceIpv4Address(const QByteArray &xml, const QString &interfaceToken,
                                const QString &address, QString *error = nullptr);
bool parseSoapOperationResponse(const QByteArray &xml, const QString &expectedResponse,
                                QString *error = nullptr);
QByteArray buildSetNetworkDefaultGatewayRequest(const QString &gateway);
}
