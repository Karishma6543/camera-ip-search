#include "main_window.h"
#include "camera_browser_dialog.h"
#include "core/diagnostic_log.h"
#include "core/device_identity.h"
#include "core/device_classifier.h"
#include "core/operation_compatibility.h"
#include "core/inventory_backup.h"

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
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTabBar>
#include <QStackedWidget>
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
#include <QSet>
#include <QMenu>
#include <QAction>
#include <QProgressBar>
#include <QPainter>
#include <QPixmap>
#include <QPainterPath>
#include <QRadialGradient>
#include <QPaintEvent>
#include "oxita_theme.h"

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

QString reservedIpv4Reason(const QString &candidate, const QTableWidget *table,
                           int excludedRow)
{
    QHostAddress parsed;
    if (!parsed.setAddress(candidate.trimmed()) ||
        parsed.protocol() != QAbstractSocket::IPv4Protocol)
        return {};
    const QString normalized = parsed.toString();
    if (table) {
        for (int row = 0; row < table->rowCount(); ++row) {
            if (row == excludedRow) continue;
            const auto *ipItem = table->item(row, 1);
            if (ipItem && QHostAddress(ipItem->text()).toString() == normalized)
                return QObject::tr("Address %1 is already assigned to another inventory device.").arg(normalized);
            if (ipItem && QHostAddress(ipItem->data(Qt::UserRole + 10).toString()).toString() == normalized)
                return QObject::tr("Address %1 is reported as another device's default gateway.").arg(normalized);
        }
    }
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
                entry.ip().toString() == normalized)
                return QObject::tr("Address %1 is already assigned to this computer's %2 interface.")
                    .arg(normalized, interface.humanReadableName());
        }
    }
    return {};
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    Q_INIT_RESOURCE(oxita_branding);
    discovery_ = new DiscoveryService(this);
    subnetScanner_ = new SubnetScanner(this);
    reachabilityChecker_ = new ReachabilityChecker(this);
    buildUi();
    initializeInventory();
    loadInventory();
    refreshOperationHistory();
    populateInterfaces();
    updateLocalNetworkSummary();
    interfaceRefreshTimer_ = new QTimer(this);
    interfaceRefreshTimer_->setObjectName(QStringLiteral("interfaceRefreshTimer"));
    interfaceRefreshTimer_->setInterval(3000);
    connect(interfaceRefreshTimer_, &QTimer::timeout, this, &MainWindow::populateInterfaces);
    interfaceRefreshTimer_->start();
    if (inventoryDb_.isOpen())
        updateStatus(tr("Ready — choose a local interface to scan your subnet."));
    connect(discovery_, &DiscoveryService::deviceFound, this, [this](const DeviceRecord &device) {
        upsertDevice(device);
        if (!pendingCameraVerificationActive_ || pendingCameraIdentityMatched_) return;
        if (DeviceIdentity::hasStrongMatch(pendingCameraVerification_, device)) {
            pendingCameraIdentityMatched_ = true;
            updateStatus(tr("The camera was rediscovered and matched using its previous identity evidence. Initialization still requires device-side verification."));
            recordOperation(tr("Post-setup camera identity check"), device.ipAddress,
                            tr("Identity matched"),
                            tr("Rediscovered device matched the selected camera by MAC, serial number or ONVIF endpoint. Activation status is not inferred."));
        }
    });
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
    connect(discovery_, &DiscoveryService::maintenanceOperationFinished, this,
            [this](const DeviceRecord &device, const QString &operation, bool succeeded,
                   const QString &message) {
        if (auto *button = findChild<QPushButton *>(QStringLiteral("factoryResetCameraButton"))) {
            button->setEnabled(false);
            button->setToolTip(tr("Disabled until factory-reset support is verified for this exact camera model and firmware."));
        }
        recordOperation(operation, device.ipAddress,
                        succeeded ? tr("Accepted") : tr("Failed"), message);
        refreshOperationHistory();
        updateStatus(message);
        if (succeeded) QMessageBox::information(this, tr("Camera maintenance request accepted"), message);
        else QMessageBox::warning(this, tr("Camera maintenance failed"), message);
    });
    connect(discovery_, &DiscoveryService::networkConfigurationFinished, this,
            [this](const DeviceRecord &updatedDevice, bool verified, const QString &message) {
        if (!bulkIpv4Running_) {
            const int row = selectedDeviceRow();
            const auto *ip = row >= 0 ? deviceTable_->item(row, 1) : nullptr;
            const bool ready = ip && deviceTable_->item(row, 8) &&
                deviceTable_->item(row, 8)->text().contains(QStringLiteral("ONVIF"), Qt::CaseInsensitive) &&
                !ip->data(Qt::UserRole + 3).toString().isEmpty() &&
                !ip->data(Qt::UserRole + 7).toString().isEmpty() &&
                ip->data(Qt::UserRole + 16).toBool();
            if (auto *button = findChild<QPushButton *>(QStringLiteral("applyStaticIpv4Button")))
                button->setEnabled(ready);
        }
        if (bulkIpv4Running_) {
            recordOperation(tr("Bulk static IPv4"), updatedDevice.ipAddress,
                            verified ? tr("Verified") : tr("Not verified"), message);
            if (verified) ++bulkIpv4Succeeded_;
            else ++bulkIpv4Failed_;
            ++bulkIpv4Next_;
            if (operationProgress_) {
                operationProgress_->setValue(bulkIpv4Next_);
                operationProgress_->setFormat(tr("Bulk IPv4 %1 / %2").arg(bulkIpv4Next_).arg(bulkIpv4Queue_.size()));
            }
            refreshOperationHistory();
            updateStatus(tr("Bulk IPv4 configuration: %1 / %2 completed.")
                             .arg(bulkIpv4Next_).arg(bulkIpv4Queue_.size()));
            startNextBulkIpv4Configuration();
            return;
        }
        recordOperation(tr("Static IPv4"), updatedDevice.ipAddress,
                        verified ? tr("Verified") : tr("Not verified"), message);
        refreshOperationHistory();
        if (!verified && message.contains(QStringLiteral("gateway change was not confirmed"),
                                             Qt::CaseInsensitive)) {
            QMessageBox recovery(this);
            recovery.setIcon(QMessageBox::Warning);
            recovery.setWindowTitle(tr("IP address verified — gateway needs attention"));
            recovery.setText(message);
            recovery.setInformativeText(
                tr("The camera is now recorded at %1. If the camera is no longer reachable from this computer, reconnect using the new address before attempting another change.")
                    .arg(updatedDevice.ipAddress));
            recovery.setDetailedText(
                tr("Suggested recovery steps:\\n"
                   "1. Confirm that your computer is connected to the camera's new subnet.\\n"
                   "2. Open the camera at %1 using a browser or ONVIF client.\\n"
                   "3. Check the IPv4 address, subnet mask and default gateway on the camera.\\n"
                   "4. Correct the gateway manually in the camera interface if it is still incorrect.\\n"
                   "5. Return to this application and run discovery again.\\n\\n"
                   "The application does not automatically roll back a verified IP change.")
                    .arg(updatedDevice.ipAddress));
            recovery.setStandardButtons(QMessageBox::Ok);
            recovery.exec();
        } else {
            QMessageBox::information(this, verified ? tr("Network configuration verified")
                                                    : tr("Network configuration result"), message);
        }
        updateStatus(message);
    });
    connect(discovery_, &DiscoveryService::cancelled, this, [this](int count) {
        scanButton_->setText(tr("Discover cameras"));
        scanButton_->setEnabled(true);
        subnetScanButton_->setEnabled(true);
        if (operationProgress_) operationProgress_->hide();
        recordOperation(tr("Discovery"), interfaceSelector_ ? interfaceSelector_->currentText() : QString(),
                        tr("Cancelled"), tr("%1 device(s) found before cancellation").arg(count));
        refreshOperationHistory();
        if (pendingCameraVerificationActive_) {
            pendingCameraVerificationActive_ = false;
            pendingCameraIdentityMatched_ = false;
            updateStatus(tr("Post-setup discovery was cancelled. Camera initialization remains unverified."));
        } else {
            updateStatus(tr("Discovery cancelled — %1 device(s) retained.").arg(count));
        }
    });
    connect(discovery_, &DiscoveryService::finished, this, [this](int count) {
        scanButton_->setText(tr("Discover cameras"));
        scanButton_->setEnabled(true);
        subnetScanButton_->setEnabled(true);
        if (operationProgress_) operationProgress_->hide();
        updateDeviceSummary();
        recordOperation(tr("Discovery"), interfaceSelector_ ? interfaceSelector_->currentText() : QString(), tr("Completed"), tr("%1 device(s) reported").arg(count));
        refreshOperationHistory();
        if (pendingCameraVerificationActive_) {
            const bool matched = pendingCameraIdentityMatched_;
            pendingCameraVerificationActive_ = false;
            pendingCameraIdentityMatched_ = false;
            updateStatus(matched
                ? tr("Camera identity was rediscovered. Review its reported status and network details; activation is not marked complete without explicit device evidence.")
                : tr("The selected camera could not be matched after setup. It may have changed address, be outside this interface's subnet, or not support the active discovery protocols. Initialization remains unverified."));
        } else {
            updateStatus(tr("Discovery finished — %1 device(s) found.").arg(count));
        }
    });
    connect(subnetScanner_, &SubnetScanner::candidateFound, this, &MainWindow::upsertDevice);
    connect(subnetScanner_, &SubnetScanner::statusChanged, this, &MainWindow::updateStatus);
    connect(subnetScanner_, &SubnetScanner::progressChanged, this,
            [this](int completed, int total, int candidates) {
        if (total > 0) {
            if (operationProgress_) {
                operationProgress_->setRange(0, total);
                operationProgress_->setValue(completed);
                operationProgress_->setFormat(tr("Subnet %1 / %2").arg(completed).arg(total));
            }
            updateStatus(tr("Subnet scan: %1 / %2 connection checks completed; %3 unverified candidate(s) found.")
                             .arg(completed).arg(total).arg(candidates));
        }
    });
    connect(subnetScanner_, &SubnetScanner::cancelled, this, [this](int count) {
        subnetScanButton_->setText(tr("Scan selected subnet"));
        subnetScanButton_->setEnabled(true);
        scanButton_->setEnabled(true);
        if (operationProgress_) operationProgress_->hide();
        recordOperation(tr("Subnet scan"), interfaceSelector_ ? interfaceSelector_->currentText() : QString(),
                        tr("Cancelled"), tr("%1 unverified candidate(s) found before cancellation").arg(count));
        refreshOperationHistory();
        updateStatus(tr("Subnet scan cancelled — %1 unverified candidate(s) retained.").arg(count));
    });
    connect(subnetScanner_, &SubnetScanner::finished, this, [this](int count) {
        subnetScanButton_->setText(tr("Scan selected subnet"));
        subnetScanButton_->setEnabled(true);
        scanButton_->setEnabled(true);
        if (operationProgress_) operationProgress_->hide();
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
    if (inventoryDb_.isOpen()) {
        recordOperation(tr("Application shutdown"), QString(), tr("Completed"),
                        tr("Active network operations were asked to cancel during shutdown"));
        inventoryDb_.close();
    }
    inventoryDb_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(QStringLiteral("camera_inventory"));
}

void MainWindow::populateInterfaces()
{
    if (!interfaceSelector_) return;
    const QString previousSelection = interfaceSelector_->currentData().toString();
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    QStringList fingerprintParts;
    int interfaceCount = 0;
    int activeInterfaceCount = 0;
    int activeIpv4Count = 0;
    for (const QNetworkInterface &interface : interfaces) {
        const auto flags = interface.flags();
        if (flags.testFlag(QNetworkInterface::IsLoopBack)) continue;
        ++interfaceCount;
        const bool active = flags.testFlag(QNetworkInterface::IsUp) &&
                            flags.testFlag(QNetworkInterface::IsRunning);
        if (active) ++activeInterfaceCount;
        QStringList addresses;
        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            if (entry.ip().isLoopback()) continue;
            addresses << QStringLiteral("%1/%2").arg(entry.ip().toString()).arg(entry.prefixLength());
            if (active && entry.ip().protocol() == QAbstractSocket::IPv4Protocol) ++activeIpv4Count;
        }
        addresses.sort();
        fingerprintParts << QStringLiteral("%1|%2|%3|%4")
            .arg(interface.name()).arg(static_cast<int>(flags))
            .arg(interface.hardwareAddress(), addresses.join(QLatin1Char(',')));
    }
    fingerprintParts.sort();
    const QString fingerprint = fingerprintParts.join(QLatin1Char(';'));
    if (fingerprint == interfaceInventoryFingerprint_) return;
    const bool changed = !interfaceInventoryFingerprint_.isEmpty();
    interfaceInventoryFingerprint_ = fingerprint;

    interfaceSelector_->clear();
    if (activeIpv4Count > 0)
        interfaceSelector_->addItem(tr("All active IPv4 interfaces"), QString());
    for (const QNetworkInterface &interface : interfaces) {
        const auto flags = interface.flags();
        if (flags.testFlag(QNetworkInterface::IsLoopBack)) continue;
        const bool isUp = flags.testFlag(QNetworkInterface::IsUp);
        const bool isRunning = flags.testFlag(QNetworkInterface::IsRunning);
        bool hasIpv4 = false;
        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
                !entry.ip().isLoopback() && entry.prefixLength() >= 1) {
                hasIpv4 = true;
                break;
            }
        }
        QString label = interface.humanReadableName().isEmpty()
            ? interface.name() : interface.humanReadableName();
        if (!isUp) label += tr(" (disabled)");
        else if (!isRunning) label += tr(" (link down)");
        else if (!hasIpv4) label += tr(" (link up — waiting for IPv4)");
        interfaceSelector_->addItem(label, interface.name());
    }
    if (interfaceCount == 0)
        interfaceSelector_->addItem(tr("No network adapters available"), QString());

    const int previousIndex = interfaceSelector_->findData(previousSelection);
    if (previousIndex >= 0) interfaceSelector_->setCurrentIndex(previousIndex);
    updateLocalNetworkSummary();
    if (changed) {
        updateStatus(tr("Network adapter state or addresses changed. Interface list refreshed; choose the camera-connected adapter if needed."));
    } else if (activeInterfaceCount == 0 && interfaceCount > 0) {
        updateStatus(tr("No network adapter currently has an active link. Connect the camera and check its power, PoE and Ethernet cable."));
    } else if (activeInterfaceCount > 0 && activeIpv4Count == 0) {
        updateStatus(tr("A network link is active but no IPv4 address is assigned yet. Check DHCP or wait for link-local addressing."));
    }
}
void MainWindow::buildUi()
{
    setWindowTitle(tr("Oxita | Camera Device Manager"));
    setMinimumSize(1100, 680);
    resize(1440, 900);

    setStyleSheet(OxitaTheme::styleSheet());

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("appRoot"));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(18, 16, 18, 12);
    root->setSpacing(12);

    auto *top = new QFrame(central);
    top->setObjectName(QStringLiteral("topBar"));
    auto *topLayout = new QHBoxLayout(top);
    topLayout->setContentsMargins(18, 11, 18, 11);
    topLayout->setSpacing(10);
    auto *brandLogo = new QLabel(top);
    brandLogo->setObjectName(QStringLiteral("brandLogo"));
    brandLogo->setFixedSize(158, 50);
    brandLogo->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    const QPixmap oxitaLogo(QStringLiteral(":/branding/oxita-logo.png"));
    if (!oxitaLogo.isNull())
        brandLogo->setPixmap(oxitaLogo.scaled(158, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    brandLogo->setAccessibleName(tr("Oxita — Driven by Innovation"));
    auto *productName = new QLabel(tr("CAMERA DEVICE MANAGER"), top);
    productName->setObjectName(QStringLiteral("productName"));
    topLayout->addWidget(brandLogo);
    topLayout->addSpacing(10);
    topLayout->addWidget(productName);
    topLayout->addStretch();
    auto *help = actionButton(tr("Help"), top);
    help->setObjectName(QStringLiteral("helpButton"));
    help->setToolTip(tr("Guidance for discovery, device details and safe network configuration."));
    auto *settings = actionButton(tr("Settings"), top);
    settings->setObjectName(QStringLiteral("settingsButton"));
    settings->setToolTip(tr("View local application storage and diagnostic information."));
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
    searchEdit_->setObjectName(QStringLiteral("deviceSearch"));
    searchEdit_->setAccessibleName(tr("Search network inventory"));
    searchEdit_->setPlaceholderText(tr("Search devices by IP, name, MAC or model"));
    searchEdit_->setMinimumWidth(235);
    searchEdit_->setToolTip(tr("Search all inventory fields, including technical details hidden from the compact table."));
    searchEdit_->setClearButtonEnabled(true);
    interfaceSelector_ = new QComboBox(toolbar);
    interfaceSelector_->setObjectName(QStringLiteral("interfaceSelector"));
    interfaceSelector_->setAccessibleName(tr("Network interface for discovery and scanning"));
    interfaceSelector_->setMinimumWidth(165);
    interfaceSelector_->setToolTip(tr("Choose the active network adapter used for discovery and subnet scanning."));
    deviceTypeFilter_ = new QComboBox(toolbar);
    deviceTypeFilter_->setObjectName(QStringLiteral("deviceTypeFilter"));
    deviceTypeFilter_->setAccessibleName(tr("Filter by device type"));
    deviceTypeFilter_->setToolTip(tr("Show cameras, NVRs, routers, switches, printers, computers or devices that cannot yet be identified."));
    deviceTypeFilter_->addItem(tr("All device types"), QStringLiteral("all"));
    deviceTypeFilter_->addItem(tr("Cameras"), QStringLiteral("camera"));
    deviceTypeFilter_->addItem(tr("NVRs"), QStringLiteral("nvr"));
    deviceTypeFilter_->addItem(tr("Routers"), QStringLiteral("router"));
    deviceTypeFilter_->addItem(tr("Switches"), QStringLiteral("switch"));
    deviceTypeFilter_->addItem(tr("Printers"), QStringLiteral("printer"));
    deviceTypeFilter_->addItem(tr("Computers"), QStringLiteral("computer"));
    deviceTypeFilter_->addItem(tr("Unknown"), QStringLiteral("unknown"));
    manufacturerFilter_ = new QComboBox(toolbar);
    manufacturerFilter_->setObjectName(QStringLiteral("manufacturerFilter"));
    manufacturerFilter_->setAccessibleName(tr("Filter by manufacturer or discovery protocol"));
    auto *vendorFilter = manufacturerFilter_;
    vendorFilter->addItems({tr("All manufacturers"), tr("ONVIF devices"), tr("Oxita devices"), tr("Other / unknown")});
    statusFilter_ = new QComboBox(toolbar);
    statusFilter_->setObjectName(QStringLiteral("statusFilter"));
    statusFilter_->setAccessibleName(tr("Filter by reachability and activation state"));
    auto *statusFilter = statusFilter_;
    statusFilter->addItems({tr("All statuses"), tr("Online"), tr("Offline"), tr("Stale"), tr("Activated"), tr("Not activated")});
    deviceCountValue_ = new QLabel(QStringLiteral("0 devices"), toolbar);
    deviceCountValue_->setObjectName(QStringLiteral("muted"));
    filterRow->addWidget(searchEdit_, 2);
    filterRow->addWidget(interfaceSelector_);
    connect(interfaceSelector_, &QComboBox::currentIndexChanged, this,
            [this](int) { updateLocalNetworkSummary(); });
    filterRow->addWidget(deviceTypeFilter_);
    filterRow->addWidget(vendorFilter);
    filterRow->addWidget(statusFilter);
    filterRow->addStretch();
    filterRow->addWidget(deviceCountValue_);
    tools->addLayout(filterRow);

    auto *actionRow = new QHBoxLayout();
    actionRow->setSpacing(8);
    addDeviceButton_ = actionButton(tr("Add device"), toolbar);
    addDeviceButton_->setToolTip(tr("Manually add a camera or network device to the local inventory."));
    addDeviceButton_->setObjectName(QStringLiteral("addDeviceButton"));
    actionRow->addWidget(addDeviceButton_);
    auto *exportButton = actionButton(tr("Export CSV"), toolbar);
    exportButton->setToolTip(tr("Export live results, or saved inventory when no live results are present, to UTF-8 CSV."));
    exportButton->setObjectName(QStringLiteral("exportCsvButton"));
    actionRow->addWidget(exportButton);
    auto *renameButton = actionButton(tr("Rename"), toolbar);
    renameButton->setToolTip(tr("Set a friendly name for the selected camera."));
    renameButton->setEnabled(false);
    renameButton->setObjectName(QStringLiteral("renameLabelButton"));
    actionRow->addWidget(renameButton);
    actionRow->addStretch();
    reachabilityButton_ = actionButton(tr("Check reachability"), toolbar);
    reachabilityButton_->setToolTip(tr("Check checked devices, selected devices or the full inventory."));
    reachabilityButton_->setObjectName(QStringLiteral("reachabilityButton"));
    actionRow->addWidget(reachabilityButton_);
    subnetScanButton_ = actionButton(tr("Scan subnet"), toolbar);
    subnetScanButton_->setToolTip(tr("Find unverified hosts on the selected interface subnet; candidates are not confirmed cameras."));
    subnetScanButton_->setObjectName(QStringLiteral("subnetScanButton"));
    actionRow->addWidget(subnetScanButton_);
    scanButton_ = actionButton(tr("Discover cameras"), toolbar, true);
    scanButton_->setToolTip(tr("Discover ONVIF and supported Oxita devices on the selected network interface."));
    scanButton_->setObjectName(QStringLiteral("discoverButton"));
    actionRow->addWidget(scanButton_);
    tools->addLayout(actionRow);
    root->addWidget(toolbar);

    auto *left = new QFrame(central);
    left->setObjectName(QStringLiteral("surface"));
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(12, 11, 12, 12);
    leftLayout->setSpacing(8);
    auto *listHeading = new QHBoxLayout();
    auto *listTitle = new QLabel(tr("Network inventory"), left);
    listTitle->setObjectName(QStringLiteral("sectionTitle"));
    summaryLabel_ = mutedLabel(tr("Discover cameras on your selected network"), left);
    auto *selectAllChecks = actionButton(tr("Select all"), left);
    auto *clearChecks = actionButton(tr("Clear"), left);
    selectAllChecks->setToolTip(tr("Check every device for the next reachability operation."));
    clearChecks->setToolTip(tr("Clear all device checkboxes."));
    listHeading->addWidget(listTitle);
    listHeading->addStretch();
    listHeading->addWidget(summaryLabel_);
    listHeading->addStretch();
    listHeading->addWidget(selectAllChecks);
    listHeading->addWidget(clearChecks);
    leftLayout->addLayout(listHeading);

    deviceTable_ = new QTableWidget(0, 13, left);
    deviceTable_->setObjectName(QStringLiteral("deviceTable"));
    deviceTable_->setHorizontalHeaderLabels({tr(""), tr("IP ADDRESS"), tr("DEVICE NAME"), tr("MANUFACTURER"), tr("MODEL"), tr("MAC ADDRESS"), tr("FIRMWARE"), tr("ACTIVATION"), tr("DISCOVERY SOURCE"), tr("SERIAL NUMBER"), tr("HARDWARE ID"), tr("REACHABILITY"), tr("DEVICE TYPE")});
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
    deviceHeader->resizeSection(12, 112);
    deviceHeader->setMinimumSectionSize(72);
    // Keep the primary inventory view scannable; technical identifiers remain
    // searchable and are available in Device details and CSV export.
    deviceTable_->setColumnHidden(5, true);
    deviceTable_->setColumnHidden(6, true);
    deviceTable_->setColumnHidden(8, true);
    deviceTable_->setColumnHidden(9, true);
    deviceTable_->setColumnHidden(10, true);
    deviceTable_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    deviceTable_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    deviceTable_->verticalHeader()->setVisible(false);
    deviceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    deviceTable_->setAccessibleName(tr("Network device inventory table"));
    deviceTable_->setAccessibleDescription(tr("Select a row to inspect device details. Use the first column to include compatible cameras in bulk operations."));
    deviceTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    deviceTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    deviceTable_->setAlternatingRowColors(true);
    deviceTable_->setShowGrid(false);
    deviceTable_->setMinimumHeight(170);
    deviceTable_->verticalHeader()->setDefaultSectionSize(38);
    deviceTable_->setWordWrap(false);
    deviceTable_->setCornerButtonEnabled(false);
    deviceHeader->setSectionsClickable(true);
    deviceHeader->setSortIndicatorShown(true);
    deviceHeader->setSortIndicator(1, Qt::AscendingOrder);
    connect(deviceHeader, &QHeaderView::sectionClicked, this,
            [this, deviceHeader](int column) {
        if (column <= 0 || column >= deviceTable_->columnCount()) return;
        if (sortColumn_ == column) sortAscending_ = !sortAscending_;
        else {
            sortColumn_ = column;
            sortAscending_ = true;
        }
        const Qt::SortOrder order = sortAscending_ ? Qt::AscendingOrder : Qt::DescendingOrder;
        deviceTable_->sortItems(column, order);
        deviceHeader->setSortIndicator(column, order);
        updateDeviceDetailsProfile(selectedDeviceRow());
        updateSelectedDevicePanel();
    });
    deviceHeader->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(deviceHeader, &QWidget::customContextMenuRequested, this,
            [this, deviceHeader](const QPoint &position) {
        QMenu menu(deviceTable_);
        for (int column = 1; column < deviceTable_->columnCount(); ++column) {
            auto *action = menu.addAction(deviceTable_->horizontalHeaderItem(column)->text());
            action->setCheckable(true);
            action->setChecked(!deviceTable_->isColumnHidden(column));
            connect(action, &QAction::toggled, this, [this, column](bool visible) {
                deviceTable_->setColumnHidden(column, !visible);
            });
        }
        menu.exec(deviceHeader->mapToGlobal(position));
    });

    deviceTable_->setToolTip(tr("Select a row to inspect device details. The type column includes a confidence level in its tooltip. Check the first column to include devices in reachability checks."));
    auto *resultTabs = new QTabWidget(left);
    resultTabs->setObjectName(QStringLiteral("deviceResultTabs"));
    auto *livePage = new QWidget(resultTabs);
    auto *liveLayout = new QVBoxLayout(livePage);
    liveLayout->setContentsMargins(0, 6, 0, 0);
    inventoryEmptyState_ = new QLabel(
        tr("No live scan results yet.\nChoose a network interface and select Discover cameras or Scan subnet."), livePage);
    inventoryEmptyState_->setObjectName(QStringLiteral("inventoryEmptyState"));
    inventoryEmptyState_->setAlignment(Qt::AlignCenter);
    inventoryEmptyState_->setWordWrap(true);
    inventoryEmptyState_->setMinimumHeight(88);
    liveLayout->addWidget(inventoryEmptyState_);
    liveLayout->addWidget(deviceTable_, 1);
    resultTabs->addTab(livePage, tr("Live Scan Results"));

    auto *savedPage = new QWidget(resultTabs);
    auto *savedLayout = new QVBoxLayout(savedPage);
    savedLayout->setContentsMargins(0, 6, 0, 0);
    auto *savedToolbar = new QHBoxLayout();
    savedToolbar->addWidget(mutedLabel(
        tr("Historical records are stored locally and are never treated as live scan results."), savedPage), 1);
    auto *retention = new QComboBox(savedPage);
    retention->setObjectName(QStringLiteral("savedInventoryRetention"));
    retention->addItem(tr("Keep forever"), 0);
    retention->addItem(tr("Keep 30 days"), 30);
    retention->addItem(tr("Keep 90 days"), 90);
    retention->addItem(tr("Keep 1 year"), 365);
    auto *applyRetention = actionButton(tr("Apply retention"), savedPage);
    applyRetention->setObjectName(QStringLiteral("applyInventoryRetentionButton"));
    auto *deleteSelected = actionButton(tr("Delete selected"), savedPage);
    deleteSelected->setObjectName(QStringLiteral("deleteSavedInventoryButton"));
    auto *clearSaved = actionButton(tr("Clear saved inventory"), savedPage);
    clearSaved->setObjectName(QStringLiteral("clearSavedInventoryButton"));
    savedToolbar->addWidget(retention);
    savedToolbar->addWidget(applyRetention);
    savedToolbar->addWidget(deleteSelected);
    savedToolbar->addWidget(clearSaved);
    savedLayout->addLayout(savedToolbar);
    savedInventoryEmptyState_ = new QLabel(tr("No saved devices yet. Devices appear here after discovery or manual inventory entry."), savedPage);
    savedInventoryEmptyState_->setObjectName(QStringLiteral("savedInventoryEmptyState"));
    savedInventoryEmptyState_->setAlignment(Qt::AlignCenter);
    savedInventoryEmptyState_->setWordWrap(true);
    savedLayout->addWidget(savedInventoryEmptyState_);
    savedInventoryTable_ = new QTableWidget(0, 7, savedPage);
    savedInventoryTable_->setObjectName(QStringLiteral("savedInventoryTable"));
    savedInventoryTable_->setHorizontalHeaderLabels({tr("IP ADDRESS"), tr("DEVICE NAME"), tr("MANUFACTURER"), tr("MODEL"), tr("MAC ADDRESS"), tr("LAST SEEN (UTC)"), tr("STATUS")});
    savedInventoryTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    savedInventoryTable_->verticalHeader()->setVisible(false);
    savedInventoryTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    savedInventoryTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    savedInventoryTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    savedInventoryTable_->setAlternatingRowColors(true);
    savedInventoryTable_->setShowGrid(false);
    savedLayout->addWidget(savedInventoryTable_, 1);
    resultTabs->addTab(savedPage, tr("Saved Inventory"));
    leftLayout->addWidget(resultTabs, 1);

    auto *workspaceNavigation = new QFrame(central);
    workspaceNavigation->setObjectName(QStringLiteral("workspaceNavigation"));
    auto *navigationLayout = new QHBoxLayout(workspaceNavigation);
    navigationLayout->setContentsMargins(10, 3, 10, 3);
    navigationLayout->setSpacing(4);
    auto *workspaceTabs = new QTabBar(workspaceNavigation);
    workspaceTabs->setObjectName(QStringLiteral("workspaceTabs"));
    workspaceTabs->setDocumentMode(true);
    workspaceTabs->setExpanding(false);
    workspaceTabs->setUsesScrollButtons(true);
    workspaceTabs->setDrawBase(false);
    workspaceTabs->addTab(tr("Device Info"));
    workspaceTabs->addTab(tr("Configuration"));
    workspaceTabs->addTab(tr("Operation History"));
    navigationLayout->addWidget(workspaceTabs);
    navigationLayout->addStretch();
    root->addWidget(workspaceNavigation);

    auto *rightWorkspace = new QFrame(central);
    rightWorkspace->setObjectName(QStringLiteral("rightWorkspacePanel"));
    rightWorkspace->setMinimumWidth(340);
    rightWorkspace->setMaximumWidth(470);
    auto *rightLayout = new QVBoxLayout(rightWorkspace);
    rightLayout->setContentsMargins(6, 6, 6, 6);
    rightLayout->setSpacing(8);
    auto *workspacePages = new QStackedWidget(rightWorkspace);
    workspacePages->setObjectName(QStringLiteral("workspacePages"));
    workspacePages->addWidget(makeInfoTab(workspacePages));
    workspacePages->addWidget(makeMaintenanceTab(workspacePages));
    workspacePages->addWidget(makeHistoryTab(workspacePages));
    rightLayout->addWidget(workspacePages, 1);
    connect(workspaceTabs, &QTabBar::currentChanged,
            workspacePages, &QStackedWidget::setCurrentIndex);
    connect(workspacePages, &QStackedWidget::currentChanged,
            workspaceTabs, &QTabBar::setCurrentIndex);
    workspaceTabs->setCurrentIndex(0);

    auto *mainWorkspace = new QSplitter(Qt::Horizontal, central);
    mainWorkspace->setObjectName(QStringLiteral("mainWorkspaceSplitter"));
    mainWorkspace->setChildrenCollapsible(false);
    mainWorkspace->setHandleWidth(7);
    mainWorkspace->addWidget(left);
    mainWorkspace->addWidget(rightWorkspace);
    mainWorkspace->setStretchFactor(0, 7);
    mainWorkspace->setStretchFactor(1, 3);
    mainWorkspace->setSizes({980, 420});
    root->addWidget(mainWorkspace, 1);

    auto *footer = mutedLabel(tr("OXITA CAMERA DEVICE MANAGER  •  Device capabilities depend on the camera model, firmware and enabled services."), central);
    root->addWidget(footer);
    setCentralWidget(central);
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("applicationStatus"));
    statusBar()->addWidget(statusLabel_, 1);
    operationProgress_ = new QProgressBar(this);
    operationProgress_->setObjectName(QStringLiteral("operationProgress"));
    operationProgress_->setAccessibleName(tr("Current network operation progress"));
    operationProgress_->setAccessibleDescription(tr("Progress of discovery, scanning, reachability or bulk configuration."));
    operationProgress_->setMaximumWidth(220);
    operationProgress_->setMinimumWidth(130);
    operationProgress_->setTextVisible(true);
    operationProgress_->setRange(0, 0);
    operationProgress_->hide();
    statusBar()->addPermanentWidget(operationProgress_);


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
        if (operationProgress_) {
            operationProgress_->setRange(0, devices.size());
            operationProgress_->setValue(0);
            operationProgress_->setFormat(tr("Reachability 0 / %1").arg(devices.size()));
            operationProgress_->show();
        }
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
                if (auto *ipItem = deviceTable_->item(row, 1)) {
                    ipItem->setData(Qt::UserRole + 14, checkedAt);
                    ipItem->setData(Qt::UserRole + 15, lastSuccessAt);
                    if (status == QStringLiteral("Reachable"))
                        ipItem->setData(Qt::UserRole + 19, checkedAt);
                }
                if (row == selectedDeviceRow()) {
                    updateDeviceDetailsProfile(row);
                    updateSelectedDevicePanel();
                }
                const QString mac = deviceTable_->item(row, 5) ? deviceTable_->item(row, 5)->text() : QString();
                if (inventoryDb_.isOpen()) {
                    QSqlQuery query(inventoryDb_);
                    query.prepare(QStringLiteral("UPDATE devices SET reachability_status=:status, reachability_checked_at=:checked, last_successful_response_at=:success, last_seen=CASE WHEN :status_seen='Reachable' THEN CURRENT_TIMESTAMP ELSE last_seen END WHERE device_key=:key"));
                    query.bindValue(":status", status);
                    query.bindValue(":checked", checkedAt);
                    query.bindValue(":success", lastSuccessAt);
                    query.bindValue(":status_seen", status);
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
        if (operationProgress_) operationProgress_->hide();
        recordOperation(tr("Reachability batch"), QString(), tr("Completed"), tr("%1 device(s) checked").arg(checked));
        refreshOperationHistory();
        updateStatus(tr("Reachability check complete — %1 device(s). No TCP response does not prove a device is offline.").arg(checked));
    });
    connect(reachabilityChecker_, &ReachabilityChecker::progressChanged, this,
            [this](int checked, int total, const QString &ip) {
        if (total > 0 && !ip.isEmpty()) {
            if (operationProgress_) {
                operationProgress_->setRange(0, total);
                operationProgress_->setValue(checked);
                operationProgress_->setFormat(tr("Reachability %1 / %2").arg(checked + 1).arg(total));
            }
            updateStatus(tr("Checking reachability: %1 / %2 — %3")
                             .arg(checked + 1).arg(total).arg(ip));
        }
    });
    connect(reachabilityChecker_, &ReachabilityChecker::cancelled, this, [this](int checked) {
        reachabilityButton_->setEnabled(true);
        reachabilityButton_->setText(tr("Check reachability"));
        if (operationProgress_) operationProgress_->hide();
        recordOperation(tr("Reachability batch"), QString(), tr("Cancelled"), tr("%1 device(s) checked before cancellation").arg(checked));
        refreshOperationHistory();
        updateStatus(tr("Reachability check cancelled after %1 device(s).").arg(checked));
    });
    connect(scanButton_, &QPushButton::clicked, this, [this] {
        if (subnetScanner_->isRunning()) {
            updateStatus(tr("Wait for the subnet scan to finish or cancel it before starting discovery."));
            return;
        }
        if (discovery_->isRunning()) {
            discovery_->cancel();
            scanButton_->setText(tr("Discover cameras"));
        } else {
            deviceTable_->setRowCount(0);
            updateDeviceSummary();
            scanButton_->setText(tr("Cancel discovery"));
            subnetScanButton_->setEnabled(false);
            if (operationProgress_) {
                operationProgress_->setRange(0, 0);
                operationProgress_->setFormat(tr("Discovering cameras…"));
                operationProgress_->show();
            }
            discovery_->start(interfaceSelector_->currentData().toString());
        }
    });
    connect(subnetScanButton_, &QPushButton::clicked, this, [this] {
        if (discovery_->isRunning()) {
            updateStatus(tr("Wait for discovery to finish or cancel it before starting a subnet scan."));
            return;
        }
        if (subnetScanner_->isRunning()) {
            subnetScanner_->cancel();
            subnetScanButton_->setText(tr("Scan selected subnet"));
            return;
        }
        const QString selectedName = interfaceSelector_->currentData().toString();
        if (selectedName.isEmpty()) {
            QMessageBox::information(this, tr("Select a network interface"),
                tr("Choose one specific IPv4 interface to scan its subnet. Discover cameras can run across all active interfaces."));
            return;
        }
        const QNetworkInterface interface = QNetworkInterface::interfaceFromName(selectedName);
        for (const auto &entry : interface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol ||
                entry.ip().isLoopback() || entry.prefixLength() < 1)
                continue;

            const quint32 ip = entry.ip().toIPv4Address();
            const quint32 mask = 0xffffffffU << (32 - entry.prefixLength());
            const quint32 network = ip & mask;
            const quint32 broadcast = network | ~mask;
            const quint32 hostCount = broadcast - network - 1U;
            quint32 scanLimit = 256;
            if (hostCount > 256 && hostCount <= 4096) {
                QMessageBox scope(this);
                scope.setIcon(QMessageBox::Information);
                scope.setWindowTitle(tr("Choose subnet scan scope"));
                scope.setText(tr("This subnet contains %1 usable host addresses.").arg(hostCount));
                scope.setInformativeText(tr("A full scan checks common TCP services on every host and may take longer. You can also scan a 256-address window around this computer."));
                auto *fullScan = scope.addButton(tr("Scan entire subnet"), QMessageBox::AcceptRole);
                auto *boundedScan = scope.addButton(tr("Scan 256-address window"), QMessageBox::ActionRole);
                scope.addButton(QMessageBox::Cancel);
                scope.exec();
                if (scope.clickedButton() == fullScan)
                    scanLimit = 0;
                else if (scope.clickedButton() != boundedScan)
                    return;
            } else if (hostCount > 4096) {
                QMessageBox::warning(this, tr("Large subnet"),
                    tr("This subnet contains %1 usable hosts. To avoid an extremely large scan, this build limits one full scan to 4096 hosts. A 256-address window will be scanned around this computer.")
                        .arg(hostCount));
            }

            if (subnetScanner_->start(interface, entry, scanLimit)) {
                deviceTable_->setRowCount(0);
                updateDeviceSummary();
                scanButton_->setEnabled(false);
                subnetScanButton_->setText(tr("Cancel subnet scan"));
                if (operationProgress_) {
                    operationProgress_->setRange(0, 0);
                    operationProgress_->setFormat(tr("Scanning subnet…"));
                    operationProgress_->show();
                }
                return;
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
    connect(deviceTypeFilter_, &QComboBox::currentIndexChanged, this, &MainWindow::applyFilters);
    connect(vendorFilter, &QComboBox::currentTextChanged, this, &MainWindow::applyFilters);
    connect(statusFilter, &QComboBox::currentTextChanged, this, &MainWindow::applyFilters);
    connect(renameButton, &QPushButton::clicked, this, &MainWindow::editSelectedLabel);
    connect(addDeviceButton_, &QPushButton::clicked, this, &MainWindow::addManualDevice);
    connect(exportButton, &QPushButton::clicked, this, [this] {
        QTableWidget *exportTable = deviceTable_ && deviceTable_->rowCount() > 0
            ? deviceTable_ : savedInventoryTable_;
        if (!exportTable || exportTable->rowCount() == 0) {
            updateStatus(tr("There are no live or saved devices to export."));
            return;
        }
        const int firstColumn = exportTable == deviceTable_ ? 1 : 0;
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
        for (int column = firstColumn; column < exportTable->columnCount(); ++column)
            headers << csvField(exportTable->horizontalHeaderItem(column)->text());
        stream << headers.join(QLatin1Char(',')) << Qt::endl;

        int exported = 0;
        for (int row = 0; row < exportTable->rowCount(); ++row) {
            QStringList fields;
            for (int column = firstColumn; column < exportTable->columnCount(); ++column) {
                const auto *item = exportTable->item(row, column);
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
        const int row = selectedDeviceRow();
        const bool hasSelection = row >= 0;
        const auto *ipItem = hasSelection ? deviceTable_->item(row, 1) : nullptr;
        const QString sources = hasSelection && deviceTable_->item(row, 8)
            ? deviceTable_->item(row, 8)->text() : QString();
        const bool hasOnvifService = hasSelection && ipItem &&
            sources.contains(QStringLiteral("ONVIF"), Qt::CaseInsensitive) &&
            !ipItem->data(Qt::UserRole + 3).toString().isEmpty();
        const bool hasVerifiedIpv4Interface = hasOnvifService &&
            !ipItem->data(Qt::UserRole + 7).toString().isEmpty() &&
            ipItem->data(Qt::UserRole + 16).toBool();
        openWebButton_->setEnabled(hasSelection);
        refreshDeviceButton_->setEnabled(hasOnvifService);
        if (auto *button = findChild<QPushButton *>(QStringLiteral("renameLabelButton")))
            button->setEnabled(hasSelection);
        if (auto *button = findChild<QPushButton *>(QStringLiteral("factoryResetCameraButton"))) {
            button->setEnabled(false);
            button->setToolTip(tr("Factory reset is disabled until support is verified for this exact camera model and firmware. ONVIF discovery alone is not sufficient evidence."));
        }
        if (auto *button = findChild<QPushButton *>(QStringLiteral("applyStaticIpv4Button")))
            button->setEnabled(hasVerifiedIpv4Interface);
        if (auto *button = findChild<QPushButton *>(QStringLiteral("previewStaticIpv4Button")))
            button->setEnabled(hasSelection);
        updateDeviceDetailsProfile(row);
        updateSelectedDevicePanel();
    });
    connect(help, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Oxita Camera Device Manager — Help"),
            tr("<b>Discover cameras</b><br>Uses ONVIF WS-Discovery and the application's supported Oxita discovery broadcast. "
               "Discovery results are not a guarantee that every camera on the network was found.<br><br>"
               "<b>Scan selected subnet</b><br>Checks candidate addresses on the selected IPv4 interface. "
               "A TCP response is reachability evidence, not proof of ONVIF compatibility or camera identity.<br><br>"
               "<b>Device information</b><br>Select a device and use Refresh device info to query its reachable ONVIF device service. "
                "<b>Initialize / open camera setup</b><br>Opens the selected device web interface inside Oxita. Complete activation on the camera page itself. " 
                "The Done action starts fresh discovery; it does not by itself prove that activation succeeded. "
               "Reported fields depend on the camera and its firmware.<br><br>"
               "<b>Network configuration</b><br>Static IPv4 changes require a verified ONVIF interface token and current interface details. "
               "Bulk IPv4 mode assigns a different address to each checked camera and processes them sequentially. "
               "The camera may disconnect after an address change; this operation currently has no automatic rollback.<br><br>"
               "<b>Maintenance</b><br>Factory reset sends the ONVIF SetSystemFactoryDefault request. Soft/Hard modes are destructive and may erase settings or change the camera IP. "
               "The camera must implement and authorize this ONVIF operation; unsupported vendor reset paths are not sent.<br><br>"
               "<b>Supported actions</b><br>Camera information, static IPv4 and factory reset depend on the camera's ONVIF support, firmware and account permissions."));
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
        path->setStyleSheet(QStringLiteral("background:#1D1D1D;color:#F4F4F4;border:1px solid #626262;border-radius:7px;padding:10px;"));
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
    connect(deviceTable_, &QTableWidget::currentCellChanged, this,
            [this](int, int, int, int) { updateDeviceDetailsProfile(selectedDeviceRow()); });
    updateDeviceSummary();
}

QWidget *MainWindow::makeInfoTab(QWidget *parent)
{
    auto *page = new QScrollArea(parent);
    page->setObjectName(QStringLiteral("deviceDetailsScrollArea"));
    page->setWidgetResizable(true);
    page->setFrameShape(QFrame::NoFrame);
    page->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto *content = new QWidget(page);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *heading = new QLabel(tr("Device information"), content);
    heading->setObjectName(QStringLiteral("sectionTitle"));
    auto *description = mutedLabel(
        tr("Select a device above to review its identity, discovery evidence, available services, network configuration and ONVIF-reported service categories."),
        content);
    layout->addWidget(heading);
    layout->addWidget(description);

    auto *selectedPanel = panel(tr("Selected device"), content);
    auto *selectedLayout = new QVBoxLayout(selectedPanel);
    selectedLayout->setSpacing(7);
    selectedDeviceSummary_ = new QLabel(tr("No device selected"), selectedPanel);
    selectedDeviceSummary_->setObjectName(QStringLiteral("sectionTitle"));
    selectedDeviceSummary_->setWordWrap(true);
    selectedNetworkSummary_ = mutedLabel(
        tr("Select a device in the inventory to inspect its network and verification state."),
        selectedPanel);
    selectedNetworkSummary_->setObjectName(QStringLiteral("selectedDeviceDetails"));
    selectedNetworkSummary_->setWordWrap(true);
    selectedNetworkSummary_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    selectedLayout->addWidget(selectedDeviceSummary_);
    selectedLayout->addWidget(selectedNetworkSummary_);
    layout->addWidget(selectedPanel);

    auto *networkPanel = panel(tr("Local network"), content);
    auto *networkLayout = new QVBoxLayout(networkPanel);
    networkLayout->setSpacing(6);
    networkLayout->addWidget(mutedLabel(
        tr("The active interface used for discovery and subnet scanning."),
        networkPanel));
    localNetworkSummary_ = new QLabel(
        tr("Select an active IPv4 interface to inspect its address and link details."),
        networkPanel);
    localNetworkSummary_->setWordWrap(true);
    localNetworkSummary_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    localNetworkSummary_->setObjectName(QStringLiteral("localNetworkDetails"));
    localNetworkSummary_->setAccessibleName(tr("Local network interface details"));
    networkLayout->addWidget(localNetworkSummary_);
    layout->addWidget(networkPanel);

    auto *identityPanel = new QFrame(content);
    identityPanel->setObjectName(QStringLiteral("metricCard"));
    auto *identityLayout = new QVBoxLayout(identityPanel);
    identityLayout->setContentsMargins(14, 12, 14, 12);
    identityLayout->setSpacing(8);
    auto *identityTitle = new QLabel(tr("DEVICE PROFILE"), identityPanel);
    identityTitle->setObjectName(QStringLiteral("eyebrow"));
    deviceDetailsLabel_ = new QLabel(
        tr("No device selected. Select a device in the inventory above to inspect its reported information."),
        identityPanel);
    deviceDetailsLabel_->setWordWrap(true);
    deviceDetailsLabel_->setTextFormat(Qt::RichText);
    deviceDetailsLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    deviceDetailsLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    deviceDetailsLabel_->setMinimumHeight(150);
    deviceDetailsLabel_->setObjectName(QStringLiteral("deviceDetails"));
    deviceDetailsLabel_->setAccessibleName(tr("Selected camera full device profile"));
    identityLayout->addWidget(identityTitle);
    identityLayout->addWidget(deviceDetailsLabel_);
    layout->addWidget(identityPanel);

    auto *note = mutedLabel(
        tr("Information is device-reported and may be incomplete. An empty field means the camera did not provide that value during the latest query; it does not necessarily indicate a fault."),
        content);
    layout->addWidget(note);
    layout->addStretch();
    page->setWidget(content);
    return page;
}
QWidget *MainWindow::makeMaintenanceTab(QWidget *parent)
{
    auto *page = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(9);
    auto *content = page;

    auto *heading = new QLabel(tr("Device configuration"), content);
    heading->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(heading);
    layout->addWidget(mutedLabel(
        tr("Choose a focused workspace below. Controls appear only for operations implemented and verified by the selected device adapter."),
        content));

    auto *configurationTabs = new QTabWidget(page);
    configurationTabs->setObjectName(QStringLiteral("configurationTabs"));
    configurationTabs->setDocumentMode(true);
    configurationTabs->setUsesScrollButtons(true);
    const auto createSectionPage = [configurationTabs](QWidget **sectionContent,
                                                        QVBoxLayout **sectionLayout) {
        auto *scroll = new QScrollArea(configurationTabs);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto *inner = new QWidget(scroll);
        auto *innerLayout = new QVBoxLayout(inner);
        innerLayout->setContentsMargins(10, 10, 10, 10);
        innerLayout->setSpacing(10);
        scroll->setWidget(inner);
        *sectionContent = inner;
        *sectionLayout = innerLayout;
        return scroll;
    };
    QWidget *deviceContent = nullptr;
    QWidget *networkContent = nullptr;
    QWidget *storageContent = nullptr;
    QVBoxLayout *devicePageLayout = nullptr;
    QVBoxLayout *networkPageLayout = nullptr;
    QVBoxLayout *storagePageLayout = nullptr;
    configurationTabs->addTab(createSectionPage(&deviceContent, &devicePageLayout), tr("Device"));
    configurationTabs->addTab(createSectionPage(&networkContent, &networkPageLayout), tr("Network"));
    configurationTabs->addTab(createSectionPage(&storageContent, &storagePageLayout), tr("Storage"));
    layout->addWidget(configurationTabs, 1);

    auto *devicePanel = panel(tr("Device access & diagnostics"), deviceContent);
    auto *deviceLayout = new QVBoxLayout(devicePanel);
    deviceLayout->setSpacing(8);
    auto *deviceDescription = mutedLabel(
        tr("Initialize opens the selected camera's own setup page inside Oxita. The camera controls its activation process; Oxita verifies the device again after setup."),
        devicePanel);
    deviceLayout->addWidget(deviceDescription);
    auto *deviceActions = new QGridLayout();
    deviceActions->setHorizontalSpacing(8);
    deviceActions->setVerticalSpacing(8);
    refreshDeviceButton_ = actionButton(tr("Refresh device info"), devicePanel);
    refreshDeviceButton_->setEnabled(false);
    openWebButton_ = actionButton(tr("Initialize / open camera setup"), devicePanel, true);
    openWebButton_->setObjectName(QStringLiteral("initializeCameraButton"));
    openWebButton_->setToolTip(tr("Open the selected camera web interface inside Oxita. Complete its own setup, then rediscover to verify."));
    openWebButton_->setEnabled(false);
    deviceActions->addWidget(refreshDeviceButton_, 0, 0);
    deviceActions->addWidget(openWebButton_, 0, 1);
    deviceLayout->addLayout(deviceActions);
    devicePageLayout->addWidget(devicePanel);

    auto *maintenancePanel = panel(tr("Camera maintenance"), deviceContent);
    auto *maintenanceLayout = new QVBoxLayout(maintenancePanel);
    maintenanceLayout->setSpacing(8);
    maintenanceLayout->addWidget(mutedLabel(
        tr("Factory reset is a destructive camera-side operation. It is available only when the selected device exposes a verified ONVIF device service and accepts the operation."),
        maintenancePanel));
    auto *factoryResetButton = actionButton(tr("Factory reset camera"), maintenancePanel);
    factoryResetButton->setObjectName(QStringLiteral("factoryResetCameraButton"));
    factoryResetButton->setEnabled(false);
    factoryResetButton->setAccessibleName(tr("Factory reset selected camera"));
    factoryResetButton->setToolTip(tr("Disabled until support is verified for this exact camera model and firmware. ONVIF discovery alone does not prove factory-reset support."));
    factoryResetButton->setEnabled(false);
    maintenanceLayout->addWidget(factoryResetButton);
    devicePageLayout->addWidget(maintenancePanel);
    connect(factoryResetButton, &QPushButton::clicked, this, [this, factoryResetButton] {
        const int row = deviceTable_ ? selectedDeviceRow() : -1;
        if (row < 0) {
            QMessageBox::information(this, tr("Factory reset"), tr("Select one camera first."));
            return;
        }
        const auto cell = [this, row](int column) {
            const auto *item = deviceTable_->item(row, column);
            return item ? item->text().trimmed() : QString();
        };
        const auto *ipItem = deviceTable_->item(row, 1);
        DeviceRecord device;
        device.ipAddress = cell(1);
        device.macAddress = cell(5);
        device.manufacturer = cell(3);
        device.model = cell(4);
        device.onvifEndpoint = ipItem ? ipItem->data(Qt::UserRole + 2).toString() : QString();
        device.deviceServiceUrl = ipItem ? ipItem->data(Qt::UserRole + 3).toString() : QString();
        device.discoverySources = cell(8).split(QStringLiteral(", "), Qt::SkipEmptyParts);
        if (device.deviceServiceUrl.isEmpty() || device.ipAddress.isEmpty() ||
            !ipItem || !ipItem->data(Qt::UserRole + 20).toBool()) {
            QMessageBox::warning(this, tr("Factory reset unavailable"),
                tr("Refresh this camera first. A verified ONVIF device-service URL is required."));
            return;
        }

        QDialog dialog(this);
        dialog.setWindowTitle(tr("Confirm camera factory reset"));
        dialog.setMinimumWidth(470);
        auto *form = new QFormLayout(&dialog);
        auto *target = new QLabel(
            tr("%1 · %2\n%3").arg(device.manufacturer.isEmpty() ? tr("Unknown manufacturer") : device.manufacturer,
                                   device.model.isEmpty() ? tr("Unknown model") : device.model,
                                   device.ipAddress), &dialog);
        target->setWordWrap(true);
        auto *mode = new QComboBox(&dialog);
        mode->setObjectName(QStringLiteral("factoryResetMode"));
        mode->addItem(tr("Soft — restore user configuration defaults"), QStringLiteral("Soft"));
        mode->addItem(tr("Hard — restore factory defaults"), QStringLiteral("Hard"));
        auto *warning = mutedLabel(
            tr("This request is sent to the camera, not just removed from this inventory. The camera may reboot, erase users/settings, disable network services or return to a different IP address. The result depends on the camera firmware and account permissions. Make sure you have physical/network recovery access."),
            &dialog);
        warning->setStyleSheet(QStringLiteral("color:#FFD8DD;background:#3A2229;border:1px solid #75404A;border-radius:7px;padding:10px;"));
        form->addRow(tr("Target camera"), target);
        form->addRow(tr("Reset mode"), mode);
        form->addRow(warning);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::Cancel, &dialog);
        buttons->button(QDialogButtonBox::Yes)->setText(tr("Send factory reset"));
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog] {
            const auto answer = QMessageBox::warning(&dialog, QObject::tr("Final confirmation"),
                QObject::tr("This can erase the camera configuration and disconnect it from the network. Continue?"),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (answer == QMessageBox::Yes) dialog.accept();
        });
        if (dialog.exec() != QDialog::Accepted) return;
        factoryResetButton->setEnabled(false);
        discovery_->factoryReset(device, mode->currentData().toString());
    });


    auto *networkPanel = panel(tr("IPv4 network configuration"), networkContent);
    auto *networkLayout = new QVBoxLayout(networkPanel);
    networkLayout->setSpacing(8);
    networkLayout->addWidget(mutedLabel(
        tr("Preview validates proposed values locally and does not contact the camera. Apply sends an ONVIF static IPv4 request only when the camera exposes the required verified interface details."),
        networkPanel));
    auto *networkActions = new QGridLayout();
    networkActions->setHorizontalSpacing(8);
    auto *previewNetworkButton = actionButton(tr("Preview IP configuration"), networkPanel);
    previewNetworkButton->setObjectName(QStringLiteral("previewStaticIpv4Button"));
    previewNetworkButton->setEnabled(false);
    auto *applyNetworkButton = actionButton(tr("Apply static IPv4 (ONVIF)"), networkPanel, true);
    applyNetworkButton->setObjectName(QStringLiteral("applyStaticIpv4Button"));
    applyNetworkButton->setEnabled(false);
    networkActions->addWidget(previewNetworkButton, 0, 0);
    networkActions->addWidget(applyNetworkButton, 0, 1);
    networkLayout->addLayout(networkActions);
    networkPageLayout->addWidget(networkPanel);

    auto *onvifOpsPanel = panel(tr("Automatic network and maintenance operations"), networkContent);
    auto *onvifOpsLayout = new QVBoxLayout(onvifOpsPanel);
    onvifOpsLayout->addWidget(mutedLabel(
        tr("These actions use standard ONVIF operations when the camera firmware supports them. Every change is read back before the app reports success. A camera may disconnect after DHCP or reboot."),
        onvifOpsPanel));
    auto *onvifOpsGrid = new QGridLayout();
    auto *enableDhcpButton = actionButton(tr("Enable DHCP"), onvifOpsPanel);
    enableDhcpButton->setObjectName(QStringLiteral("enableDhcpButton"));
    auto *disableDhcpButton = actionButton(tr("Disable DHCP"), onvifOpsPanel);
    disableDhcpButton->setObjectName(QStringLiteral("disableDhcpButton"));
    disableDhcpButton->setEnabled(false);
    disableDhcpButton->setToolTip(tr("Use Apply static IPv4 (ONVIF), which disables DHCP and applies a verified manual address together."));
    auto *configureDnsButton = actionButton(tr("Configure DNS"), onvifOpsPanel);
    configureDnsButton->setObjectName(QStringLiteral("configureDnsButton"));
    auto *rebootButton = actionButton(tr("Reboot camera"), onvifOpsPanel);
    rebootButton->setObjectName(QStringLiteral("rebootCameraButton"));
    onvifOpsGrid->addWidget(enableDhcpButton, 0, 0);
    onvifOpsGrid->addWidget(disableDhcpButton, 0, 1);
    onvifOpsGrid->addWidget(configureDnsButton, 1, 0);
    onvifOpsGrid->addWidget(rebootButton, 1, 1);
    onvifOpsLayout->addLayout(onvifOpsGrid);
    networkPageLayout->addWidget(onvifOpsPanel);

    const auto selectedOnvifDevice = [this]() {
        DeviceRecord device;
        const int row = selectedDeviceRow();
        if (row < 0 || !deviceTable_ || !deviceTable_->item(row, 1)) return device;
        const auto *ip = deviceTable_->item(row, 1);
        const auto cell = [this, row](int column) {
            const auto *item = deviceTable_->item(row, column);
            return item ? item->text().trimmed() : QString();
        };
        device.ipAddress = cell(1);
        device.manufacturer = cell(3);
        device.model = cell(4);
        device.macAddress = cell(5);
        device.deviceServiceUrl = ip->data(Qt::UserRole + 3).toString();
        device.onvifEndpoint = ip->data(Qt::UserRole + 2).toString();
        device.networkInterfaceToken = ip->data(Qt::UserRole + 7).toString();
        device.ipv4Address = ip->data(Qt::UserRole + 8).toString();
        device.ipv4SubnetMask = ip->data(Qt::UserRole + 9).toString();
        device.ipv4Gateway = ip->data(Qt::UserRole + 10).toString();
        device.ipv4DnsServer = ip->data(Qt::UserRole + 11).toString();
        device.discoverySources = cell(8).split(QStringLiteral(", "), Qt::SkipEmptyParts);
        return device;
    };
    const auto requireSelectedOnvif = [this, selectedOnvifDevice]() {
        const DeviceRecord device = selectedOnvifDevice();
        if (device.ipAddress.isEmpty() || device.deviceServiceUrl.isEmpty() ||
            device.networkInterfaceToken.isEmpty()) {
            QMessageBox::warning(this, tr("ONVIF operation unavailable"),
                tr("Select a camera and refresh its device information first. A valid ONVIF service URL and interface token are required."));
            return DeviceRecord{};
        }
        return device;
    };
    connect(enableDhcpButton, &QPushButton::clicked, this, [this, requireSelectedOnvif] {
        const DeviceRecord device = requireSelectedOnvif();
        if (device.ipAddress.isEmpty()) return;
        if (QMessageBox::question(this, tr("Enable DHCP"),
                tr("Enable DHCP on %1? Its IP address may change and the camera may disconnect. You may need to run discovery again.")
                    .arg(device.ipAddress), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
        discovery_->configureDhcp(device, true);
    });
    connect(configureDnsButton, &QPushButton::clicked, this, [this, requireSelectedOnvif] {
        const DeviceRecord device = requireSelectedOnvif();
        if (device.ipAddress.isEmpty()) return;
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Configure camera DNS"));
        dialog.setMinimumWidth(430);
        auto *form = new QFormLayout(&dialog);
        auto *mode = new QComboBox(&dialog);
        mode->addItem(tr("Obtain DNS automatically (DHCP)"), true);
        mode->addItem(tr("Use manual DNS servers"), false);
        auto *primary = new QLineEdit(&dialog);
        primary->setPlaceholderText(QStringLiteral("8.8.8.8"));
        auto *secondary = new QLineEdit(&dialog);
        secondary->setPlaceholderText(QStringLiteral("1.1.1.1 (optional)"));
        form->addRow(tr("DNS mode"), mode);
        form->addRow(tr("Primary DNS"), primary);
        form->addRow(tr("Secondary DNS"), secondary);
        form->addRow(mutedLabel(tr("Manual DNS is verified against the camera's primary reported IPv4 DNS server. Some models expose only DHCP-provided DNS."), &dialog));
        const auto updateDnsFields = [mode, primary, secondary] {
            const bool manual = !mode->currentData().toBool();
            primary->setEnabled(manual);
            secondary->setEnabled(manual);
        };
        connect(mode, &QComboBox::currentIndexChanged, &dialog, updateDnsFields);
        updateDnsFields();
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Cancel, &dialog);
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog] { dialog.accept(); });
        if (dialog.exec() != QDialog::Accepted) return;
        const bool fromDhcp = mode->currentData().toBool();
        QStringList servers;
        if (!fromDhcp) {
            servers << primary->text().trimmed();
            if (!secondary->text().trimmed().isEmpty()) servers << secondary->text().trimmed();
        }
        discovery_->configureDns(device, fromDhcp, servers);
    });
    connect(rebootButton, &QPushButton::clicked, this, [this, requireSelectedOnvif] {
        const DeviceRecord device = requireSelectedOnvif();
        if (device.ipAddress.isEmpty()) return;
        if (QMessageBox::warning(this, tr("Reboot camera"),
                tr("Reboot %1? Live streams and connections will be interrupted temporarily.")
                    .arg(device.ipAddress), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
        discovery_->rebootDevice(device);
    });

    auto *bulkPanel = panel(tr("Bulk IPv4 configuration"), networkContent);
    auto *bulkLayout = new QVBoxLayout(bulkPanel);
    bulkLayout->addWidget(mutedLabel(tr("Configure checked cameras with a different static IPv4 address per device. Each camera must first be refreshed so its ONVIF interface token and current subnet are verified. Devices are changed sequentially and verified individually."), bulkPanel));
    auto *bulkButton = actionButton(tr("Configure checked cameras"), bulkPanel, true);
    bulkButton->setObjectName(QStringLiteral("bulkIpv4ConfigureButton"));
    bulkLayout->addWidget(bulkButton);
    networkPageLayout->addWidget(bulkPanel);
    connect(bulkButton, &QPushButton::clicked, this, [this, bulkButton] {
        if (bulkIpv4Running_) return;
        QVector<int> rows;
        for (int row = 0; row < deviceTable_->rowCount(); ++row) {
            const auto *check = deviceTable_->item(row, 0);
            if (check && check->checkState() == Qt::Checked && !deviceTable_->isRowHidden(row))
                rows.append(row);
        }
        if (rows.isEmpty()) {
            for (const QModelIndex &index : deviceTable_->selectionModel()->selectedRows()) rows.append(index.row());
        }
        if (rows.size() < 2) {
            QMessageBox::information(this, tr("Bulk IPv4 configuration"), tr("Check at least two cameras in the first inventory column, or select at least two rows."));
            return;
        }
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Review bulk IPv4 assignments"));
        dialog.setMinimumSize(900, 420);
        auto *form = new QVBoxLayout(&dialog);
        form->addWidget(mutedLabel(tr("Enter a unique new IPv4 address for each camera. Existing network values are prefilled where available."), &dialog));
        auto *table = new QTableWidget(rows.size(), 4, &dialog);
        table->setObjectName(QStringLiteral("bulkIpv4Assignments"));
        table->setHorizontalHeaderLabels({tr("CURRENT IP"), tr("NEW IP"), tr("SUBNET MASK"), tr("GATEWAY")});
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->verticalHeader()->setVisible(false);
        QVector<BulkIpv4Target> targets;
        for (int i = 0; i < rows.size(); ++i) {
            const int row = rows.at(i);
            const auto *ipItem = deviceTable_->item(row, 1);
            const auto value = [this, row](int col) { const auto *item = deviceTable_->item(row, col); return item ? item->text().trimmed() : QString(); };
            DeviceRecord device;
            device.ipAddress = value(1); device.macAddress = value(5); device.manufacturer = value(3); device.model = value(4);
            device.deviceServiceUrl = ipItem ? ipItem->data(Qt::UserRole + 3).toString() : QString();
            device.onvifEndpoint = ipItem ? ipItem->data(Qt::UserRole + 2).toString() : QString();
            device.networkInterfaceToken = ipItem ? ipItem->data(Qt::UserRole + 7).toString() : QString();
            device.ipv4Address = ipItem ? ipItem->data(Qt::UserRole + 8).toString() : QString();
            device.ipv4SubnetMask = ipItem ? ipItem->data(Qt::UserRole + 9).toString() : QString();
            device.ipv4Gateway = ipItem ? ipItem->data(Qt::UserRole + 10).toString() : QString();
            device.discoverySources = value(8).split(QStringLiteral(", "), Qt::SkipEmptyParts);
            if (device.deviceServiceUrl.isEmpty() || device.networkInterfaceToken.isEmpty() || device.ipv4Address.isEmpty() || device.ipv4SubnetMask.isEmpty()) {
                QMessageBox::warning(this, tr("Bulk configuration unavailable"), tr("Camera %1 needs a successful Refresh device info before it can be configured.").arg(device.ipAddress));
                return;
            }
            targets.append({device, QString(), device.ipv4SubnetMask, device.ipv4Gateway});
            auto *current = new QTableWidgetItem(device.ipAddress); current->setFlags(current->flags() & ~Qt::ItemIsEditable); table->setItem(i, 0, current);
            table->setItem(i, 1, new QTableWidgetItem());
            table->setItem(i, 2, new QTableWidgetItem(device.ipv4SubnetMask));
            table->setItem(i, 3, new QTableWidgetItem(device.ipv4Gateway));
        }
        form->addWidget(table, 1);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Cancel, &dialog);
        form->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
            QSet<QString> addresses;
            for (int i = 0; i < table->rowCount(); ++i) {
                const QString address = table->item(i, 1)->text().trimmed();
                const QString mask = table->item(i, 2)->text().trimmed();
                const QString gateway = table->item(i, 3)->text().trimmed();
                QString error;
                if (!NetworkConfigValidator::validateStaticIpv4(address, mask, gateway, QString(), &error)) {
                    QMessageBox::warning(&dialog, tr("Invalid IPv4 assignment"), error); table->setCurrentCell(i, 1); return;
                }
                if (address == targets.at(i).device.ipAddress) {
                    QMessageBox::warning(&dialog, tr("No IP change"), tr("Enter a new address for camera %1.").arg(address));
                    table->setCurrentCell(i, 1);
                    return;
                }
                if (addresses.contains(address)) { QMessageBox::warning(&dialog, tr("Duplicate address"), tr("Each camera must have a unique new IPv4 address.")); table->setCurrentCell(i, 1); return; }
                addresses.insert(address);
                const QString reservation = reservedIpv4Reason(address, deviceTable_, rows.at(i));
                if (!reservation.isEmpty()) {
                    QMessageBox::warning(&dialog, tr("Address is already reserved"), reservation);
                    table->setCurrentCell(i, 1);
                    return;
                }
                targets[i].address = address; targets[i].subnetMask = mask; targets[i].gateway = gateway;
            }
            const auto answer = QMessageBox::warning(&dialog, tr("Confirm bulk configuration"), tr("Apply the reviewed IPv4 settings to %1 cameras sequentially? Each camera may disconnect when its address changes.").arg(targets.size()), QMessageBox::Apply | QMessageBox::Cancel, QMessageBox::Cancel);
            if (answer == QMessageBox::Apply) dialog.accept();
        });
        if (dialog.exec() != QDialog::Accepted) return;
        bulkIpv4Queue_ = targets; bulkIpv4Next_ = 0; bulkIpv4Succeeded_ = 0; bulkIpv4Failed_ = 0; bulkIpv4Running_ = true;
        bulkButton->setEnabled(false);
        for (auto *button : findChildren<QPushButton *>()) {
            if (button->text() == tr("Apply static IPv4 (ONVIF)") ||
                button->objectName() == QStringLiteral("factoryResetCameraButton"))
                button->setEnabled(false);
        }
        if (operationProgress_) {
            operationProgress_->setRange(0, bulkIpv4Queue_.size());
            operationProgress_->setValue(0);
            operationProgress_->setFormat(tr("Bulk IPv4 0 / %1").arg(bulkIpv4Queue_.size()));
            operationProgress_->show();
        }
        startNextBulkIpv4Configuration();
    });

    auto *firmwarePanel = panel(tr("Firmware information"), deviceContent);
    auto *firmwareLayout = new QVBoxLayout(firmwarePanel);
    firmwareStatusSummary_ = new QLabel(
        tr("Select a camera to inspect its reported firmware version."),
        firmwarePanel);
    firmwareStatusSummary_->setWordWrap(true);
    firmwareStatusSummary_->setObjectName(QStringLiteral("firmwareDetailsSummary"));
    firmwareLayout->addWidget(firmwareStatusSummary_);
    firmwareLayout->addWidget(mutedLabel(
        tr("This workspace displays firmware details reported by the camera. Firmware installation is not available in this release."),
        firmwarePanel));
    devicePageLayout->addWidget(firmwarePanel);

    auto *backupPanel = panel(tr("Inventory backup & recovery"), storageContent);
    auto *backupLayout = new QVBoxLayout(backupPanel);
    backupLayout->addWidget(mutedLabel(
        tr("Create a consistent SQLite snapshot or restore a previously validated backup. Restoring keeps a separate pre-restore recovery snapshot beside the active inventory."),
        backupPanel));
    auto *backupActions = new QHBoxLayout();
    auto *backupButton = actionButton(tr("Back up inventory"), backupPanel);
    backupButton->setObjectName(QStringLiteral("backupInventoryButton"));
    auto *restoreButton = actionButton(tr("Restore inventory"), backupPanel);
    restoreButton->setObjectName(QStringLiteral("restoreInventoryButton"));
    backupActions->addWidget(backupButton);
    backupActions->addWidget(restoreButton);
    backupActions->addStretch();
    backupLayout->addLayout(backupActions);
    storagePageLayout->addWidget(backupPanel);

    connect(backupButton, &QPushButton::clicked, this, [this] {
        if (!inventoryDb_.isOpen()) {
            QMessageBox::warning(this, tr("Inventory unavailable"),
                                 tr("The local inventory database is not open."));
            return;
        }
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Back up device inventory"), QStringLiteral("oxita-device-inventory.sqlite"),
            tr("SQLite database (*.sqlite *.db)"));
        if (path.isEmpty()) return;
        QString error;
        if (!InventoryBackup::create(inventoryDb_, path, &error)) {
            QMessageBox::warning(this, tr("Inventory backup failed"), error);
            return;
        }
        recordOperation(tr("Inventory backup"), path, tr("Completed"),
                        tr("A consistent SQLite backup was created and integrity-checked."));
        refreshOperationHistory();
        QMessageBox::information(this, tr("Backup created"),
                                 tr("The inventory backup was created and validated successfully."));
    });
    connect(restoreButton, &QPushButton::clicked, this, [this] {
        if (!inventoryDb_.isOpen()) {
            QMessageBox::warning(this, tr("Inventory unavailable"),
                                 tr("The local inventory database is not open."));
            return;
        }
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Restore device inventory"), QString(),
            tr("SQLite database (*.sqlite *.db)"));
        if (path.isEmpty()) return;
        const auto answer = QMessageBox::warning(
            this, tr("Confirm inventory restore"),
            tr("Replace the current device inventory with the selected backup? A separate recovery snapshot of the current database will be kept beside it."),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes) return;
        QString error;
        if (!InventoryBackup::restore(inventoryDb_, path, &error)) {
            QMessageBox::critical(this, tr("Inventory restore failed"), error);
            return;
        }
        if (deviceTable_) deviceTable_->setRowCount(0);
        loadInventory();
        refreshOperationHistory();
        updateDeviceSummary();
        updateSelectedDevicePanel();
        recordOperation(tr("Inventory restore"), path, tr("Completed"),
                        tr("Validated backup restored; pre-restore snapshot retained beside the database."));
        refreshOperationHistory();
        QMessageBox::information(this, tr("Inventory restored"),
                                 tr("The backup was restored successfully. The previous inventory is retained as a recovery snapshot."));
    });

    connect(findChild<QPushButton *>(QStringLiteral("deleteSavedInventoryButton")), &QPushButton::clicked, this, [this] {
        if (!inventoryDb_.isOpen() || !savedInventoryTable_) return;
        const auto rows = savedInventoryTable_->selectionModel()->selectedRows();
        if (rows.isEmpty()) { updateStatus(tr("Select saved devices to delete.")); return; }
        if (QMessageBox::question(this, tr("Delete saved devices"),
                tr("Permanently delete %1 selected saved record(s)? Live results are not affected.").arg(rows.size())) != QMessageBox::Yes) return;
        if (!inventoryDb_.transaction()) {
            updateStatus(tr("Could not start saved-inventory deletion: %1").arg(inventoryDb_.lastError().text()));
            return;
        }
        QSqlQuery remove(inventoryDb_);
        remove.prepare(QStringLiteral("DELETE FROM devices WHERE device_key=:key"));
        for (const QModelIndex &index : rows) {
            const auto *item = savedInventoryTable_->item(index.row(), 0);
            if (!item) continue;
            remove.bindValue(QStringLiteral(":key"), item->data(Qt::UserRole).toString());
            if (!remove.exec()) {
                const QString error = remove.lastError().text();
                inventoryDb_.rollback();
                updateStatus(tr("Delete failed; no selected records were removed: %1").arg(error));
                return;
            }
        }
        if (!inventoryDb_.commit()) {
            const QString error = inventoryDb_.lastError().text();
            inventoryDb_.rollback();
            updateStatus(tr("Could not commit saved-inventory deletion: %1").arg(error));
            return;
        }
        refreshSavedInventory();
        recordOperation(tr("Saved inventory deletion"), QString(), tr("Completed"), tr("%1 record(s) deleted").arg(rows.size()));
        refreshOperationHistory();
    });
    connect(findChild<QPushButton *>(QStringLiteral("clearSavedInventoryButton")), &QPushButton::clicked, this, [this] {
        if (!inventoryDb_.isOpen()) return;
        if (QMessageBox::question(this, tr("Clear saved inventory"),
                tr("Permanently delete every saved device record? This does not affect live scan results.")) != QMessageBox::Yes) return;
        QSqlQuery clear(inventoryDb_);
        if (!clear.exec(QStringLiteral("DELETE FROM devices"))) { updateStatus(tr("Clear failed: %1").arg(clear.lastError().text())); return; }
        refreshSavedInventory();
        recordOperation(tr("Saved inventory deletion"), QString(), tr("Completed"), tr("All saved device records deleted"));
        refreshOperationHistory();
    });
    connect(findChild<QPushButton *>(QStringLiteral("applyInventoryRetentionButton")), &QPushButton::clicked, this, [this] {
        if (!inventoryDb_.isOpen()) return;
        const auto *retention = findChild<QComboBox *>(QStringLiteral("savedInventoryRetention"));
        const int days = retention ? retention->currentData().toInt() : 0;
        if (days <= 0) { updateStatus(tr("Retention is Keep forever; no records were deleted.")); return; }
        if (QMessageBox::question(this, tr("Apply retention"),
                tr("Delete saved records not seen in the last %1 days?").arg(days)) != QMessageBox::Yes) return;
        QSqlQuery prune(inventoryDb_);
        prune.prepare(QStringLiteral("DELETE FROM devices WHERE julianday(COALESCE(NULLIF(last_seen,''),created_at)) < julianday('now', :age)"));
        prune.bindValue(QStringLiteral(":age"), QStringLiteral("-%1 days").arg(days));
        if (!prune.exec()) { updateStatus(tr("Retention failed: %1").arg(prune.lastError().text())); return; }
        const int removed = prune.numRowsAffected();
        refreshSavedInventory();
        recordOperation(tr("Saved inventory retention"), QString(), tr("Completed"), tr("%1 old record(s) deleted").arg(removed));
        refreshOperationHistory();
        updateStatus(tr("Retention applied: %1 record(s) deleted.").arg(removed));
    });

    auto *safetyNote = new QLabel(
        tr("Network changes can interrupt camera connectivity. Confirm the new address is reachable from this computer before applying. This build does not provide automatic rollback."),
        networkContent);
    safetyNote->setWordWrap(true);
    safetyNote->setStyleSheet(QStringLiteral("color:#F5D89A;background:#302A20;border:1px solid #665538;border-radius:7px;padding:9px;"));
    networkPageLayout->addWidget(safetyNote);
    devicePageLayout->addStretch();
    networkPageLayout->addStretch();
    storagePageLayout->addStretch();
    connect(applyNetworkButton, &QPushButton::clicked, this, [this, applyNetworkButton] {
        const int row = selectedDeviceRow();
        if (row < 0) {
            QMessageBox::information(this, tr("IP configuration"), tr("Select one camera first."));
            return;
        }
        const auto *ipItem = deviceTable_->item(row, 1);
        if (!ipItem) return;
        DeviceRecord device;
        // The device-service URL is bound to the discovered host address.
        // The ONVIF-reported interface address is kept separately in ipv4Address.
        device.ipAddress = ipItem->text().trimmed();
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
            const QString reservation = reservedIpv4Reason(address->text(), deviceTable_, row);
            if (!reservation.isEmpty()) {
                QMessageBox::warning(&dialog, tr("Address is already reserved"), reservation);
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
        const int row = selectedDeviceRow();
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
        const int row = selectedDeviceRow();
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
    auto *exportButton = actionButton(tr("Export diagnostics"), page, true);
    actions->addWidget(refresh);
    actions->addWidget(exportButton);
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
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportDiagnosticBundle);
    refreshOperationHistory();
    return page;
}

void MainWindow::recordOperation(const QString &operation, const QString &target,
                                 const QString &outcome, const QString &details)
{
    if (!inventoryDb_.isOpen()) return;
    const auto safe = [](QString value) {
        return DiagnosticLog::redactSensitiveData(value).left(2000);
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
        const QStringList lines = QString::fromUtf8(logFile.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
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

void MainWindow::startNextBulkIpv4Configuration()
{
    if (!bulkIpv4Running_) return;
    if (bulkIpv4Next_ >= bulkIpv4Queue_.size()) {
        bulkIpv4Running_ = false;
        if (auto *button = findChild<QPushButton *>(QStringLiteral("bulkIpv4ConfigureButton"))) button->setEnabled(true);
        const int selected = selectedDeviceRow();
        const auto *selectedIp = selected >= 0 ? deviceTable_->item(selected, 1) : nullptr;
        const bool selectedOnvif = selected >= 0 && selectedIp &&
            deviceTable_->item(selected, 8) &&
            deviceTable_->item(selected, 8)->text().contains(QStringLiteral("ONVIF"), Qt::CaseInsensitive) &&
            !selectedIp->data(Qt::UserRole + 3).toString().isEmpty();
        const bool selectedIpv4Ready = selectedOnvif &&
            !selectedIp->data(Qt::UserRole + 7).toString().isEmpty() &&
            selectedIp->data(Qt::UserRole + 16).toBool();
        if (auto *button = findChild<QPushButton *>(QStringLiteral("applyStaticIpv4Button")))
            button->setEnabled(selectedIpv4Ready);
        if (auto *button = findChild<QPushButton *>(QStringLiteral("factoryResetCameraButton")))
            button->setEnabled(false);
        if (auto *button = findChild<QPushButton *>(QStringLiteral("previewStaticIpv4Button")))
            button->setEnabled(selected >= 0);
        if (operationProgress_) operationProgress_->hide();
        const QString summary = tr("Bulk IPv4 configuration finished: %1 verified, %2 failed, %3 total.")
            .arg(bulkIpv4Succeeded_).arg(bulkIpv4Failed_).arg(bulkIpv4Queue_.size());
        recordOperation(tr("Bulk static IPv4"), QString(),
                        bulkIpv4Failed_ == 0 ? tr("Completed") : tr("Completed with failures"), summary);
        refreshOperationHistory();
        updateStatus(summary);
        QMessageBox::information(this, tr("Bulk configuration results"), summary + tr("\nReview Operation history for each camera's result. Rediscover cameras after network changes."));
        bulkIpv4Queue_.clear();
        return;
    }
    const BulkIpv4Target target = bulkIpv4Queue_.at(bulkIpv4Next_);
    updateStatus(tr("Configuring camera %1 of %2 at %3…")
        .arg(bulkIpv4Next_ + 1).arg(bulkIpv4Queue_.size()).arg(target.device.ipAddress));
    discovery_->applyStaticIpv4(target.device, target.address, target.subnetMask, target.gateway);
}
int MainWindow::selectedDeviceRow() const
{
    if (!deviceTable_ || !deviceTable_->selectionModel()) return -1;
    const QModelIndexList selected = deviceTable_->selectionModel()->selectedRows();
    if (selected.isEmpty()) return -1;
    const int current = deviceTable_->currentRow();
    for (const QModelIndex &index : selected)
        if (index.row() == current) return current;
    return selected.first().row();
}

void MainWindow::updateDeviceDetailsProfile(int row)
{
    if (!deviceDetailsLabel_ || !deviceTable_) return;
    if (row < 0 || row >= deviceTable_->rowCount()) {
        deviceDetailsLabel_->setText(
            tr("No device selected. Select a camera in the inventory above to inspect its reported information."));
        return;
    }

    QStringList details;
    for (int column = 1; column < deviceTable_->columnCount(); ++column) {
        const auto *item = deviceTable_->item(row, column);
        const QString value = item && !item->text().trimmed().isEmpty()
            ? item->text() : tr("Not reported");
        details << deviceTable_->horizontalHeaderItem(column)->text() +
                   QStringLiteral(": ") + value;
    }
    const auto *ipItem = deviceTable_->item(row, 1);
    if (ipItem) {
        const auto addDetail = [&details, ipItem](int role, const QString &title) {
            const QString value = ipItem->data(role).toString();
            details << title + QStringLiteral(": ") +
                (value.isEmpty() ? QObject::tr("Not reported") : value);
        };
        addDetail(Qt::UserRole + 1, tr("Network interface"));
        addDetail(Qt::UserRole + 2, tr("ONVIF endpoint"));
        addDetail(Qt::UserRole + 3, tr("Device service URL"));
        addDetail(Qt::UserRole + 4, tr("Network interfaces"));
        addDetail(Qt::UserRole + 5, tr("Reported ONVIF categories and services"));
        addDetail(Qt::UserRole + 6, tr("Open TCP ports (candidate evidence)"));
        addDetail(Qt::UserRole + 7, tr("ONVIF interface token"));
        addDetail(Qt::UserRole + 8, tr("Camera IPv4 address"));
        addDetail(Qt::UserRole + 9, tr("Camera subnet mask"));
        addDetail(Qt::UserRole + 10, tr("Camera gateway"));
        addDetail(Qt::UserRole + 11, tr("Camera DNS server"));
        addDetail(Qt::UserRole + 12, tr("Camera DHCP enabled"));
        addDetail(Qt::UserRole + 14, tr("Last reachability check"));
        addDetail(Qt::UserRole + 15, tr("Last successful response"));
        addDetail(Qt::UserRole + 16, tr("IPv4 configuration verified"));
        addDetail(Qt::UserRole + 19, tr("Last seen (UTC)"));
    }

    if (ipItem) {
        DeviceRecord compatibilityDevice;
        compatibilityDevice.ipAddress = ipItem->text();
        compatibilityDevice.deviceServiceUrl = ipItem->data(Qt::UserRole + 3).toString();
        compatibilityDevice.capabilities = ipItem->data(Qt::UserRole + 5).toString()
            .split(QStringLiteral(", "), Qt::SkipEmptyParts);
        if (const auto *sourcesItem = deviceTable_->item(row, 8))
            compatibilityDevice.discoverySources = sourcesItem->text().split(QStringLiteral(", "), Qt::SkipEmptyParts);
        details << tr("OPERATION COMPATIBILITY (evidence-based)");
        for (const auto &entry : OperationCompatibility::evaluate(compatibilityDevice)) {
            details << tr("%1: %2. %3")
                .arg(entry.operation, OperationCompatibility::statusLabel(entry.status), entry.evidence);
        }
    }

    QString html = QStringLiteral("<html><body style='margin:2px;font-family:Segoe UI, sans-serif;color:#E7EDF4;'>"
        "<table width='100%' cellspacing='0' cellpadding='8' style='table-layout:fixed;'>");
    for (const QString &detail : details) {
        const int separator = detail.indexOf(QStringLiteral(": "));
        const QString key = (separator >= 0 ? detail.left(separator) : detail).toHtmlEscaped();
        const QString value = (separator >= 0 ? detail.mid(separator + 2) : QString()).toHtmlEscaped();
        html += QStringLiteral("<tr><td width='32%' valign='top' style='color:#A0ADBB;border-bottom:1px solid #2B3948;font-size:12px;'>%1</td>"
            "<td width='68%' valign='top' style='color:#E7EDF4;border-bottom:1px solid #2B3948;font-size:12px;font-weight:600;white-space:normal;overflow-wrap:anywhere;word-break:break-all;'>%2</td></tr>")
            .arg(key, value);
    }
    html += QStringLiteral("</table></body></html>");
    deviceDetailsLabel_->setText(html);
}

void MainWindow::updateSelectedDevicePanel()
{
    if (!selectedDeviceSummary_ || !selectedNetworkSummary_ || !deviceTable_) return;
    const int row = selectedDeviceRow();
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
    const QString deviceType = ipItem
        ? ipItem->data(Qt::UserRole + 17).toString() : QStringLiteral("unknown");
    const QString typeConfidence = ipItem
        ? ipItem->data(Qt::UserRole + 18).toString() : QStringLiteral("Unverified");
    DeviceRecord selectedDevice;
    selectedDevice.manufacturer = cell(3);
    selectedDevice.model = cell(4);
    selectedDevice.discoverySources = cell(8).split(QStringLiteral(", "), Qt::SkipEmptyParts);
    selectedDevice.openPorts = ipItem
        ? ipItem->data(Qt::UserRole + 6).toString().split(QStringLiteral(", "), Qt::SkipEmptyParts)
        : QStringList{};
    selectedDevice.isCandidate = deviceType == QStringLiteral("unknown");
    const DeviceClassifier::Result classification = DeviceClassifier::classify(selectedDevice);
    selectedDeviceSummary_->setText(
        tr("%1  ·  %2 (%3)\n%4")
            .arg(cell(2).isEmpty() ? cell(1) : cell(2),
                 classification.displayLabel(), typeConfidence, cell(1)));
    QStringList details;
    details << tr("Device type: %1 (%2)").arg(classification.displayLabel(), typeConfidence);
    details << tr("Classification evidence: %1").arg(classification.reason);
    details << tr("Manufacturer: %1").arg(cell(3).isEmpty() ? tr("Not reported") : cell(3));
    details << tr("Model: %1").arg(cell(4).isEmpty() ? tr("Not reported") : cell(4));
    details << tr("Reachability: %1").arg(cell(11).isEmpty() ? tr("Not checked") : cell(11));
    if (ipItem) {
        const QString lastSeen = ipItem->data(Qt::UserRole + 19).toString();
        details << tr("Last seen (UTC): %1").arg(lastSeen.isEmpty() ? tr("Never") : lastSeen);
    }
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
    if (interfaceName.isEmpty()) {
        QStringList details{tr("Discovery scope: all active IPv4 interfaces")};
        int interfaceCount = 0;
        for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
            const auto flags = interface.flags();
            if (!flags.testFlag(QNetworkInterface::IsUp) ||
                !flags.testFlag(QNetworkInterface::IsRunning) ||
                flags.testFlag(QNetworkInterface::IsLoopBack)) continue;
            QStringList addresses;
            for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.ip().isLoopback())
                    addresses << QStringLiteral("%1/%2").arg(entry.ip().toString()).arg(entry.prefixLength());
            }
            if (addresses.isEmpty()) continue;
            ++interfaceCount;
            details << tr("%1 (%2): %3")
                .arg(interface.humanReadableName(), interface.name(), addresses.join(QStringLiteral(", ")));
        }
        details << tr("Subnet scanning requires selecting one specific interface.");
        localNetworkSummary_->setText(details.join(QLatin1Char('\n')));
        if (interfaceCount == 0)
            localNetworkSummary_->setText(tr("No active IPv4 interfaces are available."));
        return;
    }
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
    details << tr("MTU: %1").arg(tr("Not reported"));
    bool foundIpv4 = false;
    QStringList ipv6Addresses;
    for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
        if (entry.ip().isLoopback()) continue;
        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
            foundIpv4 = true;
            details << tr("IPv4 address: %1").arg(entry.ip().toString());
            details << tr("Subnet prefix: /%1").arg(entry.prefixLength());
            if (!entry.broadcast().isNull())
                details << tr("Broadcast: %1").arg(entry.broadcast().toString());
        } else if (entry.ip().protocol() == QAbstractSocket::IPv6Protocol) {
            ipv6Addresses << QStringLiteral("%1/%2").arg(entry.ip().toString()).arg(entry.prefixLength());
        }
    }
    if (!ipv6Addresses.isEmpty())
        details << tr("IPv6 addresses: %1").arg(ipv6Addresses.join(QStringLiteral(", ")));
    const auto interfaceFlags = interface.flags();
    const QString linkState = !interfaceFlags.testFlag(QNetworkInterface::IsUp)
        ? tr("Disabled")
        : (interfaceFlags.testFlag(QNetworkInterface::IsRunning) ? tr("Up") : tr("Link down"));
    details << tr("Link state: %1").arg(linkState);
    if (!foundIpv4) {
        details << tr("IPv4 address: Not available");
        if (!interfaceFlags.testFlag(QNetworkInterface::IsRunning)) {
            details << tr("Check the Ethernet cable, camera power/PoE and selected PC adapter.");
        } else {
            details << tr("Wait for DHCP or automatic link-local addressing. If the camera uses a factory static IP, consult its exact model manual and set a temporary PC adapter address in the same subnet yourself. Oxita will not change PC network settings silently.");
        }
    }
    localNetworkSummary_->setText(details.join(QLatin1Char('\n')));
}

