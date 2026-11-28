#include "ui/camera_browser_policy.h"

#include <QtTest>

class CameraBrowserPolicyTest final : public QObject
{
    Q_OBJECT
private slots:
    void allowsCameraHttpAndHttpsNavigation();
    void rejectsCrossHostAndUnsafeNavigation();
    void allowsSameHostResourcesAndWebSockets();
    void rejectsCrossHostResources();
};

void CameraBrowserPolicyTest::allowsCameraHttpAndHttpsNavigation()
{
    QVERIFY(CameraBrowserPolicy::allowsMainFrameNavigation(
        QUrl(QStringLiteral("http://192.168.1.20/setup")), QStringLiteral("192.168.1.20")));
    QVERIFY(CameraBrowserPolicy::allowsMainFrameNavigation(
        QUrl(QStringLiteral("https://192.168.1.20:8443/login")), QStringLiteral("192.168.1.20")));
}

void CameraBrowserPolicyTest::rejectsCrossHostAndUnsafeNavigation()
{
    QVERIFY(!CameraBrowserPolicy::allowsMainFrameNavigation(
        QUrl(QStringLiteral("https://example.com/")), QStringLiteral("192.168.1.20")));
    QVERIFY(!CameraBrowserPolicy::allowsMainFrameNavigation(
        QUrl(QStringLiteral("file:///etc/passwd")), QStringLiteral("192.168.1.20")));
    QVERIFY(!CameraBrowserPolicy::allowsMainFrameNavigation(
        QUrl(QStringLiteral("javascript:alert(1)")), QStringLiteral("192.168.1.20")));
    QVERIFY(!CameraBrowserPolicy::allowsMainFrameNavigation(
        QUrl(QStringLiteral("http://admin:secret@192.168.1.20/")), QStringLiteral("192.168.1.20")));
    QUrl invalidPort(QStringLiteral("http://192.168.1.20/"));
    invalidPort.setPort(70000);
    QVERIFY(!CameraBrowserPolicy::allowsMainFrameNavigation(invalidPort, QStringLiteral("192.168.1.20")));
}

void CameraBrowserPolicyTest::allowsSameHostResourcesAndWebSockets()
{
    QVERIFY(CameraBrowserPolicy::allowsResourceRequest(
        QUrl(QStringLiteral("https://192.168.1.20/assets/app.js")), QStringLiteral("192.168.1.20")));
    QVERIFY(CameraBrowserPolicy::allowsResourceRequest(
        QUrl(QStringLiteral("ws://192.168.1.20/live")), QStringLiteral("192.168.1.20")));
    QVERIFY(CameraBrowserPolicy::allowsResourceRequest(
        QUrl(QStringLiteral("data:image/png;base64,AA==")), QStringLiteral("192.168.1.20")));
}

void CameraBrowserPolicyTest::rejectsCrossHostResources()
{
    QVERIFY(!CameraBrowserPolicy::allowsResourceRequest(
        QUrl(QStringLiteral("https://example.com/tracker.js")), QStringLiteral("192.168.1.20")));
    QVERIFY(!CameraBrowserPolicy::allowsResourceRequest(
        QUrl(QStringLiteral("file:///tmp/config")), QStringLiteral("192.168.1.20")));
}

QTEST_MAIN(CameraBrowserPolicyTest)
#include "camera_browser_policy_test.moc"
