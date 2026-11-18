#pragma once

#include <QMainWindow>

class QLabel;
class QTableWidget;
class QPushButton;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
private:
    void buildUi();
    void updateStatus(const QString &message);
    QTableWidget *deviceTable_{nullptr};
    QLabel *statusLabel_{nullptr};
    QPushButton *scanButton_{nullptr};
};
