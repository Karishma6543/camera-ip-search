#pragma once

#include "core/device_record.h"

#include <QMainWindow>

class QLabel;
class QTableWidget;
class QPushButton;
class QLineEdit;
class QComboBox;
class DiscoveryService;
class SubnetScanner;
class QWidget;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
private:
    void buildUi();
    void populateInterfaces();
    QWidget *makeInfoTab(QWidget *parent);
    QWidget *makeBulkTab(QWidget *parent);
    QWidget *makeMaintenanceTab(QWidget *parent);
    QWidget *makeNvrTab(QWidget *parent);
    void updateStatus(const QString &message);
    void updateDeviceSummary();
    void upsertDevice(const DeviceRecord &device);
    DiscoveryService *discovery_{nullptr};
    SubnetScanner *subnetScanner_{nullptr};
    QTableWidget *deviceTable_{nullptr};
    QLabel *statusLabel_{nullptr};
    QLabel *summaryLabel_{nullptr};
    QLabel *deviceCountValue_{nullptr};
    QPushButton *scanButton_{nullptr};
    QPushButton *subnetScanButton_{nullptr};
    QLineEdit *searchEdit_{nullptr};
    QComboBox *interfaceSelector_{nullptr};
};
