#include "ip_conflict_probe.h"

#include <QHostAddress>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QProcess>
#include <QRegularExpression>
#include <QtGlobal>

namespace {
bool isIpv4(const QString &text, QHostAddress *parsed = nullptr)
{
    QHostAddress address;
    if (!address.setAddress(text.trimmed()) ||
        address.protocol() != QAbstractSocket::IPv4Protocol)
        return false;
    if (parsed) *parsed = address;
    return true;
}

IpAddressProbeResult unknown(const QString &reason)
{
    return {IpAddressProbeState::Unknown, reason};
}
}

IpAddressProbeResult IpConflictProbe::probe(const QString &targetAddress,
                                            const QString &interfaceName,
                                            const QString &arpingProgram)
{
#ifndef Q_OS_LINUX
    Q_UNUSED(targetAddress)
    Q_UNUSED(interfaceName)
    Q_UNUSED(arpingProgram)
    return unknown(QStringLiteral("This build currently supports verified iputils ARP probing on Linux only. Windows and macOS require a separately validated native backend."));
#else
    QHostAddress target;
    if (!isIpv4(targetAddress, &target))
        return unknown(QStringLiteral("ARP probe requires a valid IPv4 target."));
    if (interfaceName.trimmed().isEmpty())
        return unknown(QStringLiteral("Select a network interface before probing."));
    if (arpingProgram.trimmed().isEmpty())
        return unknown(QStringLiteral("The arping executable is not configured."));

    const QNetworkInterface interface = QNetworkInterface::interfaceFromName(interfaceName.trimmed());
    if (!interface.isValid() || !(interface.flags() & QNetworkInterface::IsUp) ||
        !(interface.flags() & QNetworkInterface::IsRunning))
        return unknown(QStringLiteral("The selected network interface is unavailable or not running."));

    bool inSelectedSubnet = false;
    for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
        if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol ||
            entry.netmask().isNull())
            continue;
        if (entry.ip() == target)
            return {IpAddressProbeState::Conflict,
                    QStringLiteral("Target address belongs to this computer's selected interface.")};
        if (target.isInSubnet(entry.ip(), entry.prefixLength()))
            inSelectedSubnet = true;
    }
    if (!inSelectedSubnet)
        return unknown(QStringLiteral("Target is not on an IPv4 subnet directly attached to the selected interface; no probe was sent."));

    // No shell is used. The target and interface are passed as separate,
    // validated process arguments to avoid command injection.
    QProcess process;
    process.setProgram(arpingProgram.trimmed());
    process.setArguments({QStringLiteral("-D"), QStringLiteral("-c"),
                          QStringLiteral("2"), QStringLiteral("-w"),
                          QStringLiteral("3"), QStringLiteral("-I"),
                          interfaceName.trimmed(), target.toString()});
    process.start();
    if (!process.waitForStarted(1000))
        return unknown(QStringLiteral("Could not start arping. Install an iputils-compatible arping utility and check permissions."));
    if (!process.waitForFinished(4500)) {
        process.kill();
        process.waitForFinished(500);
        return unknown(QStringLiteral("ARP duplicate-address probe timed out; the address is not considered clear."));
    }

    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput() +
                                                   process.readAllStandardError());
    const QString lower = output.toLower();
    const QRegularExpression positiveResponse(
        QStringLiteral("received\\s+[1-9][0-9]*\\s+response"));
    if (lower.contains(QStringLiteral("reply from")) ||
        lower.contains(QStringLiteral("unicast reply")) ||
        lower.contains(QStringLiteral("duplicate")) ||
        positiveResponse.match(lower).hasMatch())
        return {IpAddressProbeState::Conflict,
                QStringLiteral("An ARP response was received for %1 on %2. %3")
                    .arg(target.toString(), interfaceName, output.trimmed())};

    // iputils arping -D returns exit code 0 only when no duplicate is found.
    // Require its explicit zero-response summary as well; exit status alone
    // is insufficient because other arping implementations differ.
    const bool explicitNoReplies =
        lower.contains(QStringLiteral("received 0 response")) ||
        lower.contains(QStringLiteral("received 0 packet"));
    if (process.exitStatus() == QProcess::NormalExit &&
        process.exitCode() == 0 && explicitNoReplies) {
        return {IpAddressProbeState::Clear,
                QStringLiteral("iputils ARP duplicate-address detection reported zero replies on %1.")
                    .arg(interfaceName)};
    }

    return unknown(QStringLiteral("ARP probe result was inconclusive (exit %1, status %2): %3")
                       .arg(process.exitCode())
                       .arg(process.exitStatus() == QProcess::NormalExit
                                ? QStringLiteral("normal") : QStringLiteral("crashed"),
                            output.trimmed()));
#endif
}
