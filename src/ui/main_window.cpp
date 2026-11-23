#include "main_window.h"
#include "core/diagnostic_log.h"
#include "core/device_identity.h"

#include "discovery/discovery_service.h"
#include "discovery/subnet_scanner.h"
#include "discovery/reachability_checker.h"
#include "adapters/network_config_validator.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QColor>
#include <QFont>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
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
#include <QAuthenticator>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QHostAddress>
#include <QClipboard>
#include <QApplication>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QStringConverter>
#include <QTextStream>
#include <QDateTime>
#include <QSysInfo>
#include <QSaveFile>
#include <QRegularExpression>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QPaintEvent>
#include "oxita_theme.h"

namespace {
class OxitaMark final : public QWidget
{
public:
    explicit OxitaMark(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(42, 42);
        setAttribute(Qt::WA_TranslucentBackground);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath outerSwirl;
        outerSwirl.moveTo(5.5, 25.5);
        outerSwirl.cubicTo(1.5, 14.5, 9.5, 3.5, 21.0, 3.5);
        outerSwirl.cubicTo(33.0, 3.5, 41.0, 13.5, 37.5, 25.0);
        outerSwirl.cubicTo(34.0, 36.5, 20.0, 42.0, 9.5, 33.0);
        painter.setPen(QPen(QColor(QStringLiteral("#C92D3B")), 5.2,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(outerSwirl);

        QPainterPath innerSwirl;
        innerSwirl.moveTo(11.5, 26.0);
        innerSwirl.cubicTo(8.0, 18.0, 14.0, 10.0, 22.0, 9.5);
        innerSwirl.cubicTo(30.0, 9.0, 35.5, 16.0, 32.0, 23.5);
        innerSwirl.cubicTo(29.5, 29.5, 22.0, 33.0, 16.5, 28.5);
        painter.setPen(QPen(QColor(QStringLiteral("#D84B56")), 3.2,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(innerSwirl);

        QRadialGradient core(QPointF(20.5, 19.5), 8.5, QPointF(17.5, 16.5));
        core.setColorAt(0.0, QColor(QStringLiteral("#D7D9DE")));
        core.setColorAt(0.55, QColor(QStringLiteral("#9297A0")));
        core.setColorAt(1.0, QColor(QStringLiteral("#555B65")));
        painter.setPen(QPen(Qt::white, 0.8));
        painter.setBrush(core);
        painter.drawEllipse(QRectF(13.5, 12.5, 15.5, 15.5));
    }
};
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
    refreshOperationHistory();
    populateInterfaces();
    updateLocalNetworkSummary();
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
        recordOperation(tr("Static IPv4"), updatedDevice.ipAddress,
                        verified ? tr("Verified") : tr("Not verified"), message);
        refreshOperationHistory();
        QMessageBox::information(this, verified ? tr("Network configuration verified")
                                                : tr("Network configuration result"), message);
        updateStatus(message);
    });
    connect(discovery_, &DiscoveryService::cancelled, this, [this](int count) {
        scanButton_->setText(tr("Discover devices"));
        scanButton_->setEnabled(true);
        recordOperation(tr("Discovery"), interfaceSelector_ ? interfaceSelector_->currentText() : QString(),
                        tr("Cancelled"), tr("%1 device(s) found before cancellation").arg(count));
        refreshOperationHistory();
        updateStatus(tr("Discovery cancelled — %1 device(s) retained.").arg(count));
    });
    connect(discovery_, &DiscoveryService::finished, this, [this](int count) {
        scanButton_->setText(tr("Discover devices"));
        scanButton_->setEnabled(true);
        updateDeviceSummary();
        recordOperation(tr("Discovery"), interfaceSelector_ ? interfaceSelector_->currentText() : QString(), tr("Completed"), tr("%1 device(s) reported").arg(count));
        refreshOperationHistory();
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
        recordOperation(tr("Subnet scan"), interfaceSelector_ ? interfaceSelector_->currentText() : QString(), tr("Completed"), tr("%1 unverified candidate(s)").arg(count));
        refreshOperationHistory();
        updateStatus(tr("Subnet scan finished — %1 unverified candidate(s).").arg(count));
    });
}

MainWindow::~MainWindow()
{
    if (discovery_) discovery_->cancel();
    if (subnetScanner_) subnetScanner_->cancel();
    if (reachabilityChecker_) reachabilityChecker_->cancel();
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

    setStyleSheet(OxitaTheme::styleSheet());

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("appRoot"));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(22, 20, 22, 14);
    root->setSpacing(14);

    auto *top = new QFrame(central);
    top->setObjectName(QStringLiteral("topBar"));
    auto *topLayout = new QHBoxLayout(top);
    topLayout->setContentsMargins(20, 14, 20, 14);
    topLayout->setSpacing(10);
    auto *mark = new OxitaMark(top);
    auto *brand = new QLabel(tr("OXITA®"), top);
    brand->setObjectName(QStringLiteral("brand"));
    auto *productName = new QLabel(tr("CAMERA DEVICE MANAGER"), top);
    productName->setObjectName(QStringLiteral("productName"));
    auto *brandStack = new QVBoxLayout();
    brandStack->setSpacing(1);
    brandStack->addWidget(brand);
    brandStack->addWidget(productName);
    topLayout->addWidget(mark);
    topLayout->addSpacing(5);
    topLayout->addLayout(brandStack);
    topLayout->addStretch();
    auto *brandTagline = mutedLabel(tr("DRIVEN BY INNOVATION"), top);
    brandTagline->setObjectName(QStringLiteral("brandTagline"));
    topLayout->addWidget(brandTagline);
    topLayout->addSpacing(18);
    topLayout->addStretch();
    auto *help = actionButton(tr("Help"), top);
    auto *settings = actionButton(tr("Settings"), top);
    topLayout->addWidget(help);
    topLayout->addWidget(settings);
    root->addWidget(top);

    auto *toolbar = new QFrame(central);
    toolbar->setObjectName(QStringLiteral("toolbar"));
    auto *tools = new QVBoxLayout(toolbar);
    tools->setContentsMargins(16, 13, 16, 13);
    tools->setSpacing(11);

    auto *filterRow = new QHBoxLayout();
    filterRow->setSpacing(9);
    searchEdit_ = new QLineEdit(toolbar);
    searchEdit_->setPlaceholderText(tr("Search by IP address, MAC, model or manufacturer"));
    searchEdit_->setMinimumWidth(250);
    searchEdit_->setClearButtonEnabled(true);
    interfaceSelector_ = new QComboBox(toolbar);
    interfaceSelector_->setMinimumWidth(175);
    manufacturerFilter_ = new QComboBox(toolbar);
    auto *vendorFilter = manufacturerFilter_;
    vendorFilter->addItems({tr("All manufacturers"), tr("ONVIF devices"), tr("Oxita devices"), tr("Other / unknown")});
    statusFilter_ = new QComboBox(toolbar);
    auto *statusFilter = statusFilter_;
    statusFilter->addItems({tr("All statuses"), tr("Online"), tr("Activated"), tr("Not activated")});
    deviceCountValue_ = new QLabel(QStringLiteral("0 devices"), toolbar);
    deviceCountValue_->setObjectName(QStringLiteral("metric"));
    filterRow->addWidget(searchEdit_, 2);
    filterRow->addWidget(interfaceSelector_);
    connect(interfaceSelector_, &QComboBox::currentIndexChanged, this,
            [this](int) { updateLocalNetworkSummary(); });
    filterRow->addWidget(vendorFilter);
    filterRow->addWidget(statusFilter);
    filterRow->addStretch();
    filterRow->addWidget(deviceCountValue_);
    tools->addLayout(filterRow);

    auto *actionRow = new QHBoxLayout();
    actionRow->setSpacing(8);
    addDeviceButton_ = actionButton(tr("Add device"), toolbar);
    actionRow->addWidget(addDeviceButton_);
    auto *exportButton = actionButton(tr("Export CSV"), toolbar);
    actionRow->addWidget(exportButton);
    auto *renameButton = actionButton(tr("Rename label"), toolbar);
    actionRow->addWidget(renameButton);
    actionRow->addStretch();
    reachabilityButton_ = actionButton(tr("Check reachability"), toolbar);
    actionRow->addWidget(reachabilityButton_);
    subnetScanButton_ = actionButton(tr("Scan selected subnet"), toolbar);
    actionRow->addWidget(subnetScanButton_);
    scanButton_ = actionButton(tr("Discover devices"), toolbar, true);
    actionRow->addWidget(scanButton_);
    tools->addLayout(actionRow);
    root->addWidget(toolbar);

