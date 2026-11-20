#include "discovery/ip_assignment_planner.h"

#include <QtTest>

class IpAssignmentPlannerTest final : public QObject
{
    Q_OBJECT
private slots:
    void createsSequentialPlan();
    void skipsReservedAddresses();
    void rejectsCrossSubnetRange();
    void rejectsNetworkAndBroadcastAddresses();
    void rejectsInsufficientFreeAddresses();
    void rejectsInvalidMask();
};

void IpAssignmentPlannerTest::createsSequentialPlan()
{
    QStringList plan;
    QString error;
    QVERIFY2(IpAssignmentPlanner::buildRangePlan(
        QStringLiteral("192.168.1.20"), QStringLiteral("192.168.1.25"),
        QStringLiteral("255.255.255.0"), 3, {}, &plan, &error), qPrintable(error));
    QCOMPARE(plan, (QStringList{QStringLiteral("192.168.1.20"),
                                QStringLiteral("192.168.1.21"),
                                QStringLiteral("192.168.1.22")}));
}

void IpAssignmentPlannerTest::skipsReservedAddresses()
{
    QStringList plan;
    QSet<QString> reserved{QStringLiteral("192.168.1.20"), QStringLiteral("192.168.1.22")};
    QVERIFY(IpAssignmentPlanner::buildRangePlan(
        QStringLiteral("192.168.1.20"), QStringLiteral("192.168.1.24"),
        QStringLiteral("255.255.255.0"), 3, reserved, &plan));
    QCOMPARE(plan, (QStringList{QStringLiteral("192.168.1.21"),
                                QStringLiteral("192.168.1.23"),
                                QStringLiteral("192.168.1.24")}));
}

void IpAssignmentPlannerTest::rejectsCrossSubnetRange()
{
    QStringList plan;
    QVERIFY(!IpAssignmentPlanner::buildRangePlan(
        QStringLiteral("192.168.1.250"), QStringLiteral("192.168.2.10"),
        QStringLiteral("255.255.255.0"), 2, {}, &plan));
}

void IpAssignmentPlannerTest::rejectsNetworkAndBroadcastAddresses()
{
    QStringList plan;
    QVERIFY(!IpAssignmentPlanner::buildRangePlan(
        QStringLiteral("192.168.1.0"), QStringLiteral("192.168.1.20"),
        QStringLiteral("255.255.255.0"), 1, {}, &plan));
    QVERIFY(!IpAssignmentPlanner::buildRangePlan(
        QStringLiteral("192.168.1.20"), QStringLiteral("192.168.1.255"),
        QStringLiteral("255.255.255.0"), 1, {}, &plan));
}

void IpAssignmentPlannerTest::rejectsInsufficientFreeAddresses()
{
    QStringList plan;
    QSet<QString> reserved{QStringLiteral("10.0.0.10"), QStringLiteral("10.0.0.11")};
    QVERIFY(!IpAssignmentPlanner::buildRangePlan(
        QStringLiteral("10.0.0.10"), QStringLiteral("10.0.0.11"),
        QStringLiteral("255.255.255.0"), 1, reserved, &plan));
    QVERIFY(plan.isEmpty());
}

void IpAssignmentPlannerTest::rejectsInvalidMask()
{
    QStringList plan;
    QVERIFY(!IpAssignmentPlanner::buildRangePlan(
        QStringLiteral("10.0.0.10"), QStringLiteral("10.0.0.20"),
        QStringLiteral("255.0.255.0"), 1, {}, &plan));
}

QTEST_MAIN(IpAssignmentPlannerTest)
#include "ip_assignment_planner_test.moc"
