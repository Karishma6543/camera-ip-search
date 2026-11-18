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

bool SubnetScanner::start(const QNetworkInterface &interface,
                          const QNetworkAddressEntry &entry)
{
    if (running_ || entry.ip().protocol() != QAbstractSocket::IPv4Protocol ||
        entry.prefixLength() < 1 || entry.prefixLength() > 32 ||
        entry.ip().isLoopback() || entry.ip().isNull())
        return false;

    cancel();
    pending_.clear();
    active_.clear();
    emitted_.clear();
    openServices_.clear();
    completed_ = 0;
    candidates_ = 0;

    const quint32 ip = entry.ip().toIPv4Address();
    const quint32 mask = entry.netmask().toIPv4Address();
    const quint32 network = ip & mask;
    const quint32 broadcast = network | ~mask;
    if (broadcast <= network + 1) return false;

    quint32 first = network + 1;
    const quint32 last = broadcast - 1;
    const quint32 hostCount = last - first + 1;
    if (hostCount > kMaxHosts) {
        const quint32 cappedLast = qMin(last, first + static_cast<quint32>(kMaxHosts) - 1);
        emit statusChanged(tr("Subnet contains %1 hosts; scanning a bounded first %2 addresses.")
                               .arg(hostCount).arg(cappedLast - first + 1));
    } else {
        emit statusChanged(tr("Scanning %1 local IPv4 host(s) on %2.")
                               .arg(hostCount).arg(interface.humanReadableName()));
    }

    const quint32 cappedLast = qMin(last, first + static_cast<quint32>(kMaxHosts) - 1);
    for (quint32 host = first; host <= cappedLast; ++host) {
        if (host == ip) continue;
        const QString address = QHostAddress(host).toString();
        for (quint16 port : kCameraPorts)
            pending_.append(qMakePair(address, port));
    }

    if (pending_.isEmpty()) return false;
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
    emit finished(candidates_);
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
    ++completed_;

    if (connected) {
        openServices_.insert(ip + QLatin1Char(':') + QString::number(port));
        if (!emitted_.contains(ip)) {
            emitted_.insert(ip);
            ++candidates_;
            DeviceRecord candidate;
            candidate.id = ip;
            candidate.ipAddress = ip;
            candidate.manufacturer = QStringLiteral("Unknown");
            candidate.model = QStringLiteral("Unverified network device");
            candidate.activationState = QStringLiteral("Unverified");
            candidate.discoverySources << QStringLiteral("Local subnet TCP scan");
            candidate.openPorts << QString::number(port);
            candidate.isCandidate = true;
            emit candidateFound(candidate);
        } else {
            emit statusChanged(tr("Additional open service found at %1:%2").arg(ip).arg(port));
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