    // At-a-glance inventory metrics keep the dashboard useful before and after discovery.
    auto *metricsRow = new QHBoxLayout();
    metricsRow->setSpacing(10);
    const auto addMetricCard = [central, metricsRow](const QString &caption, QLabel **valueLabel) {
        auto *card = new QFrame(central);
        card->setObjectName(QStringLiteral("metricCard"));
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(15, 11, 15, 12);
        layout->setSpacing(4);
        auto *eyebrow = new QLabel(caption, card);
        eyebrow->setObjectName(QStringLiteral("eyebrow"));
        auto *value = new QLabel(QStringLiteral("0"), card);
        value->setObjectName(QStringLiteral("metric"));
        layout->addWidget(eyebrow);
        layout->addWidget(value);
        metricsRow->addWidget(card, 1);
        *valueLabel = value;
    };
    addMetricCard(tr("DISCOVERED DEVICES"), &discoveredMetric_);
    addMetricCard(tr("REACHABLE"), &reachableMetric_);
    addMetricCard(tr("ONVIF VERIFIED"), &verifiedMetric_);
    addMetricCard(tr("NEEDS REVIEW"), &unverifiedMetric_);
    root->addLayout(metricsRow);

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
    auto *selectAllChecks = actionButton(tr("Select all"), left);
    auto *clearChecks = actionButton(tr("Clear"), left);
    selectAllChecks->setToolTip(tr("Check every device for the next reachability operation."));
    clearChecks->setToolTip(tr("Clear all device checkboxes."));
    listHeading->addWidget(listTitle);
    listHeading->addStretch();
    listHeading->addWidget(summaryLabel_);
    listHeading->addWidget(selectAllChecks);
    listHeading->addWidget(clearChecks);
    leftLayout->addLayout(listHeading);

    deviceTable_ = new QTableWidget(0, 12, left);
    deviceTable_->setHorizontalHeaderLabels({tr(""), tr("IP ADDRESS"), tr("DESCRIPTION"), tr("MANUFACTURER"), tr("PRODUCT SERIES / MODEL"), tr("MAC ADDRESS"), tr("FIRMWARE"), tr("STATUS"), tr("SOURCE"), tr("SERIAL NUMBER"), tr("HARDWARE ID"), tr("REACHABILITY")});
    auto *deviceHeader = deviceTable_->horizontalHeader();
    deviceHeader->setSectionResizeMode(QHeaderView::Interactive);
    deviceHeader->setSectionResizeMode(0, QHeaderView::Fixed);
    deviceHeader->resizeSection(0, 34);
    deviceHeader->resizeSection(1, 128);
    deviceHeader->resizeSection(2, 160);
    deviceHeader->resizeSection(3, 132);
    deviceHeader->setSectionResizeMode(4, QHeaderView::Stretch);
    deviceHeader->resizeSection(5, 138);
    deviceHeader->resizeSection(6, 112);
    deviceHeader->resizeSection(7, 112);
    deviceHeader->resizeSection(8, 116);
    deviceHeader->resizeSection(9, 126);
    deviceHeader->resizeSection(10, 118);
    deviceHeader->resizeSection(11, 118);
    deviceHeader->setMinimumSectionSize(72);
    deviceTable_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    deviceTable_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    deviceTable_->verticalHeader()->setVisible(false);
    deviceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    deviceTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    deviceTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    deviceTable_->setAlternatingRowColors(true);
    deviceTable_->setShowGrid(false);
    deviceTable_->setMinimumHeight(310);
    deviceTable_->verticalHeader()->setDefaultSectionSize(48);
    deviceTable_->setWordWrap(false);
    deviceTable_->setCornerButtonEnabled(false);
    deviceTable_->setToolTip(tr("Select a row to inspect device details. Check the first column to include specific cameras in reachability checks."));
    leftLayout->addWidget(deviceTable_, 1);

    auto *tabs = new QTabWidget(left);
    tabs->setTabPosition(QTabWidget::North);
    tabs->addTab(makeInfoTab(tabs), tr("Device Info"));
    tabs->addTab(makeBulkTab(tabs), tr("Bulk Config"));
    tabs->addTab(makeMaintenanceTab(tabs), tr("Maintenance"));
    tabs->addTab(makeNvrTab(tabs), tr("NVR Channels"));
    tabs->addTab(makeHistoryTab(tabs), tr("Operation History"));
    leftLayout->addWidget(tabs);

    auto *rightScroll = new QScrollArea(central);
    rightScroll->setWidgetResizable(true);
    rightScroll->setMinimumWidth(340);
    rightScroll->setMaximumWidth(440);
    auto *rightContent = new QWidget(rightScroll);
    auto *rightLayout = new QVBoxLayout(rightContent);
    rightLayout->setContentsMargins(2, 2, 5, 2);
    rightLayout->setSpacing(9);

    auto *selectedPanel = panel(tr("Selected device"), rightContent);
    auto *selectedLayout = new QVBoxLayout(selectedPanel);
    selectedLayout->setSpacing(7);
    selectedDeviceSummary_ = new QLabel(tr("No device selected"), selectedPanel);
    selectedDeviceSummary_->setObjectName(QStringLiteral("sectionTitle"));
    selectedDeviceSummary_->setWordWrap(true);
    selectedNetworkSummary_ = mutedLabel(
        tr("Select a camera in the inventory to inspect its network and verification state."),
        selectedPanel);
    selectedNetworkSummary_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    selectedLayout->addWidget(selectedDeviceSummary_);
    selectedLayout->addWidget(selectedNetworkSummary_);
    rightLayout->addWidget(selectedPanel);

    auto *networkPanel = panel(tr("Network configuration"), rightContent);
    auto *networkLayout = new QVBoxLayout(networkPanel);
    networkLayout->setSpacing(8);
    networkLayout->addWidget(mutedLabel(
        tr("Live details for the selected computer interface. Confirm which network discovery and subnet scanning are using."),
        networkPanel));
    localNetworkSummary_ = new QLabel(tr("Select an active IPv4 interface to inspect its address and link details."), networkPanel);
    localNetworkSummary_->setWordWrap(true);
    localNetworkSummary_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    localNetworkSummary_->setObjectName(QStringLiteral("deviceDetails"));
    networkLayout->addWidget(localNetworkSummary_);
    networkLayout->addWidget(mutedLabel(
        tr("Camera-side IPv4 settings are shown in the Selected device panel and can be previewed or applied from the Maintenance tab."),
        networkPanel));
    rightLayout->addWidget(networkPanel);

    auto *capabilityPanel = panel(tr("Feature availability"), rightContent);
    auto *capabilityLayout = new QVBoxLayout(capabilityPanel);
    capabilityLayout->setSpacing(8);
    const auto addCapability = [capabilityPanel, capabilityLayout](const QString &name,
                                                                    const QString &state,
                                                                    bool available) {
        auto *row = new QHBoxLayout();
        auto *nameLabel = new QLabel(name, capabilityPanel);
        auto *stateLabel = new QLabel(state, capabilityPanel);
        stateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        stateLabel->setStyleSheet(available
            ? QStringLiteral("color:#176B4D;font-weight:700;")
            : QStringLiteral("color:#8A909A;font-weight:600;"));
        row->addWidget(nameLabel, 1);
        row->addWidget(stateLabel);
        capabilityLayout->addLayout(row);
    };
    addCapability(tr("ONVIF discovery"), tr("Available"), true);
    addCapability(tr("Device information"), tr("Available"), true);
    addCapability(tr("TCP reachability"), tr("Available"), true);
    addCapability(tr("Static IPv4"), tr("Conditional"), false);
    addCapability(tr("Bulk image settings"), tr("Planned"), false);
    addCapability(tr("Firmware update"), tr("Planned"), false);
    addCapability(tr("NVR channels"), tr("Planned"), false);
    addCapability(tr("Remote reboot"), tr("Planned"), false);
    rightLayout->addWidget(capabilityPanel);
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
        // Checked rows take priority so operators can explicitly target a group.
        for (int row = 0; row < deviceTable_->rowCount(); ++row) {
            const auto *check = deviceTable_->item(row, 0);
            if (!check || check->checkState() != Qt::Checked) continue;
            DeviceRecord device;
            device.ipAddress = deviceTable_->item(row, 1) ? deviceTable_->item(row, 1)->text() : QString();
            if (!device.ipAddress.isEmpty()) devices.append(device);
        }
        if (devices.isEmpty()) {
            const auto selected = deviceTable_->selectionModel()->selectedRows();
            for (const QModelIndex &index : selected) {
                DeviceRecord device;
                device.ipAddress = deviceTable_->item(index.row(), 1) ? deviceTable_->item(index.row(), 1)->text() : QString();
                if (!device.ipAddress.isEmpty()) devices.append(device);
            }
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
        updateStatus(tr("Checking TCP reachability. Checked cameras take priority, then highlighted rows; if neither is selected, all devices are checked."));
        reachabilityChecker_->start(devices);
    });
    connect(reachabilityChecker_, &ReachabilityChecker::deviceChecked, this,
        [this](const QString &ip, const QString &status, const QString &checkedAt, const QString &lastSuccessAt) {
            for (int row = 0; row < deviceTable_->rowCount(); ++row) {
                if (!deviceTable_->item(row, 1) || deviceTable_->item(row, 1)->text() != ip) continue;
                auto *reachabilityItem = new QTableWidgetItem(status);
                reachabilityItem->setTextAlignment(Qt::AlignCenter);
                reachabilityItem->setFont(QFont(QStringLiteral("Segoe UI"), 9, QFont::DemiBold));
                if (status == QStringLiteral("Reachable")) {
                    reachabilityItem->setForeground(QColor(QStringLiteral("#176B4D")));
                    reachabilityItem->setBackground(QColor(QStringLiteral("#EAF5EF")));
                } else if (status == QStringLiteral("Unreachable")) {
                    reachabilityItem->setForeground(QColor(QStringLiteral("#A33A42")));
                    reachabilityItem->setBackground(QColor(QStringLiteral("#FBECEE")));
                } else {
                    reachabilityItem->setForeground(QColor(QStringLiteral("#68707C")));
                    reachabilityItem->setBackground(QColor(QStringLiteral("#F1F3F5")));
                }
                reachabilityItem->setToolTip(tr("TCP reachability check: %1").arg(status));
                deviceTable_->setItem(row, 11, reachabilityItem);
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
            recordOperation(tr("Reachability"), ip, status, tr("TCP connection check completed"));
            updateDeviceSummary();
            applyFilters();
        });
    connect(reachabilityChecker_, &ReachabilityChecker::finished, this, [this](int checked) {
        reachabilityButton_->setEnabled(true);
        reachabilityButton_->setText(tr("Check reachability"));
        recordOperation(tr("Reachability batch"), QString(), tr("Completed"), tr("%1 device(s) checked").arg(checked));
        refreshOperationHistory();
        updateStatus(tr("Reachability check complete — %1 device(s). No response does not prove a device is offline.").arg(checked));
    });
    connect(reachabilityChecker_, &ReachabilityChecker::progressChanged, this,
            [this](int checked, int total, const QString &ip) {
        if (total > 0 && !ip.isEmpty())
            updateStatus(tr("Checking reachability: %1 / %2 — %3")
                             .arg(checked + 1).arg(total).arg(ip));
    });
    connect(reachabilityChecker_, &ReachabilityChecker::cancelled, this, [this](int checked) {
        reachabilityButton_->setEnabled(true);
        reachabilityButton_->setText(tr("Check reachability"));
        recordOperation(tr("Reachability batch"), QString(), tr("Cancelled"), tr("%1 device(s) checked before cancellation").arg(checked));
        refreshOperationHistory();
        updateStatus(tr("Reachability check cancelled after %1 device(s).").arg(checked));
    });
    connect(scanButton_, &QPushButton::clicked, this, [this] {
        if (discovery_->isRunning()) {
            discovery_->cancel();
            scanButton_->setText(tr("Discover devices"));
        } else {
            deviceTable_->setRowCount(0);
            updateDeviceSummary();
            scanButton_->setText(tr("Cancel discovery"));
            discovery_->start(interfaceSelector_->currentData().toString());
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
    connect(selectAllChecks, &QPushButton::clicked, this, [this] {
        for (int row = 0; row < deviceTable_->rowCount(); ++row)
            if (auto *item = deviceTable_->item(row, 0)) item->setCheckState(Qt::Checked);
        updateStatus(tr("All %1 device(s) checked for the next reachability operation.").arg(deviceTable_->rowCount()));
    });
    connect(clearChecks, &QPushButton::clicked, this, [this] {
        for (int row = 0; row < deviceTable_->rowCount(); ++row)
            if (auto *item = deviceTable_->item(row, 0)) item->setCheckState(Qt::Unchecked);
        updateStatus(tr("Device checkboxes cleared."));
    });
    connect(deviceTable_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (item && item->column() == 0) updateDeviceSummary();
    });
    connect(interfaceSelector_, &QComboBox::currentIndexChanged, this, [this](int) {
        updateLocalNetworkSummary();
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
        recordOperation(tr("Inventory export"), QFileInfo(path).fileName(), tr("Completed"), tr("%1 device(s) exported").arg(exported));
        refreshOperationHistory();
        updateStatus(tr("Exported %1 device(s) to %2.").arg(exported).arg(QFileInfo(path).fileName()));
    });
    connect(openWebButton_, &QPushButton::clicked, this, &MainWindow::openSelectedWebInterface);
    connect(deviceTable_, &QTableWidget::itemSelectionChanged, this, [this] {
        const bool hasSelection = deviceTable_->currentRow() >= 0;
        openWebButton_->setEnabled(hasSelection);
        refreshDeviceButton_->setEnabled(hasSelection);
        updateSelectedDevicePanel();
    });
    connect(help, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Oxita Camera Device Manager — Help"),
            tr("<b>Discover devices</b><br>Uses ONVIF WS-Discovery and the application's supported vendor discovery broadcast. "
               "Discovery results are not a guarantee that every camera on the network was found.<br><br>"
               "<b>Scan selected subnet</b><br>Checks candidate addresses on the selected IPv4 interface. "
               "A TCP response is reachability evidence, not proof of ONVIF compatibility or camera identity.<br><br>"
               "<b>Device information</b><br>Select a device and use Refresh device info to query its reachable ONVIF device service. "
               "Reported fields depend on the camera and its firmware.<br><br>"
               "<b>Network configuration</b><br>Static IPv4 changes require a verified ONVIF interface token and current interface details. "
               "The camera may disconnect after an address change; this operation currently has no automatic rollback.<br><br>"
               "<b>Feature availability</b><br>Bulk camera settings, NVR channel management and firmware update are not yet implemented. "
               "They are intentionally not presented as working controls."));
    });
    connect(settings, &QPushButton::clicked, this, [this] {
        const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Application settings & storage"));
        dialog.setMinimumWidth(500);
        auto *layout = new QVBoxLayout(&dialog);
        auto *heading = new QLabel(tr("Local application data"), &dialog);
        heading->setObjectName(QStringLiteral("sectionTitle"));
        layout->addWidget(heading);
        layout->addWidget(mutedLabel(
            tr("The device inventory is stored locally on this computer. Camera credentials are not written to the inventory database."),
            &dialog));
        const QString databasePath = dataDirectory + QStringLiteral("/inventory.sqlite");
        const QFileInfo databaseInfo(databasePath);
        auto *path = new QLabel(dataDirectory, &dialog);
        path->setWordWrap(true);
        path->setTextInteractionFlags(Qt::TextSelectableByMouse);
        path->setStyleSheet(QStringLiteral("background:#F6F7F9;border:1px solid #E7EAF0;border-radius:7px;padding:10px;"));
        layout->addWidget(path);
        auto *databaseHeading = new QLabel(tr("Inventory database"), &dialog);
        databaseHeading->setObjectName(QStringLiteral("sectionTitle"));
        layout->addWidget(databaseHeading);
        auto *database = mutedLabel(
            tr("%1\n%2 · %3")
                .arg(databasePath,
                     databaseInfo.exists() ? tr("File present") : tr("Not created yet"),
                     databaseInfo.exists() ? QLocale().formattedDataSize(databaseInfo.size()) : tr("No stored data")),
            &dialog);
        database->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(database);
        layout->addWidget(mutedLabel(
            tr("Application version: %1\nDatabase connection: %2\nSelected scan interface: %3")
                .arg(QCoreApplication::applicationVersion(),
                     inventoryDb_.isOpen() ? tr("Connected") : tr("Unavailable"),
                     interfaceSelector_ ? interfaceSelector_->currentText() : tr("Not selected")),
            &dialog));
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
        auto *copyPath = buttons->addButton(tr("Copy location"), QDialogButtonBox::ActionRole);
        auto *openFolder = buttons->addButton(tr("Open data folder"), QDialogButtonBox::ActionRole);
        layout->addWidget(buttons);
        connect(copyPath, &QPushButton::clicked, &dialog, [dataDirectory] {
            if (auto *clipboard = QApplication::clipboard()) clipboard->setText(dataDirectory);
        });
        connect(openFolder, &QPushButton::clicked, &dialog, [this, dataDirectory] {
            if (!QDir().mkpath(dataDirectory)) {
                QMessageBox::warning(this, tr("Storage folder unavailable"),
                                     tr("Could not create the application data folder."));
                return;
            }
            QDesktopServices::openUrl(QUrl::fromLocalFile(dataDirectory));
        });
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        dialog.exec();
    });
    // Device checkboxes are the explicit multi-select mechanism. Highlighted
    // rows remain available for single-device inspection and keyboard workflows.
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
        QString html = QStringLiteral("<html><body style='margin:4px;font-family:Segoe UI, sans-serif;color:#252A32;'>"
            "<div style='font-size:10px;font-weight:700;letter-spacing:1px;color:#8A909A;margin-bottom:8px;'>DEVICE PROFILE</div>"
            "<table width='100%' cellspacing='0' cellpadding='5'>");
        for (const QString &detail : details) {
            const int separator = detail.indexOf(QStringLiteral(": "));
            const QString key = (separator >= 0 ? detail.left(separator) : detail).toHtmlEscaped();
            const QString value = (separator >= 0 ? detail.mid(separator + 2) : QString()).toHtmlEscaped();
            html += QStringLiteral("<tr><td width='34%' valign='top' style='color:#7B818B;border-bottom:1px solid #ECEEF1;font-size:11px;'>%1</td>"
                "<td valign='top' style='color:#303640;border-bottom:1px solid #ECEEF1;font-size:11px;font-weight:600;'>%2</td></tr>")
                .arg(key, value);
        }
        html += QStringLiteral("</table></body></html>");
        deviceDetailsLabel_->setText(html);
    });
    updateDeviceSummary();
}

