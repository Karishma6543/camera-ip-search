#include "discovery_service.h"
#include "core/device_identity.h"
#include "onvif_network_config.h"
#include "onvif_maintenance_operations.h"
#include "onvif_dns_parser.h"
#include "onvif_service_parser.h"
#include "onvif_soap_fault_parser.h"
#include "onvif_wsse.h"
#include "discovery_parser.h"
#include "ssdp_parser.h"
#include "adapters/network_config_validator.h"

#include <QNetworkAddressEntry>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QNetworkRequest>
#include <QSslError>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDateTime>
#include <QUrl>
#include <QUuid>
#include <algorithm>

namespace {
constexpr quint16 kProjectPort = 9988;
constexpr quint16 kOnvifPort = 3702;
constexpr quint16 kSsdpPort = 1900;
constexpr qint64 kMaxOnvifResponseBytes = 1024 * 1024;
const QHostAddress kOnvifGroup(QStringLiteral("239.255.255.250"));

bool isWsseAuthenticationFailure(const QByteArray &payload, int httpStatus)
{
    if (httpStatus == 401 || httpStatus == 403)
        return true;
    QString message;
    QString code;
    if (!OnvifSoapFaultParser::parse(payload, &message, &code))
        return false;
    return code.contains(QStringLiteral("NotAuthorized"), Qt::CaseInsensitive) ||
           message.contains(QStringLiteral("auth"), Qt::CaseInsensitive) ||
           message.contains(QStringLiteral("credential"), Qt::CaseInsensitive) ||
           message.contains(QStringLiteral("permission"), Qt::CaseInsensitive) ||
           message.contains(QStringLiteral("not authorized"), Qt::CaseInsensitive);
}

bool isSafeDeviceServiceUrl(const QUrl &url, const QString &deviceIp)
{
    return url.isValid() &&
           (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https")) &&
           url.host() == deviceIp &&
           url.userInfo().isEmpty() &&
           url.query().isEmpty() &&
           url.fragment().isEmpty();
}
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
    const QString key = storageKeyForDevice(device);
    if (key.isEmpty()) {
        emit statusChanged(tr("Cannot refresh device information without a stable device address."));
        return;
    }
    devices_.insert(key, device);
    const QString credentialKey = QUrl(device.deviceServiceUrl).authority(QUrl::FullyEncoded).toLower();
    if (!wsseCredentials_.contains(credentialKey))
        wssePromptedHosts_.remove(credentialKey);
    emit statusChanged(tr("Refreshing device information using %1…").arg(adapter->displayName()));
    requestOnvifDeviceInformation(device);
}

void DiscoveryService::start(const QString &interfaceName)
{
    if (running_) return;
    cancelEnrichmentRequests();
    devices_.clear();
    sockets_.clear();
    running_ = true;
    emit statusChanged(tr("Checking active network interfaces…"));

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto &interface : interfaces) {
        if (!interfaceName.isEmpty() && interface.name() != interfaceName) continue;
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

    emit statusChanged(interfaceName.isEmpty()
        ? tr("Sending ONVIF and project-camera discovery probes (including SSDP/UPnP) on %1 interface address(es)…").arg(sockets_.size())
        : tr("Sending ONVIF and project-camera discovery probes (including SSDP/UPnP) on the selected interface (%1 address(es))…").arg(sockets_.size()));
    for (auto &entry : sockets_) {
        sendOnvifProbe(entry);
        sendProjectProbe(entry);
        sendSsdpProbe(entry);
    }
    timeout_.start();
}

void DiscoveryService::cancel()
{
    const bool wasRunning = running_;
    if (!wasRunning && enrichmentReplies_.isEmpty()) return;
    timeout_.stop();
    running_ = false;
    cancelEnrichmentRequests();
    for (auto &entry : sockets_) {
        if (entry.socket) entry.socket->close();
    }
    sockets_.clear();
    emit statusChanged(wasRunning
        ? tr("Discovery and device enrichment cancelled.")
        : tr("Device enrichment cancelled."));
    if (wasRunning)
        emit cancelled(devices_.size());
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

void DiscoveryService::sendSsdpProbe(InterfaceSocket &entry)
{
    if (!entry.socket) return;
    entry.socket->setMulticastInterface(entry.interface);
    const QByteArray probe = QByteArrayLiteral(
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 2\r\n"
        "ST: ssdp:all\r\n"
        "USER-AGENT: Oxita-Camera-Device-Manager/1.0 UPnP/1.1\r\n\r\n");
    entry.socket->writeDatagram(probe, kOnvifGroup, kSsdpPort);
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
        } else if (payload.startsWith("HTTP/1.1 200") ||
                   payload.startsWith("HTTP/1.0 200")) {
            if (SsdpParser::parseResponse(payload, sender,
                    it->interface.humanReadableName(), &device))
                mergeAndEmit(device);
        }
    }
}

void DiscoveryService::mergeAndEmit(DeviceRecord device)
{
    QString key = DeviceIdentity::findMatchingKey(devices_, device);
    if (!key.isEmpty()) {
        DeviceRecord merged = DeviceIdentity::mergeObservations(devices_.value(key), device);
        const bool needsOnvifInfo = merged.deviceServiceUrl.isEmpty() &&
                                    !device.deviceServiceUrl.isEmpty();
        if (needsOnvifInfo)
            merged.deviceServiceUrl = device.deviceServiceUrl;
        devices_.insert(key, merged);
        emit deviceFound(merged);
        if (needsOnvifInfo)
            requestOnvifDeviceInformation(merged);
        return;
    }

    key = !device.macAddress.isEmpty() ? device.macAddress
        : (!device.ipAddress.isEmpty() ? device.ipAddress : device.onvifEndpoint);
    if (key.isEmpty()) return;
    devices_.insert(key, device);
    emit deviceFound(device);
    if (!device.deviceServiceUrl.isEmpty())
        requestOnvifDeviceInformation(device);
}

