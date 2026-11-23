#include "ui/main_window.h"

#include <QStandardPaths>
#include <QTabWidget>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include "discovery/discovery_service.h"
#include <QtTest>

class MainWindowSmokeTest final : public QObject
{
    Q_OBJECT
private slots:
    void startupShowsOperationHistory();
    void rejectsOversizedOnvifResponseDuringReceive();
};

void MainWindowSmokeTest::startupShowsOperationHistory()
{
    QStandardPaths::setTestModeEnabled(true);
    MainWindow window;
    QVERIFY(!window.windowTitle().isEmpty());
    const auto tabs = window.findChild<QTabWidget *>();
    QVERIFY(tabs);
    bool historyFound = false;
    for (int index = 0; index < tabs->count(); ++index) {
        if (tabs->tabText(index) == QStringLiteral("Operation History")) {
            historyFound = true;
            break;
        }
    }
    QVERIFY2(historyFound, "The operation-history workspace must be present at startup.");
    const QSqlDatabase database = QSqlDatabase::database(QStringLiteral("camera_inventory"), false);
    QVERIFY2(database.isValid() && database.isOpen(), "The local inventory database should open in test mode.");
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT name FROM sqlite_master WHERE type=:type AND name=:name"));
    query.bindValue(":type", QStringLiteral("table"));
    query.bindValue(":name", QStringLiteral("operation_history"));
    QVERIFY(query.exec());
    QVERIFY(query.next());
}


void MainWindowSmokeTest::rejectsOversizedOnvifResponseDuringReceive()
{
    QTcpServer camera;
    QVERIFY(camera.listen(QHostAddress::LocalHost, quint16(0)));
    connect(&camera, &QTcpServer::newConnection, &camera, [&camera] {
        while (camera.hasPendingConnections()) {
            QTcpSocket *peer = camera.nextPendingConnection();
            if (!peer) continue;
            connect(peer, &QTcpSocket::readyRead, peer, [peer] { peer->readAll(); });
            QTimer::singleShot(50, peer, [peer] {
                const QByteArray body(2 * 1024 * 1024, 'x');
                QByteArray response = QByteArrayLiteral("HTTP/1.1 200 OK\\r\\n")
                    + QByteArrayLiteral("Content-Type: application/soap+xml\\r\\n")
                    + QByteArrayLiteral("Content-Length: ") + QByteArray::number(body.size())
                    + QByteArrayLiteral("\\r\\nConnection: close\\r\\n\\r\\n");
                peer->write(response);
                peer->write(body);
                peer->flush();
            });
        }
    });

    DiscoveryService discovery;
    QSignalSpy statuses(&discovery, &DiscoveryService::statusChanged);
    DeviceRecord device;
    device.ipAddress = QStringLiteral("127.0.0.1");
    device.deviceServiceUrl = QStringLiteral("http://127.0.0.1:%1/onvif/device_service")
                                  .arg(camera.serverPort());
    device.onvifEndpoint = QStringLiteral("urn:uuid:oversized-response-test");
    device.discoverySources << QStringLiteral("ONVIF WS-Discovery");
    discovery.refreshDeviceInfo(device);

    QTRY_VERIFY_WITH_TIMEOUT([&statuses] {
        for (const QList<QVariant> &event : statuses)
            if (!event.isEmpty() && event.first().toString().contains(QStringLiteral("1 MiB")))
                return true;
        return false;
    }(), 3000);
}

QTEST_MAIN(MainWindowSmokeTest)
#include "main_window_smoke_test.moc"
