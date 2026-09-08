#include <QApplication>
#include <QLocale>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QLocale::setDefault(QLocale(QLocale::Russian, QLocale::Russia));

    QApplication app(argc, argv);
    app.setApplicationName("TagBuilder");
    app.setApplicationDisplayName("TagBuilder");

    MainWindow window;
    window.show();
    return app.exec();
}