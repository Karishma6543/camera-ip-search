#pragma once

#include "core/device_record.h"

#include <QMainWindow>
#include <QSqlDatabase>
#include <QVector>

class QLabel;
class QTableWidget;
class QPushButton;
class QLineEdit;
class QComboBox;
class QProgressBar;
class DiscoveryService;
class SubnetScanner;
class ReachabilityChecker;
class QWidget;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
private:
    void buildUi();
    void populateInterfaces();
    QWidget *makeInfoTab(QWidget *parent);
    QWidget *makeMaintenanceTab(QWidget *parent);
    QWidget *makeHistoryTab(QWidget *parent);
    void recordOperation(const QString &operation, const QString &target, const QString &outcome, const QString &details);
    void refreshOperationHistory();
    void exportDiagnosticBundle();
    void updateStatus(const QString &message);
    void updateDeviceSummary();
    void upsertDevice(DeviceRecord device);
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
    QTableWidget *historyTable_{nullptr};
    QLabel *statusLabel_{nullptr};
    QProgressBar *operationProgress_{nullptr};
    QLabel *summaryLabel_{nullptr};
    QLabel *inventoryEmptyState_{nullptr};
    QLabel *deviceCountValue_{nullptr};
    QLabel *discoveredMetric_{nullptr};
    QLabel *reachableMetric_{nullptr};
    QLabel *verifiedMetric_{nullptr};
    QLabel *unverifiedMetric_{nullptr};
    QPushButton *scanButton_{nullptr};
    QPushButton *subnetScanButton_{nullptr};
    QPushButton *reachabilityButton_{nullptr};
    ReachabilityChecker *reachabilityChecker_{nullptr};
    QLineEdit *searchEdit_{nullptr};
    QComboBox *interfaceSelector_{nullptr};
    QComboBox *manufacturerFilter_{nullptr};
    QComboBox *statusFilter_{nullptr};
    bool loadingInventory_{false};
    int sortColumn_{-1};
    bool sortAscending_{true};
    QLabel *deviceDetailsLabel_{nullptr};
    QLabel *selectedDeviceSummary_{nullptr};
    QLabel *selectedNetworkSummary_{nullptr};
    QLabel *localNetworkSummary_{nullptr};
    QLabel *firmwareStatusSummary_{nullptr};
    void updateSelectedDevicePanel();
    void updateLocalNetworkSummary();
    int selectedDeviceRow() const;
    void updateDeviceDetailsProfile(int row);
    void startNextBulkIpv4Configuration();
    struct BulkIpv4Target { DeviceRecord device; QString address; QString subnetMask; QString gateway; };
    QVector<BulkIpv4Target> bulkIpv4Queue_;
    int bulkIpv4Next_{0};
    int bulkIpv4Succeeded_{0};
    int bulkIpv4Failed_{0};
    bool bulkIpv4Running_{false};
    QPushButton *addDeviceButton_{nullptr};
    QPushButton *openWebButton_{nullptr};
    QPushButton *refreshDeviceButton_{nullptr};
    QSqlDatabase inventoryDb_;
};
