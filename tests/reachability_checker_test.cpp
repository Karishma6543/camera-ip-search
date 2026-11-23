#include "discovery/reachability_checker.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QtTest>

class ReachabilityCheckerTest final : public QObject
{
    Q_OBJECT
private slots:
    void completesEmptyInput();
    void reportsMissingAddressAsNotChecked();
    void cancelsAnActiveCheck();
    void ignoresOverlappingStartAndChecksOriginalDevice();
};

void ReachabilityCheckerTest::completesEmptyInput()
{
    ReachabilityChecker checker;
    QSignalSpy finished(&checker, &ReachabilityChecker::finished);
    checker.start(QVector<DeviceRecord>{});
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.takeFirst().at(0).toInt(), 0);
    QVERIFY(!checker.isRunning());
}

void ReachabilityCheckerTest::reportsMissingAddressAsNotChecked()
{
    ReachabilityChecker checker;
    QSignalSpy checked(&checker, &ReachabilityChecker::deviceChecked);
    QSignalSpy progress(&checker, &ReachabilityChecker::progressChanged);
    QSignalSpy finished(&checker, &ReachabilityChecker::finished);

    DeviceRecord device;
    checker.start(QVector<DeviceRecord>{device});

    QCOMPARE(checked.count(), 1);
    QCOMPARE(checked.first().at(1).toString(), QStringLiteral("Not checked"));
    QCOMPARE(progress.count(), 2);
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().at(0).toInt(), 1);
}

void ReachabilityCheckerTest::cancelsAnActiveCheck()
{
    ReachabilityChecker checker;
    QSignalSpy cancelled(&checker, &ReachabilityChecker::cancelled);
    DeviceRecord device;
    device.ipAddress = QStringLiteral("192.0.2.10");

    checker.start(QVector<DeviceRecord>{device});
    QVERIFY(checker.isRunning());
    checker.cancel();

    QVERIFY(!checker.isRunning());
    QCOMPARE(cancelled.count(), 1);
    QCOMPARE(cancelled.first().at(0).toInt(), 0);
}


void ReachabilityCheckerTest::ignoresOverlappingStartAndChecksOriginalDevice()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, quint16(0)));

    ReachabilityChecker checker;
    QSignalSpy checked(&checker, &ReachabilityChecker::deviceChecked);
    DeviceRecord first;
    first.ipAddress = QStringLiteral("127.0.0.1");
    first.openPorts << QString::number(server.serverPort());
    DeviceRecord second;
    second.ipAddress = QStringLiteral("192.0.2.77");

    checker.start(QVector<DeviceRecord>{first});
    QVERIFY(checker.isRunning());
    checker.start(QVector<DeviceRecord>{second});

    QTRY_COMPARE_WITH_TIMEOUT(checked.count(), 1, 3000);
    QCOMPARE(checked.first().at(0).toString(), QStringLiteral("127.0.0.1"));
    QCOMPARE(checked.first().at(1).toString(), QStringLiteral("Reachable"));
    QVERIFY(!checker.isRunning());
}

QTEST_GUILESS_MAIN(ReachabilityCheckerTest)
#include "reachability_checker_test.moc"
