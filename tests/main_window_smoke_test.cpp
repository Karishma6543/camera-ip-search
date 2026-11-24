#include "ui/main_window.h"

#include <QStandardPaths>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QTabWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QPixmap>
#include <QTableWidget>
#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QScrollArea>
#include <QProgressBar>
#include <QSplitter>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QRegularExpression>
#include <QPointer>
#include "discovery/discovery_service.h"
#include <QtTest>

class MainWindowSmokeTest final : public QObject
{
    Q_OBJECT
private slots:
    void startupShowsOperationHistory();
    void officialOxitaLogoIsBundledAndDisplayed();
    void exposesAllWorkspacesAndAutomationControls();
    void inventoryStartsWithGuidanceAndCompactColumns();
    void guidedManualEntryAndRenameDialogsWork();
    void searchAndManufacturerFiltersHideNonMatchingRows();
    void discoveryServiceStartsAndCancelsOnSelectedPhysicalInterface();
    void leavesCorruptInventoryDatabaseUntouched();
    void migratesLegacyInventorySchema();
    void rollsBackFailedInventoryMigration();
    void rejectsOversizedOnvifResponseDuringReceive();
    void discoveryServiceEnrichesDeviceFromLocalOnvifPeer();
    void shutdownAbortsActiveOnvifEnrichment();
    void sendsAndVerifiesOnvifFactoryResetRequest();
};

void MainWindowSmokeTest::officialOxitaLogoIsBundledAndDisplayed()
{
    QStandardPaths::setTestModeEnabled(true);
    MainWindow window;
    const QPixmap logo(QStringLiteral(":/branding/oxita-logo.png"));
    QVERIFY2(!logo.isNull(), "The official Oxita logo must be embedded in the application resources.");
    const auto *logoLabel = window.findChild<QLabel *>(QStringLiteral("brandLogo"));
    QVERIFY(logoLabel);
    const QPixmap displayed = logoLabel->pixmap(Qt::ReturnByValue);
    QVERIFY2(!displayed.isNull(), "The application header must display the bundled Oxita logo.");
}

