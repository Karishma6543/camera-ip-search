#include "core/diagnostic_logger.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class DiagnosticLoggerTest final : public QObject
{
    Q_OBJECT
private slots:
    void redactsCommonCredentialForms();
    void writesAndExportsSanitizedDiagnostics();
};

void DiagnosticLoggerTest::redactsCommonCredentialForms()
{
    const QString input = QStringLiteral(
        "password=hunter2 username:admin Authorization: BearerSecret "
        "https://admin:secret@camera.local/api?token=abc123 "
        "Authorization: Bearer bearer-secret "
        R"({"password":"json-secret"})" " "
        "<Password>soap-secret</Password>");
    const QString output = DiagnosticLogger::redact(input);
    QVERIFY(!output.contains(QStringLiteral("hunter2")));
    QVERIFY(!output.contains(QStringLiteral("admin")));
    QVERIFY(!output.contains(QStringLiteral("BearerSecret")));
    QVERIFY(!output.contains(QStringLiteral("secret@")));
    QVERIFY(!output.contains(QStringLiteral("abc123")));
    QVERIFY(!output.contains(QStringLiteral("bearer-secret")));
        R"({"password":"json-secret"})" " "
    QVERIFY(!output.contains(QStringLiteral("soap-secret")));
    QVERIFY(output.contains(QStringLiteral("[REDACTED]")));
}

void DiagnosticLoggerTest::writesAndExportsSanitizedDiagnostics()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(DiagnosticLogger::initialize(directory.path()));
    qWarning().noquote() << "password=do-not-export";
    const QString exportPath = directory.filePath(QStringLiteral("support-log.txt"));
    QString error;
    QVERIFY2(DiagnosticLogger::exportRedacted(exportPath, &error), qPrintable(error));
    QFile exported(exportPath);
    QVERIFY(exported.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray contents = exported.readAll();
    QVERIFY(contents.contains("[REDACTED]"));
    QVERIFY(!contents.contains("do-not-export"));

    QString overwriteError;
    QVERIFY(!DiagnosticLogger::exportRedacted(DiagnosticLogger::logFilePath(),
                                                &overwriteError));
    QVERIFY(overwriteError.contains(QStringLiteral("separate destination")));
}

QTEST_MAIN(DiagnosticLoggerTest)
#include "diagnostic_logger_test.moc"
