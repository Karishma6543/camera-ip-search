#include "discovery/onvif_network_config.h"

#include <QtTest>
#include <QXmlStreamReader>

class OnvifNetworkConfigTest final : public QObject
{
    Q_OBJECT
private slots:
    void parsesSuccessfulSetResponse();
    void detectsRebootRequired();
    void rejectsSoapFaultAndMalformedResponse();
    void verifiesOnlyRequestedInterface();
    void rejectsWrongAddressAndToken();
    void buildsEscapedGatewayRequest();
    void validatesGatewayOperationResponse();
    void selectsInterfaceMatchingDiscoveredAddress();
    void leavesInterfaceUnselectedWhenAddressDoesNotMatch();
};

void OnvifNetworkConfigTest::parsesSuccessfulSetResponse()
{
    bool reboot = true;
    const QByteArray xml = R"(<s:Envelope xmlns:s="urn:soap"><s:Body><tds:SetNetworkInterfacesResponse xmlns:tds="urn:onvif"><tds:RebootNeeded>false</tds:RebootNeeded></tds:SetNetworkInterfacesResponse></s:Body></s:Envelope>)";
    QVERIFY(OnvifNetworkConfig::parseSetNetworkInterfacesResponse(xml, &reboot));
    QVERIFY(!reboot);
}

void OnvifNetworkConfigTest::detectsRebootRequired()
{
    bool reboot = false;
    const QByteArray xml = R"(<Envelope><SetNetworkInterfacesResponse><RebootNeeded>true</RebootNeeded></SetNetworkInterfacesResponse></Envelope>)";
    QVERIFY(OnvifNetworkConfig::parseSetNetworkInterfacesResponse(xml, &reboot));
    QVERIFY(reboot);
}

void OnvifNetworkConfigTest::rejectsSoapFaultAndMalformedResponse()
{
    bool reboot = false;
    QVERIFY(!OnvifNetworkConfig::parseSetNetworkInterfacesResponse(
        R"(<Envelope><Body><Fault><Reason>Denied</Reason></Fault></Body></Envelope>)", &reboot));
    QVERIFY(!OnvifNetworkConfig::parseSetNetworkInterfacesResponse(
        R"(<Envelope><SetNetworkInterfacesResponse>)", &reboot));
    QVERIFY(!OnvifNetworkConfig::parseSetNetworkInterfacesResponse(QByteArray(), &reboot));
}

void OnvifNetworkConfigTest::verifiesOnlyRequestedInterface()
{
    const QByteArray xml = R"(<Envelope><GetNetworkInterfacesResponse>
      <NetworkInterface token="other"><IPv4><Config><Manual><Address>192.168.1.50</Address></Manual></Config></IPv4></NetworkInterface>
      <NetworkInterface token="wanted"><IPv4><Config><Manual><Address>192.168.1.60</Address></Manual></Config></IPv4></NetworkInterface>
    </GetNetworkInterfacesResponse></Envelope>)";
    QVERIFY(OnvifNetworkConfig::verifyInterfaceIpv4Address(xml, "wanted", "192.168.1.60"));
}

void OnvifNetworkConfigTest::rejectsWrongAddressAndToken()
{
    const QByteArray xml = R"(<Envelope><GetNetworkInterfacesResponse><NetworkInterface token="wanted"><IPv4><Address>192.168.1.60</Address></IPv4></NetworkInterface></GetNetworkInterfacesResponse></Envelope>)";
    QVERIFY(!OnvifNetworkConfig::verifyInterfaceIpv4Address(xml, "wanted", "192.168.1.61"));
    QVERIFY(!OnvifNetworkConfig::verifyInterfaceIpv4Address(xml, "other", "192.168.1.60"));
    QVERIFY(!OnvifNetworkConfig::verifyInterfaceIpv4Address(
        R"(<Envelope><Fault/></Envelope>)", "wanted", "192.168.1.60"));
}

