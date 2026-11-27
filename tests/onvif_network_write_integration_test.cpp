#include "discovery/discovery_service.h"

#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <functional>

namespace {
class SoapPeerFixture
{
public:
    QTcpServer server;
    std::function<QByteArray(const QByteArray &)> respond;

    bool listen()
    {
        QObject::connect(&server, &QTcpServer::newConnection, &server, [this] {
            while (server.hasPendingConnections()) {
                QTcpSocket *peer = server.nextPendingConnection();
                if (!peer) continue;
                QObject::connect(peer, &QTcpSocket::readyRead, peer, [this, peer] {
                    if (peer->property("handled").toBool()) return;
                    QByteArray request = peer->property("requestBuffer").toByteArray();
                    request.append(peer->readAll());
                    peer->setProperty("requestBuffer", request);
                    const qsizetype headerEnd = request.indexOf(QByteArrayLiteral("\r\n\r\n"));
                    if (headerEnd < 0) return;
                    const QRegularExpression lengthPattern(
                        QStringLiteral("Content-Length\\s*:\\s*(\\d+)"),
                        QRegularExpression::CaseInsensitiveOption);
                    const auto match = lengthPattern.match(QString::fromLatin1(request.left(headerEnd)));
                    const qsizetype bodyLength = match.hasMatch()
                        ? match.captured(1).toLongLong() : 0;
                    if (request.size() < headerEnd + 4 + bodyLength) return;
                    peer->setProperty("handled", true);
                    const QByteArray soap = request.mid(headerEnd + 4, bodyLength);
                    const QByteArray body = respond ? respond(soap) : QByteArray();
                    const QByteArray response = QByteArrayLiteral(
                        "HTTP/1.1 200 OK\r\nContent-Type: application/soap+xml\r\nContent-Length: ")
                        + QByteArray::number(body.size())
                        + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + body;
                    peer->write(response);
                    peer->disconnectFromHost();
                });
            }
        });
        return server.listen(QHostAddress::AnyIPv4, quint16(0));
    }

    QString endpoint() const
    {
        return QStringLiteral("http://127.0.0.1:%1/onvif/device_service").arg(server.serverPort());
    }
};

QString usableLocalIpv4Address()
{
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning) ||
            flags.testFlag(QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            const QHostAddress address = entry.ip();
            if (address.protocol() == QAbstractSocket::IPv4Protocol &&
                !address.isNull() && !address.isLoopback() && !address.isMulticast())
                return address.toString();
        }
    }
    return QString();
}

QString nearbyGatewayAddress(const QString &address)
{
    QHostAddress parsed;
    if (!parsed.setAddress(address))
        return QString();
    return QHostAddress(parsed.toIPv4Address() + 1U).toString();
}

DeviceRecord onvifDevice(const QString &endpoint)
{
    DeviceRecord device;
    device.ipAddress = QStringLiteral("127.0.0.1");
    device.deviceServiceUrl = endpoint;
    device.networkInterfaceToken = QStringLiteral("lan");
    device.onvifEndpoint = QStringLiteral("urn:uuid:network-write-integration");
    device.discoverySources << QStringLiteral("ONVIF WS-Discovery");
    return device;
}
}

class OnvifNetworkWriteIntegrationTest final : public QObject
{
    Q_OBJECT
private slots:
    void dhcpAcceptedButReadbackMismatchIsFailure();
    void dnsAcceptedButReadbackMismatchIsFailure();
    void staticIpv4ReconnectsAndVerifiesAtChangedAddress();
    void staticIpv4RejectsStaleReadback();
    void gatewayFailureReportsVerifiedAddressAsPartialOutcome();
};

void OnvifNetworkWriteIntegrationTest::dhcpAcceptedButReadbackMismatchIsFailure()
{
    SoapPeerFixture camera;
    camera.respond = [](const QByteArray &soap) {
        if (soap.contains("SetNetworkInterfaces"))
            return QByteArrayLiteral("<Envelope><SetNetworkInterfacesResponse/></Envelope>");
        if (soap.contains("GetNetworkInterfaces")) {
            return QByteArrayLiteral(
                "<Envelope><GetNetworkInterfacesResponse><NetworkInterfaces token='lan'>"
                "<IPv4><Config><DHCP>true</DHCP><FromDHCP><Address>127.0.0.1</Address>"
                "<PrefixLength>8</PrefixLength></FromDHCP></Config></IPv4>"
                "</NetworkInterfaces></GetNetworkInterfacesResponse></Envelope>");
        }
        return QByteArrayLiteral("<Envelope/>");
    };
    QVERIFY(camera.listen());

    DiscoveryService service;
    bool finished = false;
    bool succeeded = true;
    QString message;
    QObject::connect(&service, &DiscoveryService::maintenanceOperationFinished, &service,
        [&finished, &succeeded, &message](const DeviceRecord &, const QString &operation,
                                         bool ok, const QString &detail) {
            if (operation != QStringLiteral("DHCP configuration")) return;
            finished = true;
            succeeded = ok;
            message = detail;
        });

    service.configureDhcp(onvifDevice(camera.endpoint()), false);
    QTRY_VERIFY_WITH_TIMEOUT(finished, 3000);
    QVERIFY(!succeeded);
    QVERIFY(message.contains(QStringLiteral("read back"), Qt::CaseInsensitive));
}