void MainWindow::updateStatus(const QString &message)
{
    if (statusLabel_) statusLabel_->setText(message);
}
void MainWindow::updateDeviceSummary()
{
    const int count = deviceTable_ ? deviceTable_->rowCount() : 0;
    if (deviceCountValue_)
        deviceCountValue_->setText(tr("%1 devices").arg(count));
    if (summaryLabel_) {
        int checked = 0;
        for (int row = 0; row < count; ++row) {
            const auto *item = deviceTable_->item(row, 0);
            if (item && item->checkState() == Qt::Checked) ++checked;
        }
        summaryLabel_->setText(tr("%1 devices · %2 selected").arg(count).arg(checked));
    }
    if (inventoryEmptyState_ && count == 0) {
        inventoryEmptyState_->setText(
            tr("No live scan results yet.\nChoose a network interface above and select Discover cameras to find devices on your LAN."));
        inventoryEmptyState_->show();
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
    if (!loadingInventory_) {
        const bool observedOnNetwork = device.isCandidate ||
            device.discoverySources.contains(QStringLiteral("ONVIF WS-Discovery")) ||
            device.discoverySources.contains(QStringLiteral("ONVIF GetDeviceInformation")) ||
            device.discoverySources.contains(QStringLiteral("Project UDP 9988")) ||
            device.discoverySources.contains(QStringLiteral("SSDP/UPnP"));
        if (observedOnNetwork) {
            device.lastSeenAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
            device.reachabilityStatus = QStringLiteral("Reachable");
            device.reachabilityCheckedAt = device.lastSeenAt;
            device.lastSuccessfulResponseAt = device.lastSeenAt;
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
    keepExistingWhenEmpty(Qt::UserRole + 14, device.reachabilityCheckedAt);
    keepExistingWhenEmpty(Qt::UserRole + 15, device.lastSuccessfulResponseAt);
    ipItem->setData(Qt::UserRole + 19, device.lastSeenAt);
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
    ipItem->setData(Qt::UserRole + 16, networkInterfacesUpdated
        ? device.ipv4ConfigurationKnown
        : (device.ipv4ConfigurationKnown ||
           (previousIpItem && previousIpItem->data(Qt::UserRole + 16).toBool())));
    const DeviceClassifier::Result classification = DeviceClassifier::classify(device);
    ipItem->setData(Qt::UserRole + 17, classification.key());
    ipItem->setData(Qt::UserRole + 18, classification.confidenceLabel());
    ipItem->setToolTip(tr("Device type: %1 (%2)\n%3")
        .arg(classification.displayLabel(), classification.confidenceLabel(), classification.reason));
    const QStringList values = {
        device.ipAddress, !device.userLabel.isEmpty() ? device.userLabel : (oldLabel.isEmpty() && device.isCandidate ? QStringLiteral("Unverified candidate") : oldLabel),
        device.manufacturer, device.model, device.macAddress,
        device.firmwareVersion, device.activationState,
        device.discoverySources.join(QStringLiteral(", ")), device.serialNumber, device.hardwareId,
        (device.reachabilityStatus == QStringLiteral("Not checked") && deviceTable_->item(row, 11)
            ? deviceTable_->item(row, 11)->text() : device.reachabilityStatus),
        classification.displayLabel()
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
    if (row == selectedDeviceRow()) {
        updateDeviceDetailsProfile(row);
        updateSelectedDevicePanel();
    }
    if (!loadingInventory_) persistDevice(device);
}

void MainWindow::initializeInventory()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(directory)) {
        updateStatus(tr("Local inventory unavailable; devices remain session-only."));
        return;
    }

    inventoryDb_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                             QStringLiteral("camera_inventory"));
    inventoryDb_.setDatabaseName(directory + QStringLiteral("/inventory.sqlite"));
    if (!inventoryDb_.open()) {
        updateStatus(tr("Could not open local inventory: %1").arg(inventoryDb_.lastError().text()));
        return;
    }

    bool integrityOk = false;
    {
        QSqlQuery integrityQuery(inventoryDb_);
        integrityOk = integrityQuery.exec(QStringLiteral("PRAGMA quick_check")) &&
                      integrityQuery.next() &&
                      integrityQuery.value(0).toString().compare(
                          QStringLiteral("ok"), Qt::CaseInsensitive) == 0;
    }
    if (!integrityOk) {
        updateStatus(tr("Inventory database integrity check failed. The database was left untouched; close the app and preserve a copy before attempting recovery."));
        inventoryDb_.close();
        return;
    }

    int schemaVersion = 0;
    QString versionError;
    {
        QSqlQuery versionQuery(inventoryDb_);
        if (!versionQuery.exec(QStringLiteral("PRAGMA user_version")) || !versionQuery.next())
            versionError = versionQuery.lastError().text();
        else
            schemaVersion = versionQuery.value(0).toInt();
    }
    if (!versionError.isEmpty()) {
        updateStatus(tr("Could not read inventory schema version: %1").arg(versionError));
        inventoryDb_.close();
        return;
    }
    if (schemaVersion > 2) {
        updateStatus(tr("Inventory schema version %1 is newer than this application supports. The database was not modified.")
                         .arg(schemaVersion));
        inventoryDb_.close();
        return;
    }

    if (!inventoryDb_.transaction()) {
        updateStatus(tr("Could not start inventory schema migration: %1").arg(inventoryDb_.lastError().text()));
        inventoryDb_.close();
        return;
    }

    const auto executeMigrationSql = [this](const QString &sql, QString *error) {
        QSqlQuery query(inventoryDb_);
        if (query.exec(sql)) return true;
        if (error) *error = query.lastError().text();
        return false;
    };
    const auto failMigration = [this](const QString &message) {
        inventoryDb_.rollback();
        inventoryDb_.close();
        updateStatus(message);
    };

    QString sqlError;
    const QStringList baseSchema = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS devices (device_key TEXT PRIMARY KEY, ip TEXT, mac TEXT, manufacturer TEXT, model TEXT, firmware TEXT, serial TEXT, hardware TEXT, activation TEXT, source TEXT, is_candidate INTEGER, last_seen TEXT DEFAULT CURRENT_TIMESTAMP, created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS operation_history (id INTEGER PRIMARY KEY AUTOINCREMENT, occurred_at TEXT NOT NULL, operation TEXT NOT NULL, target TEXT NOT NULL, outcome TEXT NOT NULL, details TEXT NOT NULL)")
    };
    for (const QString &sql : baseSchema) {
        if (!executeMigrationSql(sql, &sqlError)) {
            failMigration(tr("Could not initialize inventory tables: %1").arg(sqlError));
            return;
        }
    }

    QStringList existingColumns;
    QString columnError;
    {
        QSqlQuery columnQuery(inventoryDb_);
        if (!columnQuery.exec(QStringLiteral("PRAGMA table_info(devices)"))) {
            columnError = columnQuery.lastError().text();
        } else {
            while (columnQuery.next())
                existingColumns << columnQuery.value(1).toString();
        }
    }
    if (!columnError.isEmpty()) {
        failMigration(tr("Could not inspect inventory schema: %1").arg(columnError));
        return;
    }

    // Version 1 upgrades legacy unversioned inventories idempotently. The
    // user_version marker is committed in the same transaction as every DDL step.
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
        QStringLiteral("ipv4_config_known|ALTER TABLE devices ADD COLUMN ipv4_config_known INTEGER DEFAULT 0"),
        QStringLiteral("created_at|ALTER TABLE devices ADD COLUMN created_at TEXT NOT NULL DEFAULT ''")
    };

    for (const QString &migration : migrations) {
        const int separator = migration.indexOf(QLatin1Char('|'));
        const QString columnName = migration.left(separator);
        if (existingColumns.contains(columnName)) continue;
        if (!executeMigrationSql(migration.mid(separator + 1), &sqlError)) {
            failMigration(tr("Inventory migration failed for %1: %2").arg(columnName, sqlError));
            return;
        }
        existingColumns << columnName;
    }

    if (!executeMigrationSql(QStringLiteral("UPDATE devices SET created_at=COALESCE(NULLIF(last_seen,''),CURRENT_TIMESTAMP) WHERE created_at='' OR created_at IS NULL"), &sqlError)) {
        failMigration(tr("Could not initialize saved-inventory creation timestamps: %1").arg(sqlError));
        return;
    }
    const QStringList indexes = {
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_devices_last_seen ON devices(last_seen DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_devices_created_at ON devices(created_at DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_operation_history_time ON operation_history(occurred_at DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_operation_history_target ON operation_history(target)")
    };
    for (const QString &sql : indexes) {
        if (!executeMigrationSql(sql, &sqlError)) {
            failMigration(tr("Could not create inventory index: %1").arg(sqlError));
            return;
        }
    }

    if (!executeMigrationSql(QStringLiteral("PRAGMA user_version = 2"), &sqlError)) {
        failMigration(tr("Could not update inventory schema version: %1").arg(sqlError));
        return;
    }
    if (!inventoryDb_.commit()) {
        const QString error = inventoryDb_.lastError().text();
        inventoryDb_.rollback();
        inventoryDb_.close();
        updateStatus(tr("Could not commit inventory schema migration: %1").arg(error));
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
    query.prepare(QStringLiteral("INSERT INTO devices (device_key,ip,mac,manufacturer,model,firmware,serial,hardware,activation,source,is_candidate,user_label,reachability_status,reachability_checked_at,last_successful_response_at,interface_name,onvif_endpoint,device_service_url,network_summary,capabilities,open_ports,network_interface_token,ipv4_address,ipv4_subnet_mask,ipv4_gateway,ipv4_dns_server,ipv4_dhcp_enabled,ipv4_config_known,last_seen) VALUES (:key,:ip,:mac,:manufacturer,:model,:firmware,:serial,:hardware,:activation,:source,:candidate,:label,:reachability,:checked,:success,:interface,:endpoint,:service,:network,:capabilities,:ports,:if_token,:ipv4,:mask,:gateway,:dns,:dhcp,:config_known,:last_seen) ON CONFLICT(device_key) DO UPDATE SET ip=excluded.ip,mac=excluded.mac,manufacturer=excluded.manufacturer,model=excluded.model,firmware=excluded.firmware,serial=excluded.serial,hardware=excluded.hardware,activation=excluded.activation,source=excluded.source,is_candidate=excluded.is_candidate,user_label=CASE WHEN excluded.user_label='' THEN devices.user_label ELSE excluded.user_label END,reachability_status=CASE WHEN excluded.reachability_status='Not checked' THEN devices.reachability_status ELSE excluded.reachability_status END,reachability_checked_at=CASE WHEN excluded.reachability_checked_at='' THEN devices.reachability_checked_at ELSE excluded.reachability_checked_at END,last_successful_response_at=CASE WHEN excluded.last_successful_response_at='' THEN devices.last_successful_response_at ELSE excluded.last_successful_response_at END,interface_name=CASE WHEN excluded.interface_name='' THEN devices.interface_name ELSE excluded.interface_name END,onvif_endpoint=CASE WHEN excluded.onvif_endpoint='' THEN devices.onvif_endpoint ELSE excluded.onvif_endpoint END,device_service_url=CASE WHEN excluded.device_service_url='' THEN devices.device_service_url ELSE excluded.device_service_url END,network_summary=CASE WHEN excluded.network_summary='' THEN devices.network_summary ELSE excluded.network_summary END,capabilities=CASE WHEN excluded.capabilities='' THEN devices.capabilities ELSE excluded.capabilities END,open_ports=CASE WHEN excluded.open_ports='' THEN devices.open_ports ELSE excluded.open_ports END,network_interface_token=CASE WHEN excluded.source LIKE '%ONVIF GetNetworkInterfaces%' THEN excluded.network_interface_token WHEN excluded.network_interface_token='' THEN devices.network_interface_token ELSE excluded.network_interface_token END,ipv4_address=CASE WHEN excluded.source LIKE '%ONVIF GetNetworkInterfaces%' THEN excluded.ipv4_address WHEN excluded.ipv4_address='' THEN devices.ipv4_address ELSE excluded.ipv4_address END,ipv4_subnet_mask=CASE WHEN excluded.source LIKE '%ONVIF GetNetworkInterfaces%' THEN excluded.ipv4_subnet_mask WHEN excluded.ipv4_subnet_mask='' THEN devices.ipv4_subnet_mask ELSE excluded.ipv4_subnet_mask END,ipv4_gateway=CASE WHEN excluded.source LIKE '%ONVIF GetNetworkInterfaces%' THEN excluded.ipv4_gateway WHEN excluded.ipv4_gateway='' THEN devices.ipv4_gateway ELSE excluded.ipv4_gateway END,ipv4_dns_server=CASE WHEN excluded.ipv4_dns_server='' THEN devices.ipv4_dns_server ELSE excluded.ipv4_dns_server END,ipv4_dhcp_enabled=CASE WHEN excluded.source LIKE '%ONVIF GetNetworkInterfaces%' THEN excluded.ipv4_dhcp_enabled WHEN excluded.ipv4_config_known=0 THEN devices.ipv4_dhcp_enabled ELSE excluded.ipv4_dhcp_enabled END,ipv4_config_known=CASE WHEN excluded.source LIKE '%ONVIF GetNetworkInterfaces%' THEN excluded.ipv4_config_known ELSE MAX(devices.ipv4_config_known,excluded.ipv4_config_known) END,last_seen=CASE WHEN excluded.last_seen IS NULL OR excluded.last_seen='' THEN devices.last_seen ELSE excluded.last_seen END"));
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
    query.bindValue(":last_seen", device.lastSeenAt);
    if (!query.exec()) {
        updateStatus(tr("Inventory save failed: %1").arg(query.lastError().text()));
        return;
    }
    refreshSavedInventory();
}

void MainWindow::loadInventory()
{
    // Historical records are shown only in Saved Inventory. Never hydrate the
    // live table from SQLite during application startup.
    refreshSavedInventory();
}

void MainWindow::refreshSavedInventory()
{
    if (!savedInventoryTable_) return;
    savedInventoryTable_->setRowCount(0);
    if (!inventoryDb_.isOpen()) {
        if (savedInventoryEmptyState_) savedInventoryEmptyState_->show();
        return;
    }
    QSqlQuery query(inventoryDb_);
    if (!query.exec(QStringLiteral(
            "SELECT device_key,ip,user_label,manufacturer,model,mac,last_seen,is_candidate "
            "FROM devices ORDER BY COALESCE(NULLIF(last_seen,''),created_at) DESC"))) {
        updateStatus(tr("Could not load saved inventory: %1").arg(query.lastError().text()));
        return;
    }
    while (query.next()) {
        const int row = savedInventoryTable_->rowCount();
        savedInventoryTable_->insertRow(row);
        const QString label = query.value(2).toString();
        const QString lastSeen = query.value(6).toString();
        const QStringList values{
            query.value(1).toString(),
            label.isEmpty() ? (query.value(7).toBool() ? tr("Unverified candidate") : tr("—")) : label,
            query.value(3).toString(), query.value(4).toString(), query.value(5).toString(),
            lastSeen.isEmpty() ? tr("Not observed") : lastSeen,
            lastSeen.isEmpty() ? tr("Not verified") : tr("Stale — historical")
        };
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            if (column == 0) item->setData(Qt::UserRole, query.value(0).toString());
            savedInventoryTable_->setItem(row, column, item);
        }
    }
    const bool empty = savedInventoryTable_->rowCount() == 0;
    if (savedInventoryEmptyState_) savedInventoryEmptyState_->setVisible(empty);
    savedInventoryTable_->setVisible(!empty);
}

void MainWindow::addManualDevice()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add camera to inventory"));
    dialog.setMinimumWidth(480);
    auto *layout = new QVBoxLayout(&dialog);
    auto *heading = new QLabel(tr("Add a device manually"), &dialog);
    heading->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(heading);
    layout->addWidget(mutedLabel(
        tr("Use this when a camera is already known on your network but was not found during discovery. Adding it does not test connectivity or verify its identity."),
        &dialog));

    auto *form = new QFormLayout();
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);
    auto *addressEdit = new QLineEdit(&dialog);
    addressEdit->setObjectName(QStringLiteral("manualAddress"));
    addressEdit->setPlaceholderText(tr("192.168.1.100 or camera.local"));
    auto *nameEdit = new QLineEdit(&dialog);
    nameEdit->setObjectName(QStringLiteral("manualDeviceName"));
    nameEdit->setPlaceholderText(tr("e.g. Front entrance"));
    auto *manufacturerEdit = new QLineEdit(&dialog);
    manufacturerEdit->setObjectName(QStringLiteral("manualManufacturer"));
    manufacturerEdit->setPlaceholderText(tr("Optional"));
    auto *modelEdit = new QLineEdit(&dialog);
    modelEdit->setObjectName(QStringLiteral("manualModel"));
    modelEdit->setPlaceholderText(tr("Optional"));
    form->addRow(tr("IP address or hostname *"), addressEdit);
    form->addRow(tr("Device name"), nameEdit);
    form->addRow(tr("Manufacturer"), manufacturerEdit);
    form->addRow(tr("Model"), modelEdit);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("saveManualDeviceButton"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, addressEdit] {
        const QString input = addressEdit->text().trimmed();
        QHostAddress address;
        const bool validIpv4 = address.setAddress(input) &&
            address.protocol() == QAbstractSocket::IPv4Protocol &&
            !address.isNull() && !address.isLoopback() && !address.isMulticast();
        static const QRegularExpression hostnamePattern(
            QStringLiteral("^(?=.{1,253}$)(?:[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?)(?:\\.(?:[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?))*\\.?$"));
        const bool numericAddress = QRegularExpression(QStringLiteral("^[0-9.]+$"))
                                        .match(input).hasMatch();
        const bool validHostname = !numericAddress && hostnamePattern.match(input).hasMatch();
        if (!validIpv4 && !validHostname) {
            QMessageBox::warning(&dialog, QObject::tr("Check the device address"),
                QObject::tr("Enter a valid IPv4 address or hostname, such as 192.168.1.100 or camera.local."));
            addressEdit->setFocus();
            return;
        }
        dialog.accept();
    });
    addressEdit->setFocus();
    if (dialog.exec() != QDialog::Accepted) return;

    const QString input = addressEdit->text().trimmed();
    QHostAddress address(input);
    DeviceRecord device;
    device.ipAddress = address.isNull() ? input : address.toString();
    device.userLabel = nameEdit->text().trimmed();
    device.manufacturer = manufacturerEdit->text().trimmed();
    device.model = modelEdit->text().trimmed();
    device.discoverySources << QStringLiteral("Manual");
    device.activationState = QStringLiteral("Unknown");
    persistDevice(device);
    refreshSavedInventory();
    recordOperation(tr("Manual device entry"), device.ipAddress, tr("Completed"),
                    tr("Manually added to the local inventory; camera identity and reachability are not verified."));
    refreshOperationHistory();
    updateStatus(tr("Added %1 to inventory. Discovery or a reachability check is still required to verify it.")
                     .arg(device.ipAddress));
}