QString DiscoveryService::storageKeyForDevice(const DeviceRecord &device) const
{
    const QString existingKey = DeviceIdentity::findMatchingKey(devices_, device);
    if (!existingKey.isEmpty())
        return existingKey;
    if (!device.macAddress.isEmpty()) return device.macAddress;
    if (!device.ipAddress.isEmpty()) return device.ipAddress;
    return device.onvifEndpoint;
}

void DiscoveryService::cancelEnrichmentRequests()
{
    ++enrichmentGeneration_;
    const auto pending = enrichmentReplies_;
    enrichmentReplies_.clear();
    for (const auto &reply : pending) {
        if (reply)
            reply->abort();
    }
}

void DiscoveryService::guardOnvifReply(QNetworkReply *reply)
{
    if (!reply) return;
    // The reply is not consumed until finished(), so bytesAvailable() is the
    // accumulated buffered response size. Abort as soon as it exceeds the cap.
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        if (reply->bytesAvailable() > kMaxOnvifResponseBytes) {
            emit statusChanged(tr("Rejected ONVIF response larger than the 1 MiB safety limit."));
            reply->abort();
        }
    });
    // Qt validates TLS certificates by default. Never override/ignore certificate
    // errors: abort explicitly and surface a safe diagnostic for the operator.
    connect(reply, &QNetworkReply::sslErrors, this,
            [this, reply](const QList<QSslError> &errors) {
        emit statusChanged(tr("TLS certificate validation failed for %1 (%2 error(s)); the ONVIF request was stopped.")
                               .arg(reply->url().host()).arg(errors.size()));
        reply->abort();
    });
}

QNetworkReply *DiscoveryService::postOnvifRequest(const QNetworkRequest &request,
                                                   const QByteArray &soap)
{
    const QUrl url = request.url();
    const QString credentialKey = url.authority(QUrl::FullyEncoded).toLower();
    QByteArray payload = soap;
    const auto credentials = wsseCredentials_.constFind(credentialKey);
    if (credentials != wsseCredentials_.cend()) {
        const QByteArray nonce = QUuid::createUuid().toRfc4122();
        const QString created = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        payload = OnvifWsse::addUsernameToken(soap, credentials->first,
                                               credentials->second, nonce, created);
        if (payload.isEmpty()) {
            emit statusChanged(tr("Could not construct a WS-Security UsernameToken for the ONVIF request."));
            return nullptr;
        }
    }
    return networkManager_.post(request, payload);
}

bool DiscoveryService::requestWsseCredentials(const DeviceRecord &device, const QString &operation)
{
    const QUrl url(device.deviceServiceUrl);
    const QString credentialKey = url.authority(QUrl::FullyEncoded).toLower();
    const QString attemptKey = credentialKey + QLatin1Char('|') + operation;
    if (!url.isValid() || credentialKey.isEmpty() ||
        wsseRetriedOperations_.contains(attemptKey))
        return false;
    if (wsseCredentials_.contains(credentialKey)) {
        wsseRetriedOperations_.insert(attemptKey);
        return true;
    }
    if (wssePromptedHosts_.contains(credentialKey))
        return false;
    wssePromptedHosts_.insert(credentialKey);

    QAuthenticator authenticator;
    emit statusChanged(tr("The camera requires ONVIF WS-Security credentials for %1.")
                           .arg(url.host()));
    emit authenticationRequested(&authenticator);
    if (authenticator.user().isEmpty()) {
        emit statusChanged(tr("WS-Security authentication was cancelled; no credentials were saved."));
        return false;
    }
    wsseCredentials_.insert(credentialKey,
                            qMakePair(authenticator.user(), authenticator.password()));
    wsseRetriedOperations_.insert(attemptKey);
    emit statusChanged(tr("WS-Security credentials are held in memory for this application session only."));
    return true;
}

void DiscoveryService::trackEnrichmentReply(QNetworkReply *reply)
{
    if (!reply) return;
    enrichmentReplies_.append(QPointer<QNetworkReply>(reply));
    guardOnvifReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        for (int index = enrichmentReplies_.size() - 1; index >= 0; --index) {
            if (enrichmentReplies_.at(index).isNull() ||
                enrichmentReplies_.at(index).data() == reply)
                enrichmentReplies_.removeAt(index);
        }
    });
}


