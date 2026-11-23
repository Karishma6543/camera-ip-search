#include "ui/main_window.h"

#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QTabWidget>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QRegularExpression>
#include "discovery/discovery_service.h"
#include <QtTest>

class MainWindowSmokeTest final : public QObject
{
    Q_OBJECT
private slots:
    void startupShowsOperationHistory();
    void migratesLegacyInventorySchema();
    void rollsBackFailedInventoryMigration();
    void rejectsOversizedOnvifResponseDuringReceive();
    void discoveryServiceEnrichesDeviceFromLocalOnvifPeer();
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

    QSqlQuery version(database);
    QVERIFY(version.exec(QStringLiteral("PRAGMA user_version")));
    QVERIFY(version.next());
    QCOMPARE(version.value(0).toInt(), 1);
}



void MainWindowSmokeTest::migratesLegacyInventorySchema()
{
    QStandardPaths::setTestModeEnabled(true);
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QVERIFY(QDir(appData).removeRecursively() || !QFileInfo::exists(appData));
    QVERIFY(QDir().mkpath(appData));
    const QString databasePath = appData + QStringLiteral("/inventory.sqlite");

    {
        QSqlDatabase seed = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                       QStringLiteral("legacy_seed"));
        seed.setDatabaseName(databasePath);
        QVERIFY(seed.open());
        QSqlQuery create(seed);
        QVERIFY(create.exec(QStringLiteral(
            "CREATE TABLE devices (device_key TEXT PRIMARY KEY, ip TEXT, mac TEXT, "
            "manufacturer TEXT, model TEXT, firmware TEXT, serial TEXT, hardware TEXT, "
            "activation TEXT, source TEXT, is_candidate INTEGER, last_seen TEXT)")));
        seed.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("legacy_seed"));

    MainWindow window;
    QSqlDatabase database = QSqlDatabase::database(QStringLiteral("camera_inventory"), false);
    QVERIFY(database.isValid() && database.isOpen());
    QSqlQuery version(database);
    QVERIFY(version.exec(QStringLiteral("PRAGMA user_version")));
    QVERIFY(version.next());
    QCOMPARE(version.value(0).toInt(), 1);

    QSqlQuery columns(database);
    QVERIFY(columns.exec(QStringLiteral("PRAGMA table_info(devices)")));
    QStringList names;
    while (columns.next()) names << columns.value(1).toString();
    QVERIFY(names.contains(QStringLiteral("user_label")));
    QVERIFY(names.contains(QStringLiteral("network_interface_token")));
    QVERIFY(names.contains(QStringLiteral("ipv4_config_known")));
}

