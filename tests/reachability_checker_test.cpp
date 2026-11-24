#include "discovery/reachability_checker.h"

#include <QSignalSpy>
#include <QtTest>

class ReachabilityCheckerTest final : public QObject
{
    Q_OBJECT
private slots:
    void completesEmptyInput();
    void reportsMissingAddressAsNotChecked();
    void cancelsAnActiveCheck();
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

QTEST_GUILESS_MAIN(ReachabilityCheckerTest)
#include "reachability_checker_test.moc"
