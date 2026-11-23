#include "discovery/discovery_parser.h"

#include <QtTest>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>

class DiscoveryParserTest final : public QObject
{
    Q_OBJECT
private slots:
    void parsesProjectResponse();
    void rejectsWrongSequence();
    void rejectsMalformedProjectPayload();
    void rejectsInvalidFieldTypes();
    void rejectsAdvertisedIpDifferentFromSender();
    void acceptsConfiguredIpWhenUnconfiguredCameraUsesZeroSource();
    void parsesOnvifProbeMatch();
    void rejectsMalformedOnvifXml();
    void selectsMatchingEndpointFromMultipleXAddrs();
    void rejectsEndpointWithEmbeddedCredentials();
};

void DiscoveryParserTest::parsesProjectResponse()
{
    const QByteArray payload = R"({
      "magic":"CAM_DISCOVER","version":1,"seq":42,"cmd":"PROBE_RESPONSE",
      "device":{"mac":"aa-bb-cc-dd-ee-01","ipv4":"192.168.1.253",
      "netmask":"255.255.255.0","gateway":"192.168.1.1","dns":"8.8.8.8",
      "dhcp":false,"http_port":80,"rtsp_port":554,"soc":"NT98566",
      "sensor":"SC401AI","firmware_version":"v1.0.0-rc1","activated":true}
    })";
    DeviceRecord device;
    QVERIFY(DiscoveryParser::parseProjectResponse(payload, 42,
        QHostAddress(QStringLiteral("192.168.1.253")), QStringLiteral("Ethernet"), &device));
    QCOMPARE(device.ipAddress, QStringLiteral("192.168.1.253"));
    QCOMPARE(device.macAddress, QStringLiteral("AA:BB:CC:DD:EE:01"));
    QCOMPARE(device.model, QStringLiteral("NT98566"));
    QCOMPARE(device.firmwareVersion, QStringLiteral("v1.0.0-rc1"));
    QCOMPARE(device.activationState, QStringLiteral("Activated"));
    QCOMPARE(device.interfaceName, QStringLiteral("Ethernet"));
}

void DiscoveryParserTest::rejectsWrongSequence()
{
    const QByteArray payload = R"({"magic":"CAM_DISCOVER","version":1,"seq":41,
      "cmd":"PROBE_RESPONSE","device":{"ipv4":"192.168.1.2"}})";
    DeviceRecord device;
    QVERIFY(!DiscoveryParser::parseProjectResponse(payload, 42,
        QHostAddress(QStringLiteral("192.168.1.2")), QString(), &device));
}

void DiscoveryParserTest::rejectsMalformedProjectPayload()
{
    DeviceRecord device;
    QVERIFY(!DiscoveryParser::parseProjectResponse("{not-json",
        1, QHostAddress(QStringLiteral("192.168.1.2")), QString(), &device));
    QVERIFY(!DiscoveryParser::parseProjectResponse(QByteArray(65508, 'x'),
        1, QHostAddress(QStringLiteral("192.168.1.2")), QString(), &device));
}

void DiscoveryParserTest::rejectsInvalidFieldTypes()
{
    const QByteArray payload = R"({"magic":"CAM_DISCOVER","version":1,"seq":1,
      "cmd":"PROBE_RESPONSE","device":{"activated":"yes"}})";
    DeviceRecord device;
    QVERIFY(!DiscoveryParser::parseProjectResponse(payload, 1,
        QHostAddress(QStringLiteral("192.168.1.2")), QString(), &device));
}


void DiscoveryParserTest::rejectsAdvertisedIpDifferentFromSender()
{
    const QByteArray payload = R"({"magic":"CAM_DISCOVER","version":1,"seq":7,
      "cmd":"PROBE_RESPONSE","device":{"ipv4":"192.168.1.99"}})";
    DeviceRecord device;
    QVERIFY(!DiscoveryParser::parseProjectResponse(payload, 7,
        QHostAddress(QStringLiteral("192.168.1.20")), QStringLiteral("Ethernet"), &device));
}


