#include "discovery/discovery_parser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QtTest>

class DiscoveryWireIntegrationTest final : public QObject
{
    Q_OBJECT
private slots:
    void contractBUdpRoundTripUsesSequenceAndSource();
    void onvifUdpRoundTripAcceptsSameHostServiceEndpoint();
    void tcpCandidateProbeConnectsToListeningService();
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
        {QStringLiteral("ipv4"), QStringLiteral("127.0.0.1")},
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
        receivedResponse.data(), 41, receivedResponse.senderAddress(),
        QStringLiteral("loopback-test"), &record));
    QCOMPARE(record.ipAddress, QStringLiteral("127.0.0.1"));
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

QTEST_GUILESS_MAIN(DiscoveryWireIntegrationTest)
#include "discovery_wire_integration_test.moc"