void DiscoveryService::factoryReset(const DeviceRecord &device, const QString &factoryDefaultType)
{
    const QString mode = factoryDefaultType.trimmed();
    if (mode != QStringLiteral("Soft") && mode != QStringLiteral("Hard")) {
        emit maintenanceOperationFinished(device, QStringLiteral("Factory reset"), false,
            tr("Choose a supported ONVIF factory-default mode."));
        return;
    }
    const DeviceAdapter *adapter = adapterRegistry_.adapterFor(device);
    if (!adapter || !adapter->supports(DeviceOperation::FactoryReset)) {
        emit maintenanceOperationFinished(device, QStringLiteral("Factory reset"), false,
            tr("This camera has no verified ONVIF device-service adapter. Factory reset is unavailable."));
        return;
    }
    const QUrl url(device.deviceServiceUrl);
    if (!isSafeDeviceServiceUrl(url, device.ipAddress)) {
        emit maintenanceOperationFinished(device, QStringLiteral("Factory reset"), false,
            tr("The camera device-service URL does not match its current IP address."));
        return;
    }

    const QByteArray soap = OnvifNetworkConfig::buildSetSystemFactoryDefaultRequest(mode);
    if (soap.isEmpty()) {
        emit maintenanceOperationFinished(device, QStringLiteral("Factory reset"), false,
            tr("Could not construct a valid ONVIF factory-default request."));
        return;
    }
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/soap+xml; charset=utf-8"));
    request.setTransferTimeout(8000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply *reply = postOnvifRequest(request, soap);
    guardOnvifReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, device, mode] {
        const QByteArray payload = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool transportOk = reply->error() == QNetworkReply::NoError &&
                                 status >= 200 && status < 300;
        if (isWsseAuthenticationFailure(payload, status) &&
            requestWsseCredentials(device, QStringLiteral("SetSystemFactoryDefault"))) {
            reply->deleteLater();
            factoryReset(device, mode);
            return;
        }
        QString parseError;
        const bool accepted = transportOk && OnvifNetworkConfig::parseSoapOperationResponse(
            payload, QStringLiteral("SetSystemFactoryDefaultResponse"), &parseError);
        const QString error = accepted ? QString() :
            (parseError.isEmpty() ? reply->errorString() : parseError);
        reply->deleteLater();
        emit maintenanceOperationFinished(device, QStringLiteral("Factory reset"), accepted,
            accepted
                ? tr("The camera accepted the ONVIF %1 factory-default request. It may reboot, restore defaults and change its network settings. Rediscover it before further configuration.").arg(mode)
                : tr("The camera did not confirm the ONVIF factory-default request: %1").arg(error));
    });
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

    const QStringList writeOperations = adapterRegistry_.implementedWriteOperations(device);
    if (!writeOperations.contains(QStringLiteral("SetNetworkInterfaces")) ||
        (!gateway.trimmed().isEmpty() &&
         !writeOperations.contains(QStringLiteral("SetNetworkDefaultGateway")))) {
        emit networkConfigurationFinished(device, false,
            tr("The selected device adapter does not implement the requested ONVIF network operation."));
        return;
    }

    const QUrl oldUrl(device.deviceServiceUrl);
    if (!isSafeDeviceServiceUrl(oldUrl, device.ipAddress)) {
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
    const QString prefixText = QString::number(prefix);

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
    QNetworkReply *setReply = postOnvifRequest(request, soap);
    guardOnvifReply(setReply);
    const QString key = storageKeyForDevice(device);
    connect(setReply, &QNetworkReply::finished, this, [this, setReply, device, oldUrl, address, subnetMask, gateway, key] {
        const QByteArray response = setReply->readAll();
        const int status = setReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (isWsseAuthenticationFailure(response, status) &&
            requestWsseCredentials(device, QStringLiteral("SetNetworkInterfaces.StaticIPv4"))) {
            setReply->deleteLater();
            applyStaticIpv4(device, address, subnetMask, gateway);
            return;
        }
        bool rebootNeeded = false;
        QString responseError;
        const bool transportOk = setReply->error() == QNetworkReply::NoError &&
            status >= 200 && status < 300;
        const bool accepted = transportOk &&
            OnvifNetworkConfig::parseSetNetworkInterfacesResponse(response, &rebootNeeded, &responseError);
        const QString errorText = accepted ? QString() : (responseError.isEmpty()
            ? setReply->errorString() : responseError);
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

        // A static address change moves the ONVIF endpoint to a new authority.
        // Carry the already supplied session-only credentials to that endpoint
        // so the mandatory read-back can authenticate without prompting again.
        const QString oldCredentialKey = oldUrl.authority(QUrl::FullyEncoded).toLower();
        const QString newCredentialKey = verifyUrl.authority(QUrl::FullyEncoded).toLower();
        const auto oldCredentials = wsseCredentials_.constFind(oldCredentialKey);
        if (oldCredentials != wsseCredentials_.cend())
            wsseCredentials_.insert(newCredentialKey, oldCredentials.value());

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
        QNetworkReply *verifyReply = postOnvifRequest(verifyRequest, verifySoap);
                guardOnvifReply(verifyReply);
        connect(verifyReply, &QNetworkReply::finished, this,
            [this, verifyReply, device, address, subnetMask, gateway, key, verifyUrl, prefixText] {
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
                if (!OnvifNetworkConfig::verifyInterfaceIpv4Configuration(
                        payload, device.networkInterfaceToken, address, prefixText, QString(),
                        &verificationError)) {
                    emit networkConfigurationFinished(device, false,
                        tr("The camera is reachable at the new IP, but the requested static address, subnet prefix or DHCP-disabled state could not be verified. Inventory was not updated."));
                    return;
                }
                const auto finishVerified = [this, device, address, subnetMask, gateway, key, verifyUrl]
                    (bool gatewayApplied) {
                    DeviceRecord updated = device;
                    updated.ipAddress = address;
                    updated.ipv4Address = address;
                    updated.ipv4SubnetMask = subnetMask;
                    if (!gateway.trimmed().isEmpty() && gatewayApplied)
                        updated.ipv4Gateway = gateway.trimmed();
                    updated.ipv4DhcpEnabled = false;
                    updated.ipv4ConfigurationKnown = true;
                    updated.deviceServiceUrl = verifyUrl.toString();
                    if (devices_.contains(key)) devices_[key] = updated;
                    // The address change is independently verified. Persist it
                    // even when the subsequent gateway operation fails, so the
                    // inventory never continues pointing at the camera's old IP.
                    emit deviceFound(updated);
                    const bool complete = gateway.trimmed().isEmpty() || gatewayApplied;
                    const QString message = !complete
                        ? tr("The new IP address was verified, but the gateway setting could not be confirmed by read-back. Inventory records the new IP and retains the previous gateway; review the gateway setting and reconnect if needed.")
                        : tr("Static IPv4 address was verified at %1%2.")
                            .arg(address, gateway.trimmed().isEmpty()
                                ? QString() : tr("; gateway was also applied"));
                    emit networkConfigurationFinished(updated, complete, message);
                };
                if (gateway.trimmed().isEmpty()) {
                    finishVerified(true);
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
                QNetworkReply *gatewayReply = postOnvifRequest(gatewayRequest, gatewaySoap);
                guardOnvifReply(gatewayReply);
                connect(gatewayReply, &QNetworkReply::finished, this,
                    [this, gatewayReply, device, address, gateway, finishVerified] {
                        const QByteArray gatewayResponse = gatewayReply->readAll();
                        const int gatewayStatus = gatewayReply->attribute(
                            QNetworkRequest::HttpStatusCodeAttribute).toInt();
                        const bool gatewayTransportOk = gatewayReply->error() == QNetworkReply::NoError &&
                            gatewayStatus >= 200 && gatewayStatus < 300;
                        const bool gatewayOk = gatewayTransportOk &&
                            OnvifNetworkConfig::parseSoapOperationResponse(
                                gatewayResponse, QStringLiteral("SetNetworkDefaultGatewayResponse"));
                        gatewayReply->deleteLater();
                        if (!gatewayOk) {
                            finishVerified(false);
                            return;
                        }

                        // A successful SetNetworkDefaultGateway response confirms
                        // acceptance only. Read the interface back before reporting
                        // the gateway as verified.
                        const QByteArray readBackSoap = R"(<?xml version="1.0" encoding="UTF-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
 xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
 <s:Body><tds:GetNetworkInterfaces/></s:Body>
</s:Envelope>)";
                        QNetworkRequest readBackRequest(verifyUrl);
                        readBackRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                                                  QStringLiteral("application/soap+xml; charset=utf-8"));
                        readBackRequest.setTransferTimeout(5000);
                        readBackRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                                     QNetworkRequest::ManualRedirectPolicy);
                        QNetworkReply *readBackReply = postOnvifRequest(readBackRequest, readBackSoap);
                        guardOnvifReply(readBackReply);
                        connect(readBackReply, &QNetworkReply::finished, this,
                            [this, readBackReply, device, address, prefixText, gateway, finishVerified] {
                                const QByteArray readBackPayload = readBackReply->readAll();
                                const int readBackStatus = readBackReply->attribute(
                                    QNetworkRequest::HttpStatusCodeAttribute).toInt();
                                const bool readBackTransportOk =
                                    readBackReply->error() == QNetworkReply::NoError &&
                                    readBackStatus >= 200 && readBackStatus < 300;
                                readBackReply->deleteLater();
                                const bool readBackVerified = readBackTransportOk &&
                                    OnvifNetworkConfig::verifyInterfaceIpv4Configuration(
                                        readBackPayload, device.networkInterfaceToken, address,
                                        prefixText, gateway);
                                finishVerified(readBackVerified);
                            });
                    });
            });
    });
}


