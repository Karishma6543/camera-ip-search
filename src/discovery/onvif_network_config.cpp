#include "onvif_network_config.h"

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QHostAddress>
#include <QAbstractSocket>
#include <QVector>

namespace {
void setError(QString *error, const QString &message)
{
    if (error) *error = message;
}
}


bool OnvifNetworkConfig::parseNetworkInterfaces(const QByteArray &payload,
                                                 const QString &preferredIpv4,
                                                 InterfaceIpv4Info *selected,
                                                 QStringList *summaries,
                                                 QString *error)
{
    if (selected) *selected = InterfaceIpv4Info{};
    if (summaries) summaries->clear();
    if (!selected || !summaries || payload.isEmpty() || payload.size() > 1024 * 1024 ||
        preferredIpv4.isEmpty()) {
        setError(error, QStringLiteral("Invalid or oversized GetNetworkInterfaces response input."));
        return false;
    }

    struct ParsedInterface {
        InterfaceIpv4Info info;
        QString manualAddress;
        QString manualPrefix;
        QString dhcpAddress;
        QString dhcpPrefix;
        QString linkLocalAddress;
        QString linkLocalPrefix;
    };
    QVector<ParsedInterface> interfaces;
    QXmlStreamReader xml(payload);
    QStringList path;
    ParsedInterface current;
    bool inInterface = false;
    bool responseSeen = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            path.append(name);
            if (name == QStringLiteral("Fault")) {
                setError(error, QStringLiteral("Camera returned a SOAP fault."));
                return false;
            }
            if (name == QStringLiteral("GetNetworkInterfacesResponse"))
                responseSeen = true;
            if (name == QStringLiteral("NetworkInterface") || name == QStringLiteral("NetworkInterfaces")) {
                inInterface = true;
                current = ParsedInterface{};
                current.info.token = xml.attributes().value(QStringLiteral("token")).toString();
                continue;
            }
            if (!inInterface) continue;

            const QString joined = path.join(QLatin1Char('/'));
            const bool isAddress =
                joined.endsWith(QStringLiteral("/IPv4/Config/Manual/Address")) ||
                joined.endsWith(QStringLiteral("/IPv4/Config/FromDHCP/Address")) ||
                joined.endsWith(QStringLiteral("/IPv4/Config/LinkLocal/Address"));
            const bool isPrefix =
                joined.endsWith(QStringLiteral("/IPv4/Config/Manual/PrefixLength")) ||
                joined.endsWith(QStringLiteral("/IPv4/Config/FromDHCP/PrefixLength")) ||
                joined.endsWith(QStringLiteral("/IPv4/Config/LinkLocal/PrefixLength"));
            bool consumed = false;
            if (name == QStringLiteral("Name")) {
                current.info.name = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                consumed = true;
            } else if (name == QStringLiteral("HwAddress")) {
                current.info.mac = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                consumed = true;
            } else if (joined.endsWith(QStringLiteral("/IPv4/Config/DHCP"))) {
                current.info.dhcp = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                consumed = true;
            } else if (isAddress) {
                const QString value = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                if (joined.endsWith(QStringLiteral("/Manual/Address")))
                    current.manualAddress = value;
                else if (joined.endsWith(QStringLiteral("/FromDHCP/Address")))
                    current.dhcpAddress = value;
                else if (joined.endsWith(QStringLiteral("/LinkLocal/Address")))
                    current.linkLocalAddress = value;
                consumed = true;
            } else if (isPrefix) {
                const QString value = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                if (joined.endsWith(QStringLiteral("/Manual/PrefixLength")))
                    current.manualPrefix = value;
                else if (joined.endsWith(QStringLiteral("/FromDHCP/PrefixLength")))
                    current.dhcpPrefix = value;
                else if (joined.endsWith(QStringLiteral("/LinkLocal/PrefixLength")))
                    current.linkLocalPrefix = value;
                consumed = true;
            } else if (joined.endsWith(QStringLiteral("/IPv4/Config/Gateway/IPv4Address"))) {
                current.info.gateway = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                consumed = true;
            }
            if (consumed) path.removeLast();
        } else if (xml.isEndElement()) {
            if ((xml.name() == QStringLiteral("NetworkInterface") || xml.name() == QStringLiteral("NetworkInterfaces")) && inInterface) {
                const bool dhcpEnabled =
                    current.info.dhcp.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 ||
                    current.info.dhcp == QStringLiteral("1");
                if (dhcpEnabled) {
                    current.info.address = !current.dhcpAddress.isEmpty() ? current.dhcpAddress
                        : (!current.linkLocalAddress.isEmpty() ? current.linkLocalAddress : current.manualAddress);
                    current.info.prefix = !current.dhcpPrefix.isEmpty() ? current.dhcpPrefix
                        : (!current.linkLocalPrefix.isEmpty() ? current.linkLocalPrefix : current.manualPrefix);
                } else {
                    current.info.address = !current.manualAddress.isEmpty() ? current.manualAddress
                        : (!current.linkLocalAddress.isEmpty() ? current.linkLocalAddress : current.dhcpAddress);
                    current.info.prefix = !current.manualPrefix.isEmpty() ? current.manualPrefix
                        : (!current.linkLocalPrefix.isEmpty() ? current.linkLocalPrefix : current.dhcpPrefix);
                }
                interfaces.append(current);
                inInterface = false;
            }
            if (!path.isEmpty()) path.removeLast();
        }
    }

    if (xml.hasError() || !responseSeen || interfaces.isEmpty()) {
        setError(error, QStringLiteral("Malformed or incomplete GetNetworkInterfaces response."));
        return false;
    }

    QHostAddress preferred;
    if (!preferred.setAddress(preferredIpv4) ||
        preferred.protocol() != QAbstractSocket::IPv4Protocol) {
        setError(error, QStringLiteral("The discovered device address is not a valid IPv4 address."));
        return false;
    }

    for (const ParsedInterface &entry : interfaces) {
        QStringList fields;
        if (!entry.info.name.isEmpty()) fields << entry.info.name;
        if (!entry.info.address.isEmpty()) fields << entry.info.address;
        if (!entry.info.mac.isEmpty()) fields << entry.info.mac;
        if (!fields.isEmpty()) summaries->append(fields.join(QStringLiteral(" | ")));

        QHostAddress reported;
        if (selected->token.isEmpty() && !entry.info.token.isEmpty() &&
            reported.setAddress(entry.info.address) &&
            reported.protocol() == QAbstractSocket::IPv4Protocol &&
            reported == preferred)
            *selected = entry.info;
    }
    if (selected->token.isEmpty()) {
        setError(error, QStringLiteral("No ONVIF network interface matches the discovered IPv4 address."));
        return false;
    }
    if (error) error->clear();
    return true;
}

