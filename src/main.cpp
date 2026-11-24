#include "ui/main_window.h"
#include "core/diagnostic_log.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(oxita_branding);
    app.setStyle("Fusion");
    QCoreApplication::setApplicationName("Oxita Camera Device Manager");
    QCoreApplication::setApplicationVersion("0.2.0");
    QCoreApplication::setOrganizationName("Oxita");
    DiagnosticLog::install();

    MainWindow window;
    window.show();
    return app.exec();
}