void DiscoveryService::configureDhcp(const DeviceRecord &device, bool enabled)
{
    const DeviceAdapter *adapter = adapterRegistry_.adapterFor(device);
    if (!adapter || !adapter->supports(DeviceOperation::ConfigureDhcp) ||
        device.networkInterfaceToken.isEmpty() || device.deviceServiceUrl.isEmpty() ||
        !isSafeDeviceServiceUrl(QUrl(device.deviceServiceUrl), device.ipAddress)) {
        emit maintenanceOperationFinished(device, QStringLiteral("DHCP configuration"), false,
            tr("This camera has no verified ONVIF network interface or DHCP write capability."));
        return;
    }
    const QByteArray soap = OnvifMaintenanceOperations::buildSetDhcpRequest(
        device.networkInterfaceToken, enabled);
    if (soap.isEmpty()) {
        emit maintenanceOperationFinished(device, QStringLiteral("DHCP configuration"), false,
            tr("Could not build a valid DHCP configuration request."));
        return;
    }
    QNetworkRequest request(QUrl(device.deviceServiceUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/soap+xml; charset=utf-8"));
    request.setTransferTimeout(8000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply *reply = postOnvifRequest(request, soap);
    guardOnvifReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, device, enabled] {
        const QByteArray response = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (isWsseAuthenticationFailure(response, status) &&
            requestWsseCredentials(device, QStringLiteral("SetNetworkInterfaces.DHCP"))) {
            reply->deleteLater();
            configureDhcp(device, enabled);
            return;
        }
        QString error;
        const bool accepted = reply->error() == QNetworkReply::NoError && status >= 200 &&
            status < 300 && OnvifNetworkConfig::parseSoapOperationResponse(
                response, QStringLiteral("SetNetworkInterfacesResponse"), &error);
        const QString transportError = reply->errorString();
        reply->deleteLater();
        if (!accepted) {
            emit maintenanceOperationFinished(device, QStringLiteral("DHCP configuration"), false,
                tr("Camera did not confirm the DHCP change: %1").arg(error.isEmpty() ? transportError : error));
            return;
        }
        // Read the interface back before reporting success. If DHCP changes the
        // address, the old endpoint may disappear; that is reported as pending
        // rediscovery, never as a verified success.
        const QByteArray verifySoap = R"(<?xml version="1.0" encoding="UTF-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
 xmlns:tds="http://www.onvif.org/ver10/device/wsdl"><s:Body><tds:GetNetworkInterfaces/></s:Body></s:Envelope>)";
        QNetworkRequest verifyRequest(QUrl(device.deviceServiceUrl));
        verifyRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/soap+xml; charset=utf-8"));
        verifyRequest.setTransferTimeout(5000);
        verifyRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        QNetworkReply *verify = postOnvifRequest(verifyRequest, verifySoap);
        guardOnvifReply(verify);
        connect(verify, &QNetworkReply::finished, this, [this, verify, device, enabled] {
            const QByteArray payload = verify->readAll();
            const int code = verify->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const bool transportOk = verify->error() == QNetworkReply::NoError && code >= 200 && code < 300;
            verify->deleteLater();
            OnvifNetworkConfig::InterfaceIpv4Info info;
            QStringList summaries;
            QString parseError;
            const bool parsed = transportOk && OnvifNetworkConfig::parseNetworkInterfaces(
                payload, device.ipAddress, &info, &summaries, &parseError);
            const bool verified = parsed && info.token == device.networkInterfaceToken &&
                (info.dhcp.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) == enabled;
            if (!verified) {
                emit maintenanceOperationFinished(device, QStringLiteral("DHCP configuration"), false,
                    tr("The camera accepted the DHCP request, but the setting could not be read back and verified. Rediscover the camera if its IP address changed."));
                return;
            }
            DeviceRecord updated = device;
            updated.ipv4DhcpEnabled = enabled;
            updated.ipv4ConfigurationKnown = true;
            if (devices_.contains(storageKeyForDevice(device)))
                devices_[storageKeyForDevice(device)] = updated;
            emit deviceFound(updated);
            emit maintenanceOperationFinished(updated, QStringLiteral("DHCP configuration"), true,
                enabled ? tr("DHCP is enabled and verified.") : tr("DHCP is disabled and verified."));
        });
    });
}

