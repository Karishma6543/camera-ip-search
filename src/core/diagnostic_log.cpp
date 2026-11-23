#include "diagnostic_log.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>

namespace {
QMutex logMutex;
constexpr qint64 kMaximumLogBytes = 2 * 1024 * 1024;
constexpr int kRetainedFiles = 3;

QString logDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/logs");
}

QString redact(QString message)
{
    const QRegularExpression keyValue(
        QStringLiteral(R"((password|passwd|authorization|token|credential|secret)\s*[:=]\s*("[^"]*"|'[^']*'|[^\s,;]+))"),
        QRegularExpression::CaseInsensitiveOption);
    message.replace(keyValue, QStringLiteral("\\1=[REDACTED]"));
    const QRegularExpression bearer(
        QStringLiteral(R"(Bearer\s+[A-Za-z0-9._~+/-]+=*)"),
        QRegularExpression::CaseInsensitiveOption);
    message.replace(bearer, QStringLiteral("Bearer [REDACTED]"));
    const QRegularExpression basicUrl(
        QStringLiteral(R"((https?://)[^/@\s:]+:[^/@\s]+@)"),
        QRegularExpression::CaseInsensitiveOption);
    message.replace(basicUrl, QStringLiteral("\\1[REDACTED]@"));
    const QRegularExpression xmlSecret(
        QStringLiteral(R"(<(Password|Username|Token)(\s[^>]*)?>.*?</\1>)"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    message.replace(xmlSecret, QStringLiteral("<REDACTED>"));
    return message.left(8192);
}

void rotateLogs(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists() || info.size() < kMaximumLogBytes) return;
    const QString oldest = path + QStringLiteral(".") + QString::number(kRetainedFiles);
    QFile::remove(oldest);
    for (int index = kRetainedFiles - 1; index >= 1; --index) {
        const QString from = path + QStringLiteral(".") + QString::number(index);
        const QString to = path + QStringLiteral(".") + QString::number(index + 1);
        if (QFile::exists(from)) QFile::rename(from, to);
    }
    QFile::rename(path, path + QStringLiteral(".1"));
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    Q_UNUSED(context);
    QMutexLocker locker(&logMutex);
    const QString directory = logDirectory();
    if (!QDir().mkpath(directory)) return;
    const QString path = directory + QStringLiteral("/application.log");
    rotateLogs(path);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
    const char *level = "debug";
    switch (type) {
    case QtDebugMsg: level = "debug"; break;
    case QtInfoMsg: level = "info"; break;
    case QtWarningMsg: level = "warning"; break;
    case QtCriticalMsg: level = "critical"; break;
    case QtFatalMsg: level = "fatal"; break;
    }
    QTextStream stream(&file);
    stream << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
           << " [" << level << "] " << redact(message) << '\n';
    stream.flush();
}
}

namespace DiagnosticLog {
void install()
{
    qInstallMessageHandler(messageHandler);
}

QString directoryPath()
{
    return logDirectory();
}
}
