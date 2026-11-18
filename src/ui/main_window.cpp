#include "main_window.h"

#include "discovery/discovery_service.h"

#include <QAbstractItemView>
#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    discovery_ = new DiscoveryService(this);
    buildUi();
    updateStatus(tr("Ready — select Discover to search the connected local networks."));
    connect(discovery_, &DiscoveryService::deviceFound, this, &MainWindow::upsertDevice);
    connect(discovery_, &DiscoveryService::statusChanged, this, &MainWindow::updateStatus);
    connect(discovery_, &DiscoveryService::finished, this, [this](int count) {
        scanButton_->setText(tr("Discover cameras"));
        scanButton_->setEnabled(true);
        updateDeviceSummary();
        updateStatus(tr("Discovery finished — %1 device(s) found.").arg(count));
    });
}

void MainWindow::buildUi()
{
    setWindowTitle(tr("Camera IP Search"));
    setMinimumSize(920, 620);
    resize(1280, 780);

    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget#appRoot { background: #f4f7fb; color: #17243a; }
        QLabel { color: #17243a; }
        QLabel#eyebrow { color: #52708f; font-size: 11px; font-weight: 700; letter-spacing: 1px; }
        QLabel#pageTitle { color: #10243e; font-size: 27px; font-weight: 750; }
        QLabel#subtitle { color: #667991; font-size: 13px; }
        QFrame#heroCard { background: #102b4e; border: 0; border-radius: 14px; }
        QLabel#heroTitle { color: #ffffff; font-size: 19px; font-weight: 700; }
        QLabel#heroText { color: #c5d5e8; font-size: 12px; }
        QLabel#metricValue { color: #ffffff; font-size: 24px; font-weight: 700; }
        QLabel#metricCaption { color: #c5d5e8; font-size: 11px; }
        QFrame#contentCard { background: #ffffff; border: 1px solid #e2e9f1; border-radius: 12px; }
        QLabel#sectionTitle { color: #1b304b; font-size: 15px; font-weight: 700; }
        QLabel#muted { color: #71839a; font-size: 12px; }
        QPushButton { background: #ffffff; color: #29425f; border: 1px solid #d5dfeb; border-radius: 7px; padding: 9px 15px; font-weight: 600; }
        QPushButton:hover { background: #f0f5fb; border-color: #a9bdd4; }
        QPushButton#primaryButton { background: #1672d4; color: #ffffff; border: 1px solid #1672d4; padding: 10px 18px; }
        QPushButton#primaryButton:hover { background: #0f61b8; }
        QPushButton:disabled { color: #9aa8b8; background: #edf1f5; border-color: #e1e7ee; }
        QTableWidget { background: #ffffff; alternate-background-color: #f7f9fc; border: 0; gridline-color: #edf1f5; selection-background-color: #e5f1ff; selection-color: #17385f; font-size: 12px; }
        QHeaderView::section { background: #f5f8fc; color: #61758d; border: 0; border-bottom: 1px solid #e4eaf1; padding: 11px 8px; font-size: 11px; font-weight: 700; }
        QTableWidget::item { padding: 8px 6px; border-bottom: 1px solid #f0f3f7; }
        QStatusBar { background: #ffffff; color: #61758d; border-top: 1px solid #e5ebf2; font-size: 11px; }
    )"));

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("appRoot"));
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(28, 22, 28, 16);
    layout->setSpacing(16);

    auto *topRow = new QHBoxLayout();
    auto *brandBlock = new QVBoxLayout();
    auto *eyebrow = new QLabel(tr("DEVICE MANAGEMENT  /  NETWORK TOOLS"), central);
    eyebrow->setObjectName(QStringLiteral("eyebrow"));
    auto *heading = new QLabel(tr("Camera IP Search"), central);
    heading->setObjectName(QStringLiteral("pageTitle"));
    auto *subtitle = new QLabel(tr("Discover, identify and manage IP cameras across your local network."), central);
    subtitle->setObjectName(QStringLiteral("subtitle"));
    brandBlock->addWidget(eyebrow);
    brandBlock->addWidget(heading);
    brandBlock->addWidget(subtitle);
    topRow->addLayout(brandBlock);
    topRow->addStretch();
    auto *versionLabel = new QLabel(tr("DESKTOP  •  CROSS-PLATFORM"), central);
    versionLabel->setObjectName(QStringLiteral("eyebrow"));
    topRow->addWidget(versionLabel, 0, Qt::AlignTop);
    layout->addLayout(topRow);

    auto *hero = new QFrame(central);
    hero->setObjectName(QStringLiteral("heroCard"));
    auto *heroLayout = new QHBoxLayout(hero);
    heroLayout->setContentsMargins(22, 18, 22, 18);
    heroLayout->setSpacing(24);
    auto *heroTextBlock = new QVBoxLayout();
    auto *heroTitle = new QLabel(tr("Find cameras on your network"), hero);
    heroTitle->setObjectName(QStringLiteral("heroTitle"));
    auto *heroText = new QLabel(tr("Search active IPv4 interfaces using ONVIF WS-Discovery and the project camera protocol."), hero);
    heroText->setObjectName(QStringLiteral("heroText"));
    heroText->setWordWrap(true);
    heroTextBlock->addWidget(heroTitle);
    heroTextBlock->addWidget(heroText);
    heroLayout->addLayout(heroTextBlock, 1);

    auto *metricBlock = new QVBoxLayout();
    deviceCountValue_ = new QLabel(QStringLiteral("0"), hero);
    deviceCountValue_->setObjectName(QStringLiteral("metricValue"));
    auto *metricCaption = new QLabel(tr("DEVICES FOUND"), hero);
    metricCaption->setObjectName(QStringLiteral("metricCaption"));
    metricBlock->addWidget(deviceCountValue_, 0, Qt::AlignRight);
    metricBlock->addWidget(metricCaption, 0, Qt::AlignRight);
    heroLayout->addLayout(metricBlock);
    layout->addWidget(hero);

    auto *content = new QFrame(central);
    content->setObjectName(QStringLiteral("contentCard"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(18, 16, 18, 16);
    contentLayout->setSpacing(12);

    auto *sectionRow = new QHBoxLayout();
    auto *sectionBlock = new QVBoxLayout();
    auto *sectionTitle = new QLabel(tr("Discovered devices"), content);
    sectionTitle->setObjectName(QStringLiteral("sectionTitle"));
    summaryLabel_ = new QLabel(tr("No scan has been run yet"), content);
    summaryLabel_->setObjectName(QStringLiteral("muted"));
    sectionBlock->addWidget(sectionTitle);
    sectionBlock->addWidget(summaryLabel_);
    sectionRow->addLayout(sectionBlock);
    sectionRow->addStretch();
    auto *clearButton = new QPushButton(tr("Clear results"), content);
    scanButton_ = new QPushButton(tr("Discover cameras"), content);
    scanButton_->setObjectName(QStringLiteral("primaryButton"));
    sectionRow->addWidget(clearButton);
    sectionRow->addWidget(scanButton_);
    contentLayout->addLayout(sectionRow);

    deviceTable_ = new QTableWidget(0, 8, content);
    deviceTable_->setHorizontalHeaderLabels({tr("IP ADDRESS"), tr("MAC ADDRESS"), tr("MANUFACTURER"), tr("MODEL / SOC"), tr("FIRMWARE"), tr("ACTIVATION"), tr("SOURCE"), tr("INTERFACE")});
    deviceTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    deviceTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    deviceTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    deviceTable_->horizontalHeader()->setStretchLastSection(true);
    deviceTable_->verticalHeader()->setVisible(false);
    deviceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    deviceTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    deviceTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    deviceTable_->setAlternatingRowColors(true);
    deviceTable_->setShowGrid(false);
    deviceTable_->setFocusPolicy(Qt::NoFocus);
    deviceTable_->setMinimumHeight(300);
    contentLayout->addWidget(deviceTable_, 1);

    emptyState_ = new QLabel(tr("No cameras found yet\nConnect a camera to this network, then select Discover cameras."), content);
    emptyState_->setObjectName(QStringLiteral("muted"));
    emptyState_->setAlignment(Qt::AlignCenter);
    emptyState_->setMinimumHeight(90);
    contentLayout->addWidget(emptyState_);
    layout->addWidget(content, 1);

    auto *footer = new QLabel(tr("Discovery currently covers active local IPv4 interfaces. Vendor-specific configuration is being developed."), central);
    footer->setObjectName(QStringLiteral("muted"));
    layout->addWidget(footer);

    setCentralWidget(central);

    connect(scanButton_, &QPushButton::clicked, this, [this] {
        if (discovery_->isRunning()) {
            discovery_->cancel();
            scanButton_->setText(tr("Discover cameras"));
        } else {
            deviceTable_->setRowCount(0);
            updateDeviceSummary();
            scanButton_->setText(tr("Cancel discovery"));
            scanButton_->setEnabled(true);
            discovery_->start();
        }
    });
    connect(clearButton, &QPushButton::clicked, this, [this] {
        deviceTable_->setRowCount(0);
        updateDeviceSummary();
        updateStatus(tr("Device list cleared."));
    });

    statusLabel_ = new QLabel(this);
    statusBar()->addWidget(statusLabel_, 1);
    updateDeviceSummary();
}

void MainWindow::updateStatus(const QString &message)
{
    if (statusLabel_) statusLabel_->setText(message);
}

void MainWindow::updateDeviceSummary()
{
    const int count = deviceTable_ ? deviceTable_->rowCount() : 0;
    if (deviceCountValue_) deviceCountValue_->setText(QString::number(count));
    if (summaryLabel_) {
        summaryLabel_->setText(count == 1 ? tr("1 device discovered") : tr("%1 devices discovered").arg(count));
    }
    if (emptyState_) emptyState_->setVisible(count == 0);
    if (deviceTable_) deviceTable_->setVisible(count > 0);
}

void MainWindow::upsertDevice(const DeviceRecord &device)
{
    int row = -1;
    for (int i = 0; i < deviceTable_->rowCount(); ++i) {
        const auto *item = deviceTable_->item(i, 0);
        const auto *macItem = deviceTable_->item(i, 1);
        if ((macItem && !device.macAddress.isEmpty() && macItem->text() == device.macAddress) ||
            (item && item->text() == device.ipAddress)) {
            row = i;
            break;
        }
    }
    if (row < 0) {
        row = deviceTable_->rowCount();
        deviceTable_->insertRow(row);
    }
    const QStringList values = {
        device.ipAddress, device.macAddress, device.manufacturer, device.model,
        device.firmwareVersion, device.activationState,
        device.discoverySources.join(QStringLiteral(", ")), device.interfaceName
    };
    for (int column = 0; column < values.size(); ++column)
        deviceTable_->setItem(row, column, new QTableWidgetItem(values.at(column)));
    updateDeviceSummary();
}