void DiscoveryService::configureDns(const DeviceRecord &device, bool fromDhcp,
                                    const QStringList &ipv4Servers)
{
    const DeviceAdapter *adapter = adapterRegistry_.adapterFor(device);
    if (!adapter || !adapter->supports(DeviceOperation::ConfigureDns) ||
        device.deviceServiceUrl.isEmpty() ||
        !isSafeDeviceServiceUrl(QUrl(device.deviceServiceUrl), device.ipAddress)) {
        emit maintenanceOperationFinished(device, QStringLiteral("DNS configuration"), false,
            tr("This camera has no verified ONVIF DNS write capability."));
        return;
    }
    const QByteArray soap = OnvifMaintenanceOperations::buildSetDnsRequest(fromDhcp, ipv4Servers);
    if (soap.isEmpty()) {
        emit maintenanceOperationFinished(device, QStringLiteral("DNS configuration"), false,
            tr("Enter one or two valid IPv4 DNS servers, or choose automatic DNS."));
        return;
    }
    QNetworkRequest request(QUrl(device.deviceServiceUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/soap+xml; charset=utf-8"));
    request.setTransferTimeout(8000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply *reply = postOnvifRequest(request, soap);
    guardOnvifReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, device, fromDhcp, ipv4Servers] {
        const QByteArray response = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (isWsseAuthenticationFailure(response, status) &&
            requestWsseCredentials(device, QStringLiteral("SetDNS"))) {
            reply->deleteLater();
            configureDns(device, fromDhcp, ipv4Servers);
            return;
        }
        QString error;
        const bool accepted = reply->error() == QNetworkReply::NoError && status >= 200 &&
            status < 300 && OnvifNetworkConfig::parseSoapOperationResponse(
                response, QStringLiteral("SetDNSResponse"), &error);
        const QString transportError = reply->errorString();
        reply->deleteLater();
        if (!accepted) {
            emit maintenanceOperationFinished(device, QStringLiteral("DNS configuration"), false,
                tr("Camera did not confirm the DNS change: %1").arg(error.isEmpty() ? transportError : error));
            return;
        }
        const QByteArray verifySoap = R"(<?xml version="1.0" encoding="UTF-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
 xmlns:tds="http://www.onvif.org/ver10/device/wsdl"><s:Body><tds:GetDNS/></s:Body></s:Envelope>)";
        QNetworkRequest verifyRequest(QUrl(device.deviceServiceUrl));
        verifyRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/soap+xml; charset=utf-8"));
        verifyRequest.setTransferTimeout(5000);
        verifyRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        QNetworkReply *verify = postOnvifRequest(verifyRequest, verifySoap);
        guardOnvifReply(verify);
        connect(verify, &QNetworkReply::finished, this, [this, verify, device, fromDhcp, ipv4Servers] {
            const QByteArray payload = verify->readAll();
            const int code = verify->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const bool transportOk = verify->error() == QNetworkReply::NoError && code >= 200 && code < 300;
            verify->deleteLater();
            QString actual;
            QString parseError;
            const bool parsed = transportOk && OnvifDnsParser::parseIpv4DnsAddress(payload, &actual, &parseError);
            const bool verified = parsed && (fromDhcp ||
                (!ipv4Servers.isEmpty() && actual.compare(ipv4Servers.first(), Qt::CaseInsensitive) == 0));
            if (!verified) {
                emit maintenanceOperationFinished(device, QStringLiteral("DNS configuration"), false,
                    tr("The camera accepted the DNS request, but the requested primary DNS server could not be verified."));
                return;
            }
            DeviceRecord updated = device;
            updated.ipv4DnsServer = actual;
            emit deviceFound(updated);
            emit maintenanceOperationFinished(updated, QStringLiteral("DNS configuration"), true,
                fromDhcp ? tr("Automatic DNS was enabled; the camera returned %1.").arg(actual)
                         : tr("Primary DNS server %1 was verified.").arg(actual));
        });
    });
}

void DiscoveryService::rebootDevice(const DeviceRecord &device)
{
    const DeviceAdapter *adapter = adapterRegistry_.adapterFor(device);
    if (!adapter || !adapter->supports(DeviceOperation::Reboot) ||
        device.deviceServiceUrl.isEmpty() ||
        !isSafeDeviceServiceUrl(QUrl(device.deviceServiceUrl), device.ipAddress)) {
        emit maintenanceOperationFinished(device, QStringLiteral("Reboot"), false,
            tr("This camera has no verified ONVIF reboot capability."));
        return;
    }
    QNetworkRequest request(QUrl(device.deviceServiceUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/soap+xml; charset=utf-8"));
    request.setTransferTimeout(5000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply *reply = postOnvifRequest(request, OnvifMaintenanceOperations::buildSystemRebootRequest());
    guardOnvifReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, device] {
        const QByteArray response = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (isWsseAuthenticationFailure(response, status) &&
            requestWsseCredentials(device, QStringLiteral("SystemReboot"))) {
            reply->deleteLater();
            rebootDevice(device);
            return;
        }
        QString error;
        const bool accepted = reply->error() == QNetworkReply::NoError && status >= 200 &&
            status < 300 && OnvifNetworkConfig::parseSoapOperationResponse(
                response, QStringLiteral("SystemRebootResponse"), &error);
        const QString transportError = reply->errorString();
        reply->deleteLater();
        emit maintenanceOperationFinished(device, QStringLiteral("Reboot"), accepted,
            accepted ? tr("The camera accepted the reboot request. It will temporarily disconnect; run discovery again after it restarts.")
                     : tr("The camera did not confirm the reboot request: %1").arg(error.isEmpty() ? transportError : error));
    });
}

void DiscoveryService::requestOnvifDeviceInformation(const DeviceRecord &device)
{
    const QUrl url(device.deviceServiceUrl);
    if (!isSafeDeviceServiceUrl(url, device.ipAddress))
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
    QNetworkReply *reply = postOnvifRequest(request, soap);
    trackEnrichmentReply(reply);
    const quint64 generation = enrichmentGeneration_;
    const QString key = storageKeyForDevice(device);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, generation] {
        if (generation != enrichmentGeneration_) {
            reply->deleteLater();
            return;
        }
        handleOnvifDeviceInformation(reply, key);
    });
}

void DiscoveryService::handleOnvifDeviceInformation(QNetworkReply *reply, const QString &deviceKey)
{
    const QByteArray payload = reply->readAll();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool success = reply->error() == QNetworkReply::NoError &&
                         httpStatus >= 200 && httpStatus < 300;
    const QString transportError = reply->errorString();
    reply->deleteLater();
    if (payload.size() > kMaxOnvifResponseBytes || !devices_.contains(deviceKey))
        return;

    QString faultMessage;
    QString faultCode;
    const bool soapFault = OnvifSoapFaultParser::parse(payload, &faultMessage, &faultCode);
    const bool authRejected = isWsseAuthenticationFailure(payload, httpStatus);
    if (!success || soapFault) {
        const DeviceRecord device = devices_.value(deviceKey);
        if (authRejected && requestWsseCredentials(device, QStringLiteral("GetDeviceInformation"))) {
            requestOnvifDeviceInformation(device);
            return;
        }
        if (authRejected)
            emit statusChanged(tr("ONVIF authentication was not accepted for %1. Check the account and permissions.")
                                   .arg(device.ipAddress));
        else if (soapFault)
            emit statusChanged(tr("ONVIF GetDeviceInformation fault: %1").arg(faultMessage));
        else
            emit statusChanged(tr("ONVIF GetDeviceInformation failed (HTTP %1): %2")
                                   .arg(httpStatus).arg(transportError));
        return;
    }
    if (payload.isEmpty())
        return;

    QXmlStreamReader xml(payload);
    QString manufacturer, model, firmware, serial, hardware;
    bool responseSeen = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        const QString name = xml.name().toString();
        if (name == QStringLiteral("GetDeviceInformationResponse")) responseSeen = true;
        else if (name == QStringLiteral("Manufacturer")) manufacturer = xml.readElementText().trimmed();
        else if (name == QStringLiteral("Model")) model = xml.readElementText().trimmed();
        else if (name == QStringLiteral("FirmwareVersion")) firmware = xml.readElementText().trimmed();
        else if (name == QStringLiteral("SerialNumber")) serial = xml.readElementText().trimmed();
        else if (name == QStringLiteral("HardwareId")) hardware = xml.readElementText().trimmed();
    }
    if (xml.hasError() || !responseSeen) return;

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
    requestOnvifServices(device);
    requestOnvifNetworkInterfaces(device);
}

