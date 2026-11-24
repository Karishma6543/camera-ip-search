#include "discovery/discovery_parser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <QElapsedTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QScopedPointer>
#include <QUdpSocket>
#include <QtTest>

class DiscoveryWireIntegrationTest final : public QObject
{
    Q_OBJECT
private slots:
    void contractBUdpRoundTripUsesSequenceAndSource();
    void onvifUdpRoundTripAcceptsSameHostServiceEndpoint();
    void tcpCandidateProbeConnectsToListeningService();
    void onvifMulticastDatagramReachesJoinedInterface();
    void projectBroadcastDatagramUsesSelectedInterface();
};

void DiscoveryWireIntegrationTest::contractBUdpRoundTripUsesSequenceAndSource()
{
    QUdpSocket camera;
    QUdpSocket application;
    QVERIFY(camera.bind(QHostAddress::LocalHost, quint16(0)));
    QVERIFY(application.bind(QHostAddress::LocalHost, quint16(0)));

    const QJsonObject request{
        {QStringLiteral("magic"), QStringLiteral("CAM_DISCOVER")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("seq"), 41},
        {QStringLiteral("cmd"), QStringLiteral("PROBE_REQUEST")}
    };
    const QByteArray requestBytes = QJsonDocument(request).toJson(QJsonDocument::Compact);
    QCOMPARE(application.writeDatagram(requestBytes, QHostAddress::LocalHost,
                                       camera.localPort()), qint64(requestBytes.size()));
    QTRY_VERIFY_WITH_TIMEOUT(camera.hasPendingDatagrams(), 1000);

    const QNetworkDatagram receivedRequest = camera.receiveDatagram();
    QCOMPARE(receivedRequest.senderAddress(), QHostAddress::LocalHost);
    const QJsonDocument parsedRequest = QJsonDocument::fromJson(receivedRequest.data());
    QVERIFY(parsedRequest.isObject());
    QCOMPARE(parsedRequest.object().value(QStringLiteral("seq")).toInt(), 41);

    const QJsonObject device{
        {QStringLiteral("mac"), QStringLiteral("02:00:00:00:00:41")},
        {QStringLiteral("ipv4"), QStringLiteral("192.0.2.41")},
        {QStringLiteral("http_port"), 8080},
        {QStringLiteral("rtsp_port"), 554},
        {QStringLiteral("soc"), QStringLiteral("SIM-SOC")},
        {QStringLiteral("firmware_version"), QStringLiteral("test")},
        {QStringLiteral("activated"), true}
    };
    const QByteArray response = QJsonDocument(QJsonObject{
        {QStringLiteral("magic"), QStringLiteral("CAM_DISCOVER")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("seq"), 41},
        {QStringLiteral("cmd"), QStringLiteral("PROBE_RESPONSE")},
        {QStringLiteral("device"), device}
    }).toJson(QJsonDocument::Compact);

    QCOMPARE(camera.writeDatagram(response, receivedRequest.senderAddress(),
                                   receivedRequest.senderPort()), qint64(response.size()));
    QTRY_VERIFY_WITH_TIMEOUT(application.hasPendingDatagrams(), 1000);
    const QNetworkDatagram receivedResponse = application.receiveDatagram();

    DeviceRecord record;
    QVERIFY(DiscoveryParser::parseProjectResponse(
        receivedResponse.data(), 41, QHostAddress(QStringLiteral("192.0.2.41")),
        QStringLiteral("loopback-test"), &record));
    QCOMPARE(record.ipAddress, QStringLiteral("192.0.2.41"));
    QCOMPARE(record.macAddress, QStringLiteral("02:00:00:00:00:41"));
    QCOMPARE(record.interfaceName, QStringLiteral("loopback-test"));
    QVERIFY(!DiscoveryParser::parseProjectResponse(
        receivedResponse.data(), 42, receivedResponse.senderAddress(),
        QStringLiteral("loopback-test"), &record));
}

void DiscoveryWireIntegrationTest::onvifUdpRoundTripAcceptsSameHostServiceEndpoint()
{
    QUdpSocket camera;
    QUdpSocket application;
    QVERIFY(camera.bind(QHostAddress::LocalHost, quint16(0)));
    QVERIFY(application.bind(QHostAddress::LocalHost, quint16(0)));

    const QByteArray probe = QByteArrayLiteral("<Probe><Types>NetworkVideoTransmitter</Types></Probe>");
    QCOMPARE(application.writeDatagram(probe, QHostAddress::LocalHost,
                                       camera.localPort()), qint64(probe.size()));
    QTRY_VERIFY_WITH_TIMEOUT(camera.hasPendingDatagrams(), 1000);
    const QNetworkDatagram request = camera.receiveDatagram();

    const QByteArray match = QByteArrayLiteral(
        "<?xml version=\"1.0\"?><e:Envelope "
        "xmlns:e=\"http://www.w3.org/2003/05/soap-envelope\" "
        "xmlns:w=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
        "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\">"
        "<e:Body><d:ProbeMatches><d:ProbeMatch>"
        "<w:EndpointReference><w:Address>urn:uuid:camera-41</w:Address></w:EndpointReference>"
        "<d:Types>dn:NetworkVideoTransmitter</d:Types>"
        "<d:XAddrs>http://127.0.0.1:18080/onvif/device_service</d:XAddrs>"
        "</d:ProbeMatch></d:ProbeMatches></e:Body></e:Envelope>");
    QCOMPARE(camera.writeDatagram(match, request.senderAddress(), request.senderPort()),
             qint64(match.size()));
    QTRY_VERIFY_WITH_TIMEOUT(application.hasPendingDatagrams(), 1000);
    const QNetworkDatagram response = application.receiveDatagram();

    DeviceRecord record;
    QVERIFY(DiscoveryParser::parseOnvifProbeMatch(
        response.data(), response.senderAddress(), QStringLiteral("loopback-test"), &record));
    QCOMPARE(record.deviceServiceUrl,
             QStringLiteral("http://127.0.0.1:18080/onvif/device_service"));
    QCOMPARE(record.onvifEndpoint, QStringLiteral("urn:uuid:camera-41"));
}

