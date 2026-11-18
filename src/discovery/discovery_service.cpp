#include "discovery_service.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAddressEntry>
#include <QNetworkDatagram>
#include <QUuid>
#include <algorithm>
#include <QXmlStreamReader>

namespace {
constexpr quint16 kProjectPort = 9988;
constexpr quint16 kOnvifPort = 3702;
const QHostAddress kOnvifGroup(QStringLiteral("239.255.255.250"));

QString firstNonEmpty(const QString &a, const QString &b)
{
    return a.trimmed().isEmpty() ? b : a.trimmed();
}
}

DiscoveryService::DiscoveryService(QObject *parent) : QObject(parent)
{
    timeout_.setSingleShot(true);
    timeout_.setInterval(5000);
    connect(&timeout_, &QTimer::timeout, this, [this] {
        if (!running_) return;
        running_ = false;
        emit statusChanged(tr("Discovery finished. Found %1 device(s).").arg(devices_.size()));
        emit finished(devices_.size());
        for (auto &entry : sockets_) {
            if (entry.socket) entry.socket->close();
        }
        sockets_.clear();
    });
}

bool DiscoveryService::isRunning() const
{
    return running_;
}

void DiscoveryService::start()
{
    if (running_) return;
    devices_.clear();
    sockets_.clear();
    running_ = true;
    emit statusChanged(tr("Checking active network interfaces…"));

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto &interface : interfaces) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning) ||
            flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        for (const auto &address : interface.addressEntries()) {
            if (address.ip().protocol() != QAbstractSocket::IPv4Protocol ||
                address.ip().isLoopback()) {
                continue;
            }
            addInterfaceSocket(interface, address.ip());
        }
    }

    if (sockets_.isEmpty()) {
        running_ = false;
        emit statusChanged(tr("No active IPv4 network interfaces were found."));
        emit finished(0);
        return;
    }

    emit statusChanged(tr("Sending ONVIF and project-camera discovery probes on %1 interface address(es)…")
                           .arg(sockets_.size()));
    for (auto &entry : sockets_) {
        sendOnvifProbe(entry);
        sendProjectProbe(entry);
    }
    timeout_.start();
}

void DiscoveryService::cancel()
{
    if (!running_) return;
    timeout_.stop();
    running_ = false;
    for (auto &entry : sockets_) {
        if (entry.socket) entry.socket->close();
    }
    sockets_.clear();
    emit statusChanged(tr("Discovery cancelled."));
    emit finished(devices_.size());
}

void DiscoveryService::addInterfaceSocket(const QNetworkInterface &interface,
                                          const QHostAddress &localAddress)
{
    auto *socket = new QUdpSocket(this);
    if (!socket->bind(localAddress, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit statusChanged(tr("Could not bind discovery socket on %1: %2")
                               .arg(interface.humanReadableName(), socket->errorString()));
        socket->deleteLater();
        return;
    }
    socket->setSocketOption(QAbstractSocket::MulticastTtlOption, 1);
    connect(socket, &QUdpSocket::readyRead, this, [this, socket] { processDatagram(socket); });
    sockets_.push_back({socket, interface, localAddress});
}

void DiscoveryService::sendOnvifProbe(InterfaceSocket &entry)
{
    if (!entry.socket) return;
    entry.socket->setMulticastInterface(entry.interface);
    entry.socket->joinMulticastGroup(kOnvifGroup, entry.interface);
    const QByteArray probe = R"(<?xml version="1.0" encoding="UTF-8"?>
<e:Envelope xmlns:e="http://www.w3.org/2003/05/soap-envelope"
 xmlns:w="http://schemas.xmlsoap.org/ws/2004/08/addressing"
 xmlns:d="http://schemas.xmlsoap.org/ws/2005/04/discovery"
 xmlns:dn="http://www.onvif.org/ver10/network/wsdl">
 <e:Header><w:MessageID>urn:uuid:)" +
        QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8() +
        R"(</w:MessageID><w:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</w:To>
 <w:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</w:Action></e:Header>
 <e:Body><d:Probe><d:Types>dn:NetworkVideoTransmitter</d:Types></d:Probe></e:Body>
</e:Envelope>)";
    entry.socket->writeDatagram(probe, kOnvifGroup, kOnvifPort);
}

void DiscoveryService::sendProjectProbe(InterfaceSocket &entry)
{
    if (!entry.socket) return;
    QJsonObject request;
    request.insert(QStringLiteral("magic"), QStringLiteral("CAM_DISCOVER"));
    request.insert(QStringLiteral("version"), 1);
    entry.requestSequence = ++sequence_;
    request.insert(QStringLiteral("seq"), static_cast<qint64>(entry.requestSequence));
    request.insert(QStringLiteral("cmd"), QStringLiteral("PROBE_REQUEST"));
    entry.socket->writeDatagram(QJsonDocument(request).toJson(QJsonDocument::Compact),
                                QHostAddress::Broadcast, kProjectPort);
}

