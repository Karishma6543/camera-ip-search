#include "onvif_dns_parser.h"
#include <QHostAddress>
#include <QXmlStreamReader>
namespace {
void setError(QString *error, const QString &message) { if (error) *error = message; }
}
bool OnvifDnsParser::parseIpv4DnsAddress(const QByteArray &payload, QString *address, QString *error)
{
    if (!address) { setError(error, QStringLiteral("Output DNS address is required.")); return false; }
    address->clear();
    if (payload.isEmpty() || payload.size() > 1024 * 1024) {
        setError(error, QStringLiteral("Empty or oversized ONVIF DNS response.")); return false;
    }
    QXmlStreamReader xml(payload);
    QString manual, dhcp;
    bool responseSeen = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        const QString name = xml.name().toString();
        if (name == QStringLiteral("Fault")) {
            setError(error, QStringLiteral("Camera returned a SOAP fault.")); return false;
        }
        if (name == QStringLiteral("GetDNSResponse")) responseSeen = true;
        else if (name == QStringLiteral("DNSManual") || name == QStringLiteral("DNSFromDHCP")) {
            const bool isManual = name == QStringLiteral("DNSManual");
            while (xml.readNextStartElement()) {
                if (xml.name() == QStringLiteral("IPv4Address")) {
                    const QString value = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                    QHostAddress parsed;
                    if (parsed.setAddress(value) && parsed.protocol() == QAbstractSocket::IPv4Protocol) {
                        if (isManual) manual = value;
                        else if (dhcp.isEmpty()) dhcp = value;
                    }
                } else {
                    xml.skipCurrentElement();
                }
            }
        }
    }
    if (xml.hasError() || !responseSeen) {
        setError(error, QStringLiteral("Malformed or unexpected GetDNS response.")); return false;
    }
    *address = !manual.isEmpty() ? manual : dhcp;
    return true;
}
