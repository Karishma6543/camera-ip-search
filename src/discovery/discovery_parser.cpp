#include "discovery_parser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QUrl>
#include <QtMath>
#include <QXmlStreamReader>

namespace {
constexpr int kMaximumXmlDepth = 64;
constexpr int kMaximumXmlTokens = 10000;
}

namespace {
QString normalizedMac(QString mac)
{
    mac = mac.trimmed().toUpper();
    mac.replace('-', ':');
    static const QRegularExpression pattern(
        QStringLiteral("^[0-9A-F]{2}(:[0-9A-F]{2}){5}$"));
    return pattern.match(mac).hasMatch() ? mac : QString();
}

QString usableIpv4(const QString &candidate, const QHostAddress &fallback)
{
    QHostAddress address;
    if (address.setAddress(candidate.trimmed()) &&
        address.protocol() == QAbstractSocket::IPv4Protocol &&
        !address.isNull() && !address.isLoopback() && !address.isMulticast() &&
        address.toIPv4Address() != 0U && address.toIPv4Address() != 0xffffffffU)
        return address.toString();
    return fallback.toString();
}

bool validPort(const QJsonValue &value)
{
    if (!value.isDouble()) return false;
    const double port = value.toDouble();
    return port >= 1.0 && port <= 65535.0 && qFloor(port) == port;
}
}

