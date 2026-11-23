#include "ui/main_window.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    QCoreApplication::setApplicationName("Oxita Camera Device Manager");
    QCoreApplication::setApplicationVersion("0.2.0");
    QCoreApplication::setOrganizationName("Oxita");

    MainWindow window;
    window.show();
    return app.exec();
}
