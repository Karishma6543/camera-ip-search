#include "main_window.h"

#include "discovery/discovery_service.h"
#include "discovery/subnet_scanner.h"
#include "discovery/reachability_checker.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QInputDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QHostAddress>

namespace {
QLabel *mutedLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("muted"));
    label->setWordWrap(true);
    return label;
}
QPushButton *actionButton(const QString &text, QWidget *parent, bool primary = false)
{
    auto *button = new QPushButton(text, parent);
    if (primary) button->setObjectName(QStringLiteral("primaryButton"));
    return button;
}
QGroupBox *panel(const QString &title, QWidget *parent)
{
    auto *box = new QGroupBox(title, parent);
    box->setObjectName(QStringLiteral("actionPanel"));
    return box;
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    discovery_ = new DiscoveryService(this);
    subnetScanner_ = new SubnetScanner(this);
    reachabilityChecker_ = new ReachabilityChecker(this);
    buildUi();
    initializeInventory();
    loadInventory();
    populateInterfaces();
    updateStatus(tr("Ready — choose a local interface to scan your subnet."));
    connect(discovery_, &DiscoveryService::deviceFound, this, &MainWindow::upsertDevice);
    connect(discovery_, &DiscoveryService::statusChanged, this, &MainWindow::updateStatus);
    connect(discovery_, &DiscoveryService::finished, this, [this](int count) {
        scanButton_->setText(tr("Discover devices"));
        scanButton_->setEnabled(true);
        updateDeviceSummary();
        updateStatus(tr("Discovery finished — %1 device(s) found.").arg(count));
    });
    connect(subnetScanner_, &SubnetScanner::candidateFound, this, &MainWindow::upsertDevice);
    connect(subnetScanner_, &SubnetScanner::statusChanged, this, &MainWindow::updateStatus);
    connect(subnetScanner_, &SubnetScanner::finished, this, [this](int count) {
        subnetScanButton_->setText(tr("Scan selected subnet"));
        subnetScanButton_->setEnabled(true);
        updateDeviceSummary();
        updateStatus(tr("Subnet scan finished — %1 unverified candidate(s).").arg(count));
    });
}

void MainWindow::populateInterfaces()
{
    if (!interfaceSelector_) return;
    interfaceSelector_->clear();
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning) ||
            flags.testFlag(QNetworkInterface::IsLoopBack)) continue;
        bool hasIpv4 = false;
        for (const auto &entry : interface.addressEntries())
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
                !entry.ip().isLoopback() && entry.prefixLength() >= 1) hasIpv4 = true;
        if (hasIpv4) interfaceSelector_->addItem(interface.humanReadableName(), interface.name());
    }
    if (interfaceSelector_->count() == 0)
        interfaceSelector_->addItem(tr("No active IPv4 interface"), QString());
}

