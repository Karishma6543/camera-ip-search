#include "discovery/onvif_maintenance_operations.h"

#include <QtTest>
#include <QXmlStreamReader>

class OnvifMaintenanceOperationsTest final : public QObject
{
    Q_OBJECT
private slots:
    void buildsDhcpEnableAndDisableRequests();
    void rejectsInvalidDhcpInterfaceToken();
    void buildsManualDnsRequest();
    void buildsDhcpDnsRequest();
    void rejectsInvalidDnsConfiguration();
    void buildsRebootRequest();
};

void OnvifMaintenanceOperationsTest::buildsDhcpEnableAndDisableRequests()
{
    const QByteArray enabled = OnvifMaintenanceOperations::buildSetDhcpRequest("eth0", true);
    QVERIFY(enabled.contains("SetNetworkInterfaces"));
    QVERIFY(enabled.contains("InterfaceToken"));
    QVERIFY(enabled.contains("eth0"));
    QVERIFY(enabled.contains("<tt:DHCP>true</tt:DHCP>"));
    const QByteArray disabled = OnvifMaintenanceOperations::buildSetDhcpRequest("eth0", false);
    QVERIFY(disabled.contains("<tt:DHCP>false</tt:DHCP>"));
}

void OnvifMaintenanceOperationsTest::rejectsInvalidDhcpInterfaceToken()
{
    QVERIFY(OnvifMaintenanceOperations::buildSetDhcpRequest(QString(), true).isEmpty());
    QVERIFY(OnvifMaintenanceOperations::buildSetDhcpRequest(QString(129, QLatin1Char('x')), true).isEmpty());
}

void OnvifMaintenanceOperationsTest::buildsManualDnsRequest()
{
    const QByteArray xml = OnvifMaintenanceOperations::buildSetDnsRequest(
        false, {"1.1.1.1", "8.8.8.8"});
    QVERIFY(xml.contains("SetDNS"));
    QVERIFY(xml.contains("<tds:FromDHCP>false</tds:FromDHCP>"));
    QVERIFY(xml.contains("1.1.1.1"));
    QVERIFY(xml.contains("8.8.8.8"));
}

void OnvifMaintenanceOperationsTest::buildsDhcpDnsRequest()
{
    const QByteArray xml = OnvifMaintenanceOperations::buildSetDnsRequest(true, {});
    QVERIFY(xml.contains("<tds:FromDHCP>true</tds:FromDHCP>"));
    QVERIFY(!xml.contains("DNSManual"));
}

void OnvifMaintenanceOperationsTest::rejectsInvalidDnsConfiguration()
{
    QVERIFY(OnvifMaintenanceOperations::buildSetDnsRequest(false, {}).isEmpty());
    QVERIFY(OnvifMaintenanceOperations::buildSetDnsRequest(false, {"999.1.1.1"}).isEmpty());
    QVERIFY(OnvifMaintenanceOperations::buildSetDnsRequest(false, {"1.1.1.1", "8.8.8.8", "9.9.9.9"}).isEmpty());
    QVERIFY(OnvifMaintenanceOperations::buildSetDnsRequest(true, {"1.1.1.1"}).isEmpty());
}

void OnvifMaintenanceOperationsTest::buildsRebootRequest()
{
    const QByteArray xml = OnvifMaintenanceOperations::buildSystemRebootRequest();
    QVERIFY(xml.contains("SystemReboot"));
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) reader.readNext();
    QVERIFY(!reader.hasError());
}

QTEST_GUILESS_MAIN(OnvifMaintenanceOperationsTest)
#include "onvif_maintenance_operations_test.moc"
