#include "ui/main_window.h"

#include <QStandardPaths>
#include <QTabWidget>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtTest>

class MainWindowSmokeTest final : public QObject
{
    Q_OBJECT
private slots:
    void startupShowsOperationHistory();
};

void MainWindowSmokeTest::startupShowsOperationHistory()
{
    QStandardPaths::setTestModeEnabled(true);
    MainWindow window;
    QVERIFY(!window.windowTitle().isEmpty());
    const auto tabs = window.findChild<QTabWidget *>();
    QVERIFY(tabs);
    bool historyFound = false;
    for (int index = 0; index < tabs->count(); ++index) {
        if (tabs->tabText(index) == QStringLiteral("Operation History")) {
            historyFound = true;
            break;
        }
    }
    QVERIFY2(historyFound, "The operation-history workspace must be present at startup.");
    const QSqlDatabase database = QSqlDatabase::database(QStringLiteral("camera_inventory"), false);
    QVERIFY2(database.isValid() && database.isOpen(), "The local inventory database should open in test mode.");
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT name FROM sqlite_master WHERE type=:type AND name=:name"));
    query.bindValue(":type", QStringLiteral("table"));
    query.bindValue(":name", QStringLiteral("operation_history"));
    QVERIFY(query.exec());
    QVERIFY(query.next());
}

QTEST_MAIN(MainWindowSmokeTest)
#include "main_window_smoke_test.moc"
