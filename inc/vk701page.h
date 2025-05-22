#ifndef VK701PAGE_H
#define VK701PAGE_H

#include <QWidget>
#include <QDebug>
#include <QLibrary>
#include <QThread>
#include <QTimer>
#include <QDateTime>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <QMutex>
#include <QApplication>
#include <algorithm>
#include <QMessageBox>

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

    int currentRoundID = 0;                 // 当前采样轮次
    bool startTimeflag = false;             // 开始时间标志, initialized here
    bool AllRecordStart = false;            // 全局数据记录标志, initialized here

public slots:
    // 数据库初始化与操作
    void InitDB(const QString &fileName);           // 初始化数据库
    void saveDataToDatabase(const QVector<double> &data, int channels, int pointsPerChannel); // 异步保存数据
    void cleanupOldData(int keepLastNRounds);      // 清理旧数据，保留最近N轮
    
    // 新增: 处理数据采集卡状态变化
    void handleStateChanged(DAQState newState);

private slots:
    // 处理数据采集卡获取的数据
    void handleResultValue(QVector<double> *list);
    
    // UI按钮事件处理
    void on_btn_start_2_clicked();          // 开始按钮
    void on_btn_stop_2_clicked();           // 停止按钮
    void on_btn_showDB_clicked();           // 显示数据库内容
    void on_btn_deleteData_clicked();       // 按轮次删除数据
    void on_btn_nuke_clicked();             // 删除所有数据
    void on_btn_nextPage_clicked();         // 数据库下一页
    void on_btn_prevPage_clicked();         // 数据库上一页
    
    // 新增: 退出按钮处理 (示例方法，您需要添加相应的按钮)
    void on_btn_exit_clicked();
    
    // 新增: 处理消息结果
    void handleResultMsg(QString msg);

    // 定时更新UI
    void updatePlots();                      // 更新图表
    
    // 新增: 关闭事件处理 (优雅退出)
    void closeEvent(QCloseEvent *event) override;


private:
    // 数据库相关
    QSqlDatabase db;                        // 数据库连接
    int db_currentPage;                     // 当前数据库显示页码
    int db_totalPages;                      // 总数据库页码
    const int db_pageSize = 100;            // 每页显示条目数
    int db_filter_minRoundID;               // 当前数据库查询的最小RoundID
    int db_filter_maxRoundID;               // 当前数据库查询的最大RoundID

    void loadDbPageData();                  // 加载数据库指定页的数据
    QDateTime startTime;                    // 开始时间
    QDateTime stopTime;                     // 结束时间
    
    // 批量插入相关
    QList<QVariantList> batchData;          // 批量插入缓冲
    // int batchSize = 1000;                // Unused variable, removed
    QTimer *dbCommitTimer;                  // 定时提交数据库
    
    // UI相关
    Ui::vk701page *ui;
    QCustomPlot *qcustomplot[4];            // 四个通道的绘图对象
    
    // 线程相关
    QThread *workerThread;                  // 数据采集线程
    vk701nsd *worker;                       // 数据采集对象
    QTimer *plotUpdateTimer;                // 图表更新定时器
    
    // 数据缓冲
    QVector<double> currentData;            // 当前显示的数据
    QMutex dataMutex;                       // 数据互斥锁
    
    // 绘图性能优化
    int plotUpdateInterval = 100;           // 绘图更新间隔(ms)
    bool needPlotUpdate = false;            // 是否需要更新绘图
    
    // 新增: 安全关闭工作线程
    void safelyShutdownWorker();

    Q_DISABLE_COPY_MOVE(vk701page)
};

#endif // VK701PAGE_H