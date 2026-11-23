#include "discovery/subnet_scanner.h"

#include <QtTest>

class SubnetScannerTest final : public QObject
{
    Q_OBJECT
private slots:
    void rejectsInvalidInterface();
    void rejectsNetmaskPrefixMismatch();
    void rejectsPointToPointAndHostOnlySubnets();
};

static QNetworkInterface activeIpv4Interface()
{
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
        const auto flags = interface.flags();
        if (!interface.isValid() ||
            !flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning) ||
            flags.testFlag(QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
                !entry.ip().isLoopback())
                return interface;
        }
    }
    return {};
}

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
    const QNetworkInterface interface = activeIpv4Interface();
    if (!interface.isValid())
        QSKIP("No active non-loopback IPv4 interface is available on this test host.");

    QNetworkAddressEntry entry;
    bool foundIpv4 = false;
    for (const QNetworkAddressEntry &configured : interface.addressEntries()) {
        if (configured.ip().protocol() == QAbstractSocket::IPv4Protocol &&
            !configured.ip().isLoopback()) {
            entry = configured;
            foundIpv4 = true;
            break;
        }
    }
    if (!foundIpv4) QSKIP("Selected interface has no usable IPv4 address.");
    entry.setNetmask(QHostAddress(QStringLiteral("255.0.255.0")));

    SubnetScanner scanner;
    QVERIFY(!scanner.start(interface, entry));
    QVERIFY(!scanner.isRunning());
}

void SubnetScannerTest::rejectsPointToPointAndHostOnlySubnets()
{
    const QNetworkInterface interface = activeIpv4Interface();
    if (!interface.isValid())
        QSKIP("No active non-loopback IPv4 interface is available on this test host.");

    SubnetScanner scanner;
    QNetworkAddressEntry pointToPoint;
    pointToPoint.setIp(QHostAddress(QStringLiteral("192.0.2.10")));
    pointToPoint.setNetmask(QHostAddress(QStringLiteral("255.255.255.254")));
    pointToPoint.setPrefixLength(31);
    QVERIFY(!scanner.start(interface, pointToPoint));

    QNetworkAddressEntry hostOnly;
    hostOnly.setIp(QHostAddress(QStringLiteral("192.0.2.10")));
    hostOnly.setNetmask(QHostAddress(QStringLiteral("255.255.255.255")));
    hostOnly.setPrefixLength(32);
    QVERIFY(!scanner.start(interface, hostOnly));

    // A /32 at the maximum unsigned IPv4 value must not wrap arithmetic and
    // accidentally turn into a scan of 0.0.0.0/24.
    QNetworkAddressEntry maximumAddress;
    maximumAddress.setIp(QHostAddress(QStringLiteral("255.255.255.255")));
    maximumAddress.setNetmask(QHostAddress(QStringLiteral("255.255.255.255")));
    maximumAddress.setPrefixLength(32);
    QVERIFY(!scanner.start(interface, maximumAddress));
    QVERIFY(!scanner.isRunning());
}

QTEST_GUILESS_MAIN(SubnetScannerTest)
#include "subnet_scanner_test.moc"
