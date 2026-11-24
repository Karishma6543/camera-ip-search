#include "discovery/onvif_network_interface_parser.h"
#include <QtTest>
class OnvifNetworkInterfaceParserTest final : public QObject {
 Q_OBJECT
private slots:
 void parsesNestedResponse() {
  const QByteArray xml=R"(<s:Envelope xmlns:s="soap" xmlns:tds="device" xmlns:tt="schema"><s:Body><tds:GetNetworkInterfacesResponse><tds:NetworkInterfaces token="eth0"><tt:Info><tt:Name>LAN</tt:Name><tt:HwAddress>AA:BB:CC:DD:EE:FF</tt:HwAddress></tt:Info><tt:IPv4><tt:Config><tt:DHCP>false</tt:DHCP><tt:Manual><tt:Address>192.168.1.20</tt:Address><tt:PrefixLength>24</tt:PrefixLength></tt:Manual><tt:Gateway><tt:IPv4Address>192.168.1.1</tt:IPv4Address></tt:Gateway></tt:Config></tt:IPv4></tds:NetworkInterfaces></tds:GetNetworkInterfacesResponse></s:Body></s:Envelope>)";
  QVector<OnvifNetworkInterface> v; QString e;
  QVERIFY2(OnvifNetworkInterfaceParser::parse(xml,&v,&e),qPrintable(e));
  QCOMPARE(v.size(),1); QCOMPARE(v[0].token,QString("eth0")); QCOMPARE(v[0].name,QString("LAN"));
  QCOMPARE(v[0].macAddress,QString("AA:BB:CC:DD:EE:FF")); QCOMPARE(v[0].ipv4Address,QString("192.168.1.20"));
  QCOMPARE(v[0].prefixLength,QString("24")); QCOMPARE(v[0].dhcp,QString("false"));
  QCOMPARE(v[0].gateway,QString("192.168.1.1"));
 }
 void manualAddressWins() {
  const QByteArray xml=R"(<NetworkInterface token="e"><IPv4><Config><FromDHCP><Address>192.168.1.10</Address><PrefixLength>24</PrefixLength></FromDHCP><Manual><Address>192.168.1.30</Address><PrefixLength>25</PrefixLength></Manual></Config></IPv4></NetworkInterface>)";
  QVector<OnvifNetworkInterface> v; QVERIFY(OnvifNetworkInterfaceParser::parse(xml,&v));
  QCOMPARE(v[0].ipv4Address,QString("192.168.1.30")); QCOMPARE(v[0].prefixLength,QString("25"));
 }
 void rejectsBadPayloads() {
  QVector<OnvifNetworkInterface> v; QString e;
  QVERIFY(!OnvifNetworkInterfaceParser::parse("<NetworkInterface>",&v,&e)); QVERIFY(!e.isEmpty());
  QVERIFY(!OnvifNetworkInterfaceParser::parse(QByteArray(1024*1024+1,'x'),&v,&e)); QVERIFY(v.isEmpty());
  QVERIFY(!OnvifNetworkInterfaceParser::parse("<root/>",nullptr));
 }
};
QTEST_GUILESS_MAIN(OnvifNetworkInterfaceParserTest)
#include "onvif_network_interface_parser_test.moc"