void MainWindowSmokeTest::rollsBackFailedInventoryMigration()
{
    QStandardPaths::setTestModeEnabled(true);
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QVERIFY(QDir(appData).removeRecursively() || !QFileInfo::exists(appData));
    QVERIFY(QDir().mkpath(appData));
    const QString databasePath = appData + QStringLiteral("/inventory.sqlite");

    {
        QSqlDatabase seed = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                       QStringLiteral("failed_migration_seed"));
        seed.setDatabaseName(databasePath);
        QVERIFY(seed.open());
        QSqlQuery create(seed);
        QVERIFY(create.exec(QStringLiteral("CREATE VIEW devices AS SELECT 1 AS device_key")));
        seed.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("failed_migration_seed"));

    MainWindow window;
    QSqlDatabase database = QSqlDatabase::database(QStringLiteral("camera_inventory"), false);
    QVERIFY(database.isValid());
    if (!database.isOpen()) QVERIFY(database.open());
    QSqlQuery version(database);
    QVERIFY(version.exec(QStringLiteral("PRAGMA user_version")));
    QVERIFY(version.next());
    QCOMPARE(version.value(0).toInt(), 0);

    QSqlQuery history(database);
    QVERIFY(history.exec(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='operation_history'")));
    QVERIFY(!history.next());
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
                QByteArray response = QByteArrayLiteral("HTTP/1.1 200 OK\r\n")
                    + QByteArrayLiteral("Content-Type: application/soap+xml\r\n")
                    + QByteArrayLiteral("Content-Length: ") + QByteArray::number(body.size())
                    + QByteArrayLiteral("\r\nConnection: close\r\n\r\n");
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


void MainWindowSmokeTest::discoveryServiceEnrichesDeviceFromLocalOnvifPeer()
{
    QTcpServer camera;
    QVERIFY(camera.listen(QHostAddress::LocalHost, quint16(0)));
    connect(&camera, &QTcpServer::newConnection, &camera, [&camera] {
        while (camera.hasPendingConnections()) {
            QTcpSocket *peer = camera.nextPendingConnection();
            if (!peer) continue;
            connect(peer, &QTcpSocket::readyRead, peer, [peer] {
                QByteArray request = peer->property("requestBuffer").toByteArray();
                request.append(peer->readAll());
                peer->setProperty("requestBuffer", request);
                const qsizetype headerEnd = request.indexOf(QByteArrayLiteral("\r\n\r\n"));
                if (headerEnd < 0) return;
                const QByteArray headers = request.left(headerEnd);
                const QRegularExpression lengthPattern(
                    QStringLiteral("Content-Length\\s*:\\s*(\\d+)"),
                    QRegularExpression::CaseInsensitiveOption);
                const auto match = lengthPattern.match(QString::fromLatin1(headers));
                const qsizetype bodyLength = match.hasMatch() ? match.captured(1).toLongLong() : 0;
                if (request.size() < headerEnd + 4 + bodyLength) return;

                const QByteArray body = QByteArrayLiteral(
                    "<s:Envelope xmlns:s='http://www.w3.org/2003/05/soap-envelope' "
                    "xmlns:tds='http://www.onvif.org/ver10/device/wsdl'><s:Body>"
                    "<tds:GetDeviceInformationResponse>"
                    "<tds:Manufacturer>Loopback Camera</tds:Manufacturer>"
                    "<tds:Model>Integration Fixture</tds:Model>"
                    "<tds:FirmwareVersion>test-1</tds:FirmwareVersion>"
                    "<tds:SerialNumber>LOOP-001</tds:SerialNumber>"
                    "<tds:HardwareId>HW-001</tds:HardwareId>"
                    "</tds:GetDeviceInformationResponse></s:Body></s:Envelope>");
                const QByteArray response = QByteArrayLiteral("HTTP/1.1 200 OK\r\n")
                    + QByteArrayLiteral("Content-Type: application/soap+xml; charset=utf-8\r\n")
                    + QByteArrayLiteral("Content-Length: ") + QByteArray::number(body.size())
                    + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + body;
                peer->write(response);
                peer->disconnectFromHost();
            });
        }
    });

    DiscoveryService discovery;
    DeviceRecord enriched;
    bool receivedDevice = false;
    connect(&discovery, &DiscoveryService::deviceFound, &discovery,
            [&enriched, &receivedDevice](const DeviceRecord &device) {
        enriched = device;
        receivedDevice = true;
    });

    DeviceRecord candidate;
    candidate.ipAddress = QStringLiteral("127.0.0.1");
    candidate.deviceServiceUrl = QStringLiteral("http://127.0.0.1:%1/onvif/device_service")
                                     .arg(camera.serverPort());
    candidate.onvifEndpoint = QStringLiteral("urn:uuid:local-onvif-integration");
    candidate.discoverySources << QStringLiteral("ONVIF WS-Discovery");
    discovery.refreshDeviceInfo(candidate);

    QTRY_VERIFY_WITH_TIMEOUT(receivedDevice, 3000);
    QCOMPARE(enriched.manufacturer, QStringLiteral("Loopback Camera"));
    QCOMPARE(enriched.model, QStringLiteral("Integration Fixture"));
    QCOMPARE(enriched.firmwareVersion, QStringLiteral("test-1"));
    QCOMPARE(enriched.serialNumber, QStringLiteral("LOOP-001"));
    QCOMPARE(enriched.hardwareId, QStringLiteral("HW-001"));
}

QTEST_MAIN(MainWindowSmokeTest)
#include "main_window_smoke_test.moc"
