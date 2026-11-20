#include "reachability_checker.h"
#include <QDateTime>
#include <QHostAddress>
#include <QSet>

namespace {
const QStringList commonPorts = {
    QStringLiteral("80"), QStringLiteral("443"), QStringLiteral("554"),
    QStringLiteral("8000"), QStringLiteral("8080"), QStringLiteral("8899"),
    QStringLiteral("34567")
};
}

ReachabilityChecker::ReachabilityChecker(QObject *parent) : QObject(parent)
{
    timeout_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, [this] {
        if (socket_) socket_->abort();
        tryNextPort();
    });
    socket_ = new QTcpSocket(this);
    connect(socket_, &QTcpSocket::connected, this, [this] {
        timeout_.stop();
        completeCurrent(QStringLiteral("Reachable"));
    });
    connect(socket_, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (!running_ || completed_) return;
        timeout_.stop();
        tryNextPort();
    });
}

bool ReachabilityChecker::isRunning() const { return running_; }

void ReachabilityChecker::cancel()
{
    if (!running_) return;
    running_ = false;
    completed_ = true;
    timeout_.stop();
    if (socket_) socket_->abort();
    devices_.clear();
    emit finished(checkedCount_);
}

void ReachabilityChecker::start(const QVector<DeviceRecord> &devices)
{
    if (running_) return;
    devices_ = devices;
    deviceIndex_ = -1;
    checkedCount_ = 0;
    running_ = true;
    checkNextDevice();
}

void ReachabilityChecker::checkNextDevice()
{
    ++deviceIndex_;
    if (deviceIndex_ >= devices_.size()) {
        running_ = false;
        emit finished(checkedCount_);
        return;
    }
    completed_ = false;
    portIndex_ = 0;
    QSet<QString> uniquePorts;
    ports_.clear();
    const QStringList candidates = devices_.at(deviceIndex_).openPorts + commonPorts;
    for (const QString &candidate : candidates) {
        bool ok = false;
        const int port = candidate.trimmed().toInt(&ok);
        const QString normalized = QString::number(port);
        if (ok && port > 0 && port <= 65535 && !uniquePorts.contains(normalized)) {
            uniquePorts.insert(normalized);
            ports_.append(normalized);
        }
    }
    const QString addressText = devices_.at(deviceIndex_).ipAddress.trimmed();
    QHostAddress address;
    if (addressText.isEmpty() || !address.setAddress(addressText) ||
        address.protocol() != QAbstractSocket::IPv4Protocol) {
        completeCurrent(QStringLiteral("Invalid address"));
        return;
    }
    if (ports_.isEmpty()) {
        completeCurrent(QStringLiteral("Not checked"));
        return;
    }
    tryNextPort();
}

void ReachabilityChecker::tryNextPort()
{
    if (!running_ || completed_) return;
    if (portIndex_ >= ports_.size()) {
        completeCurrent(QStringLiteral("No response"));
        return;
    }
    bool ok = false;
    const quint16 port = ports_.at(portIndex_++).toUShort(&ok);
    if (!ok) { tryNextPort(); return; }
    socket_->abort();
    socket_->connectToHost(devices_.at(deviceIndex_).ipAddress.trimmed(), port);
    timeout_.start(550);
}

void ReachabilityChecker::completeCurrent(const QString &status)
{
    if (!running_ || completed_) return;
    completed_ = true;
    timeout_.stop();
    socket_->abort();
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QString ip = devices_.at(deviceIndex_).ipAddress;
    emit deviceChecked(ip, status, now,
        status == QStringLiteral("Reachable") ? now : QString());
    ++checkedCount_;
    checkNextDevice();
}