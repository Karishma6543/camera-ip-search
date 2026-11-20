#include "discovery/ip_conflict_probe.h"

#include <QFile>
#include <QHostAddress>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QTemporaryDir>
#include <QtTest>

class IpConflictProbeTest final : public QObject
{
    Q_OBJECT
private slots:
    void rejectsNonIpv4Targets();
    void requiresExplicitInterface();
    void rejectsUnknownInterface();
    void rejectsMissingArpingProgram();
    void rejectsTargetOutsideSelectedSubnet();
    void detectsSelectedInterfaceAddressAsConflict();
    void classifiesVerifiedZeroResponseAsClear();
    void classifiesPositiveResponseAsConflict();
    void classifiesAmbiguousOutputAsUnknown();
private:
    QString usableInterfaceName() const
    {
        for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
            if (!(interface.flags() & QNetworkInterface::IsUp) ||
                !(interface.flags() & QNetworkInterface::IsRunning))
                continue;
            for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
                    entry.prefixLength() >= 1 && entry.prefixLength() <= 30 &&
                    !entry.ip().isLoopback())
                    return interface.name();
            }
        }
        return {};
    }

    QString targetOnActiveSubnet(const QString &interfaceName) const
    {
        const QNetworkInterface interface = QNetworkInterface::interfaceFromName(interfaceName);
        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol ||
                entry.prefixLength() < 1 || entry.prefixLength() > 30 ||
                entry.ip().isLoopback())
                continue;
            const quint32 mask = 0xffffffffu << (32 - entry.prefixLength());
            const quint32 network = entry.ip().toIPv4Address() & mask;
            const quint32 broadcast = network | ~mask;
            quint32 candidate = network + 1;
            if (candidate == entry.ip().toIPv4Address()) ++candidate;
            if (candidate < broadcast)
                return QHostAddress(candidate).toString();
        }
        return {};
    }

    QString fakeArping(QTemporaryDir &directory, const QByteArray &body) const
    {
        const QString path = directory.filePath(QStringLiteral("fake-arping"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return {};
        file.write("#!/bin/sh\n");
        file.write(body);
        file.close();
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                            QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                            QFileDevice::ExeGroup | QFileDevice::ReadOther |
                            QFileDevice::ExeOther);
        return path;
    }
};

void IpConflictProbeTest::rejectsNonIpv4Targets()
{
    QCOMPARE(IpConflictProbe::probe(QStringLiteral("2001:db8::1"),
                                    QStringLiteral("eth0")).state,
             IpAddressProbeState::Unknown);
}

void IpConflictProbeTest::requiresExplicitInterface()
{
    QCOMPARE(IpConflictProbe::probe(QStringLiteral("192.168.1.20"), {}).state,
             IpAddressProbeState::Unknown);
}

void IpConflictProbeTest::rejectsUnknownInterface()
{
    QCOMPARE(IpConflictProbe::probe(QStringLiteral("192.168.1.20"),
                                    QStringLiteral("oxita-interface-that-does-not-exist")).state,
             IpAddressProbeState::Unknown);
}

void IpConflictProbeTest::rejectsMissingArpingProgram()
{
    QCOMPARE(IpConflictProbe::probe(QStringLiteral("192.168.1.20"),
                                    QStringLiteral("eth0"), {}).state,
             IpAddressProbeState::Unknown);
}


void IpConflictProbeTest::rejectsTargetOutsideSelectedSubnet()
{
#ifdef Q_OS_LINUX
    const QString interfaceName = usableInterfaceName();
    if (interfaceName.isEmpty()) QSKIP("No active IPv4 interface is available.");
    QCOMPARE(IpConflictProbe::probe(QStringLiteral("203.0.113.254"),
                                    interfaceName).state,
             IpAddressProbeState::Unknown);
#else
    QSKIP("The validated arping backend is Linux-only.");
#endif
}

void IpConflictProbeTest::detectsSelectedInterfaceAddressAsConflict()
{
#ifdef Q_OS_LINUX
    const QString interfaceName = usableInterfaceName();
    if (interfaceName.isEmpty()) QSKIP("No active IPv4 interface is available.");
    const QNetworkInterface interface = QNetworkInterface::interfaceFromName(interfaceName);
    for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
            !entry.ip().isLoopback()) {
            QCOMPARE(IpConflictProbe::probe(entry.ip().toString(), interfaceName).state,
                     IpAddressProbeState::Conflict);
            return;
        }
    }
    QSKIP("No usable IPv4 address found on the selected interface.");
#else
    QSKIP("The validated arping backend is Linux-only.");
#endif
}

void IpConflictProbeTest::classifiesVerifiedZeroResponseAsClear()
{
#ifdef Q_OS_LINUX
    const QString interfaceName = usableInterfaceName();
    if (interfaceName.isEmpty()) QSKIP("No active IPv4 interface with a usable subnet is available.");
    const QString target = targetOnActiveSubnet(interfaceName);
    if (target.isEmpty()) QSKIP("No usable host address exists on the selected subnet.");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString executable = fakeArping(directory,
        "echo 'Received 0 response(s)'\nexit 0\n");
    QVERIFY(!executable.isEmpty());
    QCOMPARE(IpConflictProbe::probe(target, interfaceName, executable).state,
             IpAddressProbeState::Clear);
#else
    QSKIP("The validated arping backend is Linux-only.");
#endif
}

void IpConflictProbeTest::classifiesPositiveResponseAsConflict()
{
#ifdef Q_OS_LINUX
    const QString interfaceName = usableInterfaceName();
    if (interfaceName.isEmpty()) QSKIP("No active IPv4 interface with a usable subnet is available.");
    const QString target = targetOnActiveSubnet(interfaceName);
    if (target.isEmpty()) QSKIP("No usable host address exists on the selected subnet.");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString executable = fakeArping(directory,
        "echo 'Received 1 response(s)'\nexit 1\n");
    QVERIFY(!executable.isEmpty());
    QCOMPARE(IpConflictProbe::probe(target, interfaceName, executable).state,
             IpAddressProbeState::Conflict);
#else
    QSKIP("The validated arping backend is Linux-only.");
#endif
}

void IpConflictProbeTest::classifiesAmbiguousOutputAsUnknown()
{
#ifdef Q_OS_LINUX
    const QString interfaceName = usableInterfaceName();
    if (interfaceName.isEmpty()) QSKIP("No active IPv4 interface with a usable subnet is available.");
    const QString target = targetOnActiveSubnet(interfaceName);
    if (target.isEmpty()) QSKIP("No usable host address exists on the selected subnet.");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString executable = fakeArping(directory,
        "echo 'probe completed but no summary available'\nexit 0\n");
    QVERIFY(!executable.isEmpty());
    QCOMPARE(IpConflictProbe::probe(target, interfaceName, executable).state,
             IpAddressProbeState::Unknown);
#else
    QSKIP("The validated arping backend is Linux-only.");
#endif
}

QTEST_MAIN(IpConflictProbeTest)
#include "ip_conflict_probe_test.moc"
