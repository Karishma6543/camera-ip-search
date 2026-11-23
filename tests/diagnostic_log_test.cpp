#include "core/diagnostic_log.h"

#include <QtTest>

class DiagnosticLogTest final : public QObject
{
    Q_OBJECT
private slots:
    void redactsKeyValueSecrets();
    void redactsBearerAndUrlCredentials();
    void redactsXmlSecretElements();
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
    const QString input = QStringLiteral("<Envelope><Password>top-secret</Password><Username>operator</Username></Envelope>");
    const QString output = DiagnosticLog::redactSensitiveData(input);
    QVERIFY(!output.contains(QStringLiteral("top-secret")));
    QVERIFY(!output.contains(QStringLiteral("operator")));
    QVERIFY(output.contains(QStringLiteral("<REDACTED>")));
}

QTEST_GUILESS_MAIN(DiagnosticLogTest)
#include "diagnostic_log_test.moc"
