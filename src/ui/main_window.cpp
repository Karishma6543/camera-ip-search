#include "main_window.h"

#include "discovery/ip_assignment_planner.h"
#include "discovery/discovery_service.h"
#include "discovery/subnet_scanner.h"
#include "discovery/reachability_checker.h"
#include "adapters/network_config_validator.h"
#include "app/ip_assignment_preflight.h"

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
#include <QTableWidgetItem>
#include <QAbstractItemModel>
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
#include <QAuthenticator>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QHostAddress>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTextStream>
#include <memory>

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
    connect(discovery_, &DiscoveryService::authenticationRequested, this,
            [this](QAuthenticator *authenticator) {
        if (!authenticator) return;
        bool accepted = false;
        const QString username = QInputDialog::getText(this, tr("Camera authentication"),
            tr("Username for %1:").arg(authenticator->realm()), QLineEdit::Normal,
            QString(), &accepted);
        if (!accepted) {
            updateStatus(tr("Camera authentication cancelled; no credentials were stored."));
            return;
        }
        const QString password = QInputDialog::getText(this, tr("Camera authentication"),
            tr("Password:"), QLineEdit::Password, QString(), &accepted);
        if (!accepted) {
            updateStatus(tr("Camera authentication cancelled; no credentials were stored."));
            return;
        }
        authenticator->setUser(username);
        authenticator->setPassword(password);
        updateStatus(tr("Credentials supplied for this application session; they are not saved in inventory or to disk."));
    });
    connect(discovery_, &DiscoveryService::networkConfigurationFinished, this,
            [this](const DeviceRecord &, bool verified, const QString &message) {
        for (auto *button : findChildren<QPushButton *>()) {
            if (button->text() == tr("Apply static IPv4 (ONVIF)")) {
                button->setEnabled(true);
                break;
            }
        }
        QMessageBox::information(this, verified ? tr("Network configuration verified")
                                                : tr("Network configuration result"), message);
        updateStatus(message);
    });
    connect(discovery_, &DiscoveryService::finished, this, [this](int count) {
        scanButton_->setText(tr("Discover devices"));
        scanButton_->setEnabled(true);
        updateDeviceSummary();
        updateStatus(tr("Discovery finished — %1 device(s) found.").arg(count));
    });
    connect(subnetScanner_, &SubnetScanner::candidateFound, this, &MainWindow::upsertDevice);
    connect(subnetScanner_, &SubnetScanner::statusChanged, this, &MainWindow::updateStatus);
    connect(subnetScanner_, &SubnetScanner::progressChanged, this,
            [this](int completed, int total, int candidates) {
        if (total > 0)
            updateStatus(tr("Subnet scan: %1 / %2 connection checks completed; %3 unverified candidate(s) found.")
                             .arg(completed).arg(total).arg(candidates));
    });
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
    auto *exportButton = actionButton(tr("Export CSV"), toolbar);
    tools->addWidget(exportButton);
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

    connect(reachabilityButton_, &QPushButton::clicked, this, [this] {
        if (reachabilityChecker_->isRunning()) {
            reachabilityChecker_->cancel();
            return;
        }
        QVector<DeviceRecord> devices;
        const auto selected = deviceTable_->selectionModel()->selectedRows();
        for (const QModelIndex &index : selected) {
            DeviceRecord device;
            device.ipAddress = deviceTable_->item(index.row(), 1) ? deviceTable_->item(index.row(), 1)->text() : QString();
            if (!device.ipAddress.isEmpty()) devices.append(device);
        }
        if (devices.isEmpty()) {
            for (int row = 0; row < deviceTable_->rowCount(); ++row) {
                DeviceRecord device;
                device.ipAddress = deviceTable_->item(row, 1) ? deviceTable_->item(row, 1)->text() : QString();
                if (!device.ipAddress.isEmpty()) devices.append(device);
            }
        }
        if (devices.isEmpty()) { updateStatus(tr("Add or discover devices before checking reachability.")); return; }
        reachabilityButton_->setEnabled(true);
        reachabilityButton_->setText(tr("Cancel reachability check"));
        updateStatus(tr("Checking TCP reachability. Selected rows only; if none are selected, all devices are checked."));
        reachabilityChecker_->start(devices);
    });
    connect(reachabilityChecker_, &ReachabilityChecker::deviceChecked, this,
        [this](const QString &ip, const QString &status, const QString &checkedAt, const QString &lastSuccessAt) {
            for (int row = 0; row < deviceTable_->rowCount(); ++row) {
                if (!deviceTable_->item(row, 1) || deviceTable_->item(row, 1)->text() != ip) continue;
                deviceTable_->setItem(row, 11, new QTableWidgetItem(status));
                const QString mac = deviceTable_->item(row, 5) ? deviceTable_->item(row, 5)->text() : QString();
                if (inventoryDb_.isOpen()) {
                    QSqlQuery query(inventoryDb_);
                    query.prepare(QStringLiteral("UPDATE devices SET reachability_status=:status, reachability_checked_at=:checked, last_successful_response_at=:success WHERE device_key=:key"));
                    query.bindValue(":status", status);
                    query.bindValue(":checked", checkedAt);
                    query.bindValue(":success", lastSuccessAt);
                    query.bindValue(":key", !mac.isEmpty() ? mac : ip);
                    if (!query.exec()) updateStatus(tr("Could not save reachability result: %1").arg(query.lastError().text()));
                }
                break;
            }
            applyFilters();
        });
    connect(reachabilityChecker_, &ReachabilityChecker::finished, this, [this](int checked) {
        reachabilityButton_->setEnabled(true);
        reachabilityButton_->setText(tr("Check reachability"));
        updateStatus(tr("Reachability check ended — %1 device(s) checked. No response does not prove a device is offline.").arg(checked));
    });
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
    connect(exportButton, &QPushButton::clicked, this, [this] {
        if (!deviceTable_ || deviceTable_->rowCount() == 0) {
            updateStatus(tr("There are no devices to export."));
            return;
        }
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Export device inventory"), QStringLiteral("camera-inventory.csv"),
            tr("CSV files (*.csv)"));
        if (path.isEmpty()) return;

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, tr("Export failed"),
                                 tr("Could not write the CSV file: %1").arg(file.errorString()));
            return;
        }

        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        stream << QChar(0xFEFF);
        const auto csvField = [](QString value) {
            value.replace(QStringLiteral("\""), QStringLiteral("\"\""));
            return QStringLiteral("\"") + value + QStringLiteral("\"");
        };

        QStringList headers;
        for (int column = 1; column < deviceTable_->columnCount(); ++column)
            headers << csvField(deviceTable_->horizontalHeaderItem(column)->text());
        stream << headers.join(QLatin1Char(',')) << Qt::endl;

        int exported = 0;
        for (int row = 0; row < deviceTable_->rowCount(); ++row) {
            QStringList fields;
            for (int column = 1; column < deviceTable_->columnCount(); ++column) {
                const auto *item = deviceTable_->item(row, column);
                fields << csvField(item ? item->text() : QString());
            }
            stream << fields.join(QLatin1Char(',')) << Qt::endl;
            ++exported;
        }
        file.close();
        updateStatus(tr("Exported %1 device(s) to %2.").arg(exported).arg(QFileInfo(path).fileName()));
    });
    connect(openWebButton_, &QPushButton::clicked, this, &MainWindow::openSelectedWebInterface);
    connect(deviceTable_, &QTableWidget::itemSelectionChanged, this, [this] {
        const bool hasSelection = deviceTable_->currentRow() >= 0;
        openWebButton_->setEnabled(hasSelection);
        refreshDeviceButton_->setEnabled(hasSelection);
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
        const auto *ipItem = deviceTable_->item(row, 1);
        if (ipItem) {
            const auto addDetail = [&details, ipItem](int role, const QString &title) {
                const QString value = ipItem->data(role).toString();
                if (!value.isEmpty()) details << title + QStringLiteral(": ") + value;
            };
            addDetail(Qt::UserRole + 1, tr("Network interface"));
            addDetail(Qt::UserRole + 2, tr("ONVIF endpoint"));
            addDetail(Qt::UserRole + 3, tr("Device service URL"));
            addDetail(Qt::UserRole + 4, tr("Network interfaces"));
            addDetail(Qt::UserRole + 5, tr("Verified capabilities"));
            addDetail(Qt::UserRole + 6, tr("Open TCP ports (candidate evidence)"));
            addDetail(Qt::UserRole + 7, tr("ONVIF interface token"));
            addDetail(Qt::UserRole + 8, tr("Camera IPv4 address"));
            addDetail(Qt::UserRole + 9, tr("Camera subnet mask"));
            addDetail(Qt::UserRole + 10, tr("Camera gateway"));
            addDetail(Qt::UserRole + 11, tr("Camera DNS server"));
            addDetail(Qt::UserRole + 12, tr("Camera DHCP enabled"));
        }
        deviceDetailsLabel_->setText(details.join(QLatin1Char('\n')));
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
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(9);

    layout->addWidget(mutedLabel(
        tr("Bulk IPv4 planner — select camera rows in the inventory, create a device-mapped plan, then run the safety preflight. This release is preview-only: no camera settings are changed."),
        page));

    auto *form = new QFormLayout();
    auto *rangeStart = new QLineEdit(page);
    rangeStart->setPlaceholderText(QStringLiteral("192.168.1.100"));
    auto *rangeEnd = new QLineEdit(page);
    rangeEnd->setPlaceholderText(QStringLiteral("192.168.1.150"));
    auto *mask = new QLineEdit(QStringLiteral("255.255.255.0"), page);
    auto *reserved = new QLineEdit(page);
    reserved->setPlaceholderText(tr("Optional comma-separated IPs: gateway, printer, server…"));
    form->addRow(tr("First assignable IPv4"), rangeStart);
    form->addRow(tr("Last assignable IPv4"), rangeEnd);
    form->addRow(tr("Subnet mask"), mask);
    form->addRow(tr("Additional reserved IPs"), reserved);
    layout->addLayout(form);

    auto *selectionInfo = mutedLabel(
        tr("Select camera rows above using Ctrl / Shift-click. Devices need a MAC address or serial number for stable mapping."),
        page);
    layout->addWidget(selectionInfo);

    auto *actions = new QHBoxLayout();
    auto *previewButton = actionButton(tr("Build assignment plan"), page, true);
    auto *preflightButton = actionButton(tr("Run safety preflight"), page);
    preflightButton->setEnabled(false);
    auto *applyButton = actionButton(tr("Apply to cameras"), page, true);
    applyButton->setEnabled(false);
    applyButton->setToolTip(tr("Disabled until verified live conflict probing, adapter write support and recovery handling are available."));
    actions->addWidget(previewButton);
    actions->addWidget(preflightButton);
    actions->addWidget(applyButton);
    actions->addStretch();
    layout->addLayout(actions);

    auto *summary = mutedLabel(tr("No plan generated."), page);
    layout->addWidget(summary);

    auto *results = new QTableWidget(page);
    results->setColumnCount(5);
    results->setHorizontalHeaderLabels({
        tr("DEVICE ID"), tr("CURRENT IP"), tr("PROPOSED IP"),
        tr("PREFLIGHT"), tr("DETAILS")
    });
    results->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    results->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    results->verticalHeader()->setVisible(false);
    results->setSelectionBehavior(QAbstractItemView::SelectRows);
    results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    results->setAlternatingRowColors(true);
    results->setMinimumHeight(180);
    layout->addWidget(results, 1);

    auto *safetyNote = mutedLabel(
        tr("Safety gate: live ARP/neighbor probing is not yet implemented. An unknown or unavailable probe is treated as BLOCKED. The Apply action remains disabled until live conflict detection, adapter write capability and per-device recovery handling are verified."),
        page);
    layout->addWidget(safetyNote);

    auto planState = std::make_shared<QList<IpAssignmentTarget>>();
    auto preflightState = std::make_shared<QVector<IpAssignmentPreflightResult>>();
    auto inventoryState = std::make_shared<QSet<QString>>();
    auto localReservedState = std::make_shared<QSet<QString>>();

    connect(previewButton, &QPushButton::clicked, this,
            [this, rangeStart, rangeEnd, mask, reserved, selectionInfo,
             summary, results, preflightButton, planState, preflightState,
             inventoryState, localReservedState] {
        planState->clear();
        preflightState->clear();
        inventoryState->clear();
        localReservedState->clear();
        results->setRowCount(0);
        preflightButton->setEnabled(false);

        QList<IpAssignmentTarget> selectedDevices;
        QSet<QString> allInventoryAddresses;
        QSet<QString> localReservedAddresses;

        for (int row = 0; row < deviceTable_->rowCount(); ++row) {
            const auto *ip = deviceTable_->item(row, 1);
            const auto *mac = deviceTable_->item(row, 5);
            const auto *serial = deviceTable_->item(row, 9);
            if (ip) {
                QHostAddress inventoryIp;
                if (inventoryIp.setAddress(ip->text().trimmed()) &&
                    inventoryIp.protocol() == QAbstractSocket::IPv4Protocol)
                    allInventoryAddresses.insert(inventoryIp.toString());
                QHostAddress gatewayIp;
                const QString gateway = ip->data(Qt::UserRole + 10).toString().trimmed();
                if (gatewayIp.setAddress(gateway) &&
                    gatewayIp.protocol() == QAbstractSocket::IPv4Protocol)
                    localReservedAddresses.insert(gatewayIp.toString());
            }
            if (!deviceTable_->selectionModel()->isRowSelected(row, QModelIndex()))
                continue;

            if (!ip || ip->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, tr("Missing device address"),
                    tr("Every selected row must have a current IPv4 address."));
                return;
            }
            QString stableId = mac ? mac->text().trimmed() : QString();
            if (stableId.isEmpty() && serial)
                stableId = serial->text().trimmed();
            if (stableId.isEmpty()) {
                QMessageBox::warning(this, tr("Stable identity required"),
                    tr("A selected camera has neither a MAC address nor a serial number. Add/refresh its identity before bulk planning; an IP address alone is not a safe device identity for a migration."));
                return;
            }
            const QString current = ip->data(Qt::UserRole + 8).toString().trimmed().isEmpty()
                ? ip->text().trimmed() : ip->data(Qt::UserRole + 8).toString().trimmed();
            QHostAddress currentAddress;
            if (!currentAddress.setAddress(current) ||
                currentAddress.protocol() != QAbstractSocket::IPv4Protocol) {
                QMessageBox::warning(this, tr("IPv4 address required"),
                    tr("Selected device %1 does not have a valid current IPv4 address.").arg(stableId));
                return;
            }
            selectedDevices.append({stableId, currentAddress.toString(), {}});
        }

        if (selectedDevices.isEmpty()) {
            QMessageBox::information(this, tr("No devices selected"),
                tr("Select one or more camera rows in the inventory first."));
            return;
        }

        for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
            const auto flags = interface.flags();
            if (!flags.testFlag(QNetworkInterface::IsUp) ||
                !flags.testFlag(QNetworkInterface::IsRunning) ||
                flags.testFlag(QNetworkInterface::IsLoopBack))
                continue;
            for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
                const QHostAddress address = entry.ip();
                if (address.protocol() == QAbstractSocket::IPv4Protocol &&
                    !address.isLoopback() && !address.isNull())
                    localReservedAddresses.insert(address.toString());
            }
        }

        const QStringList extra = reserved->text().split(
            QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
        for (const QString &address : extra)
            localReservedAddresses.insert(address.trimmed());

        *inventoryState = allInventoryAddresses;
        *localReservedState = localReservedAddresses;
        QSet<QString> allReserved = allInventoryAddresses;
        allReserved.unite(localReservedAddresses);

        QString error;
        if (!IpAssignmentPlanner::buildDevicePlan(
                rangeStart->text(), rangeEnd->text(), mask->text(),
                selectedDevices, allReserved, planState.get(), &error)) {
            summary->setText(tr("Plan rejected: %1").arg(error));
            QMessageBox::warning(this, tr("Cannot create IP plan"), error);
            return;
        }

        for (const IpAssignmentTarget &target : *planState) {
            const int row = results->rowCount();
            results->insertRow(row);
            results->setItem(row, 0, new QTableWidgetItem(target.deviceId));
            results->setItem(row, 1, new QTableWidgetItem(target.currentAddress));
            results->setItem(row, 2, new QTableWidgetItem(target.proposedAddress));
            results->setItem(row, 3, new QTableWidgetItem(tr("Not checked")));
            results->setItem(row, 4, new QTableWidgetItem(tr("Plan generated; no network probe or write performed.")));
        }
        selectionInfo->setText(tr("%1 device(s) selected and mapped in inventory order.")
                                   .arg(planState->size()));
        summary->setText(tr("Plan ready: %1 target(s). Inventory IPs, known camera gateways and active local IPv4 addresses are reserved.").arg(planState->size()));
        preflightButton->setEnabled(true);
    });

    connect(preflightButton, &QPushButton::clicked, this,
            [summary, results, planState, preflightState,
             inventoryState, localReservedState] {
        // The plan builder already reserved these addresses. Repeat the
        // checks at the preflight boundary as defense in depth.
        *preflightState = IpAssignmentPreflight::evaluate(
            *planState, *inventoryState, *localReservedState, {});
        int ready = 0;
        for (int i = 0; i < preflightState->size(); ++i) {
            const auto &check = preflightState->at(i);
            const bool isReady = check.state == IpAssignmentPreflightState::Ready;
            if (isReady) ++ready;
            results->item(i, 3)->setText(isReady ? tr("READY") :
                (check.state == IpAssignmentPreflightState::AddressConflict ? tr("CONFLICT") :
                 check.state == IpAssignmentPreflightState::InvalidPlan ? tr("INVALID") : tr("BLOCKED")));
            results->item(i, 4)->setText(check.message);
        }
        summary->setText(tr("Safety preflight: %1 / %2 ready. No live probe is configured, so all targets remain blocked from apply.")
                             .arg(ready).arg(preflightState->size()));
    });

    return page;
}