void MainWindowSmokeTest::startupShowsOperationHistory()
{
    QStandardPaths::setTestModeEnabled(true);
    MainWindow window;
    QVERIFY(!window.windowTitle().isEmpty());
    const auto tabs = window.findChild<QTabWidget *>();
    QVERIFY(tabs);
    bool historyFound = false;
    for (int index = 0; index < tabs->count(); ++index) {
        if (tabs->tabText(index) == QStringLiteral("Operation history")) {
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



void MainWindowSmokeTest::exposesAllWorkspacesAndAutomationControls()
{
    QStandardPaths::setTestModeEnabled(true);
    MainWindow window;
    const auto tabs = window.findChild<QTabWidget *>(QStringLiteral("workspaceTabs"));
    QVERIFY(tabs);
    const QStringList expectedTabs{
        QStringLiteral("Device details"), QStringLiteral("Device actions"),
        QStringLiteral("Operation history")
    };
    QCOMPARE(tabs->count(), expectedTabs.size());
    for (int index = 0; index < expectedTabs.size(); ++index) {
        QCOMPARE(tabs->tabText(index), expectedTabs.at(index));
        tabs->setCurrentIndex(index);
        QCOMPARE(tabs->currentIndex(), index);
        QVERIFY(tabs->widget(index));
    }

    QVERIFY(window.findChild<QLineEdit *>(QStringLiteral("deviceSearch")));
    QVERIFY(window.findChild<QComboBox *>(QStringLiteral("interfaceSelector")));
    QVERIFY(window.findChild<QComboBox *>(QStringLiteral("manufacturerFilter")));
    QVERIFY(window.findChild<QComboBox *>(QStringLiteral("statusFilter")));
    QVERIFY(window.findChild<QTableWidget *>(QStringLiteral("deviceTable")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("addDeviceButton")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("exportCsvButton")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("renameLabelButton")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("reachabilityButton")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("subnetScanButton")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("discoverButton")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("backupInventoryButton")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("restoreInventoryButton")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("factoryResetCameraButton")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("bulkIpv4ConfigureButton")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("applyStaticIpv4Button")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("previewStaticIpv4Button")));
    QVERIFY(!window.findChild<QPushButton *>(QStringLiteral("factoryResetCameraButton"))->isEnabled());
    QVERIFY(!window.findChild<QPushButton *>(QStringLiteral("applyStaticIpv4Button"))->isEnabled());

    auto *reachability = window.findChild<QPushButton *>(QStringLiteral("reachabilityButton"));
    QTest::mouseClick(reachability, Qt::LeftButton);
    const auto status = window.findChild<QLabel *>(QStringLiteral("applicationStatus"));
    QVERIFY(status);
    QVERIFY(status->text().contains(QStringLiteral("Add or discover devices")));
}

void MainWindowSmokeTest::inventoryStartsWithGuidanceAndCompactColumns()
{
    QStandardPaths::setTestModeEnabled(true);
    MainWindow window;
    const auto *table = window.findChild<QTableWidget *>(QStringLiteral("deviceTable"));
    const auto *emptyState = window.findChild<QLabel *>(QStringLiteral("inventoryEmptyState"));
    QVERIFY(table);
    QVERIFY(emptyState);
    QVERIFY(!emptyState->isHidden());
    QVERIFY(emptyState->text().contains(QStringLiteral("Discover cameras")));
    QVERIFY(table->isColumnHidden(5));
    QVERIFY(table->isColumnHidden(6));
    QVERIFY(table->isColumnHidden(8));
    QVERIFY(table->isColumnHidden(9));
    QVERIFY(table->isColumnHidden(10));
    QVERIFY(!table->isColumnHidden(1));
    QVERIFY(!table->isColumnHidden(2));
    QVERIFY(!table->isColumnHidden(3));
    QVERIFY(!table->isColumnHidden(4));
    QVERIFY(!table->isColumnHidden(7));
    QVERIFY(!table->isColumnHidden(11));
    QVERIFY(table->horizontalHeader()->sectionsClickable());
    QCOMPARE(table->horizontalHeader()->contextMenuPolicy(), Qt::CustomContextMenu);
    QVERIFY(window.findChild<QProgressBar *>(QStringLiteral("operationProgress")));
    auto *workspace = window.findChild<QSplitter *>(QStringLiteral("deviceWorkspaceSplitter"));
    auto *detailsScroll = window.findChild<QScrollArea *>(QStringLiteral("deviceDetailsScrollArea"));
    QVERIFY(workspace);
    QVERIFY(detailsScroll);
    QVERIFY(workspace->count() == 2);
    QVERIFY(window.styleSheet().contains(QStringLiteral("#C62836")));
}

void MainWindowSmokeTest::guidedManualEntryAndRenameDialogsWork()
{
    QStandardPaths::setTestModeEnabled(true);
    MainWindow window;
    auto *table = window.findChild<QTableWidget *>(QStringLiteral("deviceTable"));
    auto *addButton = window.findChild<QPushButton *>(QStringLiteral("addDeviceButton"));
    auto *renameButton = window.findChild<QPushButton *>(QStringLiteral("renameLabelButton"));
    QVERIFY(table && addButton && renameButton);

    QTimer::singleShot(0, &window, [&window] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        auto *address = dialog->findChild<QLineEdit *>(QStringLiteral("manualAddress"));
        auto *name = dialog->findChild<QLineEdit *>(QStringLiteral("manualDeviceName"));
        auto *save = dialog->findChild<QPushButton *>(QStringLiteral("saveManualDeviceButton"));
        QVERIFY(address && name && save);
        address->setText(QStringLiteral("192.168.10.25"));
        name->setText(QStringLiteral("Front entrance"));
        QTest::mouseClick(save, Qt::LeftButton);
    });
    QTest::mouseClick(addButton, Qt::LeftButton);
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("192.168.10.25"));
    QCOMPARE(table->item(0, 2)->text(), QStringLiteral("Front entrance"));

    table->selectRow(0);
    table->setCurrentCell(0, 1);
    QTimer::singleShot(0, &window, [&window] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        auto *name = dialog->findChild<QLineEdit *>(QStringLiteral("deviceFriendlyName"));
        auto *save = dialog->findChild<QPushButton *>(QStringLiteral("saveDeviceNameButton"));
        QVERIFY(name && save);
        name->setText(QStringLiteral("Loading bay"));
        QTest::mouseClick(save, Qt::LeftButton);
    });
    QTest::mouseClick(renameButton, Qt::LeftButton);
    QCOMPARE(table->item(0, 2)->text(), QStringLiteral("Loading bay"));
}

