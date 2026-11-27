#include "ui/main_window.h"

#include <QStandardPaths>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QTabWidget>
#include <QTabBar>
#include <QStackedWidget>
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
#include <QHeaderView>
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
    void inventorySchemaDoesNotPersistCameraCredentials();
    void officialOxitaLogoIsBundledAndDisplayed();
    void exposesAllWorkspacesAndAutomationControls();
    void inventoryStartsWithGuidanceAndCompactColumns();
    void guidedManualEntryAndRenameDialogsWork();
    void exportsInventoryAsUtf8CsvWithEscapedFields();
    void searchAndManufacturerFiltersHideNonMatchingRows();
    void discoveryServiceStartsAndCancelsOnSelectedPhysicalInterface();
    void leavesCorruptInventoryDatabaseUntouched();
    void migratesLegacyInventorySchema();
    void rollsBackFailedInventoryMigration();
    void rejectsOversizedOnvifResponseDuringReceive();
    void discoveryServiceEnrichesDeviceFromLocalOnvifPeer();
    void discoveryServiceIgnoresUnrelatedSoapResponse();
    void discoveryServiceRequestsAndRecordsOnvifServices();
    void discoveryServiceRetriesHttpBasicAuthenticationChallenge();
    void discoveryServiceRetriesWsseUsernameTokenChallenge();
    void discoveryServiceRetriesWsseChallengeForGetServices();
    void discoveryServiceDoesNotFollowSoapRedirects();
    void discoveryServiceReportsSoapRequestTimeout();
    void discoveryServiceReportsSoapFaults();
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
    const auto tabs = window.findChild<QTabBar *>(QStringLiteral("workspaceTabs"));
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



void MainWindowSmokeTest::inventorySchemaDoesNotPersistCameraCredentials()
{
    QStandardPaths::setTestModeEnabled(true);
    MainWindow window;
    const QSqlDatabase database = QSqlDatabase::database(QStringLiteral("camera_inventory"), false);
    QVERIFY(database.isValid() && database.isOpen());

    const QStringList tables{QStringLiteral("devices"), QStringLiteral("operation_history")};
    const QStringList forbiddenFragments{
        QStringLiteral("username"), QStringLiteral("password"),
        QStringLiteral("credential"), QStringLiteral("secret"),
        QStringLiteral("authorization")
    };
    for (const QString &table : tables) {
        QSqlQuery columns(database);
        QVERIFY(columns.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table)));
        while (columns.next()) {
            const QString column = columns.value(1).toString().toLower();
            for (const QString &fragment : forbiddenFragments)
                QVERIFY2(!column.contains(fragment),
                         qPrintable(QStringLiteral("Sensitive credential column found: %1.%2")
                                        .arg(table, column)));
        }
    }
}

void MainWindowSmokeTest::exposesAllWorkspacesAndAutomationControls()
{
    QStandardPaths::setTestModeEnabled(true);
    MainWindow window;
    const auto tabs = window.findChild<QTabBar *>(QStringLiteral("workspaceTabs"));
    QVERIFY(tabs);
    const QStringList expectedTabs{
        QStringLiteral("Device Info"), QStringLiteral("Configuration"),
        QStringLiteral("Operation History")
    };
    QCOMPARE(tabs->count(), expectedTabs.size());
    auto *workspacePages = window.findChild<QStackedWidget *>(QStringLiteral("workspacePages"));
    QVERIFY(workspacePages);
    QCOMPARE(workspacePages->count(), expectedTabs.size());
    auto *configurationTabs = window.findChild<QTabWidget *>(QStringLiteral("configurationTabs"));
    QVERIFY(configurationTabs);
    const QStringList expectedConfigurationTabs{
        QStringLiteral("Device"), QStringLiteral("Network"), QStringLiteral("Storage")
    };
    QCOMPARE(configurationTabs->count(), expectedConfigurationTabs.size());
    for (int index = 0; index < expectedConfigurationTabs.size(); ++index)
        QCOMPARE(configurationTabs->tabText(index), expectedConfigurationTabs.at(index));
    for (int index = 0; index < expectedTabs.size(); ++index) {
        QCOMPARE(tabs->tabText(index), expectedTabs.at(index));
        tabs->setCurrentIndex(index);
        QCOMPARE(tabs->currentIndex(), index);
        QCOMPARE(workspacePages->currentIndex(), index);
        QVERIFY(workspacePages->widget(index));
    }

    QVERIFY(window.findChild<QLineEdit *>(QStringLiteral("deviceSearch")));
    QVERIFY(window.findChild<QComboBox *>(QStringLiteral("interfaceSelector")));
    QVERIFY(window.findChild<QComboBox *>(QStringLiteral("manufacturerFilter")));
    QVERIFY(window.findChild<QComboBox *>(QStringLiteral("deviceTypeFilter")));
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
    QVERIFY(!table->isColumnHidden(12));
    QVERIFY(table->horizontalHeader()->sectionsClickable());
    QCOMPARE(table->horizontalHeader()->contextMenuPolicy(), Qt::CustomContextMenu);
    QVERIFY(window.findChild<QProgressBar *>(QStringLiteral("operationProgress")));
    auto *tabs = window.findChild<QTabBar *>(QStringLiteral("workspaceTabs"));
    auto *workspace = window.findChild<QSplitter *>(QStringLiteral("mainWorkspaceSplitter"));
    auto *detailsScroll = window.findChild<QScrollArea *>(QStringLiteral("deviceDetailsScrollArea"));
    auto *rightWorkspace = window.findChild<QWidget *>(QStringLiteral("rightWorkspacePanel"));
    QVERIFY(tabs);
    QVERIFY(workspace);
    QVERIFY(detailsScroll);
    QVERIFY(rightWorkspace);
    QCOMPARE(workspace->orientation(), Qt::Horizontal);
    QCOMPARE(workspace->count(), 2);
    QCOMPARE(tabs->parentWidget()->objectName(), QStringLiteral("workspaceNavigation"));
    auto *workspacePages = window.findChild<QStackedWidget *>(QStringLiteral("workspacePages"));
    QVERIFY(workspacePages);
    QCOMPARE(workspacePages->parentWidget(), rightWorkspace);
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

    const QSqlDatabase database = QSqlDatabase::database(QStringLiteral("camera_inventory"), false);
    QVERIFY(database.isValid() && database.isOpen());
    QSqlQuery persisted(database);
    persisted.prepare(QStringLiteral("SELECT user_label FROM devices WHERE ip=:ip"));
    persisted.bindValue(QStringLiteral(":ip"), QStringLiteral("192.168.10.25"));
    QVERIFY(persisted.exec());
    QVERIFY(persisted.next());
    QCOMPARE(persisted.value(0).toString(), QStringLiteral("Loading bay"));
}

