#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include "vk701nsd.h"
#include "qcustomplot.h"

#include "motorpage.h"
#include "vk701page.h"
#include "zmotionpage.h"
#include "mdbtcp.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    motorpage *ppagemotor;       //实例化指针
    vk701page *ppagevk701;
    zmotionpage *ppagezmotion;
    MdbTCP *mdbtcp;





private slots:
    void checkThreadStatus();

private:
    Ui::MainWindow *ui;
    QThread *workerThread;
    vk701nsd *worker;
    QCustomPlot *qcustomplot[4];
    QTimer *debugtimer;
    QDateTime UnistartTime;            // 统一的采集传感器的开始时间
    QDateTime UnistopTime;
};
#endif // MAINWINDOW_H
