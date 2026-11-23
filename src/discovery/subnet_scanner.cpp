#include "subnet_scanner.h"

#include <QNetworkAddressEntry>
#include <QTimer>
#include <algorithm>

namespace {
constexpr int kMaxHosts = 256;
constexpr int kConnectTimeoutMs = 450;
const QVector<quint16> kCameraPorts{80, 443, 554, 8000, 8080, 8899, 34567};
}

SubnetScanner::SubnetScanner(QObject *parent) : QObject(parent)
{
    scheduler_.setInterval(5);
    connect(&scheduler_, &QTimer::timeout, this, &SubnetScanner::schedule);
}

bool SubnetScanner::isRunning() const { return running_; }

bool SubnetScanner::appendOpenPort(DeviceRecord &device, quint16 port)
{
    const QString value = QString::number(port);
    if (device.openPorts.contains(value)) return false;
    device.openPorts.append(value);
    return true;
}

bool SubnetScanner::isUsableSubnet(const QNetworkAddressEntry &entry)
{
    if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol ||
        entry.prefixLength() < 1 || entry.prefixLength() > 32 ||
        entry.ip().isNull() || entry.ip().isLoopback() || entry.ip().isMulticast())
        return false;

    const quint32 ip = entry.ip().toIPv4Address();
    if (ip == 0U || ip == 0xffffffffU) return false;
    const quint32 prefixMask = 0xffffffffU << (32 - entry.prefixLength());
    if (entry.netmask().protocol() != QAbstractSocket::IPv4Protocol ||
        entry.netmask().toIPv4Address() != prefixMask)
        return false;

    const quint32 network = ip & prefixMask;
    const quint32 broadcast = network | ~prefixMask;
    return broadcast > network &&
           (broadcast - network) > 1U &&
           ip > network && ip < broadcast;
}

bool SubnetScanner::start(const QNetworkInterface &interface,
                          const QNetworkAddressEntry &entry)
{
    const auto flags = interface.flags();
    if (running_ || !interface.isValid() ||
        !flags.testFlag(QNetworkInterface::IsUp) ||
        !flags.testFlag(QNetworkInterface::IsRunning) ||
        flags.testFlag(QNetworkInterface::IsLoopBack) ||
        !isUsableSubnet(entry))
        return false;

    bool belongsToInterface = false;
    for (const QNetworkAddressEntry &configured : interface.addressEntries()) {
        if (configured.ip() == entry.ip() &&
            configured.prefixLength() == entry.prefixLength()) {
            belongsToInterface = true;
            break;
        }
    }
    if (!belongsToInterface) return false;

    // Derive the mask from the validated prefix.
    const quint32 prefixMask = 0xffffffffU << (32 - entry.prefixLength());

    cancel();
    pending_.clear();
    active_.clear();
    discoveredCandidates_.clear();
    candidates_ = 0;
    completedProbes_ = 0;
    totalProbes_ = 0;

    const quint32 ip = entry.ip().toIPv4Address();
    const quint32 mask = prefixMask;
    const quint32 network = ip & mask;
    const quint32 broadcast = network | ~mask;
    if (broadcast <= network || (broadcast - network) <= 1U) return false;
    if (ip <= network || ip >= broadcast) return false;

    const quint32 first = network + 1;
    const quint32 last = broadcast - 1;
    const quint32 hostCount = last - first + 1;
    quint32 scanFirst = first;
    quint32 scanLast = last;
    if (hostCount > kMaxHosts) {
        // Keep the bounded scan useful on larger subnets by centering the
        // address window around the selected interface's own IPv4 address.
        const quint32 halfWindow = static_cast<quint32>(kMaxHosts / 2);
        if (ip > first + halfWindow)
            scanFirst = ip - halfWindow;
        if (scanFirst > last - static_cast<quint32>(kMaxHosts) + 1)
            scanFirst = last - static_cast<quint32>(kMaxHosts) + 1;
        scanLast = scanFirst + static_cast<quint32>(kMaxHosts) - 1;
        emit statusChanged(tr("Subnet contains %1 hosts; scanning a bounded %2-address window around %3.")
                               .arg(hostCount).arg(kMaxHosts).arg(entry.ip().toString()));
    } else {
        emit statusChanged(tr("Scanning %1 local IPv4 host(s) on %2.")
                               .arg(hostCount).arg(interface.humanReadableName()));
    }

    for (quint32 host = scanFirst;; ++host) {
        if (host != ip) {
            const QString address = QHostAddress(host).toString();
            for (quint16 port : kCameraPorts)
                pending_.append(qMakePair(address, port));
        }
        // Avoid unsigned wraparound when a bounded window ends at 255.255.255.255.
        if (host == scanLast) break;
    }

    if (pending_.isEmpty()) return false;
    totalProbes_ = pending_.size();
    emit progressChanged(completedProbes_, totalProbes_, candidates_);
    running_ = true;
    scheduler_.start();
    schedule();
    return true;
}