void DiscoveryService::requestOnvifCapabilities(const DeviceRecord &device)
{
    const QUrl url(device.deviceServiceUrl);
    if (!isSafeDeviceServiceUrl(url, device.ipAddress))
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
    QNetworkReply *reply = postOnvifRequest(request, soap);
    trackEnrichmentReply(reply);
    const quint64 generation = enrichmentGeneration_;
    const QString key = storageKeyForDevice(device);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, generation] {
        if (generation != enrichmentGeneration_) {
            reply->deleteLater();
            return;
        }
        handleOnvifCapabilities(reply, key);
    });
}

void DiscoveryService::handleOnvifCapabilities(QNetworkReply *reply, const QString &deviceKey)
{
    const QByteArray payload = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool success = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
    reply->deleteLater();
    if (!devices_.contains(deviceKey))
        return;
    if (isWsseAuthenticationFailure(payload, status)) {
        const DeviceRecord device = devices_.value(deviceKey);
        if (requestWsseCredentials(device, QStringLiteral("GetCapabilities"))) {
            requestOnvifCapabilities(device);
            return;
        }
        emit statusChanged(tr("ONVIF GetCapabilities requires credentials or permissions that were not supplied."));
        return;
    }
    if (!success || payload.isEmpty() || payload.size() > 1024 * 1024)
        return;

    QXmlStreamReader xml(payload);
    QStringList capabilities;
    bool responseSeen = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        const QString name = xml.name().toString();
        if (name == QStringLiteral("GetCapabilitiesResponse")) responseSeen = true;
        if (name == QStringLiteral("Analytics") || name == QStringLiteral("Device") ||
            name == QStringLiteral("Events") || name == QStringLiteral("Imaging") ||
            name == QStringLiteral("Media") || name == QStringLiteral("PTZ") ||
            name == QStringLiteral("Extension"))
            if (!capabilities.contains(name)) capabilities.append(name);
    }
    if (xml.hasError() || !responseSeen) return;
    DeviceRecord &device = devices_[deviceKey];
    for (const QString &existing : device.capabilities) {
        if (existing.startsWith(QStringLiteral("Service: ")) && !capabilities.contains(existing))
            capabilities.append(existing);
    }
    device.capabilities = capabilities;
    if (!device.discoverySources.contains(QStringLiteral("ONVIF GetCapabilities")))
        device.discoverySources << QStringLiteral("ONVIF GetCapabilities");
    emit deviceFound(device);
    requestOnvifDns(device);
}

void DiscoveryService::requestOnvifServices(const DeviceRecord &device)
{
    const QUrl url(device.deviceServiceUrl);
    if (!isSafeDeviceServiceUrl(url, device.ipAddress))
        return;
    const QByteArray soap = R"(<?xml version="1.0" encoding="UTF-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
 xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
 <s:Body><tds:GetServices><tds:IncludeCapability>false</tds:IncludeCapability></tds:GetServices></s:Body>
</s:Envelope>)";
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/soap+xml; charset=utf-8"));
    request.setTransferTimeout(3500);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply *reply = postOnvifRequest(request, soap);
    trackEnrichmentReply(reply);
    const quint64 generation = enrichmentGeneration_;
    const QString key = storageKeyForDevice(device);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, generation] {
        if (generation != enrichmentGeneration_) {
            reply->deleteLater();
            return;
        }
        handleOnvifServices(reply, key);
    });
}

void DiscoveryService::handleOnvifServices(QNetworkReply *reply, const QString &deviceKey)
{
    const QByteArray payload = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool success = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
    const QString networkError = reply->errorString();
    reply->deleteLater();
    if (!devices_.contains(deviceKey))
        return;
    if (payload.size() > kMaxOnvifResponseBytes) {
        emit statusChanged(tr("ONVIF GetServices response exceeded the 1 MiB safety limit."));
        return;
    }

    if (isWsseAuthenticationFailure(payload, status)) {
        const DeviceRecord device = devices_.value(deviceKey);
        if (requestWsseCredentials(device, QStringLiteral("GetServices"))) {
            requestOnvifServices(device);
            return;
        }
        emit statusChanged(tr("ONVIF GetServices requires credentials or permissions that were not supplied."));
        return;
    }
    QString faultMessage;
    QString faultCode;
    if (OnvifSoapFaultParser::parse(payload, &faultMessage, &faultCode)) {
        emit statusChanged(tr("Camera rejected ONVIF GetServices: %1%2")
            .arg(faultMessage, faultCode.isEmpty() ? QString() : tr(" (code: %1)").arg(faultCode)));
        return;
    }
    if (!success || payload.isEmpty()) {
        emit statusChanged(tr("ONVIF GetServices failed (HTTP %1): %2")
            .arg(status).arg(networkError));
        return;
    }

    QStringList serviceCategories;
    QString parseError;
    if (!OnvifServiceParser::parseServiceCategories(
            payload, devices_.value(deviceKey).ipAddress, &serviceCategories, &parseError)) {
        emit statusChanged(tr("Could not parse ONVIF services: %1").arg(parseError));
        return;
    }

    DeviceRecord &device = devices_[deviceKey];
    for (int index = device.capabilities.size() - 1; index >= 0; --index) {
        if (device.capabilities.at(index).startsWith(QStringLiteral("Service: ")))
            device.capabilities.removeAt(index);
    }
    for (const QString &category : serviceCategories)
        device.capabilities.append(tr("Service: %1").arg(category));
    if (!device.discoverySources.contains(QStringLiteral("ONVIF GetServices")))
        device.discoverySources << QStringLiteral("ONVIF GetServices");
    emit deviceFound(device);
}