void MainWindow::applyFilters()
{
    const QString needle = searchEdit_ ? searchEdit_->text().trimmed() : QString();
    const QString vendor = manufacturerFilter_ ? manufacturerFilter_->currentText() : tr("All manufacturers");
    const QString status = statusFilter_ ? statusFilter_->currentText() : tr("All statuses");
    const QString deviceType = deviceTypeFilter_ ? deviceTypeFilter_->currentData().toString()
                                                  : QStringLiteral("all");
    int visible = 0;
    for (int row = 0; row < deviceTable_->rowCount(); ++row) {
        bool textMatch = needle.isEmpty();
        for (int col = 1; col < deviceTable_->columnCount() && !textMatch; ++col) {
            const auto *item = deviceTable_->item(row, col);
            textMatch = item && item->text().contains(needle, Qt::CaseInsensitive);
        }
        const QString source = deviceTable_->item(row, 8) ? deviceTable_->item(row, 8)->text() : QString();
        const QString activation = deviceTable_->item(row, 7) ? deviceTable_->item(row, 7)->text() : QString();
        const QString manufacturer = deviceTable_->item(row, 3)
            ? deviceTable_->item(row, 3)->text() : QString();
        const bool isOnvif = source.contains(QStringLiteral("ONVIF"), Qt::CaseInsensitive);
        const bool isOxita = manufacturer.compare(QStringLiteral("Oxita"), Qt::CaseInsensitive) == 0 ||
            source.contains(QStringLiteral("Oxita"), Qt::CaseInsensitive) ||
            source.contains(QStringLiteral("Project UDP 9988"), Qt::CaseInsensitive);
        const bool vendorMatch = vendor == tr("All manufacturers") ||
            (vendor == tr("ONVIF devices") && isOnvif) ||
            (vendor == tr("Oxita devices") && isOxita) ||
            (vendor == tr("Other / unknown") && !isOnvif && !isOxita);
        const QString reachability = deviceTable_->item(row, 11) ? deviceTable_->item(row, 11)->text() : QStringLiteral("Not checked");
        const QString classifiedType = deviceTable_->item(row, 1)
            ? deviceTable_->item(row, 1)->data(Qt::UserRole + 17).toString()
            : QStringLiteral("unknown");
        const bool typeMatch = deviceType == QStringLiteral("all") || classifiedType == deviceType;
        const bool statusMatch = status == tr("All statuses") ||
            (status == tr("Online") && reachability == QStringLiteral("Reachable")) ||
            (status == tr("Offline") && reachability == QStringLiteral("Unreachable")) ||
            (status == tr("Stale") && reachability == QStringLiteral("Stale")) ||
            (status == tr("Activated") && activation.compare(QStringLiteral("Activated"), Qt::CaseInsensitive) == 0) ||
            (status == tr("Not activated") && activation.compare(QStringLiteral("Not activated"), Qt::CaseInsensitive) == 0);
        const bool show = textMatch && vendorMatch && typeMatch && statusMatch;
        deviceTable_->setRowHidden(row, !show);
        if (show) ++visible;
    }
    const int selectedRow = selectedDeviceRow();
    if (selectedRow >= 0 && deviceTable_->isRowHidden(selectedRow)) {
        deviceTable_->clearSelection();
        deviceTable_->setCurrentCell(-1, -1);
    }
    if (deviceCountValue_) deviceCountValue_->setText(tr("%1 of %2 devices").arg(visible).arg(deviceTable_->rowCount()));
    if (inventoryEmptyState_) {
        if (deviceTable_->rowCount() == 0) {
            inventoryEmptyState_->setText(
                tr("No live scan results yet.\nChoose a network interface above and select Discover cameras to find devices on your LAN."));
            inventoryEmptyState_->show();
        } else if (visible == 0) {
            inventoryEmptyState_->setText(
                tr("No devices match these filters.\nClear your search or choose different device type, manufacturer or status filters."));
            inventoryEmptyState_->show();
        } else {
            inventoryEmptyState_->hide();
        }
    }
}

