#include "ui/main_window.h"

#include <QStandardPaths>
#include <QTabWidget>
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
}

QTEST_MAIN(MainWindowSmokeTest)
#include "main_window_smoke_test.moc"