bool OnvifNetworkConfig::parseSetNetworkInterfacesResponse(const QByteArray &payload,
                                                             bool *rebootNeeded,
                                                             QString *error)
{
    if (rebootNeeded) *rebootNeeded = false;
    if (payload.isEmpty() || payload.size() > 1024 * 1024) {
        setError(error, QStringLiteral("Empty or oversized ONVIF response."));
        return false;
    }

    QXmlStreamReader xml(payload);
    bool responseSeen = false;
    bool rebootSeen = false;
    bool reboot = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        const QString name = xml.name().toString();
        if (name == QStringLiteral("Fault")) {
            setError(error, QStringLiteral("Camera returned a SOAP fault."));
            return false;
        }
        if (name == QStringLiteral("SetNetworkInterfacesResponse"))
            responseSeen = true;
        else if (name == QStringLiteral("RebootNeeded")) {
            const QString value = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            if (value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 || value == QStringLiteral("1"))
                reboot = true;
            else if (value.compare(QStringLiteral("false"), Qt::CaseInsensitive) != 0 && value != QStringLiteral("0")) {
                setError(error, QStringLiteral("Invalid RebootNeeded value."));
                return false;
            }
            rebootSeen = true;
        }
    }
    if (xml.hasError() || !responseSeen || !rebootSeen) {
        setError(error, QStringLiteral("Malformed or incomplete SetNetworkInterfaces response."));
        return false;
    }
    if (rebootNeeded) *rebootNeeded = reboot;
    return true;
}

