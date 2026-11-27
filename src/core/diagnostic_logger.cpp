#include "diagnostic_logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QTextStream>
#include <QtGlobal>

namespace {
QMutex logMutex;
QString activeLogPath;

QString levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return QStringLiteral("DEBUG");
    case QtInfoMsg: return QStringLiteral("INFO");
    case QtWarningMsg: return QStringLiteral("WARNING");
    case QtCriticalMsg: return QStringLiteral("CRITICAL");
    case QtFatalMsg: return QStringLiteral("FATAL");
    }
    return QStringLiteral("UNKNOWN");
}

void messageHandler(QtMsgType type, const QMessageLogContext &context,
                    const QString &message)
{
    Q_UNUSED(context)
    QMutexLocker locker(&logMutex);
    if (activeLogPath.isEmpty()) return;

    QFile file(activeLogPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;

    // Keep the file bounded. Rotate one previous log; never block on network work.
    constexpr qint64 maximumBytes = 5 * 1024 * 1024;
    if (file.size() > maximumBytes) {
        file.close();
        QFile::remove(activeLogPath + QStringLiteral(".1"));
        QFile::rename(activeLogPath, activeLogPath + QStringLiteral(".1"));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            return;
    }

    const QString line = QStringLiteral("%1 [%2] %3\n")
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
             levelName(type), DiagnosticLogger::redact(message));
    file.write(line.toUtf8());
    file.flush();
}
}

bool DiagnosticLogger::initialize(const QString &directoryPath)
{
    QDir directory;
    if (!directory.mkpath(directoryPath)) return false;
    QMutexLocker locker(&logMutex);
    activeLogPath = QDir(directoryPath).filePath(QStringLiteral("diagnostics.log"));
    QFile file(activeLogPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        activeLogPath.clear();
        return false;
    }
    file.close();
    qInstallMessageHandler(messageHandler);
    return true;
}

QString DiagnosticLogger::logFilePath()
{
    QMutexLocker locker(&logMutex);
    return activeLogPath;
}

QString DiagnosticLogger::redact(const QString &text)
{
    QString safe = text;

    // Redact the entire authorization value, including multi-word schemes.
    static const QRegularExpression authorization(
        QStringLiteral(R"(((?:proxy-)?authorization\s*:\s*)[^\r\n,;]+)"),
        QRegularExpression::CaseInsensitiveOption);
    safe.replace(authorization, QStringLiteral("\\1[REDACTED]"));

    // JSON properties and XML elements often carry ONVIF credentials.
    static const QRegularExpression jsonSecrets(
        QStringLiteral(R"rx(("(?:password|passwd|pwd|username|user_name|token|access_token|refresh_token|api[_-]?key|secret|credential)"\s*:\s*)"[^"]*")rx"),
        QRegularExpression::CaseInsensitiveOption);
    safe.replace(jsonSecrets, QStringLiteral("\\1\"[REDACTED]\""));

    // ONVIF commonly uses namespace-prefixed elements (for example,
    // wsse:Password). Preserve the opening tag while matching the same
    // namespace and local name in the closing tag. [\s\S]* also covers
    // credentials whose XML text is split across lines.
    static const QRegularExpression xmlSecrets(
        QStringLiteral(R"((<((?:[A-Za-z_][A-Za-z0-9_.-]*:)?)(Password|Username|Token|AccessToken|Secret)\b[^>]*>)[\s\S]*?</\2\3\s*>)"),
        QRegularExpression::CaseInsensitiveOption);
    safe.replace(xmlSecrets, QStringLiteral("\\1[REDACTED]</\\2\\3>"));

    static const QRegularExpression secrets(
        QStringLiteral(R"((?<![<A-Za-z0-9_:.-])(password|passwd|pwd|username|user_name|token|access_token|refresh_token|api[_-]?key|secret|credential)(\s*[:=]\s*|\s+)("[^"]*"|'[^']*'|[^\s&,;]+))"),
        QRegularExpression::CaseInsensitiveOption);
    safe.replace(secrets, QStringLiteral("\\1\\2[REDACTED]"));

    static const QRegularExpression urlCredentials(
        QStringLiteral(R"((https?://)[^/@\s]+:[^/@\s]+@)"),
        QRegularExpression::CaseInsensitiveOption);
    safe.replace(urlCredentials, QStringLiteral("\\1[REDACTED]@"));

    static const QRegularExpression querySecrets(
        QStringLiteral(R"(([?&](?:password|passwd|token|access_token|api_key|secret)=)[^&#\s]*)"),
        QRegularExpression::CaseInsensitiveOption);
    safe.replace(querySecrets, QStringLiteral("\\1[REDACTED]"));
    return safe;
}

bool DiagnosticLogger::exportRedacted(const QString &destinationPath, QString *error)
{
    const QString sourcePath = logFilePath();
    if (sourcePath.isEmpty()) {
        if (error) *error = QStringLiteral("Diagnostic logging is not initialized.");
        return false;
    }

    if (QFileInfo(sourcePath).absoluteFilePath() ==
        QFileInfo(destinationPath).absoluteFilePath()) {
        if (error) *error = QStringLiteral("Choose a separate destination; the active diagnostic log cannot be overwritten.");
        return false;
    }

    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = source.errorString();
        return false;
    }
    const QByteArray raw = source.readAll();
    if (raw.size() > 10 * 1024 * 1024) {
        if (error) *error = QStringLiteral("Diagnostic log exceeds the 10 MB export safety limit.");
        return false;
    }

    const QString sanitized = redact(QString::fromUtf8(raw));
    QFile destination(destinationPath);
    if (!destination.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (error) *error = destination.errorString();
        return false;
    }
    const QByteArray exportBytes = sanitized.toUtf8();
    if (destination.write(exportBytes) != exportBytes.size()) {
        if (error) *error = destination.errorString().isEmpty()
            ? QStringLiteral("Could not write the complete diagnostic export.")
            : destination.errorString();
        return false;
    }
    destination.close();
    return true;
}
