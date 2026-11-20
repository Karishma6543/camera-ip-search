#include "discovery/ip_conflict_probe.h"

#include <QFile>
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
                    entry.prefixLength() > 0 && !entry.ip().isLoopback())
                    return interface.name();
            }
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

void IpConflictProbeTest::classifiesVerifiedZeroResponseAsClear()
{
#ifdef Q_OS_LINUX
    const QString interfaceName = usableInterfaceName();
    if (interfaceName.isEmpty()) QSKIP("No active non-loopback IPv4 interface is available.");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString executable = fakeArping(directory,
        "echo 'Received 0 response(s)'\nexit 0\n");
    QVERIFY(!executable.isEmpty());
    const auto result = IpConflictProbe::probe(QStringLiteral("127.0.0.1"),
                                                interfaceName, executable);
    // Loopback is not in the selected interface subnet; use a target derived
    // from that interface's address and prefix for the actual subprocess path.
    Q_UNUSED(result)
    const QNetworkInterface interface = QNetworkInterface::interfaceFromName(interfaceName);
    QString target;
    for (const auto &entry : interface.addressEntries()) {
        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
            !entry.ip().isLoopback() && entry.prefixLength() > 0) {
            const quint32 host = entry.ip().toIPv4Address();
            const quint32 mask = entry.prefixLength() == 0 ? 0u :
                (0xffffffffu << (32 - entry.prefixLength()));
            const quint32 candidate = (host & mask) | ((~mask) & 0x000000feu);
            target = QHostAddress(candidate).toString();
            if (target == entry.ip().toString())
                target = QHostAddress((host & mask) | ((~mask) & 0x000000fdu)).toString();
            break;
        }
    }
    if (target.isEmpty()) QSKIP("Could not derive an IPv4 target on selected interface.");
    const auto verified = IpConflictProbe::probe(target, interfaceName, executable);
    QCOMPARE(verified.state, IpAddressProbeState::Clear);
#else
    QSKIP("The validated arping backend is Linux-only.");
#endif
}

void IpConflictProbeTest::classifiesPositiveResponseAsConflict()
{
#ifdef Q_OS_LINUX
    const QString interfaceName = usableInterfaceName();
    if (interfaceName.isEmpty()) QSKIP("No active non-loopback IPv4 interface is available.");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString executable = fakeArping(directory,
        "echo 'Received 1 response(s)'\nexit 1\n");
    QVERIFY(!executable.isEmpty());
    const QNetworkInterface interface = QNetworkInterface::interfaceFromName(interfaceName);
    QString target;
    for (const auto &entry : interface.addressEntries()) {
        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
            !entry.ip().isLoopback() && entry.prefixLength() > 0) {
            const quint32 host = entry.ip().toIPv4Address();
            const quint32 mask = 0xffffffffu << (32 - entry.prefixLength());
            target = QHostAddress((host & mask) | ((~mask) & 0x000000feu)).toString();
            if (target == entry.ip().toString())
                target = QHostAddress((host & mask) | ((~mask) & 0x000000fdu)).toString();
            break;
        }
    }
    if (target.isEmpty()) QSKIP("Could not derive an IPv4 target on selected interface.");
    const auto result = IpConflictProbe::probe(target, interfaceName, executable);
    QCOMPARE(result.state, IpAddressProbeState::Conflict);
#else
    QSKIP("The validated arping backend is Linux-only.");
#endif
}

void IpConflictProbeTest::classifiesAmbiguousOutputAsUnknown()
{
#ifdef Q_OS_LINUX
    const QString interfaceName = usableInterfaceName();
    if (interfaceName.isEmpty()) QSKIP("No active non-loopback IPv4 interface is available.");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString executable = fakeArping(directory,
        "echo 'probe completed but no summary available'\nexit 0\n");
    QVERIFY(!executable.isEmpty());
    const QNetworkInterface interface = QNetworkInterface::interfaceFromName(interfaceName);
    QString target;
    for (const auto &entry : interface.addressEntries()) {
        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
            !entry.ip().isLoopback() && entry.prefixLength() > 0) {
            const quint32 host = entry.ip().toIPv4Address();
            const quint32 mask = 0xffffffffu << (32 - entry.prefixLength());
            target = QHostAddress((host & mask) | ((~mask) & 0x000000feu)).toString();
            if (target == entry.ip().toString())
                target = QHostAddress((host & mask) | ((~mask) & 0x000000fdu)).toString();
            break;
        }
    }
    if (target.isEmpty()) QSKIP("Could not derive an IPv4 target on selected interface.");
    const auto result = IpConflictProbe::probe(target, interfaceName, executable);
    QCOMPARE(result.state, IpAddressProbeState::Unknown);
#else
    QSKIP("The validated arping backend is Linux-only.");
#endif
}

QTEST_MAIN(IpConflictProbeTest)
#include "ip_conflict_probe_test.moc"