void MainWindowSmokeTest::searchAndManufacturerFiltersHideNonMatchingRows()
{
    QStandardPaths::setTestModeEnabled(true);
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QVERIFY(QDir(appData).removeRecursively() || !QFileInfo::exists(appData));
    QVERIFY(QDir().mkpath(appData));
    const QString databasePath = appData + QStringLiteral("/inventory.sqlite");
    {
        QSqlDatabase seed = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                       QStringLiteral("filter_seed"));
        seed.setDatabaseName(databasePath);
        QVERIFY(seed.open());
        QSqlQuery create(seed);
        QVERIFY(create.exec(QStringLiteral(
            "CREATE TABLE devices (device_key TEXT PRIMARY KEY, ip TEXT, mac TEXT, "
            "manufacturer TEXT, model TEXT, firmware TEXT, serial TEXT, hardware TEXT, "
            "activation TEXT, source TEXT, is_candidate INTEGER, last_seen TEXT)")));
        QSqlQuery insert(seed);
        insert.prepare(QStringLiteral(
            "INSERT INTO devices(device_key,ip,mac,manufacturer,model,firmware,serial,hardware,"
            "activation,source,is_candidate) VALUES(?,?,?,?,?,?,?,?,?,?,?)"));
        insert.addBindValue(QStringLiteral("cam-a"));
        insert.addBindValue(QStringLiteral("192.0.2.10"));
        insert.addBindValue(QStringLiteral("02:00:00:00:00:10"));
        insert.addBindValue(QStringLiteral("Acme"));
        insert.addBindValue(QStringLiteral("A-100"));
        insert.addBindValue(QStringLiteral("1.0"));
        insert.addBindValue(QString(180, QLatin1Char('S')));
        insert.addBindValue(QStringLiteral("HW-A"));
        insert.addBindValue(QStringLiteral("Activated"));
        insert.addBindValue(QStringLiteral("ONVIF WS-Discovery"));
        insert.addBindValue(0);
        QVERIFY(insert.exec());
        seed.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("filter_seed"));

    MainWindow window;
    auto *table = window.findChild<QTableWidget *>(QStringLiteral("deviceTable"));
    auto *search = window.findChild<QLineEdit *>(QStringLiteral("deviceSearch"));
    auto *vendor = window.findChild<QComboBox *>(QStringLiteral("manufacturerFilter"));
    auto *statusFilter = window.findChild<QComboBox *>(QStringLiteral("statusFilter"));
    QVERIFY(table && search && vendor && statusFilter);
    QCOMPARE(table->rowCount(), 1);
    QVERIFY(!table->isRowHidden(0));
    table->selectRow(0);
    table->setCurrentCell(0, 1);
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("factoryResetCameraButton"))->isEnabled());
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("applyStaticIpv4Button"))->isEnabled());
    const auto *deviceDetails = window.findChild<QLabel *>(QStringLiteral("selectedDeviceDetails"));
    QVERIFY(deviceDetails);
    QVERIFY(deviceDetails->text().contains(QStringLiteral("Acme")));
    QVERIFY(deviceDetails->text().contains(QStringLiteral("A-100")));
    const auto *fullProfile = window.findChild<QLabel *>(QStringLiteral("deviceDetails"));
    QVERIFY(fullProfile);
    QVERIFY(fullProfile->text().contains(QStringLiteral("02:00:00:00:00:10")));
    QVERIFY(fullProfile->text().contains(QStringLiteral("FIRMWARE")));
    QVERIFY(fullProfile->text().contains(QStringLiteral("SERIAL NUMBER")));
    QVERIFY(fullProfile->text().contains(QStringLiteral("HARDWARE ID")));
    QVERIFY(fullProfile->text().contains(QStringLiteral("Last reachability check")));
    QVERIFY(fullProfile->text().contains(QStringLiteral("Last successful response")));
    QVERIFY(fullProfile->text().contains(QString(180, QLatin1Char('S'))));
    table->clearSelection();
    table->setCurrentCell(-1, -1);
    QVERIFY(!window.findChild<QPushButton *>(QStringLiteral("factoryResetCameraButton"))->isEnabled());
    QVERIFY(!window.findChild<QPushButton *>(QStringLiteral("applyStaticIpv4Button"))->isEnabled());
    QVERIFY(fullProfile->text().contains(QStringLiteral("No device selected")));
    table->selectRow(0);
    table->setCurrentCell(0, 1);

    search->setText(QStringLiteral("does-not-exist"));
    QVERIFY(table->isRowHidden(0));
    QVERIFY(fullProfile->text().contains(QStringLiteral("No device selected")));
    const auto *emptyState = window.findChild<QLabel *>(QStringLiteral("inventoryEmptyState"));
    QVERIFY(emptyState && !emptyState->isHidden());
    QVERIFY(emptyState->text().contains(QStringLiteral("No devices match")));
    search->clear();
    QVERIFY(!table->isRowHidden(0));
    QVERIFY(!!emptyState->isHidden());

    vendor->setCurrentText(QStringLiteral("Oxita devices"));
    QVERIFY(table->isRowHidden(0));
    vendor->setCurrentText(QStringLiteral("ONVIF devices"));
    QVERIFY(!table->isRowHidden(0));

    statusFilter->setCurrentText(QStringLiteral("Not activated"));
    QVERIFY(table->isRowHidden(0));
    statusFilter->setCurrentText(QStringLiteral("Activated"));
    QVERIFY(!table->isRowHidden(0));
}

