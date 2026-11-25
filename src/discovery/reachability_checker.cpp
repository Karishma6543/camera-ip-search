#include "reachability_checker.h"
#include <QDateTime>
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
        advanceAfterFailedAttempt();
    });
    socket_ = new QTcpSocket(this);
    connect(socket_, &QTcpSocket::connected, this, [this] {
        if (!running_ || completed_ || !attemptInProgress_) return;
        attemptInProgress_ = false;
        timeout_.stop();
        completeCurrent(QStringLiteral("Reachable"));
    });
    connect(socket_, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        advanceAfterFailedAttempt();
    });
}

bool ReachabilityChecker::isRunning() const { return running_; }

void ReachabilityChecker::cancel()
{
    if (!running_) return;
    timeout_.stop();
    completed_ = true;
    running_ = false;
    attemptInProgress_ = false;
    if (socket_) socket_->abort();
    devices_.clear();
    ports_.clear();
    deviceIndex_ = -1;
    portIndex_ = 0;
    emit cancelled(checkedCount_);
}

void ReachabilityChecker::start(const QVector<DeviceRecord> &devices)
{
    if (running_) return;
    devices_ = devices;
    deviceIndex_ = -1;
    checkedCount_ = 0;
    attemptInProgress_ = false;
    running_ = true;
    checkNextDevice();
}

void ReachabilityChecker::checkNextDevice()
{
    ++deviceIndex_;
    if (deviceIndex_ >= devices_.size()) {
        running_ = false;
        emit progressChanged(checkedCount_, devices_.size(), QString());
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
    emit progressChanged(checkedCount_, devices_.size(),
                         devices_.at(deviceIndex_).ipAddress.trimmed());
    if (devices_.at(deviceIndex_).ipAddress.trimmed().isEmpty() || ports_.isEmpty()) {
        completeCurrent(QStringLiteral("Not checked"));
        return;
    }
    tryNextPort();
}

void ReachabilityChecker::tryNextPort()
{
    if (!running_ || completed_) return;
    if (portIndex_ >= ports_.size()) {
        completeCurrent(QStringLiteral("Unreachable"));
        return;
    }
    bool ok = false;
    const quint16 port = ports_.at(portIndex_++).toUShort(&ok);
    if (!ok) { tryNextPort(); return; }
    attemptInProgress_ = true;
    timeout_.start(550);
    socket_->connectToHost(devices_.at(deviceIndex_).ipAddress.trimmed(), port);
}

void ReachabilityChecker::advanceAfterFailedAttempt()
{
    if (!running_ || completed_ || !attemptInProgress_) return;
    attemptInProgress_ = false;
    timeout_.stop();
    if (socket_) socket_->abort();
    tryNextPort();
}

void ReachabilityChecker::completeCurrent(const QString &status)
{
    if (!running_ || completed_) return;
    completed_ = true;
    attemptInProgress_ = false;
    timeout_.stop();
    socket_->abort();
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QString ip = devices_.at(deviceIndex_).ipAddress;
    emit deviceChecked(ip, status, now,
        status == QStringLiteral("Reachable") ? now : QString());
    ++checkedCount_;
    checkNextDevice();
}