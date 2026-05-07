#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QSettings settings("settings.ini", QSettings::IniFormat);

    int ppi = settings.value("Display/calibratedDPI", 0).toInt();
    int width = settings.value("Display/width", 0).toInt();
    int height = settings.value("Display/height", 0).toInt();
    double diagonal = settings.value("Display/diagonal", 0.0).toDouble();

    bool ppiFlag = false;

    if (ppi > 0)
    {
        qDebug() << "Custom screen calibration found:";
        qDebug() << "Resolution:" << width << "x" << height
                 << "Diagonal:" << diagonal << "in"
                 << "DPI:" << ppi;

        qputenv("QT_FONT_DPI", QByteArray::number(ppi));
        ppiFlag = true;

    } else {
        qDebug() << "Using system default DPI";
    }

    MainWindow w;

    if(ppiFlag)
    {
        w.writeToNotes(QString("Found Custom Screen : %1 : width, %2 : height, %3 : diagonal, %4 : ppi")
                       .arg(width).arg(height).arg(diagonal).arg(ppi));
    }
    else
    {
        w.writeToNotes("Using system default DPI");
    }


    w.show();
    return a.exec();
}