void OnvifNetworkWriteIntegrationTest::dnsAcceptedButReadbackMismatchIsFailure()
{
    SoapPeerFixture camera;
    camera.respond = [](const QByteArray &soap) {
        if (soap.contains("SetDNS"))
            return QByteArrayLiteral("<Envelope><SetDNSResponse/></Envelope>");
        if (soap.contains("GetDNS")) {
            return QByteArrayLiteral(
                "<Envelope><GetDNSResponse><DNSInformation><FromDHCP>false</FromDHCP>"
                "<DNSManual><Type>IPv4</Type><IPv4Address>1.1.1.1</IPv4Address></DNSManual>"
                "</DNSInformation></GetDNSResponse></Envelope>");
        }
        return QByteArrayLiteral("<Envelope/>");
    };
    QVERIFY(camera.listen());

    DiscoveryService service;
    bool finished = false;
    bool succeeded = true;
    QString message;
    QObject::connect(&service, &DiscoveryService::maintenanceOperationFinished, &service,
        [&finished, &succeeded, &message](const DeviceRecord &, const QString &operation,
                                         bool ok, const QString &detail) {
            if (operation != QStringLiteral("DNS configuration")) return;
            finished = true;
            succeeded = ok;
            message = detail;
        });

    service.configureDns(onvifDevice(camera.endpoint()), false,
                         QStringList{QStringLiteral("8.8.8.8")});
    QTRY_VERIFY_WITH_TIMEOUT(finished, 3000);
    QVERIFY(!succeeded);
    QVERIFY(message.contains(QStringLiteral("could not be verified"), Qt::CaseInsensitive));
}


static QByteArray networkInterfaceResponse(const QString &address, const QString &gateway = QString(),
                                          bool dhcp = false)
{
    QByteArray xml = QByteArrayLiteral(
        "<Envelope><GetNetworkInterfacesResponse><NetworkInterfaces token='lan'>"
        "<IPv4><Config><DHCP>") + (dhcp ? "true" : "false") +
        QByteArrayLiteral("</DHCP><Manual><Address>") + address.toUtf8() +
        QByteArrayLiteral("</Address><PrefixLength>8</PrefixLength></Manual>");
    if (!gateway.isEmpty())
        xml += QByteArrayLiteral("<Gateway><IPv4Address>") + gateway.toUtf8() +
               QByteArrayLiteral("</IPv4Address></Gateway>");
    xml += QByteArrayLiteral("</Config></IPv4></NetworkInterfaces></GetNetworkInterfacesResponse></Envelope>");
    return xml;
}

void OnvifNetworkWriteIntegrationTest::staticIpv4ReconnectsAndVerifiesAtChangedAddress()
{
    const QString newAddress = usableLocalIpv4Address();
    QVERIFY2(!newAddress.isEmpty(), "A non-loopback IPv4 interface is required for the changed-address test.");
    SoapPeerFixture camera;
    camera.respond = [newAddress](const QByteArray &soap) {
        if (soap.contains("SetNetworkInterfaces"))
            return QByteArrayLiteral("<Envelope><SetNetworkInterfacesResponse><RebootNeeded>false</RebootNeeded></SetNetworkInterfacesResponse></Envelope>");
        if (soap.contains("GetNetworkInterfaces"))
            return networkInterfaceResponse(newAddress);
        return QByteArrayLiteral("<Envelope/>");
    };
    QVERIFY(camera.listen());
    DiscoveryService service;
    bool finished = false, verified = false;
    QString detail;
    DeviceRecord result;
    QObject::connect(&service, &DiscoveryService::networkConfigurationFinished, &service,
        [&finished, &verified, &detail](const DeviceRecord &, bool ok, const QString &message) {
            finished = true; verified = ok; detail = message;
        });
    QObject::connect(&service, &DiscoveryService::deviceFound, &service,
        [&result](const DeviceRecord &device) { if (device.ipAddress == newAddress) result = device; });
    service.applyStaticIpv4(onvifDevice(camera.endpoint()), newAddress,
                            QStringLiteral("255.0.0.0"), QString());
    QTRY_VERIFY_WITH_TIMEOUT(finished, 5000);
    QVERIFY2(verified, qPrintable(detail));
    QCOMPARE(result.ipAddress, newAddress);
    QCOMPARE(result.ipv4SubnetMask, QStringLiteral("255.0.0.0"));
    QVERIFY(result.ipv4ConfigurationKnown);
    QVERIFY(!result.ipv4DhcpEnabled);
    QVERIFY(result.deviceServiceUrl.contains(newAddress));
}

