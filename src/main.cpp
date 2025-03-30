#include "inc/Global.h"
#include "inc/mainwindow.h"
#include <QApplication>

// 定义全局变量
bool AllRecordStart = false;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
} 