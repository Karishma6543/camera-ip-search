#pragma once

#include "core/device_record.h"

#include <QMainWindow>
#include <QSqlDatabase>

class QLabel;
class QTableWidget;
class QPushButton;
class QLineEdit;
class QComboBox;
class DiscoveryService;
class SubnetScanner;
class ReachabilityChecker;
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
    void initializeInventory();
    void persistDevice(const DeviceRecord &device);
    void loadInventory();
    void addManualDevice();
    void openSelectedWebInterface();
    void applyFilters();
    void editSelectedLabel();
    DiscoveryService *discovery_{nullptr};
    SubnetScanner *subnetScanner_{nullptr};
    QTableWidget *deviceTable_{nullptr};
    QLabel *statusLabel_{nullptr};
    QLabel *summaryLabel_{nullptr};
    QLabel *deviceCountValue_{nullptr};
    QPushButton *scanButton_{nullptr};
    QPushButton *subnetScanButton_{nullptr};
    QPushButton *reachabilityButton_{nullptr};
    ReachabilityChecker *reachabilityChecker_{nullptr};
    QLineEdit *searchEdit_{nullptr};
    QComboBox *interfaceSelector_{nullptr};
    QComboBox *manufacturerFilter_{nullptr};
    QComboBox *statusFilter_{nullptr};
    bool loadingInventory_{false};
    QLabel *deviceDetailsLabel_{nullptr};
    QPushButton *addDeviceButton_{nullptr};
    QPushButton *openWebButton_{nullptr};
    QSqlDatabase inventoryDb_;
};