QWidget *MainWindow::makeInfoTab(QWidget *parent)
{
    auto *page = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *heading = new QLabel(tr("Device information"), page);
    heading->setObjectName(QStringLiteral("sectionTitle"));
    auto *description = mutedLabel(
        tr("Review discovery identity, ONVIF service details, network interfaces and reported capabilities for the selected camera."),
        page);
    layout->addWidget(heading);
    layout->addWidget(description);

    auto *identityPanel = new QFrame(page);
    identityPanel->setObjectName(QStringLiteral("metricCard"));
    auto *identityLayout = new QVBoxLayout(identityPanel);
    identityLayout->setContentsMargins(12, 10, 12, 10);
    auto *identityTitle = new QLabel(tr("DEVICE PROFILE"), identityPanel);
    identityTitle->setObjectName(QStringLiteral("eyebrow"));
    deviceDetailsLabel_ = new QLabel(
        tr("No device selected. Select a camera in the inventory above to inspect its reported information."),
        identityPanel);
    deviceDetailsLabel_->setWordWrap(true);
    deviceDetailsLabel_->setTextFormat(Qt::RichText);
    deviceDetailsLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    deviceDetailsLabel_->setObjectName(QStringLiteral("deviceDetails"));
    identityLayout->addWidget(identityTitle);
    identityLayout->addWidget(deviceDetailsLabel_);
    layout->addWidget(identityPanel, 1);

    auto *note = mutedLabel(
        tr("Information is device-reported and may be incomplete. An empty field means the camera did not provide that value during the latest query; it does not necessarily indicate a fault."),
        page);
    layout->addWidget(note);
    return page;
}
QWidget *MainWindow::makeBulkTab(QWidget *parent)
{
    auto *page = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    auto *heading = new QLabel(tr("Multi-camera configuration"), page);
    heading->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(heading);
    layout->addWidget(mutedLabel(
        tr("The workflow is designed for applying a consistent image profile across selected cameras. "
           "Per-vendor adapters and capability checks are required before any setting can be sent."),
        page));

    auto *statusPanel = new QFrame(page);
    statusPanel->setObjectName(QStringLiteral("metricCard"));
    auto *statusLayout = new QVBoxLayout(statusPanel);
    statusLayout->setContentsMargins(14, 12, 14, 12);
    auto *statusTitle = new QLabel(tr("NOT AVAILABLE IN THIS BUILD"), statusPanel);
    statusTitle->setObjectName(QStringLiteral("eyebrow"));
    auto *statusText = new QLabel(
        tr("No bulk configuration adapter is currently registered. These controls remain unavailable rather than sending unverified vendor commands."),
        statusPanel);
    statusText->setWordWrap(true);
    statusLayout->addWidget(statusTitle);
    statusLayout->addWidget(statusText);
    layout->addWidget(statusPanel);

    auto *capabilities = new QGridLayout();
    capabilities->setHorizontalSpacing(14);
    capabilities->setVerticalSpacing(10);
    const QStringList features = {
        tr("Day / Night profile"), tr("ICR / IR-cut control"),
        tr("Image and IR settings"), tr("Easy Mode")
    };
    for (int i = 0; i < features.size(); ++i) {
        auto *name = new QLabel(features.at(i), page);
        auto *state = new QLabel(tr("Adapter required"), page);
        state->setObjectName(QStringLiteral("muted"));
        capabilities->addWidget(name, i * 2, 0);
        capabilities->addWidget(state, i * 2, 1, Qt::AlignRight);
        if (i + 1 < features.size()) {
            auto *line = new QFrame(page);
            line->setFrameShape(QFrame::HLine);
            line->setStyleSheet(QStringLiteral("color:#ECEEF2;"));
            capabilities->addWidget(line, i * 2 + 1, 0, 1, 2);
        }
    }
    layout->addLayout(capabilities);
    layout->addStretch();
    return page;
}
QWidget *MainWindow::makeMaintenanceTab(QWidget *parent)
{
    auto *page = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(12);

    auto *heading = new QLabel(tr("Device actions"), page);
    heading->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(heading);
    layout->addWidget(mutedLabel(
        tr("Actions are performed against the selected camera. Review the target address before opening its web interface or changing network settings."),
        page));

    auto *devicePanel = panel(tr("Device access & diagnostics"), page);
    auto *deviceLayout = new QVBoxLayout(devicePanel);
    deviceLayout->setSpacing(8);
    auto *deviceDescription = mutedLabel(
        tr("Refresh retrieves available ONVIF device information. Open web interface launches the camera's HTTP page in your default browser."),
        devicePanel);
    deviceLayout->addWidget(deviceDescription);
    auto *deviceActions = new QGridLayout();
    deviceActions->setHorizontalSpacing(8);
    deviceActions->setVerticalSpacing(8);
    refreshDeviceButton_ = actionButton(tr("Refresh device info"), devicePanel);
    refreshDeviceButton_->setEnabled(false);
    openWebButton_ = actionButton(tr("Open web interface"), devicePanel);
    openWebButton_->setEnabled(false);
    auto *reboot = actionButton(tr("Reboot selected"), devicePanel);
    reboot->setEnabled(false);
    reboot->setToolTip(tr("Reboot is unavailable until a verified device adapter exposes a supported reboot operation."));
    deviceActions->addWidget(refreshDeviceButton_, 0, 0);
    deviceActions->addWidget(openWebButton_, 0, 1);
    deviceActions->addWidget(reboot, 1, 0, 1, 2);
    deviceLayout->addLayout(deviceActions);
    layout->addWidget(devicePanel);

    auto *networkPanel = panel(tr("IPv4 network configuration"), page);
    auto *networkLayout = new QVBoxLayout(networkPanel);
    networkLayout->setSpacing(8);
    networkLayout->addWidget(mutedLabel(
        tr("Preview validates proposed values locally and does not contact the camera. Apply sends an ONVIF static IPv4 request only when the camera exposes the required verified interface details."),
        networkPanel));
    auto *networkActions = new QGridLayout();
    networkActions->setHorizontalSpacing(8);
    auto *previewNetworkButton = actionButton(tr("Preview IP configuration"), networkPanel);
    auto *applyNetworkButton = actionButton(tr("Apply static IPv4 (ONVIF)"), networkPanel, true);
    networkActions->addWidget(previewNetworkButton, 0, 0);
    networkActions->addWidget(applyNetworkButton, 0, 1);
    networkLayout->addLayout(networkActions);
    layout->addWidget(networkPanel);

    auto *firmwarePanel = panel(tr("Firmware & device maintenance"), page);
    auto *firmwareLayout = new QVBoxLayout(firmwarePanel);
    firmwareLayout->setSpacing(8);
    firmwareStatusSummary_ = new QLabel(tr("Select a camera to inspect its reported firmware version."), firmwarePanel);
    firmwareStatusSummary_->setWordWrap(true);
    firmwareStatusSummary_->setObjectName(QStringLiteral("deviceDetails"));
    firmwareLayout->addWidget(firmwareStatusSummary_);
    auto *firmwareNote = mutedLabel(
        tr("Firmware packages are not installed or transferred by this build. A verified vendor update adapter, package validation and recovery procedure are required before flashing can be enabled."),
        firmwarePanel);
    firmwareLayout->addWidget(firmwareNote);
    auto *firmwareActions = new QHBoxLayout();
    auto *firmwareUpdateButton = actionButton(tr("Firmware update unavailable"), firmwarePanel);
    firmwareUpdateButton->setEnabled(false);
    firmwareUpdateButton->setToolTip(tr("Firmware update is planned but not implemented. No firmware will be uploaded."));
    firmwareActions->addWidget(firmwareUpdateButton);
    firmwareActions->addStretch();
    firmwareLayout->addLayout(firmwareActions);
    layout->addWidget(firmwarePanel);

    auto *safetyNote = new QLabel(
        tr("Network changes can interrupt camera connectivity. Confirm the new address is reachable from this computer before applying. This build does not provide automatic rollback."),
        page);
    safetyNote->setWordWrap(true);
    safetyNote->setStyleSheet(QStringLiteral("color:#8A5B13;background:#FBF3E4;border:1px solid #F0E1BE;border-radius:7px;padding:9px;"));
    layout->addWidget(safetyNote);
    layout->addStretch();
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
        device.onvifEndpoint = ipItem->data(Qt::UserRole + 2).toString();
        device.discoverySources = deviceTable_->item(row, 8)
            ? deviceTable_->item(row, 8)->text().split(QStringLiteral(", "), Qt::SkipEmptyParts)
            : QStringList{};
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
        recordOperation(tr("Refresh device information"), device.ipAddress, tr("Requested"), tr("ONVIF information refresh requested"));
        refreshOperationHistory();
        discovery_->refreshDeviceInfo(device);
    });
    return page;
}

QWidget *MainWindow::makeHistoryTab(QWidget *parent)
{
    auto *page = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);
    auto *heading = new QLabel(tr("Recent application activity"), page);
    heading->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(heading);
    layout->addWidget(mutedLabel(
        tr("Local history of discovery, scanning, reachability, inventory and network-configuration outcomes. Credentials and ONVIF interface tokens are not intentionally recorded."),
        page));
    auto *actions = new QHBoxLayout();
    auto *refresh = actionButton(tr("Refresh history"), page);
    auto *export = actionButton(tr("Export diagnostics"), page, true);
    actions->addWidget(refresh);
    actions->addWidget(export);
    actions->addStretch();
    layout->addLayout(actions);
    historyTable_ = new QTableWidget(0, 5, page);
    historyTable_->setHorizontalHeaderLabels({
        tr("TIME (UTC)"), tr("OPERATION"), tr("TARGET"), tr("RESULT"), tr("DETAILS")
    });
    historyTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    historyTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    historyTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    historyTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    historyTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    historyTable_->verticalHeader()->setVisible(false);
    historyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable_->setAlternatingRowColors(true);
    historyTable_->setMinimumHeight(260);
    layout->addWidget(historyTable_, 1);
    layout->addWidget(mutedLabel(
        tr("Diagnostic exports include OS/application details, selected interface addresses and recent operation history. Review the file before sharing it."),
        page));
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshOperationHistory);
    connect(export, &QPushButton::clicked, this, &MainWindow::exportDiagnosticBundle);
    refreshOperationHistory();
    return page;
}

