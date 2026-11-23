#include "discovery/subnet_scanner.h"

#include <QtTest>

class SubnetScannerTest final : public QObject
{
    Q_OBJECT
private slots:
    void rejectsInvalidInterface();
    void rejectsNetmaskPrefixMismatch();
    void rejectsPointToPointAndHostOnlySubnets();
    void validatesSubnetBoundaryAddresses();
};

void SubnetScannerTest::rejectsInvalidInterface()
{
    QNetworkAddressEntry entry;
    entry.setIp(QHostAddress(QStringLiteral("192.0.2.10")));
    entry.setNetmask(QHostAddress(QStringLiteral("255.255.255.0")));
    entry.setPrefixLength(24);

    SubnetScanner scanner;
    QVERIFY(!scanner.start(QNetworkInterface(), entry));
    QVERIFY(!scanner.isRunning());
}

void SubnetScannerTest::rejectsNetmaskPrefixMismatch()
{
    QNetworkAddressEntry entry;
    entry.setIp(QHostAddress(QStringLiteral("192.0.2.10")));
    entry.setPrefixLength(24);
    entry.setNetmask(QHostAddress(QStringLiteral("255.0.255.0")));
    QVERIFY(!SubnetScanner::isUsableSubnet(entry));
}

void SubnetScannerTest::rejectsPointToPointAndHostOnlySubnets()
{
    QNetworkAddressEntry pointToPoint;
    pointToPoint.setIp(QHostAddress(QStringLiteral("192.0.2.10")));
    pointToPoint.setNetmask(QHostAddress(QStringLiteral("255.255.255.254")));
    pointToPoint.setPrefixLength(31);
    QVERIFY(!SubnetScanner::isUsableSubnet(pointToPoint));

    QNetworkAddressEntry hostOnly;
    hostOnly.setIp(QHostAddress(QStringLiteral("192.0.2.10")));
    hostOnly.setNetmask(QHostAddress(QStringLiteral("255.255.255.255")));
    hostOnly.setPrefixLength(32);
    QVERIFY(!SubnetScanner::isUsableSubnet(hostOnly));

    // A /32 at the maximum unsigned IPv4 value must not wrap arithmetic and
    // accidentally turn into a scan of 0.0.0.0/24.
    QNetworkAddressEntry maximumAddress;
    maximumAddress.setIp(QHostAddress(QStringLiteral("255.255.255.255")));
    maximumAddress.setNetmask(QHostAddress(QStringLiteral("255.255.255.255")));
    maximumAddress.setPrefixLength(32);
    QVERIFY(!SubnetScanner::isUsableSubnet(maximumAddress));

    QNetworkAddressEntry validHost;
    validHost.setIp(QHostAddress(QStringLiteral("192.0.2.10")));
    validHost.setNetmask(QHostAddress(QStringLiteral("255.255.255.0")));
    validHost.setPrefixLength(24);
    QVERIFY(SubnetScanner::isUsableSubnet(validHost));
}


void SubnetScannerTest::validatesSubnetBoundaryAddresses()
{
    const auto makeEntry = [](const QString &ip, const QString &mask, int prefix) {
        QNetworkAddressEntry entry;
        entry.setIp(QHostAddress(ip));
        entry.setNetmask(QHostAddress(mask));
        entry.setPrefixLength(prefix);
        return entry;
    };

    QVERIFY(!SubnetScanner::isUsableSubnet(
        makeEntry(QStringLiteral("192.0.2.0"), QStringLiteral("255.255.255.0"), 24)));
    QVERIFY(!SubnetScanner::isUsableSubnet(
        makeEntry(QStringLiteral("192.0.2.255"), QStringLiteral("255.255.255.0"), 24)));
    QVERIFY(SubnetScanner::isUsableSubnet(
        makeEntry(QStringLiteral("192.0.2.1"), QStringLiteral("255.255.255.0"), 24)));
    QVERIFY(SubnetScanner::isUsableSubnet(
        makeEntry(QStringLiteral("192.0.2.2"), QStringLiteral("255.255.255.252"), 30)));
    QVERIFY(!SubnetScanner::isUsableSubnet(
        makeEntry(QStringLiteral("192.0.2.1"), QStringLiteral("255.255.255.252"), 30)));
}

QTEST_GUILESS_MAIN(SubnetScannerTest)
#include "subnet_scanner_test.moc"
