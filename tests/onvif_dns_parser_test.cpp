#include "discovery/onvif_dns_parser.h"
#include <QtTest>
class OnvifDnsParserTest final : public QObject {
 Q_OBJECT
private slots:
 void prefersManualDns() {
  const QByteArray xml=R"(<s:Envelope xmlns:s="soap" xmlns:t="device"><s:Body><t:GetDNSResponse><t:DNSInformation><t:FromDHCP>true</t:FromDHCP><t:DNSFromDHCP><t:IPv4Address>1.1.1.1</t:IPv4Address></t:DNSFromDHCP><t:DNSManual><t:IPv4Address>8.8.8.8</t:IPv4Address></t:DNSManual></t:DNSInformation></t:GetDNSResponse></s:Body></s:Envelope>)";
  QString address,error; QVERIFY2(OnvifDnsParser::parseIpv4DnsAddress(xml,&address,&error),qPrintable(error));
  QCOMPARE(address,QString("8.8.8.8"));
 }
 void fallsBackToDhcpAndRejectsInvalidAddresses() {
  const QByteArray xml=R"(<GetDNSResponse><DNSInformation><DNSManual><IPv4Address>not-an-ip</IPv4Address></DNSManual><DNSFromDHCP><IPv4Address>192.168.1.1</IPv4Address></DNSFromDHCP></DNSInformation></GetDNSResponse>)";
  QString address; QVERIFY(OnvifDnsParser::parseIpv4DnsAddress(xml,&address));
  QCOMPARE(address,QString("192.168.1.1"));
 }
 void faultsMalformedAndOversizedAreRejected() {
  QString address,error;
  QVERIFY(!OnvifDnsParser::parseIpv4DnsAddress("<Fault/>",&address,&error));
  QVERIFY(!OnvifDnsParser::parseIpv4DnsAddress("<GetDNSResponse>",&address,&error));
  QVERIFY(!OnvifDnsParser::parseIpv4DnsAddress(QByteArray(1024*1024+1,'x'),&address,&error));
  QVERIFY(!OnvifDnsParser::parseIpv4DnsAddress("<GetDNSResponse/>",nullptr));
 }
};
QTEST_GUILESS_MAIN(OnvifDnsParserTest)
#include "onvif_dns_parser_test.moc"