void OnvifNetworkConfigTest::buildsEscapedGatewayRequest()
{
    const QByteArray xml = OnvifNetworkConfig::buildSetNetworkDefaultGatewayRequest("192.168.1.1");
    QVERIFY(xml.contains("SetNetworkDefaultGateway"));
    QVERIFY(xml.contains("<tds:IPv4Address>192.168.1.1</tds:IPv4Address>"));
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) reader.readNext();
    QVERIFY(!reader.hasError());
}

void OnvifNetworkConfigTest::validatesGatewayOperationResponse()
{
    QVERIFY(OnvifNetworkConfig::parseSoapOperationResponse(
        R"(<Envelope><SetNetworkDefaultGatewayResponse/></Envelope>)",
        "SetNetworkDefaultGatewayResponse"));
    QVERIFY(!OnvifNetworkConfig::parseSoapOperationResponse(
        R"(<Envelope><Fault/></Envelope>)", "SetNetworkDefaultGatewayResponse"));
    QVERIFY(!OnvifNetworkConfig::parseSoapOperationResponse(
        R"(<Envelope><OtherResponse/></Envelope>)", "SetNetworkDefaultGatewayResponse"));
}


void OnvifNetworkConfigTest::selectsInterfaceMatchingDiscoveredAddress()
{
    const QByteArray xml = R"(<Envelope><GetNetworkInterfacesResponse>
      <NetworkInterface token="management">
        <Name>Management</Name><HwAddress>00:11:22:33:44:55</HwAddress>
        <IPv4><Config><DHCP>false</DHCP><Manual>
          <Address>192.168.1.10</Address><PrefixLength>24</PrefixLength>
        </Manual><Gateway><IPv4Address>192.168.1.1</IPv4Address></Gateway></Config></IPv4>
      </NetworkInterface>
      <NetworkInterface token="camera">
        <Name>Camera LAN</Name><HwAddress>00:11:22:33:44:66</HwAddress>
        <IPv4><Config><DHCP>true</DHCP>
          <Manual><Address>192.168.1.99</Address><PrefixLength>24</PrefixLength></Manual>
          <FromDHCP><Address>192.168.1.20</Address><PrefixLength>24</PrefixLength></FromDHCP>
        </Config></IPv4>
      </NetworkInterface>
    </GetNetworkInterfacesResponse></Envelope>)";

    OnvifNetworkConfig::InterfaceIpv4Info selected;
    QStringList summaries;
    QVERIFY(OnvifNetworkConfig::parseNetworkInterfaces(
        xml, QStringLiteral("192.168.1.20"), &selected, &summaries));
    QCOMPARE(selected.token, QStringLiteral("camera"));
    QCOMPARE(selected.address, QStringLiteral("192.168.1.20"));
    QCOMPARE(selected.prefix, QStringLiteral("24"));
    QCOMPARE(selected.dhcp, QStringLiteral("true"));
    QCOMPARE(summaries.size(), 2);
}

void OnvifNetworkConfigTest::leavesInterfaceUnselectedWhenAddressDoesNotMatch()
{
    const QByteArray xml = R"(<Envelope><GetNetworkInterfacesResponse>
      <NetworkInterface token="management"><IPv4><Config><Manual>
        <Address>192.168.1.10</Address><PrefixLength>24</PrefixLength>
      </Manual></Config></IPv4></NetworkInterface>
    </GetNetworkInterfacesResponse></Envelope>)";

    OnvifNetworkConfig::InterfaceIpv4Info selected;
    QStringList summaries;
    QVERIFY(OnvifNetworkConfig::parseNetworkInterfaces(
        xml, QStringLiteral("192.168.1.20"), &selected, &summaries));
    QVERIFY(selected.token.isEmpty());
    QCOMPARE(summaries.size(), 1);
}

QTEST_GUILESS_MAIN(OnvifNetworkConfigTest)
#include "onvif_network_config_test.moc"
