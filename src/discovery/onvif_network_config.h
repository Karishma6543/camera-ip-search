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
inline bool verifyInterfaceIpv4Configuration(const QByteArray &xml,
                                             const QString &interfaceToken,
                                             const QString &address,
                                             const QString &prefix,
                                             const QString &gateway,
                                             QString *error = nullptr)
{
    InterfaceIpv4Info actual;
    QStringList summaries;
    if (!parseNetworkInterfaces(xml, address, &actual, &summaries, error))
        return false;
    if (actual.token != interfaceToken || actual.address != address ||
        actual.prefix != prefix ||
        actual.dhcp.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 ||
        actual.dhcp == QStringLiteral("1")) {
        if (error) *error = QStringLiteral("The selected interface address, prefix or DHCP mode does not match the requested configuration.");
        return false;
    }
    if (!gateway.trimmed().isEmpty() &&
        actual.gateway.compare(gateway.trimmed(), Qt::CaseInsensitive) != 0) {
        if (error) *error = QStringLiteral("The selected interface gateway does not match the requested gateway.");
        return false;
    }
    if (error) error->clear();
    return true;
}
bool parseSoapOperationResponse(const QByteArray &xml, const QString &expectedResponse,
                                QString *error = nullptr);
QByteArray buildSetNetworkDefaultGatewayRequest(const QString &gateway);
QByteArray buildSetSystemFactoryDefaultRequest(const QString &factoryDefaultType);
}
