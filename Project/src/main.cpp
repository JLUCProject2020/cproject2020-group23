#include <QtCore>
#include <QtWidgets>
#include "MainView.h"
#include "ImageManager.h"

int main(int argc, char * *argv)
{
    QApplication app(argc, argv);
    // For QSettings
    QCoreApplication::setOrganizationName("2020 TAQ");
    QCoreApplication::setOrganizationDomain(" ");
    QCoreApplication::setApplicationName("Plants vs Zombies");
    // For Translators
    QTranslator appTranslator;
    // TODO: change translation back after debugging
    appTranslator.load(QString(":/translations/main.%1.qm").arg(QLocale::system().name()));
    //appTranslator.load(QString(":/translations/main.%1.qm").arg("zh_CN"));
    app.installTranslator(&appTranslator);

    InitImageManager();
    qsrand((uint) QTime::currentTime().msec());
    MainWindow mainWindow;
    gMainView->switchToMenuScene();
    mainWindow.show();
    int res = app.exec();
    DestoryImageManager();
    return res;
}