void MainWindow::recordOperation(const QString &operation, const QString &target,
                                 const QString &outcome, const QString &details)
{
    if (!inventoryDb_.isOpen()) return;
    const QRegularExpression secret(
        QStringLiteral(R"((password|passwd|authorization|token|credential)\s*[:=]\s*[^\s,;]+)"),
        QRegularExpression::CaseInsensitiveOption);
    const auto safe = [&secret](QString value) {
        value.replace(secret, QStringLiteral("\\1=[REDACTED]"));
        return value.left(2000);
    };
    QSqlQuery query(inventoryDb_);
    query.prepare(QStringLiteral(
        "INSERT INTO operation_history (occurred_at,operation,target,outcome,details) "
        "VALUES (:time,:operation,:target,:outcome,:details)"));
    query.bindValue(":time", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.bindValue(":operation", safe(operation));
    query.bindValue(":target", safe(target));
    query.bindValue(":outcome", safe(outcome));
    query.bindValue(":details", safe(details));
    if (!query.exec()) {
        updateStatus(tr("Could not record operation history: %1").arg(query.lastError().text()));
        return;
    }
    QSqlQuery retention(inventoryDb_);
    retention.exec(QStringLiteral("DELETE FROM operation_history WHERE id NOT IN (SELECT id FROM operation_history ORDER BY id DESC LIMIT 1000)"));
}

void MainWindow::refreshOperationHistory()
{
    if (!historyTable_ || !inventoryDb_.isOpen()) return;
    QSqlQuery query(inventoryDb_);
    if (!query.exec(QStringLiteral(
        "SELECT occurred_at,operation,target,outcome,details "
        "FROM operation_history ORDER BY id DESC LIMIT 250"))) {
        updateStatus(tr("Could not load operation history: %1").arg(query.lastError().text()));
        return;
    }
    historyTable_->setRowCount(0);
    while (query.next()) {
        const int row = historyTable_->rowCount();
        historyTable_->insertRow(row);
        for (int column = 0; column < 5; ++column) {
            auto *item = new QTableWidgetItem(query.value(column).toString());
            item->setToolTip(item->text());
            historyTable_->setItem(row, column, item);
        }
    }
}

void MainWindow::exportDiagnosticBundle()
{
    if (!inventoryDb_.isOpen()) {
        QMessageBox::warning(this, tr("Diagnostics unavailable"),
            tr("The local inventory database is unavailable, so a support bundle cannot be created."));
        return;
    }
    const auto answer = QMessageBox::warning(this, tr("Export diagnostic bundle"),
        tr("The bundle includes OS/application details, selected interface addresses and recent operation history. "
           "It excludes the inventory database, passwords and ONVIF interface tokens. Review the file before sharing it."),
        QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Save) return;
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save diagnostic bundle"), QStringLiteral("oxita-diagnostics.txt"),
        tr("Text files (*.txt)"));
    if (path.isEmpty()) return;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Diagnostics export failed"), file.errorString());
        return;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << "OXITA CAMERA DEVICE MANAGER - DIAGNOSTIC BUNDLE\n";
    stream << "Generated UTC: " << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) << "\n";
    stream << "Application: " << QCoreApplication::applicationName() << " "
           << QCoreApplication::applicationVersion() << "\n";
    stream << "Qt: " << qVersion() << "\n";
    stream << "OS: " << QSysInfo::prettyProductName() << " ("
           << QSysInfo::kernelType() << " " << QSysInfo::kernelVersion() << ")\n";
    stream << "Architecture: " << QSysInfo::currentCpuArchitecture() << "\n\n";
    stream << "RECENT REDACTED APPLICATION LOGS\n";
    const QString logDirectory = DiagnosticLog::directoryPath();
    for (const QString &logName : {QStringLiteral("application.log"), QStringLiteral("application.log.1"),
                                   QStringLiteral("application.log.2"), QStringLiteral("application.log.3")}) {
        QFile logFile(logDirectory + QLatin1Char('/') + logName);
        if (!logFile.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        const QStringList lines = QString::fromUtf8(logFile.readAll()).split(QLatin1Char('\\n'), Qt::SkipEmptyParts);
        stream << "--- " << logName << " ---\n";
        const int first = qMax(0, lines.size() - 250);
        for (int index = first; index < lines.size(); ++index) stream << lines.at(index) << "\n";
    }
    stream << "\nNETWORK INTERFACES (IPv4 ONLY)\n";
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
        if (!(interface.flags() & QNetworkInterface::IsUp) ||
            (interface.flags() & QNetworkInterface::IsLoopBack)) continue;
        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            stream << "- " << interface.humanReadableName() << ": "
                   << entry.ip().toString() << "/" << entry.prefixLength() << "\n";
        }
    }
    stream << "\nINVENTORY SUMMARY\n";
    QSqlQuery countQuery(inventoryDb_);
    if (countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM devices")) && countQuery.next())
        stream << "Devices: " << countQuery.value(0).toInt() << "\n";
    stream << "\nRECENT OPERATIONS (MAX 250)\n";
    QSqlQuery history(inventoryDb_);
    if (history.exec(QStringLiteral(
        "SELECT occurred_at,operation,target,outcome,details "
        "FROM operation_history ORDER BY id DESC LIMIT 250"))) {
        while (history.next()) {
            stream << history.value(0).toString() << " | "
                   << history.value(1).toString() << " | "
                   << history.value(2).toString() << " | "
                   << history.value(3).toString() << " | "
                   << history.value(4).toString() << "\n";
        }
    }
    stream.flush();
    if (!file.commit()) {
        QMessageBox::warning(this, tr("Diagnostics export failed"), file.errorString());
        return;
    }
    recordOperation(tr("Diagnostic export"), QFileInfo(path).fileName(), tr("Completed"),
                    tr("Support bundle created"));
    refreshOperationHistory();
    QMessageBox::information(this, tr("Diagnostics exported"),
        tr("The support bundle was saved. Review it before sharing; network addresses and operation details may be sensitive."));
}

QWidget *MainWindow::makeNvrTab(QWidget *parent)
{
    auto *page = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    auto *heading = new QLabel(tr("Recorder channel management"), page);
    heading->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(heading);
    auto *emptyState = new QFrame(page);
    emptyState->setObjectName(QStringLiteral("metricCard"));
    auto *emptyLayout = new QVBoxLayout(emptyState);
    emptyLayout->setContentsMargins(18, 18, 18, 18);
    auto *eyebrow = new QLabel(tr("NVR INTEGRATION"), emptyState);
    eyebrow->setObjectName(QStringLiteral("eyebrow"));
    auto *title = new QLabel(tr("No recorder adapter is available"), emptyState);
    title->setObjectName(QStringLiteral("sectionTitle"));
    auto *description = mutedLabel(
        tr("Channel listing, camera-to-channel mapping, stream profiles and recorder configuration require a verified NVR protocol adapter. "
           "The current build does not send recorder-specific commands."),
        emptyState);
    emptyLayout->addWidget(eyebrow);
    emptyLayout->addWidget(title);
    emptyLayout->addWidget(description);
    layout->addWidget(emptyState);
    layout->addStretch();
    return page;
}

