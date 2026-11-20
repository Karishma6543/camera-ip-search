#include "discovery_service.h"
#include "onvif_network_config.h"
#include "discovery_parser.h"
#include "adapters/network_config_validator.h"

#include <QNetworkAddressEntry>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QNetworkRequest>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
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
    connect(&networkManager_, &QNetworkAccessManager::authenticationRequired,
            this, [this](QNetworkReply *, QAuthenticator *authenticator) {
        emit authenticationRequested(authenticator);
    });
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



void DiscoveryService::applyStaticIpv4(const DeviceRecord &device, const QString &address,
                                             const QString &subnetMask, const QString &gateway)
{
    QString validationError;
    if (!NetworkConfigValidator::validateStaticIpv4(address, subnetMask, gateway, QString(), &validationError)) {
        emit networkConfigurationFinished(device, false, validationError);
        return;
    }
    if (device.networkInterfaceToken.isEmpty() || device.deviceServiceUrl.isEmpty() ||
        device.ipAddress.isEmpty()) {
        emit networkConfigurationFinished(device, false,
            tr("The camera has no verified ONVIF interface token or device-service URL."));
        return;
    }

    const QUrl oldUrl(device.deviceServiceUrl);
    if (!oldUrl.isValid() || (oldUrl.scheme() != QStringLiteral("http") &&
        oldUrl.scheme() != QStringLiteral("https")) || oldUrl.host() != device.ipAddress) {
        emit networkConfigurationFinished(device, false,
            tr("The ONVIF service URL does not match the camera's current IP address."));
        return;
    }

    QHostAddress maskAddress;
    maskAddress.setAddress(subnetMask.trimmed());
    const quint32 mask = maskAddress.toIPv4Address();
    int prefix = 0;
    bool zeroSeen = false;
    for (int bit = 31; bit >= 0; --bit) {
        const bool set = (mask & (1U << bit)) != 0;
        if (set && zeroSeen) {
            emit networkConfigurationFinished(device, false, tr("Subnet mask is not contiguous."));
            return;
        }
        if (set) ++prefix; else zeroSeen = true;
    }

    QByteArray soap;
    QXmlStreamWriter writer(&soap);
    writer.writeStartDocument();
    writer.writeStartElement(QStringLiteral("s:Envelope"));
    writer.writeNamespace(QStringLiteral("http://www.w3.org/2003/05/soap-envelope"), QStringLiteral("s"));
    writer.writeNamespace(QStringLiteral("http://www.onvif.org/ver10/device/wsdl"), QStringLiteral("tds"));
    writer.writeNamespace(QStringLiteral("http://www.onvif.org/ver10/schema"), QStringLiteral("tt"));
    writer.writeStartElement(QStringLiteral("s:Body"));
    writer.writeStartElement(QStringLiteral("tds:SetNetworkInterfaces"));
    writer.writeStartElement(QStringLiteral("tds:InterfaceToken"));
    writer.writeCharacters(device.networkInterfaceToken);
    writer.writeEndElement();
    writer.writeStartElement(QStringLiteral("tds:NetworkInterface"));
    writer.writeStartElement(QStringLiteral("tt:IPv4"));
    writer.writeTextElement(QStringLiteral("tt:Enabled"), QStringLiteral("true"));
    writer.writeTextElement(QStringLiteral("tt:DHCP"), QStringLiteral("false"));
    writer.writeStartElement(QStringLiteral("tt:Manual"));
    writer.writeTextElement(QStringLiteral("tt:Address"), address.trimmed());
    writer.writeTextElement(QStringLiteral("tt:PrefixLength"), QString::number(prefix));
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndDocument();

    QNetworkRequest request(oldUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/soap+xml; charset=utf-8"));
    request.setTransferTimeout(5000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply *setReply = networkManager_.post(request, soap);
    const QString key = !device.macAddress.isEmpty() ? device.macAddress : device.ipAddress;
    connect(setReply, &QNetworkReply::finished, this, [this, setReply, device, oldUrl, address, subnetMask, gateway, key] {
        const QByteArray response = setReply->readAll();
        const int status = setReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        bool rebootNeeded = false;
        QString responseError;
        const bool transportOk = setReply->error() == QNetworkReply::NoError &&
            status >= 200 && status < 300;
        const bool accepted = transportOk &&
            OnvifNetworkConfig::parseSetNetworkInterfacesResponse(response, &rebootNeeded, &responseError);
        const QString errorText = setReply->errorString();
        setReply->deleteLater();
                return;
            }
        }
        setReply->deleteLater();
        if (!accepted) {
            emit networkConfigurationFinished(device, false,
                tr("Camera did not confirm the network change (%1).").arg(errorText));
            return;
        }

        if (rebootNeeded) {
            emit networkConfigurationFinished(device, false,
                tr("The camera reports that a reboot is required before the new IP becomes active. The app will not reboot it automatically; inventory remains unchanged."));
            return;
        }

        QUrl verifyUrl = oldUrl;
        verifyUrl.setHost(address.trimmed());
        QByteArray verifySoap = R"(<?xml version="1.0" encoding="UTF-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
 xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
 <s:Body><tds:GetNetworkInterfaces/></s:Body>
</s:Envelope>)";
        QNetworkRequest verifyRequest(verifyUrl);
        verifyRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                                QStringLiteral("application/soap+xml; charset=utf-8"));
        verifyRequest.setTransferTimeout(5000);
        verifyRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        QNetworkReply *verifyReply = networkManager_.post(verifyRequest, verifySoap);
        connect(verifyReply, &QNetworkReply::finished, this,
            [this, verifyReply, device, address, subnetMask, gateway, key, verifyUrl] {
                const QByteArray payload = verifyReply->readAll();
                const int verifyStatus = verifyReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const bool transportOk = verifyReply->error() == QNetworkReply::NoError &&
                                         verifyStatus >= 200 && verifyStatus < 300 &&
                                         !payload.isEmpty() && payload.size() <= 1024 * 1024;
                verifyReply->deleteLater();
                if (!transportOk) {
                    emit networkConfigurationFinished(device, false,
                        tr("The camera accepted the request, but verification at the new IP failed. Check the new address and reconnect manually if needed."));
                    return;
                }
                QString verificationError;
                if (!OnvifNetworkConfig::verifyInterfaceIpv4Address(
                        payload, device.networkInterfaceToken, address, &verificationError)) {
                    emit networkConfigurationFinished(device, false,
                        tr("The new IP is reachable, but the requested interface address could not be verified. Inventory was not updated."));
                    return;
                }
                const auto finishVerified = [this, device, address, subnetMask, gateway, key, verifyUrl] {
                    DeviceRecord updated = device;
                    updated.ipAddress = address;
                    updated.ipv4Address = address;
                    updated.ipv4SubnetMask = subnetMask;
                    if (!gateway.trimmed().isEmpty()) updated.ipv4Gateway = gateway.trimmed();
                    updated.ipv4DhcpEnabled = false;
                    updated.ipv4ConfigurationKnown = true;
                    updated.deviceServiceUrl = verifyUrl.toString();
                    if (devices_.contains(key)) devices_[key] = updated;
                    emit deviceFound(updated);
                    emit networkConfigurationFinished(updated, true,
                        tr("Static IPv4 address was verified at %1%2.")
                            .arg(address, gateway.trimmed().isEmpty()
                                ? QString() : tr("; gateway was also applied")));
                };
                if (gateway.trimmed().isEmpty()) {
                    finishVerified();
                    return;
                }
                const QByteArray gatewaySoap =
                    OnvifNetworkConfig::buildSetNetworkDefaultGatewayRequest(gateway.trimmed());
                QNetworkRequest gatewayRequest(verifyUrl);
                gatewayRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                                         QStringLiteral("application/soap+xml; charset=utf-8"));
                gatewayRequest.setTransferTimeout(5000);
                gatewayRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                            QNetworkRequest::ManualRedirectPolicy);
                QNetworkReply *gatewayReply = networkManager_.post(gatewayRequest, gatewaySoap);
                connect(gatewayReply, &QNetworkReply::finished, this,
                    [this, gatewayReply, device, address, gateway, finishVerified] {
                        const QByteArray gatewayResponse = gatewayReply->readAll();
                        const int gatewayStatus = gatewayReply->attribute(
                            QNetworkRequest::HttpStatusCodeAttribute).toInt();
                        const bool gatewayOk = gatewayReply->error() == QNetworkReply::NoError &&
                            gatewayStatus >= 200 && gatewayStatus < 300 &&
                            !gatewayResponse.isEmpty() && gatewayResponse.size() <= 1024 * 1024 &&
                            !gatewayResponse.contains("<Fault") && !gatewayResponse.contains(":Fault");
                        gatewayReply->deleteLater();
                        if (!gatewayOk) {
                            emit networkConfigurationFinished(device, false,
                                tr("The new IP was verified, but the gateway change was not confirmed. The camera may now have the new IP with its previous gateway."));
                            return;
                        }
                        finishVerified();
                    });
            });
    });
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
    requestOnvifDns(device);
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

    struct InterfaceData {
        QString token;
        QString name;
        QString mac;
        QString address;
        QString prefix;
        QString dhcp;
        QString gateway;
    };
    QVector<InterfaceData> parsed;
    QXmlStreamReader xml(payload);
    QStringList path;
    InterfaceData current;
    bool inInterface = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            path.append(name);
            if (name == QStringLiteral("NetworkInterface")) {
                inInterface = true;
                current = InterfaceData{};
                current.token = xml.attributes().value(QStringLiteral("token")).toString();
                continue;
            }
            if (!inInterface) continue;
            const QString joined = path.join(QLatin1Char('/'));
            const QString value = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            if (joined.endsWith(QStringLiteral("/NetworkInterface/Name"))) current.name = value;
            else if (joined.endsWith(QStringLiteral("/NetworkInterface/HwAddress"))) current.mac = value;
            else if (joined.endsWith(QStringLiteral("/IPv4/Config/DHCP"))) current.dhcp = value;
            else if (joined.endsWith(QStringLiteral("/IPv4/Config/Manual/IPv4Address")) ||
                     joined.endsWith(QStringLiteral("/IPv4/Config/FromDHCP/IPv4Address")) ||
                     joined.endsWith(QStringLiteral("/IPv4/Config/LinkLocal/IPv4Address"))) {
                if (current.address.isEmpty()) current.address = value;
            } else if (joined.endsWith(QStringLiteral("/IPv4/Config/Manual/PrefixLength")) ||
                       joined.endsWith(QStringLiteral("/IPv4/Config/FromDHCP/PrefixLength")) ||
                       joined.endsWith(QStringLiteral("/IPv4/Config/LinkLocal/PrefixLength"))) {
                if (current.prefix.isEmpty()) current.prefix = value;
            } else if (joined.endsWith(QStringLiteral("/IPv4/Config/Gateway/IPv4Address"))) {
                current.gateway = value;
            }
            path.removeLast();
        } else if (xml.isEndElement()) {
            if (xml.name() == QStringLiteral("NetworkInterface") && inInterface) {
                parsed.append(current);
                inInterface = false;
            }
            if (!path.isEmpty()) path.removeLast();
        }
    }
    if (xml.hasError()) return;

    QStringList summaries;
    const InterfaceData *selected = nullptr;
    for (const auto &entry : parsed) {
        QStringList fields;
        if (!entry.name.isEmpty()) fields << entry.name;
        if (!entry.address.isEmpty()) fields << entry.address;
        if (!entry.mac.isEmpty()) fields << entry.mac;
        if (!fields.isEmpty()) summaries << fields.join(QStringLiteral(" | "));
        if (!selected && !entry.token.isEmpty()) selected = &entry;
    }

    DeviceRecord &device = devices_[deviceKey];
    device.networkSummary = summaries.join(QStringLiteral("; "));
    if (selected) {
        device.networkInterfaceToken = selected->token;
        if (!selected->address.isEmpty()) device.ipv4Address = selected->address;
        bool prefixOk = false;
        const int prefix = selected->prefix.toInt(&prefixOk);
        if (prefixOk && prefix >= 1 && prefix <= 32) {
            const quint32 mask = 0xffffffffU << (32 - prefix);
            device.ipv4SubnetMask = QHostAddress(mask).toString();
        }
        if (selected->dhcp.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 ||
            selected->dhcp == QStringLiteral("1")) {
            device.ipv4DhcpEnabled = true;
            device.ipv4ConfigurationKnown = true;
        } else if (selected->dhcp.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0 ||
                   selected->dhcp == QStringLiteral("0")) {
            device.ipv4DhcpEnabled = false;
            device.ipv4ConfigurationKnown = !device.ipv4Address.isEmpty();
        }
        if (!selected->gateway.isEmpty()) device.ipv4Gateway = selected->gateway;
    }
    if (!device.discoverySources.contains(QStringLiteral("ONVIF GetNetworkInterfaces")))
        device.discoverySources << QStringLiteral("ONVIF GetNetworkInterfaces");
    emit deviceFound(device);
}