void SubnetScanner::cancel()
{
    if (!running_ && active_.isEmpty()) return;
    scheduler_.stop();
    running_ = false;
    pending_.clear();
    const auto probes = active_;
    active_.clear();
    for (const Probe &probe : probes) {
        if (probe.timeout) probe.timeout->stop();
        if (probe.socket) {
            probe.socket->abort();
            probe.socket->deleteLater();
        }
        if (probe.timeout) probe.timeout->deleteLater();
    }
    emit statusChanged(tr("Subnet scan cancelled."));
    emit cancelled(candidates_);
}

void SubnetScanner::schedule()
{
    if (!running_) return;
    while (active_.size() < maxConcurrent_ && !pending_.isEmpty()) {
        const auto task = pending_.takeLast();
        launchProbe(task.first, task.second);
    }
    finishIfDone();
}

void SubnetScanner::launchProbe(const QString &ip, quint16 port)
{
    auto *socket = new QTcpSocket(this);
    auto *timeout = new QTimer(this);
    timeout->setSingleShot(true);
    active_.append({socket, timeout, ip, port});

    connect(socket, &QTcpSocket::connected, this, [this, socket] {
        completeProbe(socket, true);
    });
    connect(socket, &QTcpSocket::errorOccurred, this, [this, socket](QAbstractSocket::SocketError) {
        completeProbe(socket, false);
    });
    connect(timeout, &QTimer::timeout, this, [this, socket] {
        completeProbe(socket, false);
    });
    timeout->start(kConnectTimeoutMs);
    socket->connectToHost(QHostAddress(ip), port);
}

void SubnetScanner::completeProbe(QTcpSocket *socket, bool connected)
{
    auto it = std::find_if(active_.begin(), active_.end(),
                           [socket](const Probe &probe) { return probe.socket == socket; });
    if (it == active_.end()) return;

    const QString ip = it->ip;
    const quint16 port = it->port;
    if (it->timeout) {
        it->timeout->stop();
        it->timeout->deleteLater();
    }
    if (socket) {
        socket->abort();
        socket->deleteLater();
    }
    active_.erase(it);
    ++completedProbes_;
    emit progressChanged(completedProbes_, totalProbes_, candidates_);

    if (connected) {
        auto it = discoveredCandidates_.find(ip);
        if (it == discoveredCandidates_.end()) {
            DeviceRecord candidate;
            candidate.id = ip;
            candidate.ipAddress = ip;
            candidate.manufacturer = QStringLiteral("Unknown");
            candidate.model = QStringLiteral("Unverified network device");
            candidate.activationState = QStringLiteral("Unverified");
            candidate.discoverySources << QStringLiteral("Local subnet TCP scan");
            appendOpenPort(candidate, port);
            candidate.isCandidate = true;
            it = discoveredCandidates_.insert(ip, candidate);
            ++candidates_;
            emit candidateFound(it.value());
        } else if (appendOpenPort(it.value(), port)) {
            emit candidateFound(it.value());
        }
    }

    if (running_) {
        schedule();
        if (pending_.isEmpty() && active_.isEmpty()) finishIfDone();
    }
}

void SubnetScanner::finishIfDone()
{
    if (!running_ || !pending_.isEmpty() || !active_.isEmpty()) return;
    scheduler_.stop();
    running_ = false;
    emit statusChanged(tr("Subnet scan complete. %1 unverified candidate(s) found.")
                           .arg(candidates_));
    emit finished(candidates_);
}