void OnvifNetworkWriteIntegrationTest::staticIpv4RejectsStaleReadback()
{
    SoapPeerFixture camera;
    camera.respond = [](const QByteArray &soap) {
        if (soap.contains("SetNetworkInterfaces"))
            return QByteArrayLiteral("<Envelope><SetNetworkInterfacesResponse><RebootNeeded>false</RebootNeeded></SetNetworkInterfacesResponse></Envelope>");
        if (soap.contains("GetNetworkInterfaces"))
            return networkInterfaceResponse(QStringLiteral("127.0.0.1"));
        return QByteArrayLiteral("<Envelope/>");
    };
    QVERIFY(camera.listen());
    DiscoveryService service;
    bool finished = false, verified = true;
    QString detail;
    QObject::connect(&service, &DiscoveryService::networkConfigurationFinished, &service,
        [&finished, &verified, &detail](const DeviceRecord &, bool ok, const QString &message) {
            finished = true; verified = ok; detail = message;
        });
    service.applyStaticIpv4(onvifDevice(camera.endpoint()), QStringLiteral("127.0.0.2"),
                            QStringLiteral("255.0.0.0"), QString());
    QTRY_VERIFY_WITH_TIMEOUT(finished, 5000);
    QVERIFY(!verified);
    QVERIFY(detail.contains(QStringLiteral("could not be verified"), Qt::CaseInsensitive));
}

void OnvifNetworkWriteIntegrationTest::gatewayFailureReportsVerifiedAddressAsPartialOutcome()
{
    const QString newAddress = usableLocalIpv4Address();
    QVERIFY2(!newAddress.isEmpty(), "A non-loopback IPv4 interface is required for the gateway test.");
    const QString gatewayAddress = nearbyGatewayAddress(newAddress);
    QVERIFY(!gatewayAddress.isEmpty() && gatewayAddress != newAddress);
    SoapPeerFixture camera;
    camera.respond = [newAddress, gatewayAddress](const QByteArray &soap) {
        if (soap.contains("SetNetworkInterfaces"))
            return QByteArrayLiteral("<Envelope><SetNetworkInterfacesResponse><RebootNeeded>false</RebootNeeded></SetNetworkInterfacesResponse></Envelope>");
        if (soap.contains("SetNetworkDefaultGateway"))
            return QByteArrayLiteral("<Envelope><SetNetworkDefaultGatewayResponse/></Envelope>");
        if (soap.contains("GetNetworkInterfaces"))
            return networkInterfaceResponse(newAddress, gatewayAddress);
        return QByteArrayLiteral("<Envelope/>");
    };
    QVERIFY(camera.listen());
    DiscoveryService service;
    bool finished = false, verified = true;
    QString detail;
    DeviceRecord updated;
    QObject::connect(&service, &DiscoveryService::networkConfigurationFinished, &service,
        [&finished, &verified, &detail](const DeviceRecord &, bool ok, const QString &message) {
            finished = true; verified = ok; detail = message;
        });
    QObject::connect(&service, &DiscoveryService::deviceFound, &service,
        [&updated](const DeviceRecord &device) { if (device.ipAddress == newAddress) updated = device; });
    service.applyStaticIpv4(onvifDevice(camera.endpoint()), newAddress,
                            QStringLiteral("255.0.0.0"), gatewayAddress);
    QTRY_VERIFY_WITH_TIMEOUT(finished, 5000);
    QVERIFY(!verified);
    QVERIFY(detail.contains(QStringLiteral("gateway"), Qt::CaseInsensitive));
    QCOMPARE(updated.ipAddress, newAddress);
    QCOMPARE(updated.ipv4Gateway, QString());
}

QTEST_GUILESS_MAIN(OnvifNetworkWriteIntegrationTest)
#include "onvif_network_write_integration_test.moc"
