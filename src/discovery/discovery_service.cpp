#include "discovery_service.h"
#include "discovery_parser.h"

#include <QNetworkAddressEntry>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QNetworkRequest>
#include <QXmlStreamReader>
#include <QUrl>
#include <QUuid>
#include <algorithm>

namespace {
constexpr quint16 kProjectPort = 9988;
constexpr quint16 kOnvifPort = 3702;
const QHostAddress kOnvifGroup(QStringLiteral("239.255.255.250"));
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

bool DiscoveryService::isRunning() const { return running_; }

void DiscoveryService::refreshDeviceInfo(const DeviceRecord &device)
{
    const DeviceAdapter *adapter = adapterRegistry_.adapterFor(device);
    if (!adapter) {
        emit statusChanged(tr("No verified protocol adapter is available for this device. Refresh requires a valid ONVIF device-service endpoint."));
        return;
    }
    const QString key = !device.macAddress.isEmpty() ? device.macAddress : device.ipAddress;
    if (key.isEmpty()) {
        emit statusChanged(tr("Cannot refresh device information without a stable device address."));
        return;
    }
    devices_.insert(key, device);
    emit statusChanged(tr("Refreshing device information using %1…").arg(adapter->displayName()));
    requestOnvifDeviceInformation(device);
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
            flags.testFlag(QNetworkInterface::IsLoopBack)) continue;
        for (const auto &address : interface.addressEntries()) {
            if (address.ip().protocol() != QAbstractSocket::IPv4Protocol ||
                address.ip().isLoopback()) continue;
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
        const QNetworkDatagram datagram = socket->receiveDatagram();
        const auto it = std::find_if(sockets_.begin(), sockets_.end(),
                                     [socket](const InterfaceSocket &entry) { return entry.socket == socket; });
        if (it == sockets_.end()) continue;
        const QByteArray payload = datagram.data();
        if (payload.size() > 65507) continue;
        DeviceRecord device;
        const QHostAddress sender = datagram.senderAddress();
        if (payload.contains("CAM_DISCOVER")) {
            if (DiscoveryParser::parseProjectResponse(payload, it->requestSequence, sender,
                    it->interface.humanReadableName(), &device))
                mergeAndEmit(device);
        } else if (payload.contains("ProbeMatches") || payload.contains("ProbeMatch")) {
            if (DiscoveryParser::parseOnvifProbeMatch(payload, sender,
                    it->interface.humanReadableName(), &device))
                mergeAndEmit(device);
        }
    }
}

void DiscoveryService::mergeAndEmit(DeviceRecord device)
{
    const QString key = !device.macAddress.isEmpty() ? device.macAddress
        : (!device.ipAddress.isEmpty() ? device.ipAddress : device.onvifEndpoint);
    if (key.isEmpty()) return;
    if (devices_.contains(key)) {
        auto &existing = devices_[key];
        for (const auto &source : device.discoverySources)
            if (!existing.discoverySources.contains(source)) existing.discoverySources.append(source);
        if (existing.macAddress.isEmpty()) existing.macAddress = device.macAddress;
        if (existing.manufacturer.isEmpty() ||
            existing.manufacturer.startsWith(QStringLiteral("ONVIF")))
            existing.manufacturer = device.manufacturer;
        if (existing.model.isEmpty() || existing.model == tr("Model unknown"))
            existing.model = device.model;
        if (existing.firmwareVersion.isEmpty()) existing.firmwareVersion = device.firmwareVersion;
        if (existing.onvifEndpoint.isEmpty()) existing.onvifEndpoint = device.onvifEndpoint;
        const bool needsOnvifInfo = existing.deviceServiceUrl.isEmpty() &&
                                    !device.deviceServiceUrl.isEmpty();
        if (needsOnvifInfo) existing.deviceServiceUrl = device.deviceServiceUrl;
        emit deviceFound(existing);
        if (needsOnvifInfo) requestOnvifDeviceInformation(existing);
        return;
    }
    devices_.insert(key, device);
    emit deviceFound(device);
    if (!device.deviceServiceUrl.isEmpty()) requestOnvifDeviceInformation(device);
}



void DiscoveryService::requestOnvifDeviceInformation(const DeviceRecord &device)
{
    const QUrl url(device.deviceServiceUrl);
    if (!url.isValid() || (url.scheme() != QStringLiteral("http") &&
                           url.scheme() != QStringLiteral("https")) ||
        url.host() != device.ipAddress)
        return;

    const QByteArray soap = R"(<?xml version="1.0" encoding="UTF-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
 xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
 <s:Body><tds:GetDeviceInformation/></s:Body>
</s:Envelope>)";
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/soap+xml; charset=utf-8"));
    request.setTransferTimeout(3500);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply *reply = networkManager_.post(request, soap);
    const QString key = !device.macAddress.isEmpty() ? device.macAddress : device.ipAddress;
    connect(reply, &QNetworkReply::finished, this, [this, reply, key] {
        handleOnvifDeviceInformation(reply, key);
    });
}