void MainWindowSmokeTest::discoveryServiceStartsAndCancelsOnSelectedPhysicalInterface()
{
    QNetworkInterface selected;
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning) ||
            flags.testFlag(QNetworkInterface::IsLoopBack))
            continue;
        bool hasIpv4 = false;
        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
                !entry.ip().isLoopback()) {
                hasIpv4 = true;
                break;
            }
        }
        if (hasIpv4) { selected = interface; break; }
    }
    if (!selected.isValid())
        QSKIP("No active non-loopback IPv4 interface is available on this runner.");

    DiscoveryService service;
    QSignalSpy cancelled(&service, &DiscoveryService::cancelled);
    QSignalSpy statuses(&service, &DiscoveryService::statusChanged);
    service.start(selected.name());
    QVERIFY2(service.isRunning(), "Discovery should bind its selected interface before sending probes.");
    bool probeStatusSeen = false;
    for (const QList<QVariant> &event : statuses) {
        if (!event.isEmpty() &&
            event.first().toString().contains(QStringLiteral("Sending ONVIF and project-camera"))) {
            probeStatusSeen = true;
            break;
        }
    }
    QVERIFY(probeStatusSeen);
    service.cancel();
    QVERIFY(!service.isRunning());
    QCOMPARE(cancelled.count(), 1);
}