void MainWindowSmokeTest::exportsInventoryAsUtf8CsvWithEscapedFields()
{
    QStandardPaths::setTestModeEnabled(true);
    MainWindow window;
    auto *addButton = window.findChild<QPushButton *>(QStringLiteral("addDeviceButton"));
    auto *exportButton = window.findChild<QPushButton *>(QStringLiteral("exportCsvButton"));
    QVERIFY(addButton && exportButton);

    QTimer::singleShot(0, &window, [&window] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        auto *address = dialog->findChild<QLineEdit *>(QStringLiteral("manualAddress"));
        auto *name = dialog->findChild<QLineEdit *>(QStringLiteral("manualDeviceName"));
        auto *manufacturer = dialog->findChild<QLineEdit *>(QStringLiteral("manualManufacturer"));
        auto *save = dialog->findChild<QPushButton *>(QStringLiteral("saveManualDeviceButton"));
        QVERIFY(address && name && manufacturer && save);
        address->setText(QStringLiteral("192.168.10.25"));
        name->setText(QStringLiteral("Front \"east\", gate"));
        manufacturer->setText(QStringLiteral("Oxita"));
        QTest::mouseClick(save, Qt::LeftButton);
    });
    QTest::mouseClick(addButton, Qt::LeftButton);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString csvPath = directory.filePath(QStringLiteral("inventory.csv"));
    QTimer::singleShot(0, &window, [&window, csvPath] {
        auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
        QVERIFY2(dialog, "CSV export must present a file selection dialog.");
        dialog->selectFile(csvPath);
        auto *buttons = dialog->findChild<QDialogButtonBox *>();
        QVERIFY2(buttons, "CSV export dialog must expose its standard action buttons.");
        auto *saveButton = buttons->button(QDialogButtonBox::Save);
        QVERIFY2(saveButton, "CSV export dialog must expose its Save/accept button.");
        QTest::mouseClick(saveButton, Qt::LeftButton);
    });
    QTest::mouseClick(exportButton, Qt::LeftButton);

    QFile csv(csvPath);
    QVERIFY2(csv.open(QIODevice::ReadOnly), "The CSV export should create the selected file.");
    const QByteArray contents = csv.readAll();
    QVERIFY(contents.startsWith(QByteArray::fromHex("efbbbf")));
    QVERIFY(contents.contains(QByteArrayLiteral("\"192.168.10.25\",\"Front \"\"east\"\", gate\",\"Oxita\"")));
    QVERIFY(contents.contains(QByteArrayLiteral("\"IP ADDRESS\"")));
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
    auto *deviceTypeFilter = window.findChild<QComboBox *>(QStringLiteral("deviceTypeFilter"));
    QVERIFY(table && search && vendor && statusFilter && deviceTypeFilter);
    QCOMPARE(table->rowCount(), 1);
    QVERIFY(!table->isRowHidden(0));
    QCOMPARE(table->item(0, 11)->text(), QStringLiteral("Stale"));
    table->selectRow(0);
    table->setCurrentCell(0, 1);
    // A saved ONVIF source label is not proof of a currently reachable service
    // endpoint or a verified network interface. Keep write actions disabled.
    QVERIFY(!window.findChild<QPushButton *>(QStringLiteral("factoryResetCameraButton"))->isEnabled());
    QVERIFY(!window.findChild<QPushButton *>(QStringLiteral("applyStaticIpv4Button"))->isEnabled());
    const auto *deviceDetails = window.findChild<QLabel *>(QStringLiteral("selectedDeviceDetails"));
    QVERIFY(deviceDetails);
    QVERIFY(deviceDetails->text().contains(QStringLiteral("Acme")));
    QVERIFY(deviceDetails->text().contains(QStringLiteral("A-100")));
    const auto *fullProfile = window.findChild<QLabel *>(QStringLiteral("deviceDetails"));
    const auto *firmwareSummary = window.findChild<QLabel *>(QStringLiteral("firmwareDetailsSummary"));
    QVERIFY(fullProfile);
    QVERIFY(firmwareSummary);
    QVERIFY(fullProfile != firmwareSummary);
    QVERIFY(fullProfile->text().contains(QStringLiteral("02:00:00:00:00:10")));
    QVERIFY(fullProfile->text().contains(QStringLiteral("FIRMWARE")));
    QVERIFY(fullProfile->text().contains(QStringLiteral("SERIAL NUMBER")));
    QVERIFY(fullProfile->text().contains(QStringLiteral("HARDWARE ID")));
    QVERIFY(fullProfile->text().contains(QStringLiteral("Last reachability check")));
    QVERIFY(fullProfile->text().contains(QStringLiteral("Last successful response")));
    QVERIFY(fullProfile->text().contains(QStringLiteral("Last seen (UTC)")));
    QVERIFY(fullProfile->text().contains(QStringLiteral("OPERATION COMPATIBILITY")));
    QVERIFY(fullProfile->text().contains(QStringLiteral("Not implemented")));
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

    deviceTypeFilter->setCurrentText(QStringLiteral("Printers"));
    QVERIFY(table->isRowHidden(0));
    deviceTypeFilter->setCurrentText(QStringLiteral("Cameras"));
    QVERIFY(!table->isRowHidden(0));
    deviceTypeFilter->setCurrentText(QStringLiteral("All device types"));

    vendor->setCurrentText(QStringLiteral("Oxita devices"));
    QVERIFY(table->isRowHidden(0));
    vendor->setCurrentText(QStringLiteral("ONVIF devices"));
    QVERIFY(!table->isRowHidden(0));

    statusFilter->setCurrentText(QStringLiteral("Stale"));
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
    QVERIFY(!service.findChildren<QUdpSocket *>().isEmpty());
    service.cancel();
    QVERIFY(!service.isRunning());
    QCOMPARE(cancelled.count(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(service.findChildren<QUdpSocket *>().size(), 0, 1000);
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




void MainWindowSmokeTest::discoveryServiceIgnoresUnrelatedSoapResponse()
{
    QTcpServer camera;
    QVERIFY(camera.listen(QHostAddress::LocalHost, quint16(0)));
    bool requestReceived = false;
    connect(&camera, &QTcpServer::newConnection, &camera, [&camera, &requestReceived] {
        while (camera.hasPendingConnections()) {
            QTcpSocket *peer = camera.nextPendingConnection();
            if (!peer) continue;
            connect(peer, &QTcpSocket::readyRead, peer, [peer, &requestReceived] {
                QByteArray request = peer->property("requestBuffer").toByteArray();
                request.append(peer->readAll());
                peer->setProperty("requestBuffer", request);
                const qsizetype headerEnd = request.indexOf(QByteArrayLiteral("\r\n\r\n"));
                if (headerEnd < 0) return;
                const QRegularExpression lengthPattern(
                    QStringLiteral("Content-Length\\s*:\\s*(\\d+)"),
                    QRegularExpression::CaseInsensitiveOption);
                const auto match = lengthPattern.match(QString::fromLatin1(request.left(headerEnd)));
                const qsizetype bodyLength = match.hasMatch() ? match.captured(1).toLongLong() : 0;
                if (request.size() < headerEnd + 4 + bodyLength) return;
                requestReceived = true;
                const QByteArray body = QByteArrayLiteral(
                    "<s:Envelope xmlns:s='http://www.w3.org/2003/05/soap-envelope'><s:Body/>"
                    "</s:Envelope>");
                const QByteArray response = QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Type: application/soap+xml\r\n")
                    + QByteArrayLiteral("Content-Length: ") + QByteArray::number(body.size())
                    + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + body;
                peer->write(response);
                peer->disconnectFromHost();
            });
        }
    });

    DiscoveryService discovery;
    int deviceFoundCount = 0;
    connect(&discovery, &DiscoveryService::deviceFound, &discovery,
            [&deviceFoundCount](const DeviceRecord &) { ++deviceFoundCount; });
    DeviceRecord candidate;
    candidate.ipAddress = QStringLiteral("127.0.0.1");
    candidate.deviceServiceUrl = QStringLiteral("http://127.0.0.1:%1/onvif/device_service")
                                     .arg(camera.serverPort());
    candidate.onvifEndpoint = QStringLiteral("urn:uuid:unrelated-soap-fixture");
    candidate.discoverySources << QStringLiteral("ONVIF WS-Discovery");
    discovery.refreshDeviceInfo(candidate);

    QTRY_VERIFY_WITH_TIMEOUT(requestReceived, 3000);
    QTest::qWait(100);
    QCOMPARE(deviceFoundCount, 0);
}

void MainWindowSmokeTest::discoveryServiceRequestsAndRecordsOnvifServices()
{
    QTcpServer camera;
    QVERIFY(camera.listen(QHostAddress::LocalHost, quint16(0)));
    bool getServicesRequested = false;
    connect(&camera, &QTcpServer::newConnection, &camera, [&camera, &getServicesRequested] {
        while (camera.hasPendingConnections()) {
            QTcpSocket *peer = camera.nextPendingConnection();
            if (!peer) continue;
            connect(peer, &QTcpSocket::readyRead, peer, [peer, &getServicesRequested] {
                QByteArray request = peer->property("requestBuffer").toByteArray();
                request.append(peer->readAll());
                peer->setProperty("requestBuffer", request);
                const qsizetype headerEnd = request.indexOf(QByteArrayLiteral("\r\n\r\n"));
                if (headerEnd < 0) return;
                const QRegularExpression lengthPattern(
                    QStringLiteral("Content-Length\\s*:\\s*(\\d+)"),
                    QRegularExpression::CaseInsensitiveOption);
                const auto match = lengthPattern.match(QString::fromLatin1(request.left(headerEnd)));
                const qsizetype bodyLength = match.hasMatch() ? match.captured(1).toLongLong() : 0;
                if (request.size() < headerEnd + 4 + bodyLength) return;
                const QByteArray soap = request.mid(headerEnd + 4, bodyLength);
                QByteArray responseBody;
                if (soap.contains("GetDeviceInformation")) {
                    responseBody = QByteArrayLiteral(
                        "<s:Envelope xmlns:s='http://www.w3.org/2003/05/soap-envelope' "
                        "xmlns:tds='http://www.onvif.org/ver10/device/wsdl'><s:Body>"
                        "<tds:GetDeviceInformationResponse><tds:Manufacturer>Loopback</tds:Manufacturer>"
                        "<tds:Model>ServiceFixture</tds:Model><tds:FirmwareVersion>1</tds:FirmwareVersion>"
                        "<tds:SerialNumber>SVC-1</tds:SerialNumber><tds:HardwareId>HW-1</tds:HardwareId>"
                        "</tds:GetDeviceInformationResponse></s:Body></s:Envelope>");
                } else if (soap.contains("GetServices")) {
                    getServicesRequested = true;
                    responseBody = QByteArrayLiteral(
                        "<s:Envelope xmlns:s='http://www.w3.org/2003/05/soap-envelope' "
                        "xmlns:tds='http://www.onvif.org/ver10/device/wsdl'><s:Body>"
                        "<tds:GetServicesResponse>"
                        "<tds:Service><tds:Namespace>http://www.onvif.org/ver10/device/wsdl</tds:Namespace>"
                        "<tds:XAddr>http://127.0.0.1/device_service</tds:XAddr></tds:Service>"
                        "<tds:Service><tds:Namespace>http://www.onvif.org/ver10/media/wsdl</tds:Namespace>"
                        "<tds:XAddr>http://127.0.0.1/media_service</tds:XAddr></tds:Service>"
                        "</tds:GetServicesResponse></s:Body></s:Envelope>");
                } else {
                    responseBody = QByteArrayLiteral(
                        "<s:Envelope xmlns:s='http://www.w3.org/2003/05/soap-envelope'><s:Body/>"
                        "</s:Envelope>");
                }
                const QByteArray response = QByteArrayLiteral("HTTP/1.1 200 OK\r\n")
                    + QByteArrayLiteral("Content-Type: application/soap+xml; charset=utf-8\r\n")
                    + QByteArrayLiteral("Content-Length: ") + QByteArray::number(responseBody.size())
                    + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + responseBody;
                peer->write(response);
                peer->disconnectFromHost();
            });
        }
    });

    DiscoveryService discovery;
    DeviceRecord enriched;
    bool receivedServices = false;
    connect(&discovery, &DiscoveryService::deviceFound, &discovery,
            [&enriched, &receivedServices](const DeviceRecord &device) {
        enriched = device;
        receivedServices = device.capabilities.contains(QStringLiteral("Service: Device")) &&
                           device.capabilities.contains(QStringLiteral("Service: Media"));
    });

    DeviceRecord candidate;
    candidate.ipAddress = QStringLiteral("127.0.0.1");
    candidate.deviceServiceUrl = QStringLiteral("http://127.0.0.1:%1/onvif/device_service")
                                     .arg(camera.serverPort());
    candidate.onvifEndpoint = QStringLiteral("urn:uuid:onvif-services-integration");
    candidate.discoverySources << QStringLiteral("ONVIF WS-Discovery");
    discovery.refreshDeviceInfo(candidate);

    QTRY_VERIFY_WITH_TIMEOUT(getServicesRequested, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(receivedServices, 3000);
    QVERIFY(enriched.discoverySources.contains(QStringLiteral("ONVIF GetServices")));
    QVERIFY(enriched.capabilities.contains(QStringLiteral("Service: Device")));
    QVERIFY(enriched.capabilities.contains(QStringLiteral("Service: Media")));
}


void MainWindowSmokeTest::discoveryServiceRetriesHttpBasicAuthenticationChallenge()
{
    QTcpServer camera;
    QVERIFY(camera.listen(QHostAddress::LocalHost, quint16(0)));
    int requests = 0;
    bool authenticated = false;
    connect(&camera, &QTcpServer::newConnection, &camera, [&camera, &requests, &authenticated] {
        while (camera.hasPendingConnections()) {
            QTcpSocket *peer = camera.nextPendingConnection();
            if (!peer) continue;
            connect(peer, &QTcpSocket::readyRead, peer, [peer, &requests, &authenticated] {
                QByteArray request = peer->property("requestBuffer").toByteArray();
                request.append(peer->readAll());
                peer->setProperty("requestBuffer", request);
                const qsizetype headerEnd = request.indexOf(QByteArrayLiteral("\r\n\r\n"));
                if (headerEnd < 0) return;
                const QRegularExpression lengthPattern(
                    QStringLiteral("Content-Length\\s*:\\s*(\\d+)"),
                    QRegularExpression::CaseInsensitiveOption);
                const auto match = lengthPattern.match(QString::fromLatin1(request.left(headerEnd)));
                const qsizetype bodyLength = match.hasMatch() ? match.captured(1).toLongLong() : 0;
                if (request.size() < headerEnd + 4 + bodyLength) return;
                ++requests;
                const QByteArray headers = request.left(headerEnd);
                if (!headers.contains(QByteArrayLiteral("Authorization: Basic dXNlcjp0ZXN0LXNlY3JldA=="))) {
                    const QByteArray response = QByteArrayLiteral(
                        "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"Fixture Camera\"\r\n"
                        "Content-Length: 0\r\nConnection: close\r\n\r\n");
                    peer->write(response);
                    peer->disconnectFromHost();
                    return;
                }
                authenticated = true;
                const QByteArray body = QByteArrayLiteral(
                    "<s:Envelope xmlns:s='http://www.w3.org/2003/05/soap-envelope' "
                    "xmlns:tds='http://www.onvif.org/ver10/device/wsdl'><s:Body>"
                    "<tds:GetDeviceInformationResponse><tds:Manufacturer>Authenticated Fixture</tds:Manufacturer>"
                    "<tds:Model>HTTP Basic</tds:Model><tds:FirmwareVersion>1</tds:FirmwareVersion>"
                    "<tds:SerialNumber>AUTH-1</tds:SerialNumber><tds:HardwareId>AUTH-HW</tds:HardwareId>"
                    "</tds:GetDeviceInformationResponse></s:Body></s:Envelope>");
                const QByteArray response = QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Type: application/soap+xml\r\n")
                    + QByteArrayLiteral("Content-Length: ") + QByteArray::number(body.size())
                    + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + body;
                peer->write(response);
                peer->disconnectFromHost();
            });
        }
    });

    DiscoveryService discovery;
    int authenticationPrompts = 0;
    bool deviceReceived = false;
    connect(&discovery, &DiscoveryService::authenticationRequested, &discovery,
            [&authenticationPrompts](QAuthenticator *authenticator) {
        ++authenticationPrompts;
        authenticator->setUser(QStringLiteral("user"));
        authenticator->setPassword(QStringLiteral("test-secret"));
    });
    connect(&discovery, &DiscoveryService::deviceFound, &discovery,
            [&deviceReceived](const DeviceRecord &device) {
        if (device.manufacturer == QStringLiteral("Authenticated Fixture"))
            deviceReceived = true;
    });

    DeviceRecord candidate;
    candidate.ipAddress = QStringLiteral("127.0.0.1");
    candidate.deviceServiceUrl = QStringLiteral("http://127.0.0.1:%1/onvif/device_service")
                                     .arg(camera.serverPort());
    candidate.onvifEndpoint = QStringLiteral("urn:uuid:http-basic-auth-fixture");
    candidate.discoverySources << QStringLiteral("ONVIF WS-Discovery");
    discovery.refreshDeviceInfo(candidate);

    QTRY_VERIFY_WITH_TIMEOUT(deviceReceived, 3000);
    QCOMPARE(authenticationPrompts, 1);
    QVERIFY(requests >= 2);
    QVERIFY(authenticated);
}



void MainWindowSmokeTest::discoveryServiceRetriesWsseUsernameTokenChallenge()
{
    QTcpServer camera;
    QVERIFY(camera.listen(QHostAddress::LocalHost, quint16(0)));
    int requests = 0;
    bool digestReceived = false;
    connect(&camera, &QTcpServer::newConnection, &camera, [&camera, &requests, &digestReceived] {
        while (camera.hasPendingConnections()) {
            QTcpSocket *peer = camera.nextPendingConnection();
            if (!peer) continue;
            connect(peer, &QTcpSocket::readyRead, peer, [peer, &requests, &digestReceived] {
                QByteArray request = peer->property("requestBuffer").toByteArray();
                request.append(peer->readAll());
                peer->setProperty("requestBuffer", request);
                const qsizetype headerEnd = request.indexOf(QByteArrayLiteral("\r\n\r\n"));
                if (headerEnd < 0) return;
                const QRegularExpression lengthPattern(
                    QStringLiteral("Content-Length\\s*:\\s*(\\d+)"),
                    QRegularExpression::CaseInsensitiveOption);
                const auto match = lengthPattern.match(QString::fromLatin1(request.left(headerEnd)));
                const qsizetype bodyLength = match.hasMatch() ? match.captured(1).toLongLong() : 0;
                if (request.size() < headerEnd + 4 + bodyLength) return;
                ++requests;
                const QByteArray soap = request.mid(headerEnd + 4, bodyLength);
                QByteArray responseBody;
                int status = 200;
                if (!soap.contains("wsse:UsernameToken") ||
                    !soap.contains("<wsse:Username>operator</wsse:Username>") ||
                    !soap.contains("PasswordDigest") || !soap.contains("wsu:Created")) {
                    status = 500;
                    responseBody = QByteArrayLiteral(
                        "<s:Envelope xmlns:s='http://www.w3.org/2003/05/soap-envelope'><s:Body>"
                        "<s:Fault><s:Code><s:Value>s:Sender</s:Value></s:Code>"
                        "<s:Reason><s:Text>Not authorized: WS-Security required</s:Text></s:Reason>"
                        "</s:Fault></s:Body></s:Envelope>");
                } else {
                    digestReceived = true;
                    responseBody = QByteArrayLiteral(
                        "<s:Envelope xmlns:s='http://www.w3.org/2003/05/soap-envelope' "
                        "xmlns:tds='http://www.onvif.org/ver10/device/wsdl'><s:Body>"
                        "<tds:GetDeviceInformationResponse><tds:Manufacturer>WSSE Fixture</tds:Manufacturer>"
                        "<tds:Model>Digest Auth</tds:Model><tds:FirmwareVersion>1</tds:FirmwareVersion>"
                        "<tds:SerialNumber>WSSE-1</tds:SerialNumber><tds:HardwareId>WSSE-HW</tds:HardwareId>"
                        "</tds:GetDeviceInformationResponse></s:Body></s:Envelope>");
                }
                const QByteArray response = QByteArray("HTTP/1.1 ") + QByteArray::number(status)
                    + (status == 500 ? QByteArrayLiteral(" Internal Server Error\r\n")
                                     : QByteArrayLiteral(" OK\r\n"))
                    + QByteArrayLiteral("Content-Type: application/soap+xml\r\nContent-Length: ")
                    + QByteArray::number(responseBody.size())
                    + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + responseBody;
                peer->write(response);
                peer->disconnectFromHost();
            });
        }
    });

    DiscoveryService discovery;
    int credentialPrompts = 0;
    bool deviceReceived = false;
    connect(&discovery, &DiscoveryService::authenticationRequested, &discovery,
            [&credentialPrompts](QAuthenticator *authenticator) {
        ++credentialPrompts;
        authenticator->setUser(QStringLiteral("operator"));
        authenticator->setPassword(QStringLiteral("fixture-password"));
    });
    connect(&discovery, &DiscoveryService::deviceFound, &discovery,
            [&deviceReceived](const DeviceRecord &device) {
        if (device.manufacturer == QStringLiteral("WSSE Fixture"))
            deviceReceived = true;
    });

    DeviceRecord candidate;
    candidate.ipAddress = QStringLiteral("127.0.0.1");
    candidate.deviceServiceUrl = QStringLiteral("http://127.0.0.1:%1/onvif/device_service")
                                     .arg(camera.serverPort());
    candidate.onvifEndpoint = QStringLiteral("urn:uuid:wsse-auth-fixture");
    candidate.discoverySources << QStringLiteral("ONVIF WS-Discovery");
    discovery.refreshDeviceInfo(candidate);

    QTRY_VERIFY_WITH_TIMEOUT(deviceReceived, 3000);
    QCOMPARE(credentialPrompts, 1);
    QVERIFY(requests >= 2);
    QVERIFY(digestReceived);
}



void MainWindowSmokeTest::discoveryServiceRetriesWsseChallengeForGetServices()
{
    QTcpServer camera;
    QVERIFY(camera.listen(QHostAddress::LocalHost, quint16(0)));
    bool servicesReturned = false;
    bool digestReceived = false;
    int prompts = 0;
    connect(&camera, &QTcpServer::newConnection, &camera,
            [&camera, &servicesReturned, &digestReceived] {
        while (camera.hasPendingConnections()) {
            QTcpSocket *peer = camera.nextPendingConnection();
            if (!peer) continue;
            connect(peer, &QTcpSocket::readyRead, peer,
                    [peer, &servicesReturned, &digestReceived] {
                QByteArray request = peer->property("requestBuffer").toByteArray();
                request.append(peer->readAll());
                peer->setProperty("requestBuffer", request);
                const qsizetype headerEnd = request.indexOf(QByteArrayLiteral("\r\n\r\n"));
                if (headerEnd < 0) return;
                const QRegularExpression lengthPattern(
                    QStringLiteral("Content-Length\\s*:\\s*(\\d+)"),
                    QRegularExpression::CaseInsensitiveOption);
                const auto match = lengthPattern.match(QString::fromLatin1(request.left(headerEnd)));
                const qsizetype bodyLength = match.hasMatch() ? match.captured(1).toLongLong() : 0;
                if (request.size() < headerEnd + 4 + bodyLength) return;
                const QByteArray soap = request.mid(headerEnd + 4, bodyLength);
                QByteArray responseBody;
                int status = 200;
                if (soap.contains("GetDeviceInformation")) {
                    responseBody = QByteArrayLiteral(
                        "<s:Envelope xmlns:s='http://www.w3.org/2003/05/soap-envelope' "
                        "xmlns:tds='http://www.onvif.org/ver10/device/wsdl'><s:Body>"
                        "<tds:GetDeviceInformationResponse><tds:Manufacturer>Service WSSE Fixture</tds:Manufacturer>"
                        "<tds:Model>Service Auth</tds:Model><tds:FirmwareVersion>1</tds:FirmwareVersion>"
                        "<tds:SerialNumber>SVC-WSSE-1</tds:SerialNumber>"
                        "</tds:GetDeviceInformationResponse></s:Body></s:Envelope>");
                } else if (soap.contains("GetServices") && soap.contains("wsse:UsernameToken")) {
                    digestReceived = true;
                    servicesReturned = true;
                    responseBody = QByteArrayLiteral(
                        "<s:Envelope xmlns:s='http://www.w3.org/2003/05/soap-envelope' "
                        "xmlns:tds='http://www.onvif.org/ver10/device/wsdl' "
                        "xmlns:dn='http://www.onvif.org/ver10/network/wsdl'><s:Body>"
                        "<tds:GetServicesResponse><tds:Service><tds:Namespace>http://www.onvif.org/ver10/network/wsdl</tds:Namespace>"
                        "<tds:XAddr>http://127.0.0.1/onvif/network_service</tds:XAddr>"
                        "</tds:Service></tds:GetServicesResponse></s:Body></s:Envelope>");
                } else if (soap.contains("GetServices")) {
                    status = 500;
                    responseBody = QByteArrayLiteral(
                        "<s:Envelope xmlns:s='http://www.w3.org/2003/05/soap-envelope'><s:Body>"
                        "<s:Fault><s:Code><s:Value>ter:NotAuthorized</s:Value></s:Code>"
                        "<s:Reason><s:Text>Access denied</s:Text></s:Reason></s:Fault>"
                        "</s:Body></s:Envelope>");
                } else {
                    responseBody = QByteArrayLiteral(
                        "<s:Envelope xmlns:s='http://www.w3.org/2003/05/soap-envelope'><s:Body/>"
                        "</s:Envelope>");
                }
                const QByteArray response = QByteArray("HTTP/1.1 ") + QByteArray::number(status)
                    + (status == 500 ? QByteArrayLiteral(" Internal Server Error\r\n")
                                     : QByteArrayLiteral(" OK\r\n"))
                    + QByteArrayLiteral("Content-Type: application/soap+xml\r\nContent-Length: ")
                    + QByteArray::number(responseBody.size())
                    + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + responseBody;
                peer->write(response);
                peer->disconnectFromHost();
            });
        }
    });

    DiscoveryService discovery;
    connect(&discovery, &DiscoveryService::authenticationRequested, &discovery,
            [&prompts](QAuthenticator *authenticator) {
        ++prompts;
        authenticator->setUser(QStringLiteral("operator"));
        authenticator->setPassword(QStringLiteral("service-password"));
    });
    DeviceRecord enriched;
    bool serviceCapabilitySeen = false;
    connect(&discovery, &DiscoveryService::deviceFound, &discovery,
            [&enriched, &serviceCapabilitySeen](const DeviceRecord &device) {
        enriched = device;
        serviceCapabilitySeen = device.capabilities.contains(QStringLiteral("Service: Network"));
    });

    DeviceRecord candidate;
    candidate.ipAddress = QStringLiteral("127.0.0.1");
    candidate.deviceServiceUrl = QStringLiteral("http://127.0.0.1:%1/onvif/device_service")
                                     .arg(camera.serverPort());
    candidate.onvifEndpoint = QStringLiteral("urn:uuid:wsse-services-fixture");
    candidate.discoverySources << QStringLiteral("ONVIF WS-Discovery");
    discovery.refreshDeviceInfo(candidate);

    QTRY_VERIFY_WITH_TIMEOUT(serviceCapabilitySeen, 4000);
    QCOMPARE(prompts, 1);
    QVERIFY(servicesReturned);
    QVERIFY(digestReceived);
}

void MainWindowSmokeTest::discoveryServiceDoesNotFollowSoapRedirects()
{
    QTcpServer camera;
    QTcpServer redirectTarget;
    QVERIFY(camera.listen(QHostAddress::LocalHost, quint16(0)));
    QVERIFY(redirectTarget.listen(QHostAddress::LocalHost, quint16(0)));
    bool requestReceived = false;
    int redirectedRequests = 0;
    connect(&redirectTarget, &QTcpServer::newConnection, &redirectTarget, [&redirectTarget, &redirectedRequests] {
        while (redirectTarget.hasPendingConnections()) {
            QTcpSocket *peer = redirectTarget.nextPendingConnection();
            if (!peer) continue;
            ++redirectedRequests;
            peer->disconnectFromHost();
        }
    });
    connect(&camera, &QTcpServer::newConnection, &camera, [&camera, &requestReceived, &redirectTarget] {
        while (camera.hasPendingConnections()) {
            QTcpSocket *peer = camera.nextPendingConnection();
            if (!peer) continue;
            connect(peer, &QTcpSocket::readyRead, peer, [peer, &requestReceived, &redirectTarget] {
                const QByteArray request = peer->readAll();
                if (!request.contains("POST ")) return;
                requestReceived = true;
                const QByteArray response = QByteArrayLiteral("HTTP/1.1 302 Found\r\nLocation: http://127.0.0.1:")
                    + QByteArray::number(redirectTarget.serverPort())
                    + QByteArrayLiteral("/onvif/device_service\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                peer->write(response);
                peer->disconnectFromHost();
            });
        }
    });

    DiscoveryService discovery;
    QString status;
    connect(&discovery, &DiscoveryService::statusChanged, &discovery,
            [&status](const QString &message) {
        if (message.contains(QStringLiteral("GetDeviceInformation failed")))
            status = message;
    });
    DeviceRecord candidate;
    candidate.ipAddress = QStringLiteral("127.0.0.1");
    candidate.deviceServiceUrl = QStringLiteral("http://127.0.0.1:%1/onvif/device_service")
                                     .arg(camera.serverPort());
    candidate.onvifEndpoint = QStringLiteral("urn:uuid:redirect-fixture");
    candidate.discoverySources << QStringLiteral("ONVIF WS-Discovery");
    discovery.refreshDeviceInfo(candidate);

    QTRY_VERIFY_WITH_TIMEOUT(requestReceived, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!status.isEmpty(), 3000);
    QTest::qWait(100);
    QCOMPARE(redirectedRequests, 0);
}

void MainWindowSmokeTest::discoveryServiceReportsSoapRequestTimeout()
{
    QTcpServer camera;
    QVERIFY(camera.listen(QHostAddress::LocalHost, quint16(0)));
    bool requestReceived = false;
    connect(&camera, &QTcpServer::newConnection, &camera, [&camera, &requestReceived] {
        while (camera.hasPendingConnections()) {
            QTcpSocket *peer = camera.nextPendingConnection();
            if (!peer) continue;
            connect(peer, &QTcpSocket::readyRead, peer, [&requestReceived, peer] {
                if (!peer->readAll().isEmpty())
                    requestReceived = true;
            });
        }
    });

    DiscoveryService discovery;
    QString status;
    connect(&discovery, &DiscoveryService::statusChanged, &discovery,
            [&status](const QString &message) {
        if (message.contains(QStringLiteral("GetDeviceInformation failed")))
            status = message;
    });
    DeviceRecord candidate;
    candidate.ipAddress = QStringLiteral("127.0.0.1");
    candidate.deviceServiceUrl = QStringLiteral("http://127.0.0.1:%1/onvif/device_service")
                                     .arg(camera.serverPort());
    candidate.onvifEndpoint = QStringLiteral("urn:uuid:timeout-fixture");
    candidate.discoverySources << QStringLiteral("ONVIF WS-Discovery");
    discovery.refreshDeviceInfo(candidate);

    QTRY_VERIFY_WITH_TIMEOUT(requestReceived, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!status.isEmpty(), 6000);
    QVERIFY(status.contains(QStringLiteral("HTTP 0")) ||
            status.contains(QStringLiteral("timed out"), Qt::CaseInsensitive));
}

void MainWindowSmokeTest::discoveryServiceReportsSoapFaults()
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
                const QRegularExpression lengthPattern(
                    QStringLiteral("Content-Length\\s*:\\s*(\\d+)"),
                    QRegularExpression::CaseInsensitiveOption);
                const auto match = lengthPattern.match(QString::fromLatin1(request.left(headerEnd)));
                const qsizetype bodyLength = match.hasMatch() ? match.captured(1).toLongLong() : 0;
                if (request.size() < headerEnd + 4 + bodyLength) return;
                const QByteArray soap = request.mid(headerEnd + 4, bodyLength);
                QByteArray responseBody;
                int status = 200;
                if (soap.contains("GetDeviceInformation")) {
                    responseBody = QByteArrayLiteral(
                        "<s:Envelope xmlns:s='http://www.w3.org/2003/05/soap-envelope' "
                        "xmlns:tds='http://www.onvif.org/ver10/device/wsdl'><s:Body>"
                        "<tds:GetDeviceInformationResponse><tds:Manufacturer>Fault Fixture</tds:Manufacturer>"
                        "<tds:Model>SOAP Fault</tds:Model><tds:FirmwareVersion>1</tds:FirmwareVersion>"
                        "<tds:SerialNumber>FAULT-1</tds:SerialNumber><tds:HardwareId>FAULT-HW</tds:HardwareId>"
                        "</tds:GetDeviceInformationResponse></s:Body></s:Envelope>");
                } else if (soap.contains("GetServices")) {
                    status = 500;
                    responseBody = QByteArrayLiteral(
                        "<s:Envelope xmlns:s='http://www.w3.org/2003/05/soap-envelope'><s:Body>"
                        "<s:Fault><s:Code><s:Value>s:Sender</s:Value></s:Code>"
                        "<s:Reason><s:Text xml:lang='en'>Action not supported for GetServices</s:Text></s:Reason>"
                        "</s:Fault></s:Body></s:Envelope>");
                } else {
                    responseBody = QByteArrayLiteral(
                        "<s:Envelope xmlns:s='http://www.w3.org/2003/05/soap-envelope'><s:Body/>"
                        "</s:Envelope>");
                }
                const QByteArray response = QByteArray("HTTP/1.1 ") + QByteArray::number(status)
                    + (status == 500 ? QByteArrayLiteral(" Internal Server Error\r\n")
                                     : QByteArrayLiteral(" OK\r\n"))
                    + QByteArrayLiteral("Content-Type: application/soap+xml\r\nContent-Length: ")
                    + QByteArray::number(responseBody.size())
                    + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + responseBody;
                peer->write(response);
                peer->disconnectFromHost();
            });
        }
    });

    DiscoveryService discovery;
    QString observedStatus;
    connect(&discovery, &DiscoveryService::statusChanged, &discovery,
            [&observedStatus](const QString &message) {
        if (message.contains(QStringLiteral("Action not supported for GetServices")) )
            observedStatus = message;
    });
    DeviceRecord candidate;
    candidate.ipAddress = QStringLiteral("127.0.0.1");
    candidate.deviceServiceUrl = QStringLiteral("http://127.0.0.1:%1/onvif/device_service")
                                     .arg(camera.serverPort());
    candidate.onvifEndpoint = QStringLiteral("urn:uuid:soap-fault-fixture");
    candidate.discoverySources << QStringLiteral("ONVIF WS-Discovery");
    discovery.refreshDeviceInfo(candidate);

    QTRY_VERIFY_WITH_TIMEOUT(!observedStatus.isEmpty(), 3000);
    QVERIFY(observedStatus.contains(QStringLiteral("Camera rejected ONVIF GetServices")));
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
