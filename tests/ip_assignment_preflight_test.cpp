#include "app/ip_assignment_preflight.h"

#include <QtTest>

class IpAssignmentPreflightTest final : public QObject
{
    Q_OBJECT
private slots:
    void allowsOnlyVerifiedClearAddresses();
    void blocksInventoryConflicts();
    void blocksLocalInfrastructureAddresses();
    void blocksLiveConflicts();
    void blocksUnknownProbeResults();
    void blocksMissingProbe();
    void rejectsDuplicateTargets();
};

void IpAssignmentPreflightTest::allowsOnlyVerifiedClearAddresses()
{
    const QList<IpAssignmentTarget> plan{
        {QStringLiteral("cam-a"), QStringLiteral("192.168.1.10"), QStringLiteral("192.168.1.20")},
        {QStringLiteral("cam-b"), QStringLiteral("192.168.1.11"), QStringLiteral("192.168.1.21")}
    };
    const auto results = IpAssignmentPreflight::evaluate(
        plan, {}, {}, [](const QString &) {
            return IpAddressProbeResult{IpAddressProbeState::Clear, QStringLiteral("ARP probe: no response")};
        });
    QCOMPARE(results.size(), 2);
    for (const auto &result : results)
        QCOMPARE(result.state, IpAssignmentPreflightState::Ready);
}

void IpAssignmentPreflightTest::blocksInventoryConflicts()
{
    const QList<IpAssignmentTarget> plan{
        {QStringLiteral("cam-a"), {}, QStringLiteral("192.168.1.20")}
    };
    const auto results = IpAssignmentPreflight::evaluate(
        plan, {QStringLiteral("192.168.1.20")}, {}, [](const QString &) {
            QFAIL("Probe must not run for an inventory conflict");
            return IpAddressProbeResult{};
        });
    QCOMPARE(results.first().state, IpAssignmentPreflightState::AddressConflict);
}

void IpAssignmentPreflightTest::blocksLocalInfrastructureAddresses()
{
    const QList<IpAssignmentTarget> plan{
        {QStringLiteral("cam-a"), {}, QStringLiteral("192.168.1.1")}
    };
    const auto results = IpAssignmentPreflight::evaluate(
        plan, {}, {QStringLiteral("192.168.1.1")}, {});
    QCOMPARE(results.first().state, IpAssignmentPreflightState::AddressConflict);
}

void IpAssignmentPreflightTest::blocksLiveConflicts()
{
    const QList<IpAssignmentTarget> plan{
        {QStringLiteral("cam-a"), {}, QStringLiteral("192.168.1.20")}
    };
    const auto results = IpAssignmentPreflight::evaluate(
        plan, {}, {}, [](const QString &) {
            return IpAddressProbeResult{IpAddressProbeState::Conflict, QStringLiteral("ARP reply received")};
        });
    QCOMPARE(results.first().state, IpAssignmentPreflightState::AddressConflict);
    QCOMPARE(results.first().message, QStringLiteral("ARP reply received"));
}

void IpAssignmentPreflightTest::blocksUnknownProbeResults()
{
    const QList<IpAssignmentTarget> plan{
        {QStringLiteral("cam-a"), {}, QStringLiteral("192.168.1.20")}
    };
    const auto results = IpAssignmentPreflight::evaluate(
        plan, {}, {}, [](const QString &) {
            return IpAddressProbeResult{IpAddressProbeState::Unknown, QStringLiteral("Probe timed out ambiguously")};
        });
    QCOMPARE(results.first().state, IpAssignmentPreflightState::ProbeUnavailable);
}

void IpAssignmentPreflightTest::blocksMissingProbe()
{
    const QList<IpAssignmentTarget> plan{
        {QStringLiteral("cam-a"), {}, QStringLiteral("192.168.1.20")}
    };
    const auto results = IpAssignmentPreflight::evaluate(plan, {}, {}, {});
    QCOMPARE(results.first().state, IpAssignmentPreflightState::ProbeUnavailable);
}

void IpAssignmentPreflightTest::rejectsDuplicateTargets()
{
    const QList<IpAssignmentTarget> plan{
        {QStringLiteral("cam-a"), {}, QStringLiteral("192.168.1.20")},
        {QStringLiteral("cam-b"), {}, QStringLiteral("192.168.1.20")}
    };
    const auto results = IpAssignmentPreflight::evaluate(
        plan, {}, {}, [](const QString &) {
            QFAIL("Probe must not run for an invalid plan");
            return IpAddressProbeResult{};
        });
    QCOMPARE(results.size(), 2);
    for (const auto &result : results)
        QCOMPARE(result.state, IpAssignmentPreflightState::InvalidPlan);
}

QTEST_MAIN(IpAssignmentPreflightTest)
#include "ip_assignment_preflight_test.moc"