void DiscoveryService::requestOnvifDns(const DeviceRecord &device)
{
    const QUrl url(device.deviceServiceUrl);
    if (!url.isValid() || (url.scheme() != QStringLiteral("http") &&
                           url.scheme() != QStringLiteral("https")) ||
        url.host() != device.ipAddress)
        return;
    const QByteArray soap = R"(<?xml version="1.0" encoding="UTF-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
 xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
 <s:Body><tds:GetDNS/></s:Body>
</s:Envelope>)";
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/soap+xml; charset=utf-8"));
    request.setTransferTimeout(3500);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply *reply = networkManager_.post(request, soap);
    const QString key = !device.macAddress.isEmpty() ? device.macAddress : device.ipAddress;
    connect(reply, &QNetworkReply::finished, this, [this, reply, key] {
        handleOnvifDns(reply, key);
    });
}

void DiscoveryService::handleOnvifDns(QNetworkReply *reply, const QString &deviceKey)
{
    const QByteArray payload = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool success = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
    reply->deleteLater();
    if (!success || payload.isEmpty() || payload.size() > 1024 * 1024 || !devices_.contains(deviceKey))
        return;

    QXmlStreamReader xml(payload);
    QStringList path;
    QString manualDnsAddress;
    QString dhcpDnsAddress;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            path.append(xml.name().toString());
            const QString joined = path.join(QLatin1Char('/'));
            if (joined.endsWith(QStringLiteral("/DNSManual/IPv4Address"))) {
                manualDnsAddress = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                path.removeLast();
            } else if (joined.endsWith(QStringLiteral("/DNSFromDHCP/IPv4Address"))) {
                dhcpDnsAddress = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                path.removeLast();
            }
        } else if (xml.isEndElement() && !path.isEmpty()) {
            path.removeLast();
        }
    }
    if (xml.hasError()) return;
    const QString dnsAddress = !manualDnsAddress.isEmpty() ? manualDnsAddress : dhcpDnsAddress;
    if (!dnsAddress.isEmpty()) {
        DeviceRecord &device = devices_[deviceKey];
        device.ipv4DnsServer = dnsAddress;
        if (!device.discoverySources.contains(QStringLiteral("ONVIF GetDNS")))
            device.discoverySources << QStringLiteral("ONVIF GetDNS");
        emit deviceFound(device);
    }
}
