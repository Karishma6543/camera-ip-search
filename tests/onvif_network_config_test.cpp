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
    void buildsFactoryResetRequestAndRejectsInvalidMode();
    void validatesGatewayOperationResponse();
    void selectsInterfaceMatchingDiscoveredAddress();
    void leavesInterfaceUnselectedWhenAddressDoesNotMatch();
    void parsesSpecStylePluralInterfacesAndVerifiesSelectedToken();
    void rejectsResponseWithoutMatchingInterface();
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
      <NetworkInterfaces token="other"><IPv4><Config><Manual><Address>192.168.1.50</Address></Manual></Config></IPv4></NetworkInterfaces>
      <NetworkInterfaces token="wanted"><IPv4><Config><Manual><Address>192.168.1.60</Address></Manual></Config></IPv4></NetworkInterfaces>
    </GetNetworkInterfacesResponse></Envelope>)";
    QVERIFY(OnvifNetworkConfig::verifyInterfaceIpv4Address(xml, "wanted", "192.168.1.60"));
}

void OnvifNetworkConfigTest::rejectsWrongAddressAndToken()
{
    const QByteArray xml = R"(<Envelope><GetNetworkInterfacesResponse><NetworkInterfaces token="wanted"><IPv4><Address>192.168.1.60</Address></IPv4></NetworkInterfaces></GetNetworkInterfacesResponse></Envelope>)";
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

void OnvifNetworkConfigTest::buildsFactoryResetRequestAndRejectsInvalidMode()
{
    const QByteArray soft = OnvifNetworkConfig::buildSetSystemFactoryDefaultRequest(QStringLiteral("Soft"));
    QVERIFY(soft.contains("SetSystemFactoryDefault"));
    QVERIFY(soft.contains("<tds:FactoryDefault>Soft</tds:FactoryDefault>"));
    QXmlStreamReader reader(soft);
    while (!reader.atEnd()) reader.readNext();
    QVERIFY(!reader.hasError());

    const QByteArray hard = OnvifNetworkConfig::buildSetSystemFactoryDefaultRequest(QStringLiteral("Hard"));
    QVERIFY(hard.contains("<tds:FactoryDefault>Hard</tds:FactoryDefault>"));
    QVERIFY(OnvifNetworkConfig::buildSetSystemFactoryDefaultRequest(QStringLiteral("erase-all")).isEmpty());
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
      <NetworkInterfaces token="management">
        <Name>Management</Name><HwAddress>00:11:22:33:44:55</HwAddress>
        <IPv4><Config><DHCP>false</DHCP><Manual>
          <Address>192.168.1.10</Address><PrefixLength>24</PrefixLength>
        </Manual><Gateway><IPv4Address>192.168.1.1</IPv4Address></Gateway></Config></IPv4>
      </NetworkInterfaces>
      <NetworkInterfaces token="camera">
        <Name>Camera LAN</Name><HwAddress>00:11:22:33:44:66</HwAddress>
        <IPv4><Config><DHCP>true</DHCP>
          <Manual><Address>192.168.1.99</Address><PrefixLength>24</PrefixLength></Manual>
          <FromDHCP><Address>192.168.1.20</Address><PrefixLength>24</PrefixLength></FromDHCP>
        </Config></IPv4>
      </NetworkInterfaces>
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
      <NetworkInterfaces token="management"><IPv4><Config><Manual>
        <Address>192.168.1.10</Address><PrefixLength>24</PrefixLength>
      </Manual></Config></IPv4></NetworkInterfaces>
    </GetNetworkInterfacesResponse></Envelope>)";

    OnvifNetworkConfig::InterfaceIpv4Info selected;
    QStringList summaries;
    QVERIFY(!OnvifNetworkConfig::parseNetworkInterfaces(
        xml, QStringLiteral("192.168.1.20"), &selected, &summaries));
    QVERIFY(selected.token.isEmpty());
    QCOMPARE(summaries.size(), 1);
}


void OnvifNetworkConfigTest::parsesSpecStylePluralInterfacesAndVerifiesSelectedToken()
{
    const QByteArray xml = R"(<s:Envelope xmlns:s="soap" xmlns:tds="device" xmlns:tt="schema">
      <s:Body><tds:GetNetworkInterfacesResponse>
        <tds:NetworkInterfaces token="eth0"><tt:Info><tt:Name>LAN</tt:Name><tt:HwAddress>AA:BB:CC:DD:EE:FF</tt:HwAddress></tt:Info>
          <tt:IPv4><tt:Config><tt:DHCP><tt:Enabled>false</tt:Enabled></tt:DHCP><tt:Manual><tt:Address>192.168.1.20</tt:Address><tt:PrefixLength>24</tt:PrefixLength></tt:Manual></tt:Config></tt:IPv4>
        </tds:NetworkInterfaces>
        <tds:NetworkInterfaces token="eth1"><tt:Info><tt:Name>WAN</tt:Name></tt:Info>
          <tt:IPv4><tt:Config><tt:DHCP>true</tt:DHCP><tt:FromDHCP><tt:Address>10.0.0.8</tt:Address><tt:PrefixLength>24</tt:PrefixLength></tt:FromDHCP></tt:Config></tt:IPv4>
        </tds:NetworkInterfaces>
      </tds:GetNetworkInterfacesResponse></s:Body></s:Envelope>)";
    OnvifNetworkConfig::InterfaceIpv4Info selected;
    QStringList summaries;
    QString error;
    QVERIFY2(OnvifNetworkConfig::parseNetworkInterfaces(xml, "192.168.1.20", &selected, &summaries, &error), qPrintable(error));
    QCOMPARE(selected.token, QString("eth0"));
    QCOMPARE(selected.name, QString("LAN"));
    QCOMPARE(selected.mac, QString("AA:BB:CC:DD:EE:FF"));
    QCOMPARE(summaries.size(), 2);
    QVERIFY2(OnvifNetworkConfig::verifyInterfaceIpv4Address(xml, "eth0", "192.168.1.20", &error), qPrintable(error));
    QVERIFY(!OnvifNetworkConfig::verifyInterfaceIpv4Address(xml, "eth1", "192.168.1.20"));
}

void OnvifNetworkConfigTest::rejectsResponseWithoutMatchingInterface()
{
    const QByteArray xml = R"(<Envelope><GetNetworkInterfacesResponse><NetworkInterfaces token="eth0"><Info><Name>LAN</Name></Info></NetworkInterfaces></GetNetworkInterfacesResponse></Envelope>)";
    OnvifNetworkConfig::InterfaceIpv4Info selected;
    QStringList summaries;
    QVERIFY(!OnvifNetworkConfig::parseNetworkInterfaces(xml, "192.168.1.20", &selected, &summaries));
}

QTEST_GUILESS_MAIN(OnvifNetworkConfigTest)
#include "onvif_network_config_test.moc"