void MainWindow::editSelectedLabel()
{
    const int row = selectedDeviceRow();
    if (row < 0) {
        updateStatus(tr("Select a device before renaming its label."));
        return;
    }
    const QString current = deviceTable_->item(row, 2) ? deviceTable_->item(row, 2)->text() : QString();
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Rename camera"));
    dialog.setMinimumWidth(420);
    auto *layout = new QVBoxLayout(&dialog);
    auto *heading = new QLabel(tr("Give this camera a recognizable name"), &dialog);
    heading->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(heading);
    layout->addWidget(mutedLabel(tr("This name is stored in the local inventory and does not change the camera's own configuration."), &dialog));
    auto *form = new QFormLayout();
    auto *nameEdit = new QLineEdit(current, &dialog);
    nameEdit->setObjectName(QStringLiteral("deviceFriendlyName"));
    nameEdit->setPlaceholderText(tr("e.g. Warehouse loading bay"));
    form->addRow(tr("Device name"), nameEdit);
    layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("saveDeviceNameButton"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    nameEdit->setFocus();
    nameEdit->selectAll();
    if (dialog.exec() != QDialog::Accepted) return;
    const QString label = nameEdit->text().trimmed();
    if (deviceTable_->item(row, 2)) deviceTable_->item(row, 2)->setText(label);
    const QString ip = deviceTable_->item(row, 1) ? deviceTable_->item(row, 1)->text() : QString();
    const QString mac = deviceTable_->item(row, 5) ? deviceTable_->item(row, 5)->text() : QString();
    if (inventoryDb_.isOpen()) {
        QSqlQuery query(inventoryDb_);
        query.prepare(QStringLiteral("UPDATE devices SET user_label=:label WHERE device_key=:key"));
        query.bindValue(":label", label);
        query.bindValue(":key", !mac.isEmpty() ? mac : ip);
        if (!query.exec()) updateStatus(tr("Could not save device name: %1").arg(query.lastError().text()));
        else updateStatus(tr("Device name saved."));
    } else {
        updateStatus(tr("Name changed for this session only; local inventory storage is unavailable."));
    }
    applyFilters();
}


QUrl MainWindow::selectedCameraWebUrl() const
{
    const int row = selectedDeviceRow();
    if (row < 0 || !deviceTable_) return {};
    const auto *item = deviceTable_->item(row, 1);
    if (!item || item->text().trimmed().isEmpty()) return {};
    const QString host = item->text().trimmed();
    QUrl url = host.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive) ||
               host.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        ? QUrl::fromUserInput(host) : QUrl(QStringLiteral("http://") + host);
    if (!url.isValid() || url.host().isEmpty() ||
        (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https")))
        return {};

    const QStringList openPorts = item->data(Qt::UserRole + 6).toString()
        .split(QStringLiteral(", "), Qt::SkipEmptyParts);
    if (!host.startsWith(QStringLiteral("http"), Qt::CaseInsensitive)) {
        if (openPorts.contains(QStringLiteral("443"))) {
            url.setScheme(QStringLiteral("https"));
        } else if (openPorts.contains(QStringLiteral("8443"))) {
            url.setScheme(QStringLiteral("https"));
            url.setPort(8443);
        } else if (openPorts.contains(QStringLiteral("8080"))) {
            url.setPort(8080);
        }
    }
    return url;
}

void MainWindow::openSelectedWebInterface()
{
    const int row = selectedDeviceRow();
    if (row < 0) return;
    const QUrl url = selectedCameraWebUrl();
    if (!url.isValid()) {
        QMessageBox::warning(this, tr("Invalid address"),
                             tr("The selected device does not have a valid HTTP or HTTPS address."));
        return;
    }

    const QString label = deviceTable_->item(row, 2)
        ? deviceTable_->item(row, 2)->text().trimmed() : QString();
    const auto *ipItem = deviceTable_->item(row, 1);
    DeviceRecord expectedDevice;
    expectedDevice.id = ipItem ? ipItem->text().trimmed() : url.host();
    expectedDevice.ipAddress = ipItem ? ipItem->text().trimmed() : url.host();
    expectedDevice.userLabel = label;
    expectedDevice.manufacturer = deviceTable_->item(row, 3)
        ? deviceTable_->item(row, 3)->text().trimmed() : QString();
    expectedDevice.model = deviceTable_->item(row, 4)
        ? deviceTable_->item(row, 4)->text().trimmed() : QString();
    expectedDevice.macAddress = deviceTable_->item(row, 5)
        ? deviceTable_->item(row, 5)->text().trimmed() : QString();
    expectedDevice.serialNumber = deviceTable_->item(row, 9)
        ? deviceTable_->item(row, 9)->text().trimmed() : QString();
    expectedDevice.firmwareVersion = deviceTable_->item(row, 10)
        ? deviceTable_->item(row, 10)->text().trimmed() : QString();
    expectedDevice.discoverySources = deviceTable_->item(row, 8)
        ? deviceTable_->item(row, 8)->text().split(QStringLiteral(", "), Qt::SkipEmptyParts)
        : QStringList{};
    if (ipItem) {
        expectedDevice.onvifEndpoint = ipItem->data(Qt::UserRole + 2).toString();
        expectedDevice.deviceServiceUrl = ipItem->data(Qt::UserRole + 3).toString();
        expectedDevice.openPorts = ipItem->data(Qt::UserRole + 6).toString()
            .split(QStringLiteral(", "), Qt::SkipEmptyParts);
        expectedDevice.networkInterfaceToken = ipItem->data(Qt::UserRole + 7).toString();
    }
    CameraBrowserDialog browser(url, label, this);
    recordOperation(tr("Camera setup opened"), url.host(), tr("Started"),
                    tr("Opened the camera's own web interface in Oxita. Camera activation is not assumed."));
    const int result = browser.exec();
    if (result != QDialog::Accepted) {
        updateStatus(tr("Camera setup window closed. Initialization has not been verified."));
        return;
    }

    pendingCameraVerification_ = expectedDevice;
    pendingCameraVerificationActive_ = true;
    pendingCameraIdentityMatched_ = false;
    recordOperation(tr("Camera setup completed by user"), url.host(), tr("Verification pending"),
                    tr("User indicated setup is complete. A fresh discovery and strong identity match are required; web-page completion alone is not proof of activation."));
    updateStatus(tr("Setup page closed. Rediscovering on the selected network interface to verify the camera…"));
    // Reuse the normal discovery action so Live Scan Results are cleared
    // before the new scan; never leave the pre-initialization snapshot mixed in.
    if (scanButton_ && scanButton_->isEnabled() &&
        discovery_ && !discovery_->isRunning() && !subnetScanner_->isRunning()) {
        scanButton_->click();
    } else {
        pendingCameraVerificationActive_ = false;
        pendingCameraIdentityMatched_ = false;
        updateStatus(tr("Could not start verification discovery because another network operation is active. Run Discover cameras when it finishes; setup remains unverified."));
    }
}
