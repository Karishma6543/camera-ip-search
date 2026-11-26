#include "discovery/onvif_service_parser.h"

#include <QtTest>

class OnvifServiceParserTest final : public QObject
{
    Q_OBJECT
private slots:
    void parsesServicesOnExpectedCameraHost();
    void rejectsServiceEndpointOnAnotherHost();
    void rejectsMalformedOrUnrelatedXml();
    void ignoresDuplicateServiceCategories();
};

void OnvifServiceParserTest::parsesServicesOnExpectedCameraHost()
{
    const QByteArray payload = R"(
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
 xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
 <s:Body><tds:GetServicesResponse>
  <tds:Service><tds:Namespace>http://www.onvif.org/ver10/media/wsdl</tds:Namespace>
   <tds:XAddr>http://192.0.2.10/onvif/media_service</tds:XAddr></tds:Service>
  <tds:Service><tds:Namespace>http://www.onvif.org/ver20/ptz/wsdl</tds:Namespace>
   <tds:XAddr>https://192.0.2.10:8443/onvif/ptz_service</tds:XAddr></tds:Service>
  <tds:Service><tds:Namespace>http://www.onvif.org/ver10/events/wsdl</tds:Namespace>
   <tds:XAddr>http://192.0.2.10/onvif/events_service</tds:XAddr></tds:Service>
 </tds:GetServicesResponse></s:Body>
</s:Envelope>)";
    QStringList categories;
    QString error;
    QVERIFY2(OnvifServiceParser::parseServiceCategories(
        payload, QStringLiteral("192.0.2.10"), &categories, &error), qPrintable(error));
    QCOMPARE(categories, QStringList({QStringLiteral("Media"),
                                      QStringLiteral("Ptz"),
                                      QStringLiteral("Events")}));
}

void OnvifServiceParserTest::rejectsServiceEndpointOnAnotherHost()
{
    const QByteArray payload = R"(
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"><s:Body><GetServicesResponse>
 <Service><Namespace>http://www.onvif.org/ver10/media/wsdl</Namespace>
 <XAddr>http://192.0.2.99/onvif/media_service</XAddr></Service>
</GetServicesResponse></s:Body></s:Envelope>)";
    QStringList categories;
    QVERIFY(OnvifServiceParser::parseServiceCategories(
        payload, QStringLiteral("192.0.2.10"), &categories));
    QVERIFY(categories.isEmpty());
}

void OnvifServiceParserTest::rejectsMalformedOrUnrelatedXml()
{
    QStringList categories;
    QString error;
    QVERIFY(!OnvifServiceParser::parseServiceCategories(
        QByteArrayLiteral("<broken>"), QStringLiteral("192.0.2.10"), &categories, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!OnvifServiceParser::parseServiceCategories(
        QByteArrayLiteral("<Envelope><Body><OtherResponse/></Body></Envelope>"),
        QStringLiteral("192.0.2.10"), &categories, &error));
}

void OnvifServiceParserTest::ignoresDuplicateServiceCategories()
{
    const QByteArray payload = R"(
<Envelope><Body><GetServicesResponse>
 <Service><Namespace>http://www.onvif.org/ver10/media/wsdl</Namespace>
 <XAddr>http://192.0.2.10/media1</XAddr></Service>
 <Service><Namespace>http://www.onvif.org/ver20/media/wsdl</Namespace>
 <XAddr>http://192.0.2.10/media2</XAddr></Service>
</GetServicesResponse></Body></Envelope>)";
    QStringList categories;
    QVERIFY(OnvifServiceParser::parseServiceCategories(
        payload, QStringLiteral("192.0.2.10"), &categories));
    QCOMPARE(categories, QStringList{QStringLiteral("Media")});
}

QTEST_GUILESS_MAIN(OnvifServiceParserTest)
#include "onvif_service_parser_test.moc"
