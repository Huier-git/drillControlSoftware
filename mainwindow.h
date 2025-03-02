#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include "inc/vk701nsd.h"
#include "inc/qcustomplot.h"

#include "inc/motorpage.h"
#include "inc/vk701page.h"
#include "inc/zmotionpage.h"
#include "inc/mdbtcp.h"

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
