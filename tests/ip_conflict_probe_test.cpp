#include "discovery/ip_conflict_probe.h"

#include <QtTest>

class IpConflictProbeTest final : public QObject
{
    Q_OBJECT
private slots:
    void rejectsNonIpv4Targets();
    void requiresExplicitInterface();
    void rejectsUnknownInterface();
    void rejectsMissingArpingProgram();
};

void IpConflictProbeTest::rejectsNonIpv4Targets()
{
    const auto result = IpConflictProbe::probe(QStringLiteral("2001:db8::1"),
                                                QStringLiteral("eth0"));
    QCOMPARE(result.state, IpAddressProbeState::Unknown);
}

void IpConflictProbeTest::requiresExplicitInterface()
{
    const auto result = IpConflictProbe::probe(QStringLiteral("192.168.1.20"), {});
    QCOMPARE(result.state, IpAddressProbeState::Unknown);
}

void IpConflictProbeTest::rejectsUnknownInterface()
{
    const auto result = IpConflictProbe::probe(QStringLiteral("192.168.1.20"),
                                                QStringLiteral("oxita-interface-that-does-not-exist"));
    QCOMPARE(result.state, IpAddressProbeState::Unknown);
}

void IpConflictProbeTest::rejectsMissingArpingProgram()
{
    const auto result = IpConflictProbe::probe(QStringLiteral("192.168.1.20"),
                                                QStringLiteral("eth0"), {});
    QCOMPARE(result.state, IpAddressProbeState::Unknown);
}

QTEST_MAIN(IpConflictProbeTest)
#include "ip_conflict_probe_test.moc"
