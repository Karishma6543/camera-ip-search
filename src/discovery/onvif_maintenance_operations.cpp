#include "onvif_maintenance_operations.h"

#include <QHostAddress>
#include <QXmlStreamWriter>
#include <QAbstractSocket>
#include <functional>

namespace {
bool validToken(const QString &token)
{
    const QString value = token.trimmed();
    return !value.isEmpty() && value.size() <= 128 &&
           !value.contains(QChar::Null);
}

bool validIpv4(const QString &value)
{
    QHostAddress address;
    return address.setAddress(value.trimmed()) &&
           address.protocol() == QAbstractSocket::IPv4Protocol;
}

QByteArray envelope(const std::function<void(QXmlStreamWriter &)> &writeBody)
{
    QByteArray payload;
    QXmlStreamWriter xml(&payload);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("s:Envelope"));
    xml.writeNamespace(QStringLiteral("http://www.w3.org/2003/05/soap-envelope"), QStringLiteral("s"));
    xml.writeNamespace(QStringLiteral("http://www.onvif.org/ver10/device/wsdl"), QStringLiteral("tds"));
    xml.writeNamespace(QStringLiteral("http://www.onvif.org/ver10/schema"), QStringLiteral("tt"));
    xml.writeStartElement(QStringLiteral("s:Body"));
    writeBody(xml);
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    return payload;
}
}

QByteArray OnvifMaintenanceOperations::buildSetDhcpRequest(const QString &interfaceToken,
                                                            bool enabled)
{
    const QString token = interfaceToken.trimmed();
    if (!validToken(token)) return {};
    return envelope([&](QXmlStreamWriter &xml) {
        xml.writeStartElement(QStringLiteral("tds:SetNetworkInterfaces"));
        xml.writeTextElement(QStringLiteral("tds:InterfaceToken"), token);
        xml.writeStartElement(QStringLiteral("tds:NetworkInterface"));
        xml.writeStartElement(QStringLiteral("tt:IPv4"));
        xml.writeTextElement(QStringLiteral("tt:Enabled"), QStringLiteral("true"));
        xml.writeStartElement(QStringLiteral("tt:Config"));
        xml.writeTextElement(QStringLiteral("tt:DHCP"), enabled ? QStringLiteral("true") : QStringLiteral("false"));
        xml.writeEndElement();
        xml.writeEndElement();
        xml.writeEndElement();
    });
}

QByteArray OnvifMaintenanceOperations::buildSetDnsRequest(bool fromDhcp,
                                                           const QStringList &ipv4Servers)
{
    if (fromDhcp) {
        if (!ipv4Servers.isEmpty()) return {};
    } else {
        if (ipv4Servers.isEmpty() || ipv4Servers.size() > 2) return {};
        for (const QString &server : ipv4Servers)
            if (!validIpv4(server)) return {};
    }
    return envelope([&](QXmlStreamWriter &xml) {
        xml.writeStartElement(QStringLiteral("tds:SetDNS"));
        xml.writeTextElement(QStringLiteral("tds:FromDHCP"),
                             fromDhcp ? QStringLiteral("true") : QStringLiteral("false"));
        for (const QString &server : ipv4Servers) {
            xml.writeStartElement(QStringLiteral("tds:DNSManual"));
            xml.writeTextElement(QStringLiteral("tt:Type"), QStringLiteral("IPv4"));
            xml.writeTextElement(QStringLiteral("tt:IPv4Address"), server.trimmed());
            xml.writeEndElement();
        }
        xml.writeEndElement();
    });
}

QByteArray OnvifMaintenanceOperations::buildSystemRebootRequest()
{
    return envelope([](QXmlStreamWriter &xml) {
        xml.writeEmptyElement(QStringLiteral("tds:SystemReboot"));
    });
}
