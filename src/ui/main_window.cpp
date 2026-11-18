#include "main_window.h"

#include "discovery/discovery_service.h"
#include "discovery/subnet_scanner.h"

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
    buildUi();
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
    auto *vendorFilter = new QComboBox(toolbar);
    vendorFilter->addItems({tr("All manufacturers"), tr("ONVIF devices"), tr("Oxita devices"), tr("Other / unknown")});
    auto *statusFilter = new QComboBox(toolbar);
    statusFilter->addItems({tr("All statuses"), tr("Online"), tr("Activated"), tr("Not activated")});
    deviceCountValue_ = new QLabel(QStringLiteral("0 devices"), toolbar);
    deviceCountValue_->setObjectName(QStringLiteral("metric"));
    tools->addWidget(searchEdit_, 2);
    tools->addWidget(interfaceSelector_);
    tools->addWidget(vendorFilter);
    tools->addWidget(statusFilter);
    tools->addStretch();
    tools->addWidget(deviceCountValue_);
    scanButton_ = actionButton(tr("Discover devices"), toolbar, true);
    tools->addWidget(scanButton_);
    subnetScanButton_ = actionButton(tr("Scan selected subnet"), toolbar);
    tools->addWidget(subnetScanButton_);
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

    deviceTable_ = new QTableWidget(0, 9, left);
    deviceTable_->setHorizontalHeaderLabels({tr(""), tr("IP ADDRESS"), tr("DESCRIPTION"), tr("MANUFACTURER"), tr("PRODUCT SERIES / MODEL"), tr("MAC ADDRESS"), tr("FIRMWARE"), tr("STATUS"), tr("SOURCE")});
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
    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString &value) {
        const QString needle = value.trimmed();
        for (int row = 0; row < deviceTable_->rowCount(); ++row) {
            bool match = needle.isEmpty();
            for (int col = 1; col < deviceTable_->columnCount() && !match; ++col) {
                const auto *item = deviceTable_->item(row, col);
                match = item && item->text().contains(needle, Qt::CaseInsensitive);
            }
            deviceTable_->setRowHidden(row, !match);
        }
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
    auto *web = actionButton(tr("Open web interface"), page); web->setEnabled(false);
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
    auto *check = new QTableWidgetItem();
    check->setFlags(check->flags() | Qt::ItemIsUserCheckable);
    check->setCheckState(Qt::Unchecked);
    deviceTable_->setItem(row, 0, check);
    const QStringList values = {
        device.ipAddress, device.isCandidate ? QStringLiteral("Unverified candidate") : QString(),
        device.manufacturer, device.model, device.macAddress,
        device.firmwareVersion, device.activationState,
        device.discoverySources.join(QStringLiteral(", "))
    };
    for (int col = 0; col < values.size(); ++col)
        deviceTable_->setItem(row, col + 1, new QTableWidgetItem(values.at(col)));
    updateDeviceSummary();
}