void DiscoveryService::handleOnvifDeviceInformation(QNetworkReply *reply, const QString &deviceKey)
{
    const QByteArray payload = reply->readAll();
    const bool success = reply->error() == QNetworkReply::NoError &&
                         reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() >= 200 &&
                         reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() < 300;
    reply->deleteLater();
    if (!success || payload.isEmpty() || payload.size() > 1024 * 1024 || !devices_.contains(deviceKey))
        return;

    QXmlStreamReader xml(payload);
    QString manufacturer, model, firmware, serial, hardware;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        const QString name = xml.name().toString();
        if (name == QStringLiteral("Manufacturer")) manufacturer = xml.readElementText().trimmed();
        else if (name == QStringLiteral("Model")) model = xml.readElementText().trimmed();
        else if (name == QStringLiteral("FirmwareVersion")) firmware = xml.readElementText().trimmed();
        else if (name == QStringLiteral("SerialNumber")) serial = xml.readElementText().trimmed();
        else if (name == QStringLiteral("HardwareId")) hardware = xml.readElementText().trimmed();
    }
    if (xml.hasError()) return;

    DeviceRecord &device = devices_[deviceKey];
    if (!manufacturer.isEmpty()) device.manufacturer = manufacturer;
    if (!model.isEmpty()) device.model = model;
    if (!firmware.isEmpty()) device.firmwareVersion = firmware;
    if (!serial.isEmpty()) device.serialNumber = serial;
    if (!hardware.isEmpty()) device.hardwareId = hardware;
    if (!device.discoverySources.contains(QStringLiteral("ONVIF GetDeviceInformation")))
        device.discoverySources << QStringLiteral("ONVIF GetDeviceInformation");
    emit deviceFound(device);
    requestOnvifCapabilities(device);
    requestOnvifNetworkInterfaces(device);
}

void DiscoveryService::requestOnvifCapabilities(const DeviceRecord &device)
{
    const QUrl url(device.deviceServiceUrl);
    if (!url.isValid() || (url.scheme() != QStringLiteral("http") &&
                           url.scheme() != QStringLiteral("https")) ||
        url.host() != device.ipAddress)
        return;
    const QByteArray soap = R"(<?xml version="1.0" encoding="UTF-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
 xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
 <s:Body><tds:GetCapabilities><tds:Category>All</tds:Category></tds:GetCapabilities></s:Body>
</s:Envelope>)";
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/soap+xml; charset=utf-8"));
    request.setTransferTimeout(3500);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply *reply = networkManager_.post(request, soap);
    const QString key = !device.macAddress.isEmpty() ? device.macAddress : device.ipAddress;
    connect(reply, &QNetworkReply::finished, this, [this, reply, key] {
        handleOnvifCapabilities(reply, key);
    });
}

void DiscoveryService::handleOnvifCapabilities(QNetworkReply *reply, const QString &deviceKey)
{
    const QByteArray payload = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool success = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
    reply->deleteLater();
    if (!success || payload.isEmpty() || payload.size() > 1024 * 1024 || !devices_.contains(deviceKey))
        return;

    QXmlStreamReader xml(payload);
    QStringList capabilities;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        const QString name = xml.name().toString();
        if (name == QStringLiteral("Analytics") || name == QStringLiteral("Device") ||
            name == QStringLiteral("Events") || name == QStringLiteral("Imaging") ||
            name == QStringLiteral("Media") || name == QStringLiteral("PTZ") ||
            name == QStringLiteral("Extension"))
            if (!capabilities.contains(name)) capabilities.append(name);
    }
    if (xml.hasError()) return;
    DeviceRecord &device = devices_[deviceKey];
    device.capabilities = capabilities;
    if (!device.discoverySources.contains(QStringLiteral("ONVIF GetCapabilities")))
        device.discoverySources << QStringLiteral("ONVIF GetCapabilities");
    emit deviceFound(device);
}

void DiscoveryService::requestOnvifNetworkInterfaces(const DeviceRecord &device)
{
    const QUrl url(device.deviceServiceUrl);
    if (!url.isValid() || (url.scheme() != QStringLiteral("http") &&
                           url.scheme() != QStringLiteral("https")) ||
        url.host() != device.ipAddress)
        return;
    const QByteArray soap = R"(<?xml version="1.0" encoding="UTF-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
 xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
 <s:Body><tds:GetNetworkInterfaces/></s:Body>
</s:Envelope>)";
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/soap+xml; charset=utf-8"));
    request.setTransferTimeout(3500);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply *reply = networkManager_.post(request, soap);
    const QString key = !device.macAddress.isEmpty() ? device.macAddress : device.ipAddress;
    connect(reply, &QNetworkReply::finished, this, [this, reply, key] {
        handleOnvifNetworkInterfaces(reply, key);
    });
}

void DiscoveryService::handleOnvifNetworkInterfaces(QNetworkReply *reply, const QString &deviceKey)
{
    const QByteArray payload = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool success = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
    reply->deleteLater();
    if (!success || payload.isEmpty() || payload.size() > 1024 * 1024 || !devices_.contains(deviceKey))
        return;

    QXmlStreamReader xml(payload);
    QStringList interfaces;
    QString currentName;
    QString currentMac;
    QString currentAddress;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            if (name == QStringLiteral("NetworkInterface")) {
                currentName.clear();
                currentMac.clear();
                currentAddress.clear();
            } else if (name == QStringLiteral("Name")) {
                currentName = xml.readElementText().trimmed();
            } else if (name == QStringLiteral("HwAddress")) {
                currentMac = xml.readElementText().trimmed();
            } else if (name == QStringLiteral("Address")) {
                currentAddress = xml.readElementText().trimmed();
            }
        } else if (xml.isEndElement() && xml.name() == QStringLiteral("NetworkInterface")) {
            QStringList parts;
            if (!currentName.isEmpty()) parts << currentName;
            if (!currentAddress.isEmpty()) parts << currentAddress;
            if (!currentMac.isEmpty()) parts << currentMac;
            if (!parts.isEmpty()) interfaces << parts.join(QStringLiteral(" | "));
        }
    }
    if (xml.hasError()) return;
    DeviceRecord &device = devices_[deviceKey];
    device.networkSummary = interfaces.join(QStringLiteral("; "));
    if (!device.discoverySources.contains(QStringLiteral("ONVIF GetNetworkInterfaces")))
        device.discoverySources << QStringLiteral("ONVIF GetNetworkInterfaces");
    emit deviceFound(device);
}