void MainWindow::updateSelectedDevicePanel()
{
    if (!selectedDeviceSummary_ || !selectedNetworkSummary_ || !deviceTable_) return;
    const int row = deviceTable_->currentRow();
    if (row < 0) {
        selectedDeviceSummary_->setText(tr("No device selected"));
        selectedNetworkSummary_->setText(
            tr("Select a camera in the inventory to inspect its network and verification state."));
        if (firmwareStatusSummary_)
            firmwareStatusSummary_->setText(tr("Select a camera to inspect its reported firmware version."));
        return;
    }

    const auto cell = [this, row](int column) {
        const auto *item = deviceTable_->item(row, column);
        return item ? item->text().trimmed() : QString();
    };
    const auto *ipItem = deviceTable_->item(row, 1);
    selectedDeviceSummary_->setText(
        cell(2).isEmpty() ? cell(1) : cell(2) + QStringLiteral("\n") + cell(1));
    QStringList details;
    details << tr("Manufacturer: %1").arg(cell(3).isEmpty() ? tr("Not reported") : cell(3));
    details << tr("Model: %1").arg(cell(4).isEmpty() ? tr("Not reported") : cell(4));
    details << tr("Reachability: %1").arg(cell(11).isEmpty() ? tr("Not checked") : cell(11));
    details << tr("Discovery: %1").arg(cell(8).isEmpty() ? tr("Manual / unknown") : cell(8));
    if (ipItem) {
        const QString interfaceToken = ipItem->data(Qt::UserRole + 7).toString();
        details << tr("ONVIF network interface: %1")
                       .arg(interfaceToken.isEmpty() ? tr("Not verified") : tr("Available"));
        const QString cameraIp = ipItem->data(Qt::UserRole + 8).toString();
        const QString mask = ipItem->data(Qt::UserRole + 9).toString();
        const QString gateway = ipItem->data(Qt::UserRole + 10).toString();
        const QString dns = ipItem->data(Qt::UserRole + 11).toString();
        details << tr("IPv4: %1").arg(cameraIp.isEmpty() ? tr("Not reported") : cameraIp);
        details << tr("Subnet: %1").arg(mask.isEmpty() ? tr("Not reported") : mask);
        details << tr("Gateway: %1").arg(gateway.isEmpty() ? tr("Not reported") : gateway);
        details << tr("DNS: %1").arg(dns.isEmpty() ? tr("Not reported") : dns);
        const QString ports = ipItem->data(Qt::UserRole + 6).toString();
        details << tr("Detected TCP ports: %1").arg(ports.isEmpty() ? tr("Not scanned") : ports);
        const QString endpoint = ipItem->data(Qt::UserRole + 2).toString();
        if (!endpoint.isEmpty()) details << tr("ONVIF endpoint: %1").arg(endpoint);
        const QString service = ipItem->data(Qt::UserRole + 3).toString();
        if (!service.isEmpty()) details << tr("Device service: %1").arg(service);
    }
    selectedNetworkSummary_->setText(details.join(QLatin1Char('\n')));
    if (firmwareStatusSummary_) {
        const QString firmware = cell(6);
        const QString model = cell(4);
        firmwareStatusSummary_->setText(
            tr("Device: %1\nModel: %2\nReported firmware: %3\nUpdate capability: Not available in this build")
                .arg(cell(1), model.isEmpty() ? tr("Not reported") : model,
                     firmware.isEmpty() ? tr("Not reported") : firmware));
    }
}

void MainWindow::updateLocalNetworkSummary()
{
    if (!localNetworkSummary_ || !interfaceSelector_) return;
    const QString interfaceName = interfaceSelector_->currentData().toString();
    const QNetworkInterface interface = QNetworkInterface::interfaceFromName(interfaceName);
    if (!interface.isValid()) {
        localNetworkSummary_->setText(tr("No active network interface is selected."));
        return;
    }

    QStringList details;
    details << tr("Interface: %1").arg(interface.humanReadableName());
    details << tr("System name: %1").arg(interface.name());
    details << tr("Hardware address: %1").arg(
        interface.hardwareAddress().isEmpty() ? tr("Not reported") : interface.hardwareAddress());
    details << tr("MTU: %1").arg(interface.mtu() > 0 ? QString::number(interface.mtu()) : tr("Not reported"));
    bool foundIpv4 = false;
    for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
        if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol || entry.ip().isLoopback()) continue;
        foundIpv4 = true;
        details << tr("IPv4 address: %1").arg(entry.ip().toString());
        details << tr("Subnet prefix: /%1").arg(entry.prefixLength());
        if (!entry.broadcast().isNull())
            details << tr("Broadcast: %1").arg(entry.broadcast().toString());
    }
    if (!foundIpv4) details << tr("IPv4 address: Not available");
    details << tr("Link state: %1").arg(
        interface.flags().testFlag(QNetworkInterface::IsRunning) ? tr("Running") : tr("Not running"));
    localNetworkSummary_->setText(details.join(QLatin1Char('\n')));
}

