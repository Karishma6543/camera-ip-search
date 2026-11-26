#include "discovery/onvif_soap_fault_parser.h"

#include <QtTest>

class OnvifSoapFaultParserTest final : public QObject
{
    Q_OBJECT
private slots:
    void parsesSoap11Fault();
    void parsesSoap12Fault();
    void rejectsNonFaultAndMalformedXml();
    void boundsReturnedText();
};

void OnvifSoapFaultParserTest::parsesSoap11Fault()
{
    const QByteArray payload = R"(
<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
 <soap:Body><soap:Fault><faultcode>soap:Client</faultcode>
 <faultstring>Not authorized</faultstring></soap:Fault></soap:Body>
</soap:Envelope>)";
    QString message, code;
    QVERIFY(OnvifSoapFaultParser::parse(payload, &message, &code));
    QCOMPARE(message, QStringLiteral("Not authorized"));
    QCOMPARE(code, QStringLiteral("soap:Client"));
}

void OnvifSoapFaultParserTest::parsesSoap12Fault()
{
    const QByteArray payload = R"(
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope">
 <s:Body><s:Fault><s:Code><s:Value>s:Sender</s:Value></s:Code>
 <s:Reason><s:Text xml:lang="en">Action not supported</s:Text></s:Reason>
 </s:Fault></s:Body></s:Envelope>)";
    QString message, code;
    QVERIFY(OnvifSoapFaultParser::parse(payload, &message, &code));
    QCOMPARE(message, QStringLiteral("Action not supported"));
    QCOMPARE(code, QStringLiteral("s:Sender"));
}

void OnvifSoapFaultParserTest::rejectsNonFaultAndMalformedXml()
{
    QString message;
    QVERIFY(!OnvifSoapFaultParser::parse(QByteArrayLiteral("<Envelope/>"), &message));
    QVERIFY(!OnvifSoapFaultParser::parse(QByteArrayLiteral("<Fault>"), &message));
    QVERIFY(!OnvifSoapFaultParser::parse(QByteArrayLiteral("<Fault/>"), nullptr));
}

void OnvifSoapFaultParserTest::boundsReturnedText()
{
    const QByteArray payload = QByteArrayLiteral("<Envelope><Body><Fault><faultstring>")
        + QByteArray(900, 'x') + QByteArrayLiteral("</faultstring></Fault></Body></Envelope>");
    QString message;
    QVERIFY(OnvifSoapFaultParser::parse(payload, &message));
    QCOMPARE(message.size(), 512);
}

QTEST_GUILESS_MAIN(OnvifSoapFaultParserTest)
#include "onvif_soap_fault_parser_test.moc"
