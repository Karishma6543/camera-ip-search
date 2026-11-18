#pragma once

#include "core/device_record.h"

#include <QMainWindow>

class QLabel;
class QTableWidget;
class QPushButton;
class DiscoveryService;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
private:
    void buildUi();
    void updateStatus(const QString &message);
    void upsertDevice(const DeviceRecord &device);
    DiscoveryService *discovery_{nullptr};
    QTableWidget *deviceTable_{nullptr};
    QLabel *statusLabel_{nullptr};
    QPushButton *scanButton_{nullptr};
};