void DiscoveryService::requestOnvifNetworkInterfaces(const DeviceRecord &device)
{
    const QUrl url(device.deviceServiceUrl);
    if (!isSafeDeviceServiceUrl(url, device.ipAddress))
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
    QNetworkReply *reply = postOnvifRequest(request, soap);
    trackEnrichmentReply(reply);
    const quint64 generation = enrichmentGeneration_;
    const QString key = storageKeyForDevice(device);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, generation] {
        if (generation != enrichmentGeneration_) {
            reply->deleteLater();
            return;
        }
        handleOnvifNetworkInterfaces(reply, key);
    });
}

void DiscoveryService::handleOnvifNetworkInterfaces(QNetworkReply *reply, const QString &deviceKey)
{
    const QByteArray payload = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool success = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
    reply->deleteLater();
    if (!devices_.contains(deviceKey))
        return;
    if (isWsseAuthenticationFailure(payload, status)) {
        const DeviceRecord device = devices_.value(deviceKey);
        if (requestWsseCredentials(device, QStringLiteral("GetNetworkInterfaces"))) {
            requestOnvifNetworkInterfaces(device);
            return;
        }
        emit statusChanged(tr("ONVIF GetNetworkInterfaces requires credentials or permissions that were not supplied."));
        return;
    }
    if (!success || payload.isEmpty() || payload.size() > 1024 * 1024)
        return;

    OnvifNetworkConfig::InterfaceIpv4Info selected;
    QStringList summaries;
    QString parseError;
    if (!OnvifNetworkConfig::parseNetworkInterfaces(
            payload, devices_.value(deviceKey).ipAddress, &selected, &summaries, &parseError)) {
        emit statusChanged(tr("Could not parse camera network interfaces: %1").arg(parseError));
        return;
    }

    DeviceRecord &device = devices_[deviceKey];
    device.networkSummary = summaries.join(QStringLiteral("; "));
    device.networkInterfaceToken = selected.token;
    device.ipv4ConfigurationKnown = false;
    if (!selected.token.isEmpty()) {
        device.ipv4SubnetMask.clear();
        device.ipv4Gateway.clear();
        device.ipv4DhcpEnabled = false;
        if (!selected.address.isEmpty()) device.ipv4Address = selected.address;
        bool prefixOk = false;
        const int prefix = selected.prefix.toInt(&prefixOk);
        const bool validPrefix = prefixOk && prefix >= 1 && prefix <= 32;
        if (validPrefix) {
            const quint32 mask = 0xffffffffU << (32 - prefix);
            device.ipv4SubnetMask = QHostAddress(mask).toString();
        }
        if (selected.dhcp.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 ||
            selected.dhcp == QStringLiteral("1")) {
            device.ipv4DhcpEnabled = true;
            device.ipv4ConfigurationKnown = validPrefix && !device.ipv4Address.isEmpty();
        } else if (selected.dhcp.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0 ||
                   selected.dhcp == QStringLiteral("0")) {
            device.ipv4DhcpEnabled = false;
            device.ipv4ConfigurationKnown = validPrefix && !device.ipv4Address.isEmpty();
        }
        if (!selected.gateway.isEmpty()) device.ipv4Gateway = selected.gateway;
    }
    if (!device.ipv4ConfigurationKnown)
        device.networkInterfaceToken.clear();
    if (!device.discoverySources.contains(QStringLiteral("ONVIF GetNetworkInterfaces")))
        device.discoverySources << QStringLiteral("ONVIF GetNetworkInterfaces");
    emit deviceFound(device);
}

void DiscoveryService::requestOnvifDns(const DeviceRecord &device)
{
    const QUrl url(device.deviceServiceUrl);
    if (!isSafeDeviceServiceUrl(url, device.ipAddress))
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
    QNetworkReply *reply = postOnvifRequest(request, soap);
    trackEnrichmentReply(reply);
    const quint64 generation = enrichmentGeneration_;
    const QString key = storageKeyForDevice(device);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, generation] {
        if (generation != enrichmentGeneration_) {
            reply->deleteLater();
            return;
        }
        handleOnvifDns(reply, key);
    });
}

void DiscoveryService::handleOnvifDns(QNetworkReply *reply, const QString &deviceKey)
{
    const QByteArray payload = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool success = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
    reply->deleteLater();
    if (!devices_.contains(deviceKey))
        return;
    if (isWsseAuthenticationFailure(payload, status)) {
        const DeviceRecord device = devices_.value(deviceKey);
        if (requestWsseCredentials(device, QStringLiteral("GetDNS"))) {
            requestOnvifDns(device);
            return;
        }
        emit statusChanged(tr("ONVIF GetDNS requires credentials or permissions that were not supplied."));
        return;
    }
    if (!success || payload.isEmpty() || payload.size() > 1024 * 1024)
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