void MainWindowSmokeTest::leavesCorruptInventoryDatabaseUntouched()
{
    QStandardPaths::setTestModeEnabled(true);
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QVERIFY(QDir(appData).removeRecursively() || !QFileInfo::exists(appData));
    QVERIFY(QDir().mkpath(appData));
    const QString databasePath = appData + QStringLiteral("/inventory.sqlite");
    const QByteArray corruptBytes = QByteArrayLiteral("this is deliberately not a sqlite database");
    QFile corrupt(databasePath);
    QVERIFY(corrupt.open(QIODevice::WriteOnly));
    QCOMPARE(corrupt.write(corruptBytes), qint64(corruptBytes.size()));
    corrupt.close();

    MainWindow window;
    QFile preserved(databasePath);
    QVERIFY(preserved.open(QIODevice::ReadOnly));
    QCOMPARE(preserved.readAll(), corruptBytes);
    const auto status = window.findChild<QLabel *>(QStringLiteral("applicationStatus"));
    QVERIFY(status);
    QVERIFY(status->text().contains(QStringLiteral("integrity check failed"), Qt::CaseInsensitive) ||
            status->text().contains(QStringLiteral("Could not open local inventory"), Qt::CaseInsensitive));
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


void MainWindowSmokeTest::sendsAndVerifiesOnvifFactoryResetRequest()
{
    QTcpServer camera;
    QVERIFY(camera.listen(QHostAddress::LocalHost, quint16(0)));
    bool requestSeen = false;
    connect(&camera, &QTcpServer::newConnection, &camera, [&camera, &requestSeen] {
        while (camera.hasPendingConnections()) {
            QTcpSocket *peer = camera.nextPendingConnection();
            connect(peer, &QTcpSocket::readyRead, peer, [peer, &requestSeen] {
                QByteArray request = peer->property("requestBuffer").toByteArray();
                request.append(peer->readAll());
                peer->setProperty("requestBuffer", request);
                const qsizetype headerEnd = request.indexOf(QByteArrayLiteral("\r\n\r\n"));
                if (headerEnd < 0) return;
                const QRegularExpression lengthPattern(QStringLiteral("Content-Length\\s*:\\s*(\\d+)"), QRegularExpression::CaseInsensitiveOption);
                const auto match = lengthPattern.match(QString::fromLatin1(request.left(headerEnd)));
                const qsizetype bodyLength = match.hasMatch() ? match.captured(1).toLongLong() : 0;
                if (request.size() < headerEnd + 4 + bodyLength) return;
                const QByteArray body = request.mid(headerEnd + 4, bodyLength);
                if (!body.contains("SetSystemFactoryDefault") || !body.contains("<tds:FactoryDefault>Soft</tds:FactoryDefault>")) return;
                requestSeen = true;
                const QByteArray responseBody = QByteArrayLiteral("<s:Envelope xmlns:s='urn:soap' xmlns:tds='urn:onvif'><s:Body><tds:SetSystemFactoryDefaultResponse/></s:Body></s:Envelope>");
                const QByteArray response = QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Type: application/soap+xml\r\nContent-Length: ") + QByteArray::number(responseBody.size()) + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + responseBody;
                peer->write(response);
                peer->disconnectFromHost();
            });
        }
    });

    DiscoveryService service;
    bool finished = false;
    bool succeeded = false;
    QString resultMessage;
    connect(&service, &DiscoveryService::maintenanceOperationFinished, &service,
            [&finished, &succeeded, &resultMessage](const DeviceRecord &, const QString &,
                                                    bool ok, const QString &message) {
        finished = true;
        succeeded = ok;
        resultMessage = message;
    });
    DeviceRecord device;
    device.ipAddress = QStringLiteral("127.0.0.1");
    device.deviceServiceUrl = QStringLiteral("http://127.0.0.1:%1/onvif/device_service").arg(camera.serverPort());
    device.onvifEndpoint = QStringLiteral("urn:uuid:factory-reset-test");
    device.discoverySources << QStringLiteral("ONVIF WS-Discovery");
    service.factoryReset(device, QStringLiteral("Soft"));
    QTRY_VERIFY_WITH_TIMEOUT(finished, 3000);
    QVERIFY(requestSeen);
    QVERIFY(succeeded);
    QVERIFY(resultMessage.contains(QStringLiteral("accepted"), Qt::CaseInsensitive));
}
void MainWindowSmokeTest::shutdownAbortsActiveOnvifEnrichment()
{
    QStandardPaths::setTestModeEnabled(true);
    QTcpServer camera;
    QVERIFY(camera.listen(QHostAddress::LocalHost, quint16(0)));
    QPointer<QTcpSocket> acceptedPeer;
    connect(&camera, &QTcpServer::newConnection, &camera, [&camera, &acceptedPeer] {
        while (camera.hasPendingConnections())
            acceptedPeer = camera.nextPendingConnection();
    });

    auto *window = new MainWindow;
    auto *discovery = window->findChild<DiscoveryService *>();
    QVERIFY(discovery);
    DeviceRecord device;
    device.ipAddress = QStringLiteral("127.0.0.1");
    device.deviceServiceUrl = QStringLiteral("http://127.0.0.1:%1/onvif/device_service")
                                  .arg(camera.serverPort());
    device.onvifEndpoint = QStringLiteral("urn:uuid:shutdown-cancel-test");
    device.discoverySources << QStringLiteral("ONVIF WS-Discovery");
    discovery->refreshDeviceInfo(device);

    QTRY_VERIFY_WITH_TIMEOUT(!acceptedPeer.isNull(), 3000);
    delete window;
    QTRY_VERIFY_WITH_TIMEOUT(acceptedPeer.isNull() ||
                             acceptedPeer->state() == QAbstractSocket::UnconnectedState, 3000);
}

QTEST_MAIN(MainWindowSmokeTest)
#include "main_window_smoke_test.moc"
