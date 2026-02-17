#include "app.h"
#include <QCoreApplication>

int main(int argc, char **argv)
{
    QCoreApplication qapp(argc, argv);
    qapp.setOrganizationName(QStringLiteral("Remus"));
    qapp.setApplicationName(QStringLiteral("Remus"));

    TuiApp app;
    return app.run();
}
