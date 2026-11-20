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
    void mapsAddressesToSelectedDevices();
    void rejectsDuplicateDeviceIds();
    void reservesCurrentInventoryAddresses();
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


void IpAssignmentPlannerTest::mapsAddressesToSelectedDevices()
{
    const QList<IpAssignmentTarget> devices{
        {QStringLiteral("cam-2"), QStringLiteral("192.168.1.10"), {}},
        {QStringLiteral("cam-1"), QStringLiteral("192.168.1.11"), {}}
    };
    QList<IpAssignmentTarget> plan;
    QString error;
    QVERIFY2(IpAssignmentPlanner::buildDevicePlan(
        QStringLiteral("192.168.1.10"), QStringLiteral("192.168.1.20"),
        QStringLiteral("255.255.255.0"), devices, {}, &plan, &error), qPrintable(error));
    QCOMPARE(plan.size(), 2);
    QCOMPARE(plan.at(0).deviceId, QStringLiteral("cam-2"));
    QCOMPARE(plan.at(0).proposedAddress, QStringLiteral("192.168.1.12"));
    QCOMPARE(plan.at(1).deviceId, QStringLiteral("cam-1"));
    QCOMPARE(plan.at(1).proposedAddress, QStringLiteral("192.168.1.13"));
}

void IpAssignmentPlannerTest::rejectsDuplicateDeviceIds()
{
    const QList<IpAssignmentTarget> devices{
        {QStringLiteral("same"), QStringLiteral("192.168.1.10"), {}},
        {QStringLiteral("same"), QStringLiteral("192.168.1.11"), {}}
    };
    QList<IpAssignmentTarget> plan;
    QVERIFY(!IpAssignmentPlanner::buildDevicePlan(
        QStringLiteral("192.168.1.20"), QStringLiteral("192.168.1.30"),
        QStringLiteral("255.255.255.0"), devices, {}, &plan));
    QVERIFY(plan.isEmpty());
}

void IpAssignmentPlannerTest::reservesCurrentInventoryAddresses()
{
    const QList<IpAssignmentTarget> devices{
        {QStringLiteral("cam-a"), QStringLiteral("192.168.1.10"), {}}
    };
    QList<IpAssignmentTarget> plan;
    QVERIFY(IpAssignmentPlanner::buildDevicePlan(
        QStringLiteral("192.168.1.10"), QStringLiteral("192.168.1.12"),
        QStringLiteral("255.255.255.0"), devices,
        QSet<QString>{QStringLiteral("192.168.1.11")}, &plan));
    QCOMPARE(plan.size(), 1);
    QCOMPARE(plan.first().proposedAddress, QStringLiteral("192.168.1.12"));
}

QTEST_MAIN(IpAssignmentPlannerTest)
#include "ip_assignment_planner_test.moc"