void DiscoveryWireIntegrationTest::tcpCandidateProbeConnectsToListeningService()
{
    QTcpServer cameraService;
    QVERIFY(cameraService.listen(QHostAddress::LocalHost, quint16(0)));

    QTcpSocket scannerProbe;
    scannerProbe.connectToHost(QHostAddress::LocalHost, cameraService.serverPort());
    QTRY_COMPARE_WITH_TIMEOUT(scannerProbe.state(), QAbstractSocket::ConnectedState, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(cameraService.hasPendingConnections(), 1000);
    QScopedPointer<QTcpSocket> accepted(cameraService.nextPendingConnection());
    QVERIFY(accepted);
    QCOMPARE(scannerProbe.peerAddress(), QHostAddress::LocalHost);
    QCOMPARE(scannerProbe.peerPort(), cameraService.serverPort());
}


void DiscoveryWireIntegrationTest::onvifMulticastDatagramReachesJoinedInterface()
{
    const QHostAddress group(QStringLiteral("239.255.255.250"));
    QNetworkInterface loopback;
    for (const QNetworkInterface &candidate : QNetworkInterface::allInterfaces()) {
        if (!candidate.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        for (const QNetworkAddressEntry &entry : candidate.addressEntries()) {
            if (entry.ip() == QHostAddress::LocalHost) { loopback = candidate; break; }
        }
        if (loopback.isValid()) break;
    }
    if (!loopback.isValid()) QSKIP("No IPv4 loopback interface for multicast test.");

    QUdpSocket receiver;
    if (!receiver.bind(QHostAddress::AnyIPv4, quint16(0),
                       QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
        QSKIP("Cannot bind multicast receiver on this runner.");
    if (!receiver.joinMulticastGroup(group, loopback))
        QSKIP("Multicast group join on loopback is unsupported by this runner.");

    QUdpSocket sender;
    if (!sender.bind(QHostAddress::LocalHost, quint16(0)))
        QSKIP("Cannot bind multicast sender on loopback.");
    sender.setMulticastInterface(loopback);
    sender.setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);
    const QByteArray probe("ONVIF-WS-DISCOVERY-LOOPBACK");
    QCOMPARE(sender.writeDatagram(probe, group, receiver.localPort()), qint64(probe.size()));

    QElapsedTimer wait;
    wait.start();
    while (!receiver.hasPendingDatagrams() && wait.elapsed() < 750) QTest::qWait(10);
    if (!receiver.hasPendingDatagrams()) QSKIP("Multicast loopback delivery is disabled.");
    const QNetworkDatagram datagram = receiver.receiveDatagram();
    QCOMPARE(datagram.data(), probe);
    QCOMPARE(datagram.destinationAddress(), group);
}

void DiscoveryWireIntegrationTest::projectBroadcastDatagramUsesSelectedInterface()
{
    QNetworkInterface selected;
    QHostAddress localAddress;
    QHostAddress broadcastAddress;
    for (const QNetworkInterface &candidate : QNetworkInterface::allInterfaces()) {
        const auto flags = candidate.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning) ||
            flags.testFlag(QNetworkInterface::IsLoopBack)) continue;
        for (const QNetworkAddressEntry &entry : candidate.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol ||
                entry.broadcast().isNull()) continue;
            selected = candidate;
            localAddress = entry.ip();
            broadcastAddress = entry.broadcast();
            break;
        }
        if (selected.isValid()) break;
    }
    if (!selected.isValid()) QSKIP("No active IPv4 interface with broadcast address.");

    QUdpSocket receiver;
    if (!receiver.bind(QHostAddress::AnyIPv4, quint16(0),
                       QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
        QSKIP("Cannot bind broadcast receiver.");
    QUdpSocket sender;
    if (!sender.bind(localAddress, quint16(0))) QSKIP("Cannot bind sender to selected NIC.");
    const QByteArray probe("CAM-DISCOVER-BROADCAST-TEST");
    if (sender.writeDatagram(probe, broadcastAddress, receiver.localPort()) != probe.size())
        QSKIP("Directed broadcast send is not supported on this runner.");

    QElapsedTimer wait;
    wait.start();
    while (!receiver.hasPendingDatagrams() && wait.elapsed() < 750) QTest::qWait(10);
    if (!receiver.hasPendingDatagrams()) QSKIP("Directed broadcast loopback delivery is disabled.");
    const QNetworkDatagram datagram = receiver.receiveDatagram();
    QCOMPARE(datagram.data(), probe);
    QCOMPARE(datagram.senderAddress(), localAddress);
}

QTEST_GUILESS_MAIN(DiscoveryWireIntegrationTest)
#include "discovery_wire_integration_test.moc"