QWidget *MainWindow::makeMaintenanceTab(QWidget *parent)
{
    auto *page = new QWidget(parent);
    auto *layout = new QHBoxLayout(page);
    auto *reboot = actionButton(tr("Reboot selected"), page); reboot->setEnabled(false);
    refreshDeviceButton_ = actionButton(tr("Refresh device info"), page);
    refreshDeviceButton_->setEnabled(false);
    openWebButton_ = actionButton(tr("Open web interface"), page);
    openWebButton_->setEnabled(false);
    auto *web = openWebButton_;
    layout->addWidget(reboot); layout->addWidget(refreshDeviceButton_); layout->addWidget(web); layout->addStretch();
    auto *previewNetworkButton = actionButton(tr("Preview IP configuration"), page);
    layout->addWidget(previewNetworkButton);
    auto *applyNetworkButton = actionButton(tr("Apply static IPv4 (ONVIF)"), page, true);
    layout->addWidget(applyNetworkButton);
    connect(applyNetworkButton, &QPushButton::clicked, this, [this, applyNetworkButton] {
        const int row = deviceTable_->currentRow();
        if (row < 0) {
            QMessageBox::information(this, tr("IP configuration"), tr("Select one camera first."));
            return;
        }
        const auto *ipItem = deviceTable_->item(row, 1);
        if (!ipItem) return;
        DeviceRecord device;
        device.ipAddress = ipItem->data(Qt::UserRole + 8).toString();
        if (device.ipAddress.isEmpty()) device.ipAddress = ipItem->text();
        device.networkInterfaceToken = ipItem->data(Qt::UserRole + 7).toString();
        device.ipv4Address = ipItem->data(Qt::UserRole + 8).toString();
        device.ipv4SubnetMask = ipItem->data(Qt::UserRole + 9).toString();
        device.ipv4Gateway = ipItem->data(Qt::UserRole + 10).toString();
        device.ipv4DnsServer = ipItem->data(Qt::UserRole + 11).toString();
        device.deviceServiceUrl = ipItem->data(Qt::UserRole + 3).toString();
        device.macAddress = deviceTable_->item(row, 5) ? deviceTable_->item(row, 5)->text() : QString();
        device.manufacturer = deviceTable_->item(row, 3) ? deviceTable_->item(row, 3)->text() : QString();
        device.model = deviceTable_->item(row, 4) ? deviceTable_->item(row, 4)->text() : QString();
        if (device.networkInterfaceToken.isEmpty() || device.deviceServiceUrl.isEmpty() ||
            device.ipv4Address.isEmpty() || device.ipv4SubnetMask.isEmpty()) {
            QMessageBox::warning(this, tr("ONVIF configuration unavailable"),
                tr("Refresh this camera first. A verified ONVIF interface token, service URL, current IP and subnet mask are required."));
            return;
        }
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Apply static IPv4 configuration"));
        dialog.setMinimumWidth(440);
        auto *form = new QFormLayout(&dialog);
        auto *address = new QLineEdit(device.ipv4Address, &dialog);
        auto *mask = new QLineEdit(device.ipv4SubnetMask, &dialog);
        auto *gateway = new QLineEdit(device.ipv4Gateway, &dialog);
        form->addRow(tr("Current camera IP"), new QLabel(device.ipAddress, &dialog));
        form->addRow(tr("New IPv4 address"), address);
        form->addRow(tr("Subnet mask"), mask);
        form->addRow(tr("Gateway (optional)"), gateway);
        form->addRow(mutedLabel(tr("DNS is not modified by this operation. The app will reconnect to the new IP and verify the reported interface address. A failed reconnect may require manual network recovery or a camera reset."), &dialog));
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Cancel, &dialog);
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
            QString error;
            if (!NetworkConfigValidator::validateStaticIpv4(address->text(), mask->text(),
                    gateway->text(), QString(), &error)) {
                QMessageBox::warning(&dialog, tr("Invalid network configuration"), error);
                return;
            }
            if (address->text().trimmed() == device.ipAddress) {
                QMessageBox::information(&dialog, tr("No IP change"), tr("The new address is the same as the current address."));
                return;
            }
            const auto answer = QMessageBox::warning(&dialog, tr("Confirm camera IP change"),
                tr("Change camera %1 to %2?\n\nThe camera may disconnect immediately. Ensure your computer can reach the new subnet. This action has no automatic rollback.")
                    .arg(device.ipAddress, address->text().trimmed()),
                QMessageBox::Apply | QMessageBox::Cancel, QMessageBox::Cancel);
            if (answer != QMessageBox::Apply) return;
            dialog.accept();
            applyNetworkButton->setEnabled(false);
            discovery_->applyStaticIpv4(device, address->text(), mask->text(), gateway->text());
        });
        dialog.exec();
    });
    connect(previewNetworkButton, &QPushButton::clicked, this, [this] {
        const int row = deviceTable_->currentRow();
        if (row < 0) {
            QMessageBox::information(this, tr("IP configuration"), tr("Select one camera first."));
            return;
        }
        const auto *ipItem = deviceTable_->item(row, 1);
        if (!ipItem) return;
        const QString token = ipItem->data(Qt::UserRole + 7).toString();
        const QString currentIp = ipItem->data(Qt::UserRole + 8).toString();
        const QString currentMask = ipItem->data(Qt::UserRole + 9).toString();
        if (token.isEmpty() || currentIp.isEmpty() || currentMask.isEmpty()) {
            QMessageBox::warning(this, tr("IP configuration unavailable"),
                tr("Refresh this camera and confirm it exposes ONVIF network-interface details before preparing a configuration."));
            return;
        }

        QDialog dialog(this);
        dialog.setWindowTitle(tr("Preview static IPv4 configuration"));
        dialog.setMinimumWidth(430);
        auto *form = new QFormLayout(&dialog);
        auto *current = new QLabel(
            tr("Current: %1 / %2\nGateway: %3\nDNS: %4\nInterface token: %5")
                .arg(currentIp, currentMask,
                     ipItem->data(Qt::UserRole + 10).toString().isEmpty()
                         ? tr("not reported") : ipItem->data(Qt::UserRole + 10).toString(),
                     ipItem->data(Qt::UserRole + 11).toString().isEmpty()
                         ? tr("not reported") : ipItem->data(Qt::UserRole + 11).toString(),
                     token), &dialog);
        current->setWordWrap(true);
        auto *address = new QLineEdit(currentIp, &dialog);
        auto *mask = new QLineEdit(currentMask, &dialog);
        auto *gateway = new QLineEdit(ipItem->data(Qt::UserRole + 10).toString(), &dialog);
        auto *dns = new QLineEdit(ipItem->data(Qt::UserRole + 11).toString(), &dialog);
        form->addRow(tr("Current camera settings"), current);
        form->addRow(tr("Proposed IPv4 address"), address);
        form->addRow(tr("Subnet mask"), mask);
        form->addRow(tr("Gateway (optional)"), gateway);
        form->addRow(tr("DNS server (optional)"), dns);
        form->addRow(mutedLabel(tr("This is a validation and preview only. It does not send SetNetworkInterfaces or change the camera."), &dialog));
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        buttons->button(QDialogButtonBox::Ok)->setText(tr("Validate preview"));
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, address, mask, gateway, dns] {
            QString error;
            if (!NetworkConfigValidator::validateStaticIpv4(
                    address->text(), mask->text(), gateway->text(), dns->text(), &error)) {
                QMessageBox::warning(&dialog, QObject::tr("Invalid network configuration"), error);
                return;
            }
            QMessageBox::information(&dialog, QObject::tr("Configuration validated"),
                QObject::tr("The proposed static IPv4 values passed local validation. No network request was sent and the camera has not been changed."));
        });
        dialog.exec();
    });
    connect(refreshDeviceButton_, &QPushButton::clicked, this, [this] {
        const int row = deviceTable_->currentRow();
        if (row < 0) return;
        const auto *ip = deviceTable_->item(row, 1);
        if (!ip) return;
        DeviceRecord device;
        device.ipAddress = ip->text();
        device.interfaceName = ip->data(Qt::UserRole + 1).toString();
        device.onvifEndpoint = ip->data(Qt::UserRole + 2).toString();
        device.deviceServiceUrl = ip->data(Qt::UserRole + 3).toString();
        device.networkSummary = ip->data(Qt::UserRole + 4).toString();
        device.capabilities = ip->data(Qt::UserRole + 5).toString().split(QStringLiteral(", "), Qt::SkipEmptyParts);
        device.openPorts = ip->data(Qt::UserRole + 6).toString().split(QStringLiteral(", "), Qt::SkipEmptyParts);
        device.networkInterfaceToken = ip->data(Qt::UserRole + 7).toString();
        device.ipv4Address = ip->data(Qt::UserRole + 8).toString();
        device.ipv4SubnetMask = ip->data(Qt::UserRole + 9).toString();
        device.ipv4Gateway = ip->data(Qt::UserRole + 10).toString();
        device.ipv4DnsServer = ip->data(Qt::UserRole + 11).toString();
        device.ipv4DhcpEnabled = ip->data(Qt::UserRole + 12).toString() == QStringLiteral("true");
        device.ipv4ConfigurationKnown = !device.networkInterfaceToken.isEmpty();
        device.userLabel = deviceTable_->item(row, 2) ? deviceTable_->item(row, 2)->text() : QString();
        device.manufacturer = deviceTable_->item(row, 3) ? deviceTable_->item(row, 3)->text() : QString();
        device.model = deviceTable_->item(row, 4) ? deviceTable_->item(row, 4)->text() : QString();
        device.macAddress = deviceTable_->item(row, 5) ? deviceTable_->item(row, 5)->text() : QString();
        device.firmwareVersion = deviceTable_->item(row, 6) ? deviceTable_->item(row, 6)->text() : QString();
        device.activationState = deviceTable_->item(row, 7) ? deviceTable_->item(row, 7)->text() : QStringLiteral("Unknown");
        device.discoverySources = deviceTable_->item(row, 8)
            ? deviceTable_->item(row, 8)->text().split(QStringLiteral(", "), Qt::SkipEmptyParts) : QStringList{};
        device.serialNumber = deviceTable_->item(row, 9) ? deviceTable_->item(row, 9)->text() : QString();
        device.hardwareId = deviceTable_->item(row, 10) ? deviceTable_->item(row, 10)->text() : QString();
        discovery_->refreshDeviceInfo(device);
    });
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
    const auto *previousIpItem = deviceTable_->item(row, 1);
    auto *ipItem = new QTableWidgetItem(device.ipAddress);
    const auto keepExistingWhenEmpty = [previousIpItem, ipItem](int role, const QString &incoming) {
        const QString value = incoming.isEmpty() && previousIpItem
            ? previousIpItem->data(role).toString() : incoming;
        ipItem->setData(role, value);
    };
    keepExistingWhenEmpty(Qt::UserRole + 1, device.interfaceName);
    keepExistingWhenEmpty(Qt::UserRole + 2, device.onvifEndpoint);
    keepExistingWhenEmpty(Qt::UserRole + 3, device.deviceServiceUrl);
    keepExistingWhenEmpty(Qt::UserRole + 4, device.networkSummary);
    keepExistingWhenEmpty(Qt::UserRole + 5, device.capabilities.join(QStringLiteral(", ")));
    keepExistingWhenEmpty(Qt::UserRole + 6, device.openPorts.join(QStringLiteral(", ")));
    keepExistingWhenEmpty(Qt::UserRole + 7, device.networkInterfaceToken);
    keepExistingWhenEmpty(Qt::UserRole + 8, device.ipv4Address);
    keepExistingWhenEmpty(Qt::UserRole + 9, device.ipv4SubnetMask);
    keepExistingWhenEmpty(Qt::UserRole + 10, device.ipv4Gateway);
    keepExistingWhenEmpty(Qt::UserRole + 11, device.ipv4DnsServer);
    ipItem->setData(Qt::UserRole + 12, device.ipv4ConfigurationKnown
        ? (device.ipv4DhcpEnabled ? QStringLiteral("true") : QStringLiteral("false"))
        : (previousIpItem ? previousIpItem->data(Qt::UserRole + 12) : QVariant()));
    const QStringList values = {
        device.ipAddress, !device.userLabel.isEmpty() ? device.userLabel : (oldLabel.isEmpty() && device.isCandidate ? QStringLiteral("Unverified candidate") : oldLabel),
        device.manufacturer, device.model, device.macAddress,
        device.firmwareVersion, device.activationState,
        device.discoverySources.join(QStringLiteral(", ")), device.serialNumber, device.hardwareId,
        (device.reachabilityStatus == QStringLiteral("Not checked") && deviceTable_->item(row, 11)
            ? deviceTable_->item(row, 11)->text() : device.reachabilityStatus)
    };
    for (int col = 0; col < values.size(); ++col)
        deviceTable_->setItem(row, col + 1, col == 0 ? ipItem : new QTableWidgetItem(values.at(col)));
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
    query.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN interface_name TEXT DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN onvif_endpoint TEXT DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN device_service_url TEXT DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN network_summary TEXT DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN capabilities TEXT DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN open_ports TEXT DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN network_interface_token TEXT DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN ipv4_address TEXT DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN ipv4_subnet_mask TEXT DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN ipv4_gateway TEXT DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN ipv4_dns_server TEXT DEFAULT ''"));
    query.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN ipv4_dhcp_enabled INTEGER DEFAULT 0"));
    query.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN ipv4_config_known INTEGER DEFAULT 0"));
}

