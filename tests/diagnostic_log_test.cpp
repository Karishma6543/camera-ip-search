#include "core/diagnostic_log.h"

#include <QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

class DiagnosticLogTest final : public QObject
{
    Q_OBJECT
private slots:
    void redactsKeyValueSecrets();
    void redactsBearerAndUrlCredentials();
    void redactsXmlSecretElements();
    void redactsAuthorizationCookiesAndQuerySecrets();
    void rotatesLogsAndRetainsOnlyThreeArchives();
};

void DiagnosticLogTest::redactsKeyValueSecrets()
{
    const QString input = QStringLiteral("password=hunter2 token:abc123 authorization=BasicXYZ");
    const QString output = DiagnosticLog::redactSensitiveData(input);
    QVERIFY(!output.contains(QStringLiteral("hunter2")));
    QVERIFY(!output.contains(QStringLiteral("abc123")));
    QVERIFY(!output.contains(QStringLiteral("BasicXYZ")));
    QVERIFY(output.contains(QStringLiteral("[REDACTED]")));
}

void DiagnosticLogTest::redactsBearerAndUrlCredentials()
{
    const QString input = QStringLiteral("Bearer abc.def https://operator:secret@camera.local/device");
    const QString output = DiagnosticLog::redactSensitiveData(input);
    QVERIFY(!output.contains(QStringLiteral("abc.def")));
    QVERIFY(!output.contains(QStringLiteral("operator:secret")));
    QVERIFY(output.contains(QStringLiteral("[REDACTED]")));
}

void DiagnosticLogTest::redactsXmlSecretElements()
{
    const QString input = QStringLiteral("<Envelope><wsse:Password>top-secret</wsse:Password><wsse:Username>operator</wsse:Username><wsse:Nonce>nonce-value</wsse:Nonce></Envelope>");
    const QString output = DiagnosticLog::redactSensitiveData(input);
    QVERIFY(!output.contains(QStringLiteral("top-secret")));
    QVERIFY(!output.contains(QStringLiteral("operator")));
    QVERIFY(!output.contains(QStringLiteral("nonce-value")));
    QVERIFY(output.contains(QStringLiteral("<REDACTED>")));
}



void DiagnosticLogTest::redactsAuthorizationCookiesAndQuerySecrets()
{
    const QString input = QStringLiteral(
        "Authorization: Basic dXNlcjpwYXNz\r\n"
        "Cookie: sessionid=private-value\r\n"
        "GET /device?access_token=private-token&mode=read");
    const QString output = DiagnosticLog::redactSensitiveData(input);
    QVERIFY(!output.contains(QStringLiteral("dXNlcjpwYXNz")));
    QVERIFY(!output.contains(QStringLiteral("private-value")));
    QVERIFY(!output.contains(QStringLiteral("private-token")));
    QVERIFY(output.contains(QStringLiteral("Authorization: [REDACTED]")));
    QVERIFY(output.contains(QStringLiteral("Cookie: [REDACTED]")));
}

void DiagnosticLogTest::rotatesLogsAndRetainsOnlyThreeArchives()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("OxitaTestSuite"));
    QCoreApplication::setApplicationName(QStringLiteral("DiagnosticLogRotation"));
    const QString directory = DiagnosticLog::directoryPath();
    QVERIFY(QDir(directory).removeRecursively() || !QFileInfo::exists(directory));

    const QtMessageHandler previousHandler = qInstallMessageHandler(nullptr);
    qInstallMessageHandler(previousHandler);
    DiagnosticLog::install();

    // Each message is capped at 8 KiB by the logger. Repeated writes cross the
    // 2 MiB threshold several times and exercise the three-archive retention.
    const QString payload(8192, QLatin1Char('x'));
    for (int index = 0; index < 1200; ++index)
        qInfo().noquote() << payload;

    qInstallMessageHandler(previousHandler);
    const QString active = directory + QStringLiteral("/application.log");
    QVERIFY(QFileInfo::exists(active));
    QVERIFY(QFileInfo(active).size() <= 2 * 1024 * 1024 + 16384);
    for (int index = 1; index <= 3; ++index) {
        const QString archive = active + QStringLiteral(".") + QString::number(index);
        QVERIFY2(QFileInfo::exists(archive), qPrintable(archive));
        QVERIFY(QFileInfo(archive).size() > 0);
        QVERIFY(QFileInfo(archive).size() <= 2 * 1024 * 1024 + 16384);
    }
    QVERIFY(!QFileInfo::exists(active + QStringLiteral(".4")));
}

QTEST_GUILESS_MAIN(DiagnosticLogTest)
#include "diagnostic_log_test.moc"