namespace DiscoveryParser {

bool parseProjectResponse(const QByteArray &payload, quint32 expectedSequence,
                          const QHostAddress &sender, const QString &interfaceName,
                          DeviceRecord *device)
{
    if (!device || payload.isEmpty() || payload.size() > 65507 ||
        sender.protocol() != QAbstractSocket::IPv4Protocol)
        return false;

    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return false;

    const QJsonObject root = document.object();
    const QJsonValue sequence = root.value(QStringLiteral("seq"));
    if (root.value(QStringLiteral("magic")).toString() != QStringLiteral("CAM_DISCOVER") ||
        root.value(QStringLiteral("version")).toInt(-1) != 1 ||
        root.value(QStringLiteral("cmd")).toString() != QStringLiteral("PROBE_RESPONSE") ||
        !sequence.isDouble() || sequence.toDouble() != static_cast<double>(expectedSequence))
        return false;

    const QJsonValue deviceValue = root.value(QStringLiteral("device"));
    if (!deviceValue.isObject()) return false;
    const QJsonObject fields = deviceValue.toObject();

    const QJsonValue macValue = fields.value(QStringLiteral("mac"));
    const QJsonValue ipValue = fields.value(QStringLiteral("ipv4"));
    const QJsonValue socValue = fields.value(QStringLiteral("soc"));
    const QJsonValue firmwareValue = fields.value(QStringLiteral("firmware_version"));
    const QJsonValue activatedValue = fields.value(QStringLiteral("activated"));
    const QJsonValue netmaskValue = fields.value(QStringLiteral("netmask"));
    const QJsonValue gatewayValue = fields.value(QStringLiteral("gateway"));
    const QJsonValue dnsValue = fields.value(QStringLiteral("dns"));

    if ((!macValue.isUndefined() && !macValue.isString()) ||
        (!ipValue.isUndefined() && !ipValue.isString()) ||
        (!socValue.isUndefined() && !socValue.isString()) ||
        (!firmwareValue.isUndefined() && !firmwareValue.isString()) ||
        (!activatedValue.isUndefined() && !activatedValue.isBool()) ||
        (!netmaskValue.isUndefined() && !netmaskValue.isString()) ||
        (!gatewayValue.isUndefined() && !gatewayValue.isString()) ||
        (!dnsValue.isUndefined() && !dnsValue.isString()))
        return false;

    if (!fields.value(QStringLiteral("http_port")).isUndefined() &&
        !validPort(fields.value(QStringLiteral("http_port")))) return false;
    if (!fields.value(QStringLiteral("rtsp_port")).isUndefined() &&
        !validPort(fields.value(QStringLiteral("rtsp_port")))) return false;
    if (!fields.value(QStringLiteral("dhcp")).isUndefined() &&
        !fields.value(QStringLiteral("dhcp")).isBool()) return false;

    DeviceRecord parsed;
    const QString advertisedIp = usableIpv4(ipValue.toString(), sender);
    QHostAddress resolvedAddress;
    if (!resolvedAddress.setAddress(advertisedIp) ||
        resolvedAddress.protocol() != QAbstractSocket::IPv4Protocol ||
        resolvedAddress.isLoopback() || resolvedAddress.isMulticast() ||
        resolvedAddress.toIPv4Address() == 0U ||
        resolvedAddress.toIPv4Address() == 0xffffffffU)
        return false;
    const quint32 senderIpv4 = sender.toIPv4Address();
    // The payload may supply the configured address when an unconfigured
    // camera replies from 0.0.0.0. Otherwise the UDP source is authoritative;
    // reject a payload that attempts to redirect the inventory to another host.
    if (senderIpv4 != 0U && advertisedIp != sender.toString())
        return false;
    parsed.ipAddress = senderIpv4 == 0U ? advertisedIp : sender.toString();
    parsed.macAddress = normalizedMac(macValue.toString());
    parsed.manufacturer = QStringLiteral("Oxita");
    parsed.model = socValue.toString().trimmed();
    parsed.firmwareVersion = firmwareValue.toString().trimmed();
    parsed.activationState = activatedValue.isBool()
        ? (activatedValue.toBool() ? QStringLiteral("Activated")
                                   : QStringLiteral("Not activated"))
        : QStringLiteral("Unknown");
    parsed.discoverySources << QStringLiteral("Project UDP 9988");
    parsed.interfaceName = interfaceName;
    parsed.id = parsed.macAddress.isEmpty() ? parsed.ipAddress : parsed.macAddress;
    *device = parsed;
    return true;
}

bool parseOnvifProbeMatch(const QByteArray &payload, const QHostAddress &sender,
                          const QString &interfaceName, DeviceRecord *device)
{
    if (!device || payload.isEmpty() || payload.size() > 65507 ||
        sender.protocol() != QAbstractSocket::IPv4Protocol)
        return false;

    QXmlStreamReader xml(payload);
    QString endpoint;
    QString xaddrs;
    QString scopes;
    int depth = 0;
    int tokenCount = 0;
    while (!xml.atEnd()) {
        xml.readNext();
        if (++tokenCount > kMaximumXmlTokens || xml.isDTD() || xml.isEntityReference())
            return false;

        if (xml.isStartElement()) {
            if (++depth > kMaximumXmlDepth) return false;
            const QString name = xml.name().toString();
            if (name == QStringLiteral("Address") && endpoint.isEmpty()) {
                endpoint = xml.readElementText().trimmed();
                --depth; // readElementText consumes the matching end element.
            } else if (name == QStringLiteral("XAddrs") && xaddrs.isEmpty()) {
                xaddrs = xml.readElementText().trimmed();
                --depth;
            } else if (name == QStringLiteral("Scopes") && scopes.isEmpty()) {
                scopes = xml.readElementText().trimmed();
                --depth;
            }
        } else if (xml.isEndElement()) {
            if (--depth < 0) return false;
        }
    }
    if (xml.hasError() || depth != 0 || (endpoint.isEmpty() && xaddrs.isEmpty()))
        return false;

    DeviceRecord parsed;
    parsed.ipAddress = sender.toString();
    parsed.onvifEndpoint = endpoint;
    const QStringList advertisedEndpoints = xaddrs.split(
        QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const QString &candidate : advertisedEndpoints) {
        const QUrl serviceUrl(candidate);
        if (serviceUrl.isValid() &&
            (serviceUrl.scheme() == QStringLiteral("http") ||
             serviceUrl.scheme() == QStringLiteral("https")) &&
            serviceUrl.host().compare(sender.toString(), Qt::CaseInsensitive) == 0 &&
            serviceUrl.userInfo().isEmpty() && serviceUrl.query().isEmpty() && serviceUrl.fragment().isEmpty()) {
            parsed.deviceServiceUrl = serviceUrl.toString();
            break;
        }
    }
    parsed.discoverySources << QStringLiteral("ONVIF WS-Discovery");
    parsed.interfaceName = interfaceName;
    parsed.manufacturer = QStringLiteral("ONVIF device (manufacturer unknown)");
    parsed.model = QStringLiteral("Model unknown");

    const QStringList scopeTokens = scopes.split(QRegularExpression(QStringLiteral("\\s+")),
                                                 Qt::SkipEmptyParts);
    for (const QString &scope : scopeTokens) {
        const QString lower = scope.toLower();
        const QString namePrefix = QStringLiteral("onvif://www.onvif.org/name/");
        const QString hardwarePrefix = QStringLiteral("onvif://www.onvif.org/hardware/");
        if (lower.startsWith(namePrefix))
            parsed.model = QUrl::fromPercentEncoding(scope.mid(namePrefix.size()).toUtf8());
        else if (lower.startsWith(QStringLiteral("onvif://www.onvif.org/manufacturer/")))
            parsed.manufacturer = QUrl::fromPercentEncoding(
                scope.mid(QStringLiteral("onvif://www.onvif.org/manufacturer/").size()).toUtf8());
        else if (lower.startsWith(hardwarePrefix) && parsed.model == QStringLiteral("Model unknown"))
            parsed.model = QUrl::fromPercentEncoding(scope.mid(hardwarePrefix.size()).toUtf8());
    }
    parsed.id = !endpoint.isEmpty() ? endpoint : parsed.ipAddress;
    *device = parsed;
    return true;
}

}
