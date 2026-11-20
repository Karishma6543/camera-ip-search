#include "ui/main_window.h"
#include "core/diagnostic_logger.h"

#include <QApplication>
#include <QCoreApplication>
#include <QStandardPaths>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("Camera IP Search");
    QCoreApplication::setApplicationVersion("0.1.0");
    QCoreApplication::setOrganizationName("Camera Tools");

    DiagnosticLogger::initialize(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));

    MainWindow window;
    window.show();
    return app.exec();
}