void MainWindow::persistDevice(const DeviceRecord &device)
{
    if (!inventoryDb_.isOpen() || device.ipAddress.isEmpty()) return;
    const QString key = !device.macAddress.isEmpty() ? device.macAddress : device.ipAddress;
    QSqlQuery query(inventoryDb_);
    query.prepare(QStringLiteral("INSERT INTO devices (device_key,ip,mac,manufacturer,model,firmware,serial,hardware,activation,source,is_candidate,user_label,reachability_status,reachability_checked_at,last_successful_response_at,interface_name,onvif_endpoint,device_service_url,network_summary,capabilities,open_ports,network_interface_token,ipv4_address,ipv4_subnet_mask,ipv4_gateway,ipv4_dns_server,ipv4_dhcp_enabled,ipv4_config_known,last_seen) VALUES (:key,:ip,:mac,:manufacturer,:model,:firmware,:serial,:hardware,:activation,:source,:candidate,:label,:reachability,:checked,:success,:interface,:endpoint,:service,:network,:capabilities,:ports,:if_token,:ipv4,:mask,:gateway,:dns,:dhcp,:config_known,CURRENT_TIMESTAMP) ON CONFLICT(device_key) DO UPDATE SET ip=excluded.ip,mac=excluded.mac,manufacturer=excluded.manufacturer,model=excluded.model,firmware=excluded.firmware,serial=excluded.serial,hardware=excluded.hardware,activation=excluded.activation,source=excluded.source,is_candidate=excluded.is_candidate,user_label=CASE WHEN excluded.user_label='' THEN devices.user_label ELSE excluded.user_label END,reachability_status=CASE WHEN excluded.reachability_status='Not checked' THEN devices.reachability_status ELSE excluded.reachability_status END,reachability_checked_at=CASE WHEN excluded.reachability_checked_at='' THEN devices.reachability_checked_at ELSE excluded.reachability_checked_at END,last_successful_response_at=CASE WHEN excluded.last_successful_response_at='' THEN devices.last_successful_response_at ELSE excluded.last_successful_response_at END,interface_name=CASE WHEN excluded.interface_name='' THEN devices.interface_name ELSE excluded.interface_name END,onvif_endpoint=CASE WHEN excluded.onvif_endpoint='' THEN devices.onvif_endpoint ELSE excluded.onvif_endpoint END,device_service_url=CASE WHEN excluded.device_service_url='' THEN devices.device_service_url ELSE excluded.device_service_url END,network_summary=CASE WHEN excluded.network_summary='' THEN devices.network_summary ELSE excluded.network_summary END,capabilities=CASE WHEN excluded.capabilities='' THEN devices.capabilities ELSE excluded.capabilities END,open_ports=CASE WHEN excluded.open_ports='' THEN devices.open_ports ELSE excluded.open_ports END,network_interface_token=CASE WHEN excluded.network_interface_token='' THEN devices.network_interface_token ELSE excluded.network_interface_token END,ipv4_address=CASE WHEN excluded.ipv4_address='' THEN devices.ipv4_address ELSE excluded.ipv4_address END,ipv4_subnet_mask=CASE WHEN excluded.ipv4_subnet_mask='' THEN devices.ipv4_subnet_mask ELSE excluded.ipv4_subnet_mask END,ipv4_gateway=CASE WHEN excluded.ipv4_gateway='' THEN devices.ipv4_gateway ELSE excluded.ipv4_gateway END,ipv4_dns_server=CASE WHEN excluded.ipv4_dns_server='' THEN devices.ipv4_dns_server ELSE excluded.ipv4_dns_server END,ipv4_dhcp_enabled=CASE WHEN excluded.ipv4_config_known=0 THEN devices.ipv4_dhcp_enabled ELSE excluded.ipv4_dhcp_enabled END,ipv4_config_known=MAX(devices.ipv4_config_known,excluded.ipv4_config_known),last_seen=CURRENT_TIMESTAMP"));
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
    query.bindValue(":interface", device.interfaceName);
    query.bindValue(":endpoint", device.onvifEndpoint);
    query.bindValue(":service", device.deviceServiceUrl);
    query.bindValue(":network", device.networkSummary);
    query.bindValue(":capabilities", device.capabilities.join(QStringLiteral(", ")));
    query.bindValue(":ports", device.openPorts.join(QStringLiteral(", ")));
    query.bindValue(":if_token", device.networkInterfaceToken);
    query.bindValue(":ipv4", device.ipv4Address);
    query.bindValue(":mask", device.ipv4SubnetMask);
    query.bindValue(":gateway", device.ipv4Gateway);
    query.bindValue(":dns", device.ipv4DnsServer);
    query.bindValue(":dhcp", device.ipv4DhcpEnabled ? 1 : 0);
    query.bindValue(":config_known", device.ipv4ConfigurationKnown ? 1 : 0);
    if (!query.exec()) updateStatus(tr("Inventory save failed: %1").arg(query.lastError().text()));
}