void MainWindow::buildUi()
{
    setWindowTitle(tr("Oxita | Camera Device Manager"));
    setMinimumSize(1120, 720);
    resize(1480, 900);

    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget#appRoot { background: #202226; color: #eceef1; }
        QWidget { font-family: "Segoe UI", "Inter", sans-serif; font-size: 12px; }
        QLabel { color: #e6e8eb; }
        QLabel#brand { color: #ffffff; font-size: 21px; font-weight: 800; }
        QLabel#brandMark { color: #ffffff; background: #c62835; border-radius: 8px; font-size: 17px; font-weight: 900; padding: 7px; }
        QLabel#muted { color: #a4a8b0; font-size: 11px; }
        QLabel#sectionTitle { color: #f4f4f5; font-size: 14px; font-weight: 750; }
        QLabel#metric { color: #ffffff; font-size: 16px; font-weight: 800; }
        QFrame#topBar { background: #292c31; border: 1px solid #3c4048; border-radius: 10px; }
        QFrame#surface { background: #292c31; border: 1px solid #41454e; border-radius: 10px; }
        QFrame#toolbar { background: #25282d; border: 1px solid #3b3f47; border-radius: 8px; }
        QGroupBox#actionPanel { background: #2b2e34; border: 1px solid #454a54; border-radius: 9px; margin-top: 12px; padding: 13px 10px 10px 10px; color: #f1f1f2; font-weight: 700; }
        QGroupBox#actionPanel::title { subcontrol-origin: margin; left: 12px; padding: 0 5px; color: #f2f2f3; }
        QLineEdit, QComboBox { background: #1f2227; color: #f1f2f4; border: 1px solid #646a75; border-radius: 6px; padding: 8px 9px; selection-background-color: #bd2634; min-height: 18px; }
        QLineEdit:focus, QComboBox:focus { border: 1px solid #d43a47; }
        QComboBox::drop-down { border: 0; width: 22px; }
        QPushButton { background: #383c44; color: #f4f4f5; border: 1px solid #555b66; border-radius: 6px; padding: 8px 12px; font-weight: 650; }
        QPushButton:hover { background: #454a54; border-color: #858b96; }
        QPushButton#primaryButton { background: #bd2634; color: white; border: 1px solid #d13b48; padding: 9px 15px; }
        QPushButton#primaryButton:hover { background: #d33240; }
        QPushButton:disabled { color: #858a93; background: #30333a; border-color: #454952; }
        QTableWidget { background: #23262b; alternate-background-color: #292d34; border: 0; gridline-color: #383d46; selection-background-color: #573139; selection-color: #ffffff; font-size: 11px; }
        QHeaderView::section { background: #30343b; color: #c4c8cf; border: 0; border-bottom: 1px solid #505661; padding: 10px 7px; font-size: 10px; font-weight: 800; }
        QTableWidget::item { padding: 7px 5px; border-bottom: 1px solid #343941; }
        QTabWidget::pane { border: 1px solid #414650; background: #292c31; border-radius: 7px; }
        QTabBar::tab { background: #30343b; color: #c5c8ce; padding: 11px 20px; border: 1px solid #454a54; border-bottom: 0; margin-right: 3px; border-top-left-radius: 6px; border-top-right-radius: 6px; font-weight: 700; }
        QTabBar::tab:selected { background: #bd2634; color: white; border-color: #d13b48; }
        QCheckBox { color: #e8e9eb; spacing: 7px; }
        QCheckBox::indicator { width: 15px; height: 15px; }
        QStatusBar { background: #25282d; color: #b7bbc2; border-top: 1px solid #41454e; }
        QScrollArea { border: 0; background: transparent; }
    )"));

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("appRoot"));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(16, 14, 16, 10);
    root->setSpacing(10);

    auto *top = new QFrame(central);
    top->setObjectName(QStringLiteral("topBar"));
    auto *topLayout = new QHBoxLayout(top);
    topLayout->setContentsMargins(14, 9, 14, 9);
    auto *mark = new QLabel(QStringLiteral("O"), top);
    mark->setObjectName(QStringLiteral("brandMark"));
    auto *brand = new QLabel(tr("OXITA  |  CAMERA DEVICE MANAGER"), top);
    brand->setObjectName(QStringLiteral("brand"));
    auto *brandStack = new QVBoxLayout();
    brandStack->setSpacing(0);
    brandStack->addWidget(brand);
    brandStack->addWidget(mutedLabel(tr("IP SEARCH  •  DISCOVERY  •  DEVICE CONFIGURATION"), top));
    topLayout->addWidget(mark);
    topLayout->addLayout(brandStack);
    topLayout->addStretch();
    auto *help = actionButton(tr("Help"), top);
    auto *settings = actionButton(tr("Settings"), top);
    topLayout->addWidget(help);
    topLayout->addWidget(settings);
    root->addWidget(top);

    auto *toolbar = new QFrame(central);
    toolbar->setObjectName(QStringLiteral("toolbar"));
    auto *tools = new QHBoxLayout(toolbar);
    tools->setContentsMargins(9, 7, 9, 7);
    searchEdit_ = new QLineEdit(toolbar);
    searchEdit_->setPlaceholderText(tr("Search IP, MAC, model or manufacturer"));
    searchEdit_->setMinimumWidth(250);
    interfaceSelector_ = new QComboBox(toolbar);
    interfaceSelector_->setMinimumWidth(155);
    manufacturerFilter_ = new QComboBox(toolbar);
    auto *vendorFilter = manufacturerFilter_;
    vendorFilter->addItems({tr("All manufacturers"), tr("ONVIF devices"), tr("Oxita devices"), tr("Other / unknown")});
    statusFilter_ = new QComboBox(toolbar);
    auto *statusFilter = statusFilter_;
    statusFilter->addItems({tr("All statuses"), tr("Online"), tr("Activated"), tr("Not activated")});
    deviceCountValue_ = new QLabel(QStringLiteral("0 devices"), toolbar);
    deviceCountValue_->setObjectName(QStringLiteral("metric"));
    tools->addWidget(searchEdit_, 2);
    tools->addWidget(interfaceSelector_);
    tools->addWidget(vendorFilter);
    tools->addWidget(statusFilter);
    tools->addStretch();
    tools->addWidget(deviceCountValue_);
    addDeviceButton_ = actionButton(tr("Add device"), toolbar);
    tools->addWidget(addDeviceButton_);
    auto *renameButton = actionButton(tr("Rename label"), toolbar);
    tools->addWidget(renameButton);
    scanButton_ = actionButton(tr("Discover devices"), toolbar, true);
    tools->addWidget(scanButton_);
    subnetScanButton_ = actionButton(tr("Scan selected subnet"), toolbar);
    tools->addWidget(subnetScanButton_);
    reachabilityButton_ = actionButton(tr("Check reachability"), toolbar);
    tools->addWidget(reachabilityButton_);
    root->addWidget(toolbar);

    auto *mainRow = new QHBoxLayout();
    mainRow->setSpacing(10);

    auto *left = new QFrame(central);
    left->setObjectName(QStringLiteral("surface"));
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(12, 11, 12, 12);
    auto *listHeading = new QHBoxLayout();
    auto *listTitle = new QLabel(tr("Discovered devices"), left);
    listTitle->setObjectName(QStringLiteral("sectionTitle"));
    summaryLabel_ = mutedLabel(tr("Ready to scan your local network"), left);
    listHeading->addWidget(listTitle);
    listHeading->addStretch();
    listHeading->addWidget(summaryLabel_);
    leftLayout->addLayout(listHeading);

    deviceTable_ = new QTableWidget(0, 12, left);
    deviceTable_->setHorizontalHeaderLabels({tr(""), tr("IP ADDRESS"), tr("DESCRIPTION"), tr("MANUFACTURER"), tr("PRODUCT SERIES / MODEL"), tr("MAC ADDRESS"), tr("FIRMWARE"), tr("STATUS"), tr("SOURCE"), tr("SERIAL NUMBER"), tr("HARDWARE ID"), tr("REACHABILITY")});
    deviceTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    deviceTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    deviceTable_->horizontalHeader()->setStretchLastSection(true);
    deviceTable_->verticalHeader()->setVisible(false);
    deviceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    deviceTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    deviceTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    deviceTable_->setAlternatingRowColors(true);
    deviceTable_->setShowGrid(false);
    deviceTable_->setMinimumHeight(310);
    leftLayout->addWidget(deviceTable_, 1);

    auto *tabs = new QTabWidget(left);
    tabs->setTabPosition(QTabWidget::South);
    tabs->addTab(makeInfoTab(tabs), tr("Device Info"));
    tabs->addTab(makeBulkTab(tabs), tr("Bulk Config"));
    tabs->addTab(makeMaintenanceTab(tabs), tr("Maintenance"));
    tabs->addTab(makeNvrTab(tabs), tr("NVR Channels"));
    leftLayout->addWidget(tabs);

    auto *rightScroll = new QScrollArea(central);
    rightScroll->setWidgetResizable(true);
    rightScroll->setMinimumWidth(340);
    rightScroll->setMaximumWidth(440);
    auto *rightContent = new QWidget(rightScroll);
    auto *rightLayout = new QVBoxLayout(rightContent);
    rightLayout->setContentsMargins(2, 2, 5, 2);
    rightLayout->setSpacing(9);

    auto *ipPanel = panel(tr("IP Assignment"), rightContent);
    auto *ipGrid = new QGridLayout(ipPanel);
    ipGrid->addWidget(mutedLabel(tr("Applies to selected devices. Configuration controls will activate when a verified device adapter is available."), ipPanel), 0, 0, 1, 2);
    ipGrid->addWidget(new QLabel(tr("Gateway"), ipPanel), 1, 0);
    ipGrid->addWidget(new QLabel(tr("Subnet Mask"), ipPanel), 1, 1);
    auto *gateway = new QLineEdit(ipPanel); gateway->setPlaceholderText("192.168.1.1"); gateway->setEnabled(false);
    auto *mask = new QLineEdit(ipPanel); mask->setPlaceholderText("255.255.255.0"); mask->setEnabled(false);
    ipGrid->addWidget(gateway, 2, 0); ipGrid->addWidget(mask, 2, 1);
    ipGrid->addWidget(new QLabel(tr("IP address / range"), ipPanel), 3, 0);
    ipGrid->addWidget(new QLabel(tr("DNS server"), ipPanel), 3, 1);
    auto *ipRange = new QLineEdit(ipPanel); ipRange->setPlaceholderText(tr("Start – End")); ipRange->setEnabled(false);
    auto *dns = new QLineEdit(ipPanel); dns->setPlaceholderText("8.8.8.8"); dns->setEnabled(false);
    ipGrid->addWidget(ipRange, 4, 0); ipGrid->addWidget(dns, 4, 1);
    auto *dhcp = new QCheckBox(tr("Enable DHCP"), ipPanel); dhcp->setEnabled(false);
    ipGrid->addWidget(dhcp, 5, 0, 1, 2);
    auto *autoIp = actionButton(tr("Auto IP Assign"), ipPanel, true); autoIp->setEnabled(false);
    auto *rangeIp = actionButton(tr("Range IP Assign"), ipPanel); rangeIp->setEnabled(false);
    ipGrid->addWidget(autoIp, 6, 0); ipGrid->addWidget(rangeIp, 6, 1);
    rightLayout->addWidget(ipPanel);

    auto *firmwarePanel = panel(tr("Firmware Update"), rightContent);
    auto *fwLayout = new QVBoxLayout(firmwarePanel);
    fwLayout->addWidget(mutedLabel(tr("Choose a firmware package and compatible product series. Device and firmware compatibility checks will be required before updating."), firmwarePanel));
    auto *chooseFw = actionButton(tr("Choose firmware file"), firmwarePanel); chooseFw->setEnabled(false);
    fwLayout->addWidget(chooseFw);
    auto *series = new QComboBox(firmwarePanel); series->addItem(tr("Select product series")); series->setEnabled(false);
    fwLayout->addWidget(series);
    auto *fwButtons = new QHBoxLayout();
    auto *update = actionButton(tr("Update"), firmwarePanel, true); update->setEnabled(false);
    auto *smartUpdate = actionButton(tr("Smart Update"), firmwarePanel); smartUpdate->setEnabled(false);
    fwButtons->addWidget(update); fwButtons->addWidget(smartUpdate);
    fwLayout->addLayout(fwButtons);
    rightLayout->addWidget(firmwarePanel);

    auto *bulkPanel = panel(tr("Camera Bulk Config"), rightContent);
    auto *bulkLayout = new QVBoxLayout(bulkPanel);
    auto *selectionRow = new QHBoxLayout();
    auto *selectAll = actionButton(tr("Select all"), bulkPanel); selectAll->setEnabled(false);
    auto *clearSelection = actionButton(tr("Clear selection"), bulkPanel); clearSelection->setEnabled(false);
    selectionRow->addWidget(selectAll); selectionRow->addWidget(clearSelection);
    bulkLayout->addLayout(selectionRow);
    bulkLayout->addWidget(mutedLabel(tr("Bulk settings are shown here and will be enabled per supported model and firmware."), bulkPanel));
    auto *dayNight = new QComboBox(bulkPanel); dayNight->addItems({tr("Day-Night: Select"), tr("Day"), tr("Night"), tr("Auto")}); dayNight->setEnabled(false);
    auto *icr = new QComboBox(bulkPanel); icr->addItems({tr("ICR: Select"), tr("On"), tr("Off"), tr("Auto")}); icr->setEnabled(false);
    bulkLayout->addWidget(dayNight); bulkLayout->addWidget(icr);
    auto *easyMode = actionButton(tr("Easy Mode"), bulkPanel); easyMode->setEnabled(false);
    auto *imageIr = actionButton(tr("Apply Image / IR"), bulkPanel); imageIr->setEnabled(false);
    bulkLayout->addWidget(easyMode); bulkLayout->addWidget(imageIr);
    rightLayout->addWidget(bulkPanel);
    rightLayout->addStretch();
    rightScroll->setWidget(rightContent);
    mainRow->addWidget(left, 3);
    mainRow->addWidget(rightScroll, 1);
    root->addLayout(mainRow, 1);

    auto *footer = mutedLabel(tr("OXITA CAMERA DEVICE MANAGER  •  Multi-vendor support is capability-based. Unverified configuration and firmware actions remain safely disabled."), central);
    root->addWidget(footer);
    setCentralWidget(central);
    statusLabel_ = new QLabel(this);
    statusBar()->addWidget(statusLabel_, 1);

    connect(scanButton_, &QPushButton::clicked, this, [this] {
        if (discovery_->isRunning()) {
            discovery_->cancel();
            scanButton_->setText(tr("Discover devices"));
        } else {
            deviceTable_->setRowCount(0);
            updateDeviceSummary();
            scanButton_->setText(tr("Cancel discovery"));
            discovery_->start();
        }
    });
    connect(subnetScanButton_, &QPushButton::clicked, this, [this] {
        if (subnetScanner_->isRunning()) {
            subnetScanner_->cancel();
            subnetScanButton_->setText(tr("Scan selected subnet"));
            return;
        }
        const QString selectedName = interfaceSelector_->currentData().toString();
        const QNetworkInterface interface = QNetworkInterface::interfaceFromName(selectedName);
        for (const auto &entry : interface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
                !entry.ip().isLoopback() && entry.prefixLength() >= 1) {
                if (subnetScanner_->start(interface, entry)) {
                    subnetScanButton_->setText(tr("Cancel subnet scan"));
                    return;
                }
            }
        }
        updateStatus(tr("Could not start subnet scan. Select an active IPv4 interface."));
    });
    connect(searchEdit_, &QLineEdit::textChanged, this, &MainWindow::applyFilters);
    connect(vendorFilter, &QComboBox::currentTextChanged, this, &MainWindow::applyFilters);
    connect(statusFilter, &QComboBox::currentTextChanged, this, &MainWindow::applyFilters);
    connect(renameButton, &QPushButton::clicked, this, &MainWindow::editSelectedLabel);
    connect(addDeviceButton_, &QPushButton::clicked, this, &MainWindow::addManualDevice);
    connect(openWebButton_, &QPushButton::clicked, this, &MainWindow::openSelectedWebInterface);
    connect(deviceTable_, &QTableWidget::itemSelectionChanged, this, [this] {
        openWebButton_->setEnabled(deviceTable_->currentRow() >= 0);
    });
    connect(help, &QPushButton::clicked, this, [this] { updateStatus(tr("See the project documentation for supported discovery protocols and current feature status.")); });
    connect(settings, &QPushButton::clicked, this, [this] { updateStatus(tr("Application preferences are planned; no settings have been changed.")); });
    connect(selectAll, &QPushButton::clicked, this, [this] { updateStatus(tr("Bulk selection actions will be enabled with the bulk configuration implementation.")); });
    connect(clearSelection, &QPushButton::clicked, this, [this] { deviceTable_->clearSelection(); });
    connect(deviceTable_, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
        if (row < 0 || !deviceDetailsLabel_) return;
        QStringList details;
        for (int col = 1; col < deviceTable_->columnCount(); ++col) {
            const auto *item = deviceTable_->item(row, col);
            if (item && !item->text().isEmpty())
                details << deviceTable_->horizontalHeaderItem(col)->text() + QStringLiteral(": ") + item->text();
        }
        deviceDetailsLabel_->setText(details.join(QStringLiteral("\\n")));
    });
    updateDeviceSummary();
}

QWidget *MainWindow::makeInfoTab(QWidget *parent)
{
    auto *page = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    deviceDetailsLabel_ = new QLabel(tr("Select a discovered device to view its details. ONVIF information is queried automatically when the device exposes a reachable device service."), page);
    deviceDetailsLabel_->setWordWrap(true);
    layout->addWidget(deviceDetailsLabel_);
    layout->addStretch();
    return page;
}
QWidget *MainWindow::makeBulkTab(QWidget *parent)
{
    auto *page = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(mutedLabel(tr("Select multiple devices to apply model-supported settings together. Day/Night, ICR, Easy Mode and Image / IR controls will be enabled only for verified adapters."), page));
    layout->addStretch();
    return page;
}
QWidget *MainWindow::makeMaintenanceTab(QWidget *parent)
{
    auto *page = new QWidget(parent);
    auto *layout = new QHBoxLayout(page);
    auto *reboot = actionButton(tr("Reboot selected"), page); reboot->setEnabled(false);
    auto *refresh = actionButton(tr("Refresh device info"), page); refresh->setEnabled(false);
    openWebButton_ = actionButton(tr("Open web interface"), page);
    openWebButton_->setEnabled(false);
    auto *web = openWebButton_;
    layout->addWidget(reboot); layout->addWidget(refresh); layout->addWidget(web); layout->addStretch();
    return page;
}
QWidget *MainWindow::makeNvrTab(QWidget *parent)
{
    auto *page = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(mutedLabel(tr("NVR channel discovery and management will appear here for recorders that expose a verified supported interface."), page));
    layout->addStretch();
    return page;
}

void MainWindow::updateStatus(const QString &message)
{
    if (statusLabel_) statusLabel_->setText(message);
}
void MainWindow::updateDeviceSummary()
{
    const int count = deviceTable_ ? deviceTable_->rowCount() : 0;
    if (deviceCountValue_) deviceCountValue_->setText(tr("%1 devices").arg(count));
    if (summaryLabel_) summaryLabel_->setText(count == 1 ? tr("1 device found") : tr("%1 devices found").arg(count));
}
void MainWindow::upsertDevice(const DeviceRecord &device)
{
    int row = -1;
    for (int i = 0; i < deviceTable_->rowCount(); ++i) {
        const auto *macItem = deviceTable_->item(i, 5);
        const auto *ipItem = deviceTable_->item(i, 1);
        if ((macItem && !device.macAddress.isEmpty() && macItem->text() == device.macAddress) ||
            (ipItem && ipItem->text() == device.ipAddress)) { row = i; break; }
    }
    if (row < 0) { row = deviceTable_->rowCount(); deviceTable_->insertRow(row); }
    const QString oldLabel = deviceTable_->item(row, 2) ? deviceTable_->item(row, 2)->text() : QString();
    auto *check = new QTableWidgetItem();
    check->setFlags(check->flags() | Qt::ItemIsUserCheckable);
    check->setCheckState(Qt::Unchecked);
    deviceTable_->setItem(row, 0, check);
    const QStringList values = {
        device.ipAddress, !device.userLabel.isEmpty() ? device.userLabel : (oldLabel.isEmpty() && device.isCandidate ? QStringLiteral("Unverified candidate") : oldLabel),
        device.manufacturer, device.model, device.macAddress,
        device.firmwareVersion, device.activationState,
        device.discoverySources.join(QStringLiteral(", ")), device.serialNumber, device.hardwareId,
        device.reachabilityStatus
    };
    for (int col = 0; col < values.size(); ++col)
        deviceTable_->setItem(row, col + 1, new QTableWidgetItem(values.at(col)));
    updateDeviceSummary();
    applyFilters();
    if (!loadingInventory_) persistDevice(device);
}

void MainWindow::initializeInventory()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(directory)) {
        updateStatus(tr("Local inventory unavailable; devices remain session-only."));
        return;
    }
    inventoryDb_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("camera_inventory"));
    inventoryDb_.setDatabaseName(directory + QStringLiteral("/inventory.sqlite"));
    if (!inventoryDb_.open()) {
        updateStatus(tr("Could not open local inventory: %1").arg(inventoryDb_.lastError().text()));
        return;
    }
    QSqlQuery query(inventoryDb_);
    if (!query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS devices (device_key TEXT PRIMARY KEY, ip TEXT, mac TEXT, manufacturer TEXT, model TEXT, firmware TEXT, serial TEXT, hardware TEXT, activation TEXT, source TEXT, is_candidate INTEGER, last_seen TEXT DEFAULT CURRENT_TIMESTAMP)"))) {
        updateStatus(tr("Could not initialize inventory: %1").arg(query.lastError().text()));
        return;
    }
    query.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN user_label TEXT DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN reachability_status TEXT DEFAULT 'Not checked'"));
    query.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN reachability_checked_at TEXT DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN last_successful_response_at TEXT DEFAULT ''"));
}

void MainWindow::persistDevice(const DeviceRecord &device)
{
    if (!inventoryDb_.isOpen() || device.ipAddress.isEmpty()) return;
    const QString key = !device.macAddress.isEmpty() ? device.macAddress : device.ipAddress;
    QSqlQuery query(inventoryDb_);
    query.prepare(QStringLiteral("INSERT INTO devices (device_key,ip,mac,manufacturer,model,firmware,serial,hardware,activation,source,is_candidate,user_label,last_seen) VALUES (:key,:ip,:mac,:manufacturer,:model,:firmware,:serial,:hardware,:activation,:source,:candidate,:label,CURRENT_TIMESTAMP) ON CONFLICT(device_key) DO UPDATE SET ip=excluded.ip,mac=excluded.mac,manufacturer=excluded.manufacturer,model=excluded.model,firmware=excluded.firmware,serial=excluded.serial,hardware=excluded.hardware,activation=excluded.activation,source=excluded.source,is_candidate=excluded.is_candidate,user_label=CASE WHEN excluded.user_label='' THEN devices.user_label ELSE excluded.user_label END,last_seen=CURRENT_TIMESTAMP"));
    query.bindValue(":key", key); query.bindValue(":ip", device.ipAddress); query.bindValue(":mac", device.macAddress);
    query.bindValue(":manufacturer", device.manufacturer); query.bindValue(":model", device.model);
    query.bindValue(":firmware", device.firmwareVersion); query.bindValue(":serial", device.serialNumber);
    query.bindValue(":hardware", device.hardwareId); query.bindValue(":activation", device.activationState);
    query.bindValue(":source", device.discoverySources.join(QStringLiteral(", ")));
    query.bindValue(":candidate", device.isCandidate ? 1 : 0);
    query.bindValue(":label", device.userLabel);
    query.bindValue(":reachability", device.reachabilityStatus);
    query.bindValue(":checked", device.reachabilityCheckedAt);
    query.bindValue(":success", device.lastSuccessfulResponseAt);
    if (!query.exec()) updateStatus(tr("Inventory save failed: %1").arg(query.lastError().text()));
}

void MainWindow::loadInventory()
{
    if (!inventoryDb_.isOpen()) return;
    QSqlQuery query(inventoryDb_);
    if (!query.exec(QStringLiteral("SELECT ip,mac,manufacturer,model,firmware,serial,hardware,activation,source,is_candidate,user_label,reachability_status,reachability_checked_at,last_successful_response_at FROM devices ORDER BY last_seen DESC"))) return;
    loadingInventory_ = true;
    while (query.next()) {
        DeviceRecord device;
        device.ipAddress=query.value(0).toString(); device.macAddress=query.value(1).toString();
        device.manufacturer=query.value(2).toString(); device.model=query.value(3).toString();
        device.firmwareVersion=query.value(4).toString(); device.serialNumber=query.value(5).toString();
        device.hardwareId=query.value(6).toString(); device.activationState=query.value(7).toString();
        device.discoverySources=query.value(8).toString().split(QStringLiteral(", "), Qt::SkipEmptyParts);
        device.isCandidate=query.value(9).toInt()!=0;
        device.userLabel=query.value(10).toString();
        device.reachabilityStatus=query.value(11).toString();
        if (device.reachabilityStatus.isEmpty()) device.reachabilityStatus=QStringLiteral("Not checked");
        device.reachabilityCheckedAt=query.value(12).toString();
        device.lastSuccessfulResponseAt=query.value(13).toString();
        upsertDevice(device);
    }
    loadingInventory_ = false;
    applyFilters();
}

void MainWindow::addManualDevice()
{
    bool accepted=false;
    const QString input=QInputDialog::getText(this,tr("Add device manually"),tr("Camera IPv4 address or hostname:"),QLineEdit::Normal,QString(),&accepted).trimmed();
    if (!accepted) return;
    const QHostAddress address(input);
    if (input.isEmpty() || (address.isNull() && (input.contains(QLatin1Char('/')) || input.contains(QLatin1Char(' '))))) {
        QMessageBox::warning(this,tr("Invalid address"),tr("Enter a valid IP address or hostname."));
        return;
    }
    DeviceRecord device;
    device.ipAddress=address.isNull()?input:address.toString();
    device.discoverySources << QStringLiteral("Manual");
    device.activationState=QStringLiteral("Unknown");
    upsertDevice(device);
    updateStatus(tr("Added %1 manually. No credentials were requested or stored.").arg(device.ipAddress));
}


void MainWindow::applyFilters()
{
    const QString needle = searchEdit_ ? searchEdit_->text().trimmed() : QString();
    const QString vendor = manufacturerFilter_ ? manufacturerFilter_->currentText() : tr("All manufacturers");
    const QString status = statusFilter_ ? statusFilter_->currentText() : tr("All statuses");
    int visible = 0;
    for (int row = 0; row < deviceTable_->rowCount(); ++row) {
        bool textMatch = needle.isEmpty();
        for (int col = 1; col < deviceTable_->columnCount() && !textMatch; ++col) {
            const auto *item = deviceTable_->item(row, col);
            textMatch = item && item->text().contains(needle, Qt::CaseInsensitive);
        }
        const QString source = deviceTable_->item(row, 8) ? deviceTable_->item(row, 8)->text() : QString();
        const QString activation = deviceTable_->item(row, 7) ? deviceTable_->item(row, 7)->text() : QString();
        const bool vendorMatch = vendor == tr("All manufacturers") ||
            (vendor == tr("ONVIF devices") && source.contains(QStringLiteral("ONVIF"), Qt::CaseInsensitive)) ||
            (vendor == tr("Oxita devices") && source.contains(QStringLiteral("Oxita"), Qt::CaseInsensitive)) ||
            (vendor == tr("Other / unknown") && !source.contains(QStringLiteral("ONVIF"), Qt::CaseInsensitive) &&
             !source.contains(QStringLiteral("Oxita"), Qt::CaseInsensitive));
        const QString reachability = deviceTable_->item(row, 11) ? deviceTable_->item(row, 11)->text() : QStringLiteral("Not checked");
        const bool statusMatch = status == tr("All statuses") ||
            (status == tr("Online") && reachability == QStringLiteral("Reachable")) ||
            (status == tr("Activated") && activation.compare(QStringLiteral("Activated"), Qt::CaseInsensitive) == 0) ||
            (status == tr("Not activated") && activation.compare(QStringLiteral("Not activated"), Qt::CaseInsensitive) == 0);
        const bool show = textMatch && vendorMatch && statusMatch;
        deviceTable_->setRowHidden(row, !show);
        if (show) ++visible;
    }
    if (deviceCountValue_) deviceCountValue_->setText(tr("%1 / %2 devices").arg(visible).arg(deviceTable_->rowCount()));
}

void MainWindow::editSelectedLabel()
{
    const int row = deviceTable_->currentRow();
    if (row < 0) {
        updateStatus(tr("Select a device before renaming its label."));
        return;
    }
    const QString current = deviceTable_->item(row, 2) ? deviceTable_->item(row, 2)->text() : QString();
    bool accepted = false;
    const QString label = QInputDialog::getText(this, tr("Rename device label"),
        tr("Friendly device name:"), QLineEdit::Normal, current, &accepted).trimmed();
    if (!accepted) return;
    if (deviceTable_->item(row, 2)) deviceTable_->item(row, 2)->setText(label);
    const QString ip = deviceTable_->item(row, 1) ? deviceTable_->item(row, 1)->text() : QString();
    const QString mac = deviceTable_->item(row, 5) ? deviceTable_->item(row, 5)->text() : QString();
    if (inventoryDb_.isOpen()) {
        QSqlQuery query(inventoryDb_);
        query.prepare(QStringLiteral("UPDATE devices SET user_label=:label WHERE device_key=:key"));
        query.bindValue(":label", label);
        query.bindValue(":key", !mac.isEmpty() ? mac : ip);
        if (!query.exec()) updateStatus(tr("Could not save label: %1").arg(query.lastError().text()));
        else updateStatus(tr("Device label saved."));
    } else {
        updateStatus(tr("Label changed for this session only; inventory database is unavailable."));
    }
    applyFilters();
}


void MainWindow::openSelectedWebInterface()
{
    const int row=deviceTable_->currentRow();
    if (row<0) return;
    const auto *item=deviceTable_->item(row,1);
    if (!item || item->text().isEmpty()) return;
    const QString host=item->text().trimmed();
    const QUrl url=host.startsWith(QStringLiteral("https://"),Qt::CaseInsensitive) || host.startsWith(QStringLiteral("http://"),Qt::CaseInsensitive)
        ? QUrl::fromUserInput(host) : QUrl(QStringLiteral("http://")+host);
    if (!url.isValid() || url.host().isEmpty()) {
        QMessageBox::warning(this,tr("Invalid address"),tr("Selected device has no valid web address."));
        return;
    }
    QDesktopServices::openUrl(url);
}