bool OnvifNetworkConfig::verifyInterfaceIpv4Address(const QByteArray &payload,
                                                     const QString &interfaceToken,
                                                     const QString &address,
                                                     QString *error)
{
    if (payload.isEmpty() || payload.size() > 1024 * 1024 ||
        interfaceToken.isEmpty() || address.isEmpty()) {
        setError(error, QStringLiteral("Invalid network-interface verification input."));
        return false;
    }
    QXmlStreamReader xml(payload);
    bool responseSeen = false;
    bool matched = false;
    QString currentToken;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            if (name == QStringLiteral("Fault")) {
                setError(error, QStringLiteral("Camera returned a SOAP fault."));
                return false;
            }
            if (name == QStringLiteral("GetNetworkInterfacesResponse"))
                responseSeen = true;
            else if (name == QStringLiteral("NetworkInterface") || name == QStringLiteral("NetworkInterfaces"))
                currentToken = xml.attributes().value(QStringLiteral("token")).toString();
            else if ((name == QStringLiteral("Address") || name == QStringLiteral("IPv4Address")) &&
                     currentToken == interfaceToken) {
                if (xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed() == address)
                    matched = true;
            }
        } else if (xml.isEndElement() && (xml.name() == QStringLiteral("NetworkInterface") || xml.name() == QStringLiteral("NetworkInterfaces"))) {
            currentToken.clear();
        }
    }
    if (xml.hasError() || !responseSeen) {
        setError(error, QStringLiteral("Malformed or incomplete GetNetworkInterfaces response."));
        return false;
    }
    if (!matched) {
        setError(error, QStringLiteral("Requested IPv4 address was not found on the selected interface."));
        return false;
    }
    return true;
}

bool OnvifNetworkConfig::parseSoapOperationResponse(const QByteArray &payload,
                                                           const QString &expectedResponse,
                                                           QString *error)
{
    if (payload.isEmpty() || payload.size() > 1024 * 1024 || expectedResponse.isEmpty()) {
        setError(error, QStringLiteral("Empty, oversized, or invalid ONVIF operation response."));
        return false;
    }
    QXmlStreamReader xml(payload);
    bool responseSeen = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        const QString name = xml.name().toString();
        if (name == QStringLiteral("Fault")) {
            setError(error, QStringLiteral("Camera returned a SOAP fault."));
            return false;
        }
        if (name == expectedResponse) responseSeen = true;
    }
    if (xml.hasError() || !responseSeen) {
        setError(error, QStringLiteral("Malformed or unexpected ONVIF operation response."));
        return false;
    }
    return true;
}

QByteArray OnvifNetworkConfig::buildSetNetworkDefaultGatewayRequest(const QString &gateway)
{
    QByteArray payload;
    QXmlStreamWriter writer(&payload);
    writer.writeStartDocument();
    writer.writeStartElement(QStringLiteral("s:Envelope"));
    writer.writeNamespace(QStringLiteral("http://www.w3.org/2003/05/soap-envelope"), QStringLiteral("s"));
    writer.writeNamespace(QStringLiteral("http://www.onvif.org/ver10/device/wsdl"), QStringLiteral("tds"));
    writer.writeStartElement(QStringLiteral("s:Body"));
    writer.writeStartElement(QStringLiteral("tds:SetNetworkDefaultGateway"));
    writer.writeTextElement(QStringLiteral("tds:IPv4Address"), gateway.trimmed());
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndDocument();
    return payload;
}

QByteArray OnvifNetworkConfig::buildSetSystemFactoryDefaultRequest(const QString &factoryDefaultType)
{
    const QString normalized = factoryDefaultType.trimmed();
    if (normalized != QStringLiteral("Soft") && normalized != QStringLiteral("Hard"))
        return {};

    QByteArray payload;
    QXmlStreamWriter writer(&payload);
    writer.writeStartDocument();
    writer.writeStartElement(QStringLiteral("s:Envelope"));
    writer.writeNamespace(QStringLiteral("http://www.w3.org/2003/05/soap-envelope"), QStringLiteral("s"));
    writer.writeNamespace(QStringLiteral("http://www.onvif.org/ver10/device/wsdl"), QStringLiteral("tds"));
    writer.writeStartElement(QStringLiteral("s:Body"));
    writer.writeStartElement(QStringLiteral("tds:SetSystemFactoryDefault"));
    writer.writeTextElement(QStringLiteral("tds:FactoryDefault"), normalized);
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndDocument();
    return payload;
}