void MainWindow::updateStatus(const QString &message)
{
    if (statusLabel_) statusLabel_->setText(message);
}
void MainWindow::updateDeviceSummary()
{
    const int count = deviceTable_ ? deviceTable_->rowCount() : 0;
    int reachable = 0;
    int verified = 0;
    int needsReview = 0;
    for (int row = 0; row < count; ++row) {
        const auto *reachability = deviceTable_->item(row, 11);
        if (reachability && reachability->text() == QStringLiteral("Reachable"))
            ++reachable;
        const auto *ipItem = deviceTable_->item(row, 1);
        const auto *sourceItem = deviceTable_->item(row, 8);
        const QStringList sources = sourceItem
            ? sourceItem->text().split(QStringLiteral(", "), Qt::SkipEmptyParts)
            : QStringList{};
        // The ONVIF card counts only devices with an ONVIF endpoint. Project
        // firmware discovery is also valid protocol evidence and should not be
        // mislabeled as an unverified candidate.
        if (ipItem && (!ipItem->data(Qt::UserRole + 2).toString().isEmpty() ||
                       !ipItem->data(Qt::UserRole + 3).toString().isEmpty()))
            ++verified;
        const bool protocolVerified =
            sources.contains(QStringLiteral("ONVIF WS-Discovery")) ||
            sources.contains(QStringLiteral("ONVIF GetDeviceInformation")) ||
            sources.contains(QStringLiteral("Project UDP 9988"));
        if (!protocolVerified) ++needsReview;
    }
    if (deviceCountValue_) deviceCountValue_->setText(tr("%1 devices").arg(count));
    if (discoveredMetric_) discoveredMetric_->setText(QString::number(count));
    if (reachableMetric_) reachableMetric_->setText(QString::number(reachable));
    if (verifiedMetric_) verifiedMetric_->setText(QString::number(verified));
    if (unverifiedMetric_) unverifiedMetric_->setText(QString::number(needsReview));
    if (summaryLabel_) {
        int checked = 0;
        for (int row = 0; row < count; ++row) {
            const auto *item = deviceTable_->item(row, 0);
            if (item && item->checkState() == Qt::Checked) ++checked;
        }
        summaryLabel_->setText(tr("%1 devices · %2 checked").arg(count).arg(checked));
    }
}
void MainWindow::upsertDevice(DeviceRecord device)
{
    int row = -1;
    for (int i = 0; i < deviceTable_->rowCount(); ++i) {
        const auto *macItem = deviceTable_->item(i, 5);
        const auto *ipItem = deviceTable_->item(i, 1);
        const auto *serialItem = deviceTable_->item(i, 9);
        const auto *manufacturerItem = deviceTable_->item(i, 3);
        const QString existingMac = macItem ? macItem->text().trimmed() : QString();
        const QString existingIp = ipItem ? ipItem->text().trimmed() : QString();
        const QString existingEndpoint = ipItem
            ? ipItem->data(Qt::UserRole + 2).toString().trimmed() : QString();
        const QString incomingMac = device.macAddress.trimmed();
        const QString incomingEndpoint = device.onvifEndpoint.trimmed();
        const auto normalizeMac = [](QString value) {
            value = value.trimmed().toUpper();
            value.replace(QLatin1Char('-'), QLatin1Char(':'));
            return value;
        };
        const QString normalizedExistingMac = normalizeMac(existingMac);
        const QString normalizedIncomingMac = normalizeMac(incomingMac);
        const bool compatibleMac = normalizedExistingMac.isEmpty() ||
            normalizedIncomingMac.isEmpty() ||
            normalizedExistingMac == normalizedIncomingMac;
        const bool sameMac = !normalizedIncomingMac.isEmpty() &&
            normalizedIncomingMac == normalizedExistingMac;
        const bool sameEndpoint = compatibleMac && !incomingEndpoint.isEmpty() &&
            !existingEndpoint.isEmpty() &&
            incomingEndpoint.compare(existingEndpoint, Qt::CaseInsensitive) == 0;
        const bool sameSerial = compatibleMac && !device.serialNumber.trimmed().isEmpty() && serialItem &&
            !serialItem->text().trimmed().isEmpty() && manufacturerItem &&
            !manufacturerItem->text().trimmed().isEmpty() &&
            device.serialNumber.trimmed().compare(serialItem->text().trimmed(), Qt::CaseInsensitive) == 0 &&
            device.manufacturer.trimmed().compare(manufacturerItem->text().trimmed(), Qt::CaseInsensitive) == 0;
        const bool sameIpWithoutIdentityConflict = compatibleMac &&
            !device.ipAddress.trimmed().isEmpty() &&
            existingIp.compare(device.ipAddress.trimmed(), Qt::CaseInsensitive) == 0;
        if (sameMac || sameEndpoint || sameSerial || sameIpWithoutIdentityConflict) {
            row = i;
            break;
        }
    }
    if (row >= 0) {
        DeviceRecord existing;
        const auto *ipItem = deviceTable_->item(row, 1);
        const auto cellText = [this, row](int column) {
            const auto *item = deviceTable_->item(row, column);
            return item ? item->text() : QString();
        };
        existing.ipAddress = cellText(1);
        existing.userLabel = cellText(2);
        if (existing.userLabel == QStringLiteral("Unverified candidate"))
            existing.userLabel.clear();
        existing.manufacturer = cellText(3);
        existing.model = cellText(4);
        existing.macAddress = cellText(5);
        existing.firmwareVersion = cellText(6);
        existing.activationState = cellText(7);
        existing.discoverySources = cellText(8).split(QStringLiteral(", "), Qt::SkipEmptyParts);
        existing.serialNumber = cellText(9);
        existing.hardwareId = cellText(10);
        existing.reachabilityStatus = cellText(11);
        if (existing.reachabilityStatus.isEmpty())
            existing.reachabilityStatus = QStringLiteral("Not checked");
        if (existing.activationState.isEmpty())
            existing.activationState = QStringLiteral("Unknown");
        if (ipItem) {
            existing.interfaceName = ipItem->data(Qt::UserRole + 1).toString();
            existing.onvifEndpoint = ipItem->data(Qt::UserRole + 2).toString();
            existing.deviceServiceUrl = ipItem->data(Qt::UserRole + 3).toString();
            existing.networkSummary = ipItem->data(Qt::UserRole + 4).toString();
            existing.capabilities = ipItem->data(Qt::UserRole + 5).toString().split(QStringLiteral(", "), Qt::SkipEmptyParts);
            existing.openPorts = ipItem->data(Qt::UserRole + 6).toString().split(QStringLiteral(", "), Qt::SkipEmptyParts);
            existing.networkInterfaceToken = ipItem->data(Qt::UserRole + 7).toString();
            existing.ipv4Address = ipItem->data(Qt::UserRole + 8).toString();
            existing.ipv4SubnetMask = ipItem->data(Qt::UserRole + 9).toString();
            existing.ipv4Gateway = ipItem->data(Qt::UserRole + 10).toString();
            existing.ipv4DnsServer = ipItem->data(Qt::UserRole + 11).toString();
            const QString dhcp = ipItem->data(Qt::UserRole + 12).toString();
            existing.ipv4ConfigurationKnown = dhcp == QStringLiteral("true") ||
                                               dhcp == QStringLiteral("false");
            existing.ipv4DhcpEnabled = dhcp == QStringLiteral("true");
            existing.isCandidate = ipItem->data(Qt::UserRole + 13).toBool();
        }
        device = DeviceIdentity::mergeObservations(existing, device);
    }
    if (row < 0) { row = deviceTable_->rowCount(); deviceTable_->insertRow(row); }
    QString oldLabel = deviceTable_->item(row, 2) ? deviceTable_->item(row, 2)->text() : QString();
    if (oldLabel == QStringLiteral("Unverified candidate"))
        oldLabel.clear();
    const auto *previousCheck = deviceTable_->item(row, 0);
    const Qt::CheckState previousCheckState = previousCheck ? previousCheck->checkState() : Qt::Unchecked;
    auto *check = new QTableWidgetItem();
    check->setFlags(check->flags() | Qt::ItemIsUserCheckable);
    check->setCheckState(previousCheckState);
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
    const bool networkInterfacesUpdated =
        device.discoverySources.contains(QStringLiteral("ONVIF GetNetworkInterfaces"));
    if (networkInterfacesUpdated && !device.ipv4ConfigurationKnown) {
        // An unmatched ONVIF interface response must revoke stale write data.
        ipItem->setData(Qt::UserRole + 7, QString());
        ipItem->setData(Qt::UserRole + 8, QString());
        ipItem->setData(Qt::UserRole + 9, QString());
        ipItem->setData(Qt::UserRole + 10, QString());
    } else {
        keepExistingWhenEmpty(Qt::UserRole + 7, device.networkInterfaceToken);
        keepExistingWhenEmpty(Qt::UserRole + 8, device.ipv4Address);
        keepExistingWhenEmpty(Qt::UserRole + 9, device.ipv4SubnetMask);
        keepExistingWhenEmpty(Qt::UserRole + 10, device.ipv4Gateway);
    }
    keepExistingWhenEmpty(Qt::UserRole + 11, device.ipv4DnsServer);
    if (networkInterfacesUpdated) {
        ipItem->setData(Qt::UserRole + 12, device.ipv4ConfigurationKnown
            ? (device.ipv4DhcpEnabled ? QStringLiteral("true") : QStringLiteral("false"))
            : QString());
    } else {
        ipItem->setData(Qt::UserRole + 12, device.ipv4ConfigurationKnown
            ? (device.ipv4DhcpEnabled ? QStringLiteral("true") : QStringLiteral("false"))
            : (previousIpItem ? previousIpItem->data(Qt::UserRole + 12) : QVariant()));
    }
    ipItem->setData(Qt::UserRole + 13, device.isCandidate);
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

    // Give operational states a restrained visual treatment so users can scan
    // the inventory quickly without relying on colour alone.
    for (int column : {7, 11}) {
        auto *stateItem = deviceTable_->item(row, column);
        if (!stateItem) continue;
        stateItem->setTextAlignment(Qt::AlignCenter);
        stateItem->setFont(QFont(QStringLiteral("Segoe UI"), 9, QFont::DemiBold));
        const QString state = stateItem->text();
        if (column == 11 && state == QStringLiteral("Reachable")) {
            stateItem->setForeground(QColor(QStringLiteral("#176B4D")));
            stateItem->setBackground(QColor(QStringLiteral("#EAF5EF")));
        } else if ((column == 7 && state.compare(QStringLiteral("Activated"), Qt::CaseInsensitive) == 0) ||
                   (column == 11 && state == QStringLiteral("Unreachable"))) {
            stateItem->setForeground(column == 7 ? QColor(QStringLiteral("#176B4D")) : QColor(QStringLiteral("#A33A42")));
            stateItem->setBackground(column == 7 ? QColor(QStringLiteral("#EAF5EF")) : QColor(QStringLiteral("#FBECEE")));
        } else if (column == 7 && state.compare(QStringLiteral("Not activated"), Qt::CaseInsensitive) == 0) {
            stateItem->setForeground(QColor(QStringLiteral("#8A5B13")));
            stateItem->setBackground(QColor(QStringLiteral("#FBF3E4")));
        } else {
            stateItem->setForeground(QColor(QStringLiteral("#68707C")));
            stateItem->setBackground(QColor(QStringLiteral("#F1F3F5")));
        }
    }
    for (int column = 1; column < deviceTable_->columnCount(); ++column) {
        if (auto *item = deviceTable_->item(row, column))
            item->setToolTip(item->text().isEmpty() ? tr("Not reported by this device") : item->text());
    }
    updateDeviceSummary();
    applyFilters();
    if (row == deviceTable_->currentRow())
        updateSelectedDevicePanel();
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
    QSqlQuery integrityQuery(inventoryDb_);
    if (!integrityQuery.exec(QStringLiteral("PRAGMA quick_check")) ||
        !integrityQuery.next() || integrityQuery.value(0).toString().compare(QStringLiteral("ok"), Qt::CaseInsensitive) != 0) {
        updateStatus(tr("Inventory database integrity check failed. The database was left untouched; close the app and preserve a copy before attempting recovery."));
        inventoryDb_.close();
        return;
    }
    QSqlQuery query(inventoryDb_);
    if (!query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS devices (device_key TEXT PRIMARY KEY, ip TEXT, mac TEXT, manufacturer TEXT, model TEXT, firmware TEXT, serial TEXT, hardware TEXT, activation TEXT, source TEXT, is_candidate INTEGER, last_seen TEXT DEFAULT CURRENT_TIMESTAMP)"))) {
        updateStatus(tr("Could not initialize inventory: %1").arg(query.lastError().text()));
        return;
    }
    QSqlQuery historyTable(inventoryDb_);
    if (!historyTable.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS operation_history (id INTEGER PRIMARY KEY AUTOINCREMENT, occurred_at TEXT NOT NULL, operation TEXT NOT NULL, target TEXT NOT NULL, outcome TEXT NOT NULL, details TEXT NOT NULL)"))) {
        updateStatus(tr("Could not initialize operation history: %1").arg(historyTable.lastError().text()));
        return;
    }
    QSqlQuery columnQuery(inventoryDb_);
    if (!columnQuery.exec(QStringLiteral("PRAGMA table_info(devices)"))) {
        updateStatus(tr("Could not inspect inventory schema: %1").arg(columnQuery.lastError().text()));
        return;
    }
    QStringList existingColumns;
    while (columnQuery.next())
        existingColumns << columnQuery.value(1).toString();

    const QStringList migrations = {
        QStringLiteral("user_label|ALTER TABLE devices ADD COLUMN user_label TEXT DEFAULT ''"),
        QStringLiteral("reachability_status|ALTER TABLE devices ADD COLUMN reachability_status TEXT DEFAULT 'Not checked'"),
        QStringLiteral("reachability_checked_at|ALTER TABLE devices ADD COLUMN reachability_checked_at TEXT DEFAULT ''"),
        QStringLiteral("last_successful_response_at|ALTER TABLE devices ADD COLUMN last_successful_response_at TEXT DEFAULT ''"),
        QStringLiteral("interface_name|ALTER TABLE devices ADD COLUMN interface_name TEXT DEFAULT ''"),
        QStringLiteral("onvif_endpoint|ALTER TABLE devices ADD COLUMN onvif_endpoint TEXT DEFAULT ''"),
        QStringLiteral("device_service_url|ALTER TABLE devices ADD COLUMN device_service_url TEXT DEFAULT ''"),
        QStringLiteral("network_summary|ALTER TABLE devices ADD COLUMN network_summary TEXT DEFAULT ''"),
        QStringLiteral("capabilities|ALTER TABLE devices ADD COLUMN capabilities TEXT DEFAULT ''"),
        QStringLiteral("open_ports|ALTER TABLE devices ADD COLUMN open_ports TEXT DEFAULT ''"),
        QStringLiteral("network_interface_token|ALTER TABLE devices ADD COLUMN network_interface_token TEXT DEFAULT ''"),
        QStringLiteral("ipv4_address|ALTER TABLE devices ADD COLUMN ipv4_address TEXT DEFAULT ''"),
        QStringLiteral("ipv4_subnet_mask|ALTER TABLE devices ADD COLUMN ipv4_subnet_mask TEXT DEFAULT ''"),
        QStringLiteral("ipv4_gateway|ALTER TABLE devices ADD COLUMN ipv4_gateway TEXT DEFAULT ''"),
        QStringLiteral("ipv4_dns_server|ALTER TABLE devices ADD COLUMN ipv4_dns_server TEXT DEFAULT ''"),
        QStringLiteral("ipv4_dhcp_enabled|ALTER TABLE devices ADD COLUMN ipv4_dhcp_enabled INTEGER DEFAULT 0"),
        QStringLiteral("ipv4_config_known|ALTER TABLE devices ADD COLUMN ipv4_config_known INTEGER DEFAULT 0")
    };

    if (!inventoryDb_.transaction()) {
        updateStatus(tr("Could not start inventory schema migration: %1").arg(inventoryDb_.lastError().text()));
        return;
    }
    for (const QString &migration : migrations) {
        const int separator = migration.indexOf(QLatin1Char('|'));
        const QString columnName = migration.left(separator);
        if (existingColumns.contains(columnName)) continue;
        QSqlQuery alterQuery(inventoryDb_);
        if (!alterQuery.exec(migration.mid(separator + 1))) {
            inventoryDb_.rollback();
            updateStatus(tr("Inventory migration failed for %1: %2")
                             .arg(columnName, alterQuery.lastError().text()));
            return;
        }
        existingColumns << columnName;
    }
    if (!inventoryDb_.commit()) {
        inventoryDb_.rollback();
        updateStatus(tr("Could not commit inventory schema migration: %1").arg(inventoryDb_.lastError().text()));
        return;
    }
}

