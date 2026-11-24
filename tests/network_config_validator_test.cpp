#include "adapters/network_config_validator.h"

#include <QtTest>

class NetworkConfigValidatorTest final : public QObject
{
    Q_OBJECT
private slots:
    void acceptsValidStaticConfiguration();
    void rejectsInvalidAddress();
    void rejectsNonContiguousMask();
    void rejectsOffSubnetGateway();
    void allowsOptionalGatewayAndDns();
    void rejectsUnspecifiedAndBroadcastAddresses();
};

void NetworkConfigValidatorTest::acceptsValidStaticConfiguration()
{
    QString error;
    QVERIFY2(NetworkConfigValidator::validateStaticIpv4(
        QStringLiteral("192.168.1.50"), QStringLiteral("255.255.255.0"),
        QStringLiteral("192.168.1.1"), QStringLiteral("1.1.1.1"), &error),
        qPrintable(error));
}

void NetworkConfigValidatorTest::rejectsInvalidAddress()
{
    QVERIFY(!NetworkConfigValidator::validateStaticIpv4(
        QStringLiteral("192.168.1.999"), QStringLiteral("255.255.255.0"),
        QString(), QString(), nullptr));
}

void NetworkConfigValidatorTest::rejectsNonContiguousMask()
{
    QVERIFY(!NetworkConfigValidator::validateStaticIpv4(
        QStringLiteral("192.168.1.50"), QStringLiteral("255.0.255.0"),
        QString(), QString(), nullptr));
}

void NetworkConfigValidatorTest::rejectsOffSubnetGateway()
{
    QVERIFY(!NetworkConfigValidator::validateStaticIpv4(
        QStringLiteral("192.168.1.50"), QStringLiteral("255.255.255.0"),
        QStringLiteral("192.168.2.1"), QString(), nullptr));
}

void NetworkConfigValidatorTest::allowsOptionalGatewayAndDns()
{
    QVERIFY(NetworkConfigValidator::validateStaticIpv4(
        QStringLiteral("10.0.0.12"), QStringLiteral("255.255.255.0"),
        QString(), QString(), nullptr));
}


void NetworkConfigValidatorTest::rejectsUnspecifiedAndBroadcastAddresses()
{
    QVERIFY(!NetworkConfigValidator::validateStaticIpv4(
        QStringLiteral("0.0.0.0"), QStringLiteral("255.255.255.0"),
        QString(), QString(), nullptr));
    QVERIFY(!NetworkConfigValidator::validateStaticIpv4(
        QStringLiteral("192.168.1.50"), QStringLiteral("255.255.255.0"),
        QString(), QStringLiteral("255.255.255.255"), nullptr));
    QVERIFY(!NetworkConfigValidator::validateStaticIpv4(
        QStringLiteral("192.168.1.50"), QStringLiteral("255.255.255.0"),
        QStringLiteral("255.255.255.255"), QString(), nullptr));
}

QTEST_MAIN(NetworkConfigValidatorTest)
#include "network_config_validator_test.moc"
