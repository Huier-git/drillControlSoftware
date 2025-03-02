#ifndef VK701PAGE_H
#define VK701PAGE_H

#include <QWidget>
#include <QDebug>
#include <QLibrary>
#include <QThread>
#include <QTimer>
#include <QDateTime>

// 添加外部的库
#include "./inc/VK70xNMC_DAQ2.h"
#include "inc/vk701nsd.h"
#include "inc/qcustomplot.h"
#include "inc/Global.h"

// 添加Sqlite 数据库
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QtSql>

#include <QTableWidget>

namespace Ui {
class vk701page;
}

class vk701page : public QWidget
{
    Q_OBJECT

public:
    explicit vk701page(QWidget *parent = nullptr);
    ~vk701page();

    int currentRoundID = 0;                         // 读取的轮次，即按了几次停止采样
    bool startTimeflag = false;

private slots:
    void handleResultValue(QList<double> *list);

    void on_btn_start_2_clicked();

    void on_btn_stop_2_clicked();

    void InitDB(const QString &fileName);       // 初始化数据库

    void on_btn_showDB_clicked();               // 把数据库的内容显示到Table上

    void on_btn_deleteData_clicked();           // 根据RoundID来删除数据

    void on_btn_nuke_clicked();                 // 删除所有的数据

private:
    QSqlDatabase db;                // 存储数据的数据库
    QDateTime startTime;            // 开始时间
    QDateTime stopTime;             // 结束时间

private:
    Ui::vk701page *ui;
    QCustomPlot *qcustomplot[4];
    QThread *workerThread;          // Loop to read DAQ data
    vk701nsd *worker;
};

#endif // VK701PAGE_H
