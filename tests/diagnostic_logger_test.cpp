#include "core/diagnostic_logger.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class DiagnosticLoggerTest final : public QObject
{
    Q_OBJECT
private slots:
    void redactsCommonCredentialForms();
    void redactsNamespacedOnvifXmlCredentials();
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
    QVERIFY(!output.contains(QStringLiteral("json-secret")));
    QVERIFY(!output.contains(QStringLiteral("soap-secret")));
    QVERIFY(output.contains(QStringLiteral("[REDACTED]")));
}

void DiagnosticLoggerTest::redactsNamespacedOnvifXmlCredentials()
{
    const QString input = QStringLiteral(
        R"(<s:Envelope xmlns:s="urn:soap" xmlns:wsse="urn:security">)"
        R"(<wsse:Security><wsse:UsernameToken>)"
        R"(<wsse:Username>camera-admin</wsse:Username>)"
        R"(<wsse:Password Type="urn:digest">multi)"
        R"(-line-secret</wsse:Password>)"
        R"(</wsse:UsernameToken><tt:Token>session-secret</tt:Token>)"
        R"(</wsse:Security></s:Envelope>)");

    const QString output = DiagnosticLogger::redact(input);
    QVERIFY(!output.contains(QStringLiteral("camera-admin")));
    QVERIFY(!output.contains(QStringLiteral("multi")));
    QVERIFY(!output.contains(QStringLiteral("line-secret")));
    QVERIFY(!output.contains(QStringLiteral("session-secret")));
    QVERIFY(output.contains(QStringLiteral("<wsse:Password Type=")));
    QVERIFY(output.contains(QStringLiteral("</wsse:Password>")));
    QVERIFY(output.contains(QStringLiteral("<tt:Token>")));
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