void DiscoveryParserTest::acceptsConfiguredIpWhenUnconfiguredCameraUsesZeroSource()
{
    const QByteArray payload = R"({"magic":"CAM_DISCOVER","version":1,"seq":8,
      "cmd":"PROBE_RESPONSE","device":{"ipv4":"192.168.1.253"}})";
    DeviceRecord device;
    QVERIFY(DiscoveryParser::parseProjectResponse(payload, 8,
        QHostAddress(QStringLiteral("0.0.0.0")), QStringLiteral("Ethernet"), &device));
    QCOMPARE(device.ipAddress, QStringLiteral("192.168.1.253"));
}

void DiscoveryParserTest::parsesOnvifProbeMatch()
{
    const QByteArray payload = R"(
      <d:ProbeMatches xmlns:d="http://schemas.xmlsoap.org/ws/2005/04/discovery"
        xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">
        <d:ProbeMatch><a:EndpointReference><a:Address>urn:uuid:camera-123</a:Address></a:EndpointReference>
        <d:Scopes>onvif://www.onvif.org/name/Front%20Gate onvif://www.onvif.org/manufacturer/Oxita</d:Scopes>
        <d:XAddrs>http://192.168.1.20/onvif/device_service</d:XAddrs></d:ProbeMatch>
      </d:ProbeMatches>)";
    DeviceRecord device;
    QVERIFY(DiscoveryParser::parseOnvifProbeMatch(payload,
        QHostAddress(QStringLiteral("192.168.1.20")), QStringLiteral("Wi-Fi"), &device));
    QCOMPARE(device.ipAddress, QStringLiteral("192.168.1.20"));
    QCOMPARE(device.onvifEndpoint, QStringLiteral("urn:uuid:camera-123"));
    QCOMPARE(device.model, QStringLiteral("Front Gate"));
    QCOMPARE(device.manufacturer, QStringLiteral("Oxita"));
}

void DiscoveryParserTest::rejectsMalformedOnvifXml()
{
    DeviceRecord device;
    QVERIFY(!DiscoveryParser::parseOnvifProbeMatch("<ProbeMatches>",
        QHostAddress(QStringLiteral("192.168.1.20")), QString(), &device));
}


void DiscoveryParserTest::selectsMatchingEndpointFromMultipleXAddrs()
{
    const QByteArray payload = R"(
      <d:ProbeMatches xmlns:d="http://schemas.xmlsoap.org/ws/2005/04/discovery"
        xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">
        <d:ProbeMatch>
          <a:EndpointReference><a:Address>urn:uuid:multi-endpoint</a:Address></a:EndpointReference>
          <d:XAddrs>http://camera-host.invalid/onvif/device_service http://192.168.1.20/onvif/device_service</d:XAddrs>
        </d:ProbeMatch>
      </d:ProbeMatches>)";
    DeviceRecord device;
    QVERIFY(DiscoveryParser::parseOnvifProbeMatch(payload,
        QHostAddress(QStringLiteral("192.168.1.20")), QStringLiteral("Ethernet"), &device));
    QCOMPARE(device.deviceServiceUrl,
             QStringLiteral("http://192.168.1.20/onvif/device_service"));
}


void DiscoveryParserTest::rejectsEndpointWithEmbeddedCredentials()
{
    const QByteArray payload = R"(
      <d:ProbeMatches xmlns:d="http://schemas.xmlsoap.org/ws/2005/04/discovery"
        xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">
        <d:ProbeMatch>
          <a:EndpointReference><a:Address>urn:uuid:credential-endpoint</a:Address></a:EndpointReference>
          <d:XAddrs>http://operator:secret@192.168.1.20/onvif/device_service</d:XAddrs>
        </d:ProbeMatch>
      </d:ProbeMatches>)";
    DeviceRecord device;
    QVERIFY(DiscoveryParser::parseOnvifProbeMatch(payload,
        QHostAddress(QStringLiteral("192.168.1.20")), QStringLiteral("Ethernet"), &device));
    QVERIFY(device.deviceServiceUrl.isEmpty());
}

QTEST_GUILESS_MAIN(DiscoveryParserTest)
#include "discovery_parser_test.moc"