void MainWindow::persistDevice(const DeviceRecord &device)
{
    if (!inventoryDb_.isOpen() || device.ipAddress.isEmpty()) return;
    QString key = !device.macAddress.isEmpty() ? device.macAddress : device.ipAddress;
    // Keep the original SQLite primary key when a later discovery source adds
    // a MAC address or the camera receives a new IP. This prevents a second
    // row from being created for the same physical device after restart.
    QSqlQuery identityQuery(inventoryDb_);
    identityQuery.prepare(QStringLiteral(
        "SELECT device_key FROM devices WHERE "
        "(:mac_match <> '' AND upper(replace(COALESCE(mac,''),'-',':')) = upper(replace(:mac_match,'-',':'))) OR "
        "(:endpoint_match <> '' AND COALESCE(onvif_endpoint,'') = :endpoint_match AND "
        "(:incoming_mac = '' OR COALESCE(mac,'') = '' OR "
        "upper(replace(COALESCE(mac,''),'-',':')) = upper(replace(:incoming_mac,'-',':')))) OR "
        "(:serial_match <> '' AND :manufacturer_match <> '' AND "
        "COALESCE(serial,'') = :serial_match AND "
        "lower(COALESCE(manufacturer,'')) = lower(:manufacturer_match) AND "
        "(:incoming_mac = '' OR COALESCE(mac,'') = '' OR "
        "upper(replace(COALESCE(mac,''),'-',':')) = upper(replace(:incoming_mac,'-',':')))) OR "
        "(COALESCE(ip,'') = :ip_match AND "
        "(COALESCE(mac,'') = '' OR :incoming_mac = '' OR "
        "upper(replace(COALESCE(mac,''),'-',':')) = upper(replace(:incoming_mac,'-',':')))) "
        "ORDER BY CASE "
        "WHEN :mac_order <> '' AND upper(replace(COALESCE(mac,''),'-',':')) = upper(replace(:mac_order,'-',':')) THEN 0 "
        "WHEN :endpoint_order <> '' AND COALESCE(onvif_endpoint,'') = :endpoint_order THEN 1 "
        "WHEN :serial_order <> '' AND COALESCE(serial,'') = :serial_order THEN 2 "
        "ELSE 3 END LIMIT 1"));
    identityQuery.bindValue(":mac_match", device.macAddress.trimmed());
    identityQuery.bindValue(":endpoint_match", device.onvifEndpoint.trimmed());
    identityQuery.bindValue(":serial_match", device.serialNumber.trimmed());
    identityQuery.bindValue(":manufacturer_match", device.manufacturer.trimmed());
    identityQuery.bindValue(":ip_match", device.ipAddress.trimmed());
    identityQuery.bindValue(":incoming_mac", device.macAddress.trimmed());
    identityQuery.bindValue(":mac_order", device.macAddress.trimmed());
    identityQuery.bindValue(":endpoint_order", device.onvifEndpoint.trimmed());
    identityQuery.bindValue(":serial_order", device.serialNumber.trimmed());
    if (!identityQuery.exec()) {
        updateStatus(tr("Could not resolve the existing inventory identity: %1")
                         .arg(identityQuery.lastError().text()));
        return;
    }
    if (identityQuery.next())
        key = identityQuery.value(0).toString();

    QSqlQuery query(inventoryDb_);
    query.prepare(QStringLiteral("INSERT INTO devices (device_key,ip,mac,manufacturer,model,firmware,serial,hardware,activation,source,is_candidate,user_label,reachability_status,reachability_checked_at,last_successful_response_at,interface_name,onvif_endpoint,device_service_url,network_summary,capabilities,open_ports,network_interface_token,ipv4_address,ipv4_subnet_mask,ipv4_gateway,ipv4_dns_server,ipv4_dhcp_enabled,ipv4_config_known,last_seen) VALUES (:key,:ip,:mac,:manufacturer,:model,:firmware,:serial,:hardware,:activation,:source,:candidate,:label,:reachability,:checked,:success,:interface,:endpoint,:service,:network,:capabilities,:ports,:if_token,:ipv4,:mask,:gateway,:dns,:dhcp,:config_known,CURRENT_TIMESTAMP) ON CONFLICT(device_key) DO UPDATE SET ip=excluded.ip,mac=excluded.mac,manufacturer=excluded.manufacturer,model=excluded.model,firmware=excluded.firmware,serial=excluded.serial,hardware=excluded.hardware,activation=excluded.activation,source=excluded.source,is_candidate=excluded.is_candidate,user_label=CASE WHEN excluded.user_label='' THEN devices.user_label ELSE excluded.user_label END,reachability_status=CASE WHEN excluded.reachability_status='Not checked' THEN devices.reachability_status ELSE excluded.reachability_status END,reachability_checked_at=CASE WHEN excluded.reachability_checked_at='' THEN devices.reachability_checked_at ELSE excluded.reachability_checked_at END,last_successful_response_at=CASE WHEN excluded.last_successful_response_at='' THEN devices.last_successful_response_at ELSE excluded.last_successful_response_at END,interface_name=CASE WHEN excluded.interface_name='' THEN devices.interface_name ELSE excluded.interface_name END,onvif_endpoint=CASE WHEN excluded.onvif_endpoint='' THEN devices.onvif_endpoint ELSE excluded.onvif_endpoint END,device_service_url=CASE WHEN excluded.device_service_url='' THEN devices.device_service_url ELSE excluded.device_service_url END,network_summary=CASE WHEN excluded.network_summary='' THEN devices.network_summary ELSE excluded.network_summary END,capabilities=CASE WHEN excluded.capabilities='' THEN devices.capabilities ELSE excluded.capabilities END,open_ports=CASE WHEN excluded.open_ports='' THEN devices.open_ports ELSE excluded.open_ports END,network_interface_token=CASE WHEN excluded.source LIKE '%ONVIF GetNetworkInterfaces%' THEN excluded.network_interface_token WHEN excluded.network_interface_token='' THEN devices.network_interface_token ELSE excluded.network_interface_token END,ipv4_address=CASE WHEN excluded.source LIKE '%ONVIF GetNetworkInterfaces%' THEN excluded.ipv4_address WHEN excluded.ipv4_address='' THEN devices.ipv4_address ELSE excluded.ipv4_address END,ipv4_subnet_mask=CASE WHEN excluded.source LIKE '%ONVIF GetNetworkInterfaces%' THEN excluded.ipv4_subnet_mask WHEN excluded.ipv4_subnet_mask='' THEN devices.ipv4_subnet_mask ELSE excluded.ipv4_subnet_mask END,ipv4_gateway=CASE WHEN excluded.source LIKE '%ONVIF GetNetworkInterfaces%' THEN excluded.ipv4_gateway WHEN excluded.ipv4_gateway='' THEN devices.ipv4_gateway ELSE excluded.ipv4_gateway END,ipv4_dns_server=CASE WHEN excluded.ipv4_dns_server='' THEN devices.ipv4_dns_server ELSE excluded.ipv4_dns_server END,ipv4_dhcp_enabled=CASE WHEN excluded.source LIKE '%ONVIF GetNetworkInterfaces%' THEN excluded.ipv4_dhcp_enabled WHEN excluded.ipv4_config_known=0 THEN devices.ipv4_dhcp_enabled ELSE excluded.ipv4_dhcp_enabled END,ipv4_config_known=CASE WHEN excluded.source LIKE '%ONVIF GetNetworkInterfaces%' THEN excluded.ipv4_config_known ELSE MAX(devices.ipv4_config_known,excluded.ipv4_config_known) END,last_seen=CURRENT_TIMESTAMP"));
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
    recordOperation(tr("Manual device entry"), device.ipAddress, tr("Completed"), tr("Device added to local inventory"));
    refreshOperationHistory();
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
