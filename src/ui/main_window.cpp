#include "main_window.h"

#include "discovery/discovery_service.h"

#include <QAbstractItemView>
#include <QFont>
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
    updateStatus(tr("Ready. Discover using ONVIF and the project's own camera protocol."));
    connect(discovery_, &DiscoveryService::deviceFound, this, &MainWindow::upsertDevice);
    connect(discovery_, &DiscoveryService::statusChanged, this, &MainWindow::updateStatus);
    connect(discovery_, &DiscoveryService::finished, this, [this](int count) {
        scanButton_->setText(tr("Discover cameras"));
        scanButton_->setEnabled(true);
        updateStatus(tr("Discovery complete — %1 unique device(s) found.").arg(count));
    });
}

void MainWindow::buildUi()
{
    setWindowTitle(tr("Camera IP Search"));
    resize(1180, 700);
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(14);

    auto *heading = new QLabel(tr("Camera IP Search"), central);
    QFont headingFont = heading->font();
    headingFont.setPointSize(20);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    auto *subtitle = new QLabel(tr("Multi-brand discovery for ONVIF cameras and our own camera firmware."), central);

    auto *toolbar = new QHBoxLayout();
    scanButton_ = new QPushButton(tr("Discover cameras"), central);
    auto *refreshButton = new QPushButton(tr("Clear results"), central);
    toolbar->addWidget(scanButton_);
    toolbar->addWidget(refreshButton);
    toolbar->addStretch();

    deviceTable_ = new QTableWidget(0, 8, central);
    deviceTable_->setHorizontalHeaderLabels({tr("IP address"), tr("MAC address"), tr("Manufacturer"), tr("Model / SoC"), tr("Firmware"), tr("Activation"), tr("Discovery source"), tr("Network interface")});
    deviceTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    deviceTable_->horizontalHeader()->setStretchLastSection(true);
    deviceTable_->verticalHeader()->setVisible(false);
    deviceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    deviceTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    deviceTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    deviceTable_->setAlternatingRowColors(true);

    auto *emptyState = new QLabel(tr("No devices discovered. Select Discover cameras to scan active IPv4 interfaces."), central);
    emptyState->setAlignment(Qt::AlignCenter);
    emptyState->setMinimumHeight(70);
    layout->addWidget(heading);
    layout->addWidget(subtitle);
    layout->addLayout(toolbar);
    layout->addWidget(deviceTable_, 1);
    layout->addWidget(emptyState);
    setCentralWidget(central);

    connect(scanButton_, &QPushButton::clicked, this, [this] {
        if (discovery_->isRunning()) {
            discovery_->cancel();
            scanButton_->setText(tr("Discover cameras"));
        } else {
            deviceTable_->setRowCount(0);
            scanButton_->setText(tr("Cancel discovery"));
            scanButton_->setEnabled(true);
            discovery_->start();
        }
    });
    connect(refreshButton, &QPushButton::clicked, this, [this] {
        deviceTable_->setRowCount(0);
        updateStatus(tr("Results cleared."));
    });
    statusLabel_ = new QLabel(this);
    statusBar()->addWidget(statusLabel_);
}

void MainWindow::updateStatus(const QString &message)
{
    if (statusLabel_) statusLabel_->setText(message);
}

void MainWindow::upsertDevice(const DeviceRecord &device)
{
    int row = -1;
    for (int i = 0; i < deviceTable_->rowCount(); ++i) {
        const auto *item = deviceTable_->item(i, 0);
        if (item && item->text() == device.ipAddress) { row = i; break; }
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
}