void MainWindow::loadInventory()
{
    if (!inventoryDb_.isOpen()) return;
    QSqlQuery query(inventoryDb_);
    if (!query.exec(QStringLiteral("SELECT ip,mac,manufacturer,model,firmware,serial,hardware,activation,source,is_candidate,user_label,reachability_status,reachability_checked_at,last_successful_response_at,interface_name,onvif_endpoint,device_service_url,network_summary,capabilities,open_ports,network_interface_token,ipv4_address,ipv4_subnet_mask,ipv4_gateway,ipv4_dns_server,ipv4_dhcp_enabled,ipv4_config_known FROM devices ORDER BY last_seen DESC"))) return;
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
        device.interfaceName=query.value(14).toString();
        device.onvifEndpoint=query.value(15).toString();
        device.deviceServiceUrl=query.value(16).toString();
        device.networkSummary=query.value(17).toString();
        device.capabilities=query.value(18).toString().split(QStringLiteral(", "), Qt::SkipEmptyParts);
        device.openPorts=query.value(19).toString().split(QStringLiteral(", "), Qt::SkipEmptyParts);
        device.networkInterfaceToken=query.value(20).toString();
        device.ipv4Address=query.value(21).toString();
        device.ipv4SubnetMask=query.value(22).toString();
        device.ipv4Gateway=query.value(23).toString();
        device.ipv4DnsServer=query.value(24).toString();
        device.ipv4DhcpEnabled=query.value(25).toInt()!=0;
        device.ipv4ConfigurationKnown=query.value(26).toInt()!=0;
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
