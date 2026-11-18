#include "main_window.h"

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
    buildUi();
    updateStatus(tr("Ready — discovery protocol adapter is not connected yet."));
}

void MainWindow::buildUi()
{
    setWindowTitle(tr("Camera IP Search"));
    resize(1100, 680);
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(14);

    auto *heading = new QLabel(tr("Camera IP Search"), central);
    QFont headingFont = heading->font();
    headingFont.setPointSize(20);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    auto *subtitle = new QLabel(tr("Discover, activate and configure IP cameras on your network."), central);

    auto *toolbar = new QHBoxLayout();
    scanButton_ = new QPushButton(tr("Discover cameras"), central);
    scanButton_->setEnabled(false);
    scanButton_->setToolTip(tr("Enabled after the camera discovery protocol adapter is implemented."));
    auto *refreshButton = new QPushButton(tr("Refresh view"), central);
    toolbar->addWidget(scanButton_);
    toolbar->addWidget(refreshButton);
    toolbar->addStretch();

    deviceTable_ = new QTableWidget(0, 7, central);
    deviceTable_->setHorizontalHeaderLabels({tr("Status"), tr("IP address"), tr("MAC address"), tr("Model"), tr("Firmware"), tr("Activation"), tr("Interface")});
    deviceTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    deviceTable_->verticalHeader()->setVisible(false);
    deviceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    deviceTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    deviceTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    deviceTable_->setAlternatingRowColors(true);

    auto *emptyState = new QLabel(tr("No cameras discovered yet. The discovery engine will populate this list."), central);
    emptyState->setAlignment(Qt::AlignCenter);
    emptyState->setMinimumHeight(90);
    layout->addWidget(heading);
    layout->addWidget(subtitle);
    layout->addLayout(toolbar);
    layout->addWidget(deviceTable_, 1);
    layout->addWidget(emptyState);
    setCentralWidget(central);

    connect(refreshButton, &QPushButton::clicked, this, [this] {
        updateStatus(tr("View refreshed. No discovery adapter is configured."));
    });
    statusLabel_ = new QLabel(this);
    statusBar()->addWidget(statusLabel_);
}

void MainWindow::updateStatus(const QString &message)
{
    if (statusLabel_) statusLabel_->setText(message);
}
