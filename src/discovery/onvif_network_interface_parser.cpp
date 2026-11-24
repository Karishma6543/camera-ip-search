#include "onvif_network_interface_parser.h"
#include <QXmlStreamReader>
#include <QStringList>
bool OnvifNetworkInterfaceParser::parse(const QByteArray &payload,
    QVector<OnvifNetworkInterface> *interfaces, QString *error)
{
    if (!interfaces) { if (error) *error = QStringLiteral("Output interface list is required."); return false; }
    interfaces->clear();
    if (payload.isEmpty() || payload.size() > 1024 * 1024) {
        if (error) *error = QStringLiteral("ONVIF response is empty or exceeds the 1 MiB limit.");
        return false;
    }
    QXmlStreamReader xml(payload);
    QStringList path;
    OnvifNetworkInterface current;
    bool inInterface = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            path.append(name);
            if (name == QStringLiteral("NetworkInterface") || name == QStringLiteral("NetworkInterfaces")) {
                inInterface = true; current = OnvifNetworkInterface{};
                current.token = xml.attributes().value(QStringLiteral("token")).toString();
                continue;
            }
            if (!inInterface) continue;
            const QString joined = path.join(QLatin1Char('/'));
            const bool field =
                joined.endsWith(QStringLiteral("/NetworkInterface/Name")) || joined.endsWith(QStringLiteral("/NetworkInterfaces/Info/Name")) ||
                joined.endsWith(QStringLiteral("/NetworkInterface/HwAddress")) || joined.endsWith(QStringLiteral("/NetworkInterfaces/Info/HwAddress")) ||
                joined.endsWith(QStringLiteral("/IPv4/Config/DHCP")) ||
                joined.endsWith(QStringLiteral("/IPv4/Config/Manual/Address")) ||
                joined.endsWith(QStringLiteral("/IPv4/Config/FromDHCP/Address")) ||
                joined.endsWith(QStringLiteral("/IPv4/Config/LinkLocal/Address")) ||
                joined.endsWith(QStringLiteral("/IPv4/Config/Manual/PrefixLength")) ||
                joined.endsWith(QStringLiteral("/IPv4/Config/FromDHCP/PrefixLength")) ||
                joined.endsWith(QStringLiteral("/IPv4/Config/LinkLocal/PrefixLength")) ||
                joined.endsWith(QStringLiteral("/IPv4/Config/Gateway/IPv4Address"));
            if (!field) continue;
            const QString value = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            if (joined.endsWith(QStringLiteral("/NetworkInterface/Name")) || joined.endsWith(QStringLiteral("/NetworkInterfaces/Info/Name"))) current.name = value;
            else if (joined.endsWith(QStringLiteral("/NetworkInterface/HwAddress")) || joined.endsWith(QStringLiteral("/NetworkInterfaces/Info/HwAddress"))) current.macAddress = value;
            else if (joined.endsWith(QStringLiteral("/IPv4/Config/DHCP"))) current.dhcp = value;
            else if (joined.endsWith(QStringLiteral("/IPv4/Config/Manual/Address"))) current.ipv4Address = value;
            else if (joined.endsWith(QStringLiteral("/IPv4/Config/FromDHCP/Address")) && current.ipv4Address.isEmpty()) current.ipv4Address = value;
            else if (joined.endsWith(QStringLiteral("/IPv4/Config/LinkLocal/Address")) && current.ipv4Address.isEmpty()) current.ipv4Address = value;
            else if (joined.endsWith(QStringLiteral("/IPv4/Config/Manual/PrefixLength"))) current.prefixLength = value;
            else if (joined.endsWith(QStringLiteral("/IPv4/Config/FromDHCP/PrefixLength")) && current.prefixLength.isEmpty()) current.prefixLength = value;
            else if (joined.endsWith(QStringLiteral("/IPv4/Config/LinkLocal/PrefixLength")) && current.prefixLength.isEmpty()) current.prefixLength = value;
            else if (joined.endsWith(QStringLiteral("/IPv4/Config/Gateway/IPv4Address")) ||
                     joined.endsWith(QStringLiteral("/IPv4/Config/Gateway/Address"))) current.gateway = value;
            path.removeLast();
        } else if (xml.isEndElement()) {
            if ((xml.name() == QStringLiteral("NetworkInterface") || xml.name() == QStringLiteral("NetworkInterfaces")) && inInterface) {
                interfaces->append(current); inInterface = false;
            }
            if (!path.isEmpty()) path.removeLast();
        }
    }
    if (xml.hasError()) { interfaces->clear(); if (error) *error = xml.errorString(); return false; }
    return true;
}
