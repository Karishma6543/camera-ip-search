#include "discovery/ssdp_parser.h"

#include <QtTest>

class SsdpParserTest final : public QObject
{
    Q_OBJECT
private slots:
    void parsesInternetGatewayResponse();
    void parsesPrinterResponse();
    void rejectsNonSuccessResponses();
    void rejectsMissingIdentityHeaders();
    void rejectsOversizedAndInvalidSender();
};

void SsdpParserTest::parsesInternetGatewayResponse()
{
    const QByteArray payload = QByteArrayLiteral(
        "HTTP/1.1 200 OK\r\nST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
        "USN: uuid:gateway-1::urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
        "LOCATION: http://192.0.2.1:1900/root.xml\r\n\r\n");
    DeviceRecord device;
    QVERIFY(SsdpParser::parseResponse(payload, QHostAddress(QStringLiteral("192.0.2.1")),
                                      QStringLiteral("eth0"), &device));
    QCOMPARE(device.ipAddress, QStringLiteral("192.0.2.1"));
    QCOMPARE(device.discoverySources, QStringList{QStringLiteral("SSDP/UPnP")});
    QVERIFY(device.isCandidate);
    QCOMPARE(device.model, QStringLiteral("urn:schemas-upnp-org:device:InternetGatewayDevice:1"));
}

void SsdpParserTest::parsesPrinterResponse()
{
    const QByteArray payload = QByteArrayLiteral(
        "HTTP/1.1 200 OK\r\nST: urn:schemas-upnp-org:device:Printer:1\r\n"
        "SERVER: PrinterOS/1.0 UPnP/1.1\r\n\r\n");
    DeviceRecord device;
    QVERIFY(SsdpParser::parseResponse(payload, QHostAddress(QStringLiteral("192.0.2.44")),
                                      QStringLiteral("wlan0"), &device));
    QVERIFY(device.model.contains(QStringLiteral("Printer")));
}

void SsdpParserTest::rejectsNonSuccessResponses()
{
    DeviceRecord device;
    QVERIFY(!SsdpParser::parseResponse(QByteArrayLiteral("HTTP/1.1 404 Not Found\r\nST: x\r\n"),
        QHostAddress(QStringLiteral("192.0.2.1")), QStringLiteral("eth0"), &device));
}

void SsdpParserTest::rejectsMissingIdentityHeaders()
{
    DeviceRecord device;
    QVERIFY(!SsdpParser::parseResponse(QByteArrayLiteral("HTTP/1.1 200 OK\r\nCACHE-CONTROL: max-age=10\r\n"),
        QHostAddress(QStringLiteral("192.0.2.1")), QStringLiteral("eth0"), &device));
}

void SsdpParserTest::rejectsOversizedAndInvalidSender()
{
    DeviceRecord device;
    QVERIFY(!SsdpParser::parseResponse(QByteArray(17 * 1024, 'x'),
        QHostAddress(QStringLiteral("192.0.2.1")), QStringLiteral("eth0"), &device));
    QVERIFY(!SsdpParser::parseResponse(QByteArrayLiteral("HTTP/1.1 200 OK\r\nST: x\r\n"),
        QHostAddress(QStringLiteral("127.0.0.1")), QStringLiteral("lo"), &device));
}

QTEST_GUILESS_MAIN(SsdpParserTest)
#include "ssdp_parser_test.moc"