void DiscoveryService::processDatagram(QUdpSocket *socket)
{
    while (socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = socket->receiveDatagram();
        const QByteArray payload = datagram.data();
        const auto it = std::find_if(sockets_.begin(), sockets_.end(),
                                     [socket](const InterfaceSocket &entry) {
                                         return entry.socket == socket;
                                     });
        if (it == sockets_.end()) continue;
        if (payload.size() > 65507) continue;
        if (payload.contains("CAM_DISCOVER")) {
            processProjectResponse(payload, datagram.senderAddress(), *it);
        } else if (payload.contains("ProbeMatches") || payload.contains("ProbeMatch")) {
            processOnvifResponse(payload, datagram.senderAddress(), *it);
        }
    }
}

void DiscoveryService::processProjectResponse(const QByteArray &payload,
                                              const QHostAddress &sender,
                                              const InterfaceSocket &entry)
{
    QJsonParseError error{};
    const auto document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return;
    const auto root = document.object();
    if (root.value(QStringLiteral("magic")).toString() != QStringLiteral("CAM_DISCOVER") ||
        root.value(QStringLiteral("version")).toInt(-1) != 1 ||
        root.value(QStringLiteral("cmd")).toString() != QStringLiteral("PROBE_RESPONSE") ||
        root.value(QStringLiteral("seq")).toVariant().toULongLong() != entry.requestSequence) return;
    const auto deviceObject = root.value(QStringLiteral("device")).toObject();
    if (deviceObject.isEmpty()) return;

    DeviceRecord device;
    device.ipAddress = firstNonEmpty(deviceObject.value(QStringLiteral("ipv4")).toString(),
                                     sender.toString());
    device.macAddress = normalizedMac(deviceObject.value(QStringLiteral("mac")).toString());
    device.manufacturer = QStringLiteral("Project camera");
    device.model = deviceObject.value(QStringLiteral("soc")).toString();
    device.firmwareVersion = deviceObject.value(QStringLiteral("firmware_version")).toString();
    device.activationState = deviceObject.contains(QStringLiteral("activated"))
        ? (deviceObject.value(QStringLiteral("activated")).toBool() ? tr("Activated") : tr("Not activated"))
        : tr("Unknown");
    device.discoverySources << QStringLiteral("Project UDP 9988");
    device.interfaceName = entry.interface.humanReadableName();
    device.id = device.macAddress.isEmpty() ? device.ipAddress : device.macAddress;
    mergeAndEmit(device);
}

void DiscoveryService::processOnvifResponse(const QByteArray &payload,
                                            const QHostAddress &sender,
                                            const InterfaceSocket &entry)
{
    QXmlStreamReader xml(payload);
    QString endpoint;
    QString xaddrs;
    QString scopes;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        const QString name = xml.name().toString();
        if (name == QStringLiteral("Address")) endpoint = xml.readElementText();
        else if (name == QStringLiteral("XAddrs")) xaddrs = xml.readElementText();
        else if (name == QStringLiteral("Scopes")) scopes = xml.readElementText();
    }
    if (xml.hasError() || (endpoint.isEmpty() && xaddrs.isEmpty())) return;

    DeviceRecord device;
    device.ipAddress = sender.toString();
    device.onvifEndpoint = endpoint;
    device.discoverySources << QStringLiteral("ONVIF WS-Discovery");
    device.interfaceName = entry.interface.humanReadableName();
    device.manufacturer = tr("ONVIF device (manufacturer unknown)");
    device.model = tr("Model unknown");
    const QString lowerScopes = scopes.toLower();
    if (lowerScopes.contains(QStringLiteral("onvif://www.onvif.org/name/"))) {
        const auto marker = QStringLiteral("onvif://www.onvif.org/name/");
        const int index = lowerScopes.indexOf(marker);
        device.model = scopes.mid(index + marker.size()).section(' ', 0, 0);
    }
    device.id = !endpoint.isEmpty() ? endpoint : device.ipAddress;
    mergeAndEmit(device);
}

void DiscoveryService::mergeAndEmit(DeviceRecord device)
{
    const QString key = !device.macAddress.isEmpty() ? device.macAddress
                                                      : (!device.ipAddress.isEmpty()
                                                             ? device.ipAddress
                                                             : device.onvifEndpoint);
    if (key.isEmpty()) return;
    if (devices_.contains(key)) {
        auto &existing = devices_[key];
        if (existing.discoverySources.contains(device.discoverySources.value(0))) return;
        existing.discoverySources.append(device.discoverySources);
        if (existing.macAddress.isEmpty()) existing.macAddress = device.macAddress;
        if (existing.manufacturer.isEmpty() || existing.manufacturer.startsWith(QStringLiteral("ONVIF")))
            existing.manufacturer = device.manufacturer;
        if (existing.model.isEmpty() || existing.model == tr("Model unknown"))
            existing.model = device.model;
        if (existing.firmwareVersion.isEmpty()) existing.firmwareVersion = device.firmwareVersion;
        if (existing.onvifEndpoint.isEmpty()) existing.onvifEndpoint = device.onvifEndpoint;
        emit deviceFound(existing);
        return;
    }
    devices_.insert(key, device);
    emit deviceFound(device);
}

QString DiscoveryService::normalizedMac(const QString &mac)
{
    QString value = mac.trimmed().toUpper();
    value.replace('-', ':');
    return value;
}
