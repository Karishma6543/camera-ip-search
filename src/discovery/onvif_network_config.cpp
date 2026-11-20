#include "onvif_network_config.h"

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace {
void setError(QString *error, const QString &message)
{
    if (error) *error = message;
}
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
            else if (name == QStringLiteral("NetworkInterface"))
                currentToken = xml.attributes().value(QStringLiteral("token")).toString();
            else if ((name == QStringLiteral("Address") || name == QStringLiteral("IPv4Address")) &&
                     currentToken == interfaceToken) {
                if (xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed() == address)
                    matched = true;
            }
        } else if (xml.isEndElement() && xml.name() == QStringLiteral("NetworkInterface")) {
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
