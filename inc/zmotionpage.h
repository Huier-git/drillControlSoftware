#ifndef ZMOTIONPAGE_H
#define ZMOTIONPAGE_H

#include <QWidget>
#include <QDebug>
#include <QTimer>
#include <QDateTime>
#include <QList>
#include <QTableWidget>
#include <QRegularExpression>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QMessageBox>
#include "inc/zmcaux.h"
#include "inc/zmotion.h"
#include "inc/mdbprocess.h"
#include "inc/Global.h"

#define MAX_AXIS        20  //最大轴数
#define TOP_COUNT       1300000     //13000000 为最高点脉冲数
#define ROTATE_COUNT    850000      //850000对应120rpm


namespace Ui {
class zmotionpage;
}

class AutoModeThread;  // 前向声明

class zmotionpage : public QWidget
{
    Q_OBJECT

public:
    explicit zmotionpage(QWidget *parent = nullptr);
    ~zmotionpage();

    bool waitForConfirmation();
    void ShowMotorMap();

private slots:
    void on_btn_IP_Scan_clicked();

    void on_btn_IP_Connect_clicked();

    void on_btn_BusInit_clicked();

    void basicInfoRefresh();                                // 基础信息的刷新函数

    void advanceInfoRefreash();                             // 高级信息的刷新函数

    void initMotorTable();                                  // 初始化电机参数表格

    void modifyMotorTable(QTableWidgetItem *item);          // 找出修改电机表格的值

    void unmodifyMotorTable(int row, int column);

    void setMotorParm(int row, int col, QString value);     // 设置电机的值

    void PauseAllAxis();

    void ResumeAllAxis();

    void on_Btn_Enable_clicked();

    void on_Btn_ClearAlm_clicked();

    void on_Btn_setCurrZero_clicked();

    void RefreshTableContent();

    void RefreshTableRealTimeContent();

    void on_Btn_StopMove_clicked();

    void handleModbusCommand(const QString &cmd);

    void handleZmotion(const QString& cmd);

    void handleReceivedData(const QVector<quint16>& data, int startReg);


private slots:
    void on_btn_sendCmd_clicked();      //终端发送
    void on_btn_rapidStop_clicked();    //快速停止
    void runningMode();                 //切换运行模式
    void onAutoModeCompleted();
    void handleConfirmation();

    void on_btn_pipeConnect_clicked();  // 张开的连接、

signals:
    void confirmationReceived(bool isConfirmed);

private:
    bool initflag;                                          // 正常初始化的标志 0-未初始化，1-已初始化
    /////基础信息显示/////
    float fBusType;                                         // 总线类型
    float fInitStatus;                                      // 总线初始化状态
    int   iNodeNum;                                         // 总线上的节点数量
    /////对比信息//////
    QString oldCellValue;                                   // 表格中旧的值
    int     oldRow;
    int     oldCol;
    int     basicAxisCB;
    /////极限限制//////
    int     limitDACRotate;
    int     limitDACDownClamp;
    int     limitPosTop;
    int     limitPosDown;
    /////钻管撑开//////
    mdbprocess *mdbProcessor;

private:
    Ui::zmotionpage *ui;
    QTimer *basicInfoTimer;                                 // 用于定时刷新基础的信息的定时器
    QTimer *advanceInfoTimer;                               // 用于定时刷新高级的信息的定时器
    QTimer *realtimeParmTimer;
    QList<QTableWidgetItem*> tableItems;                    // 用于存储对象

private:
    AutoModeThread *m_autoModeThread;
    bool m_isAutoModeRunning;
    void startAutoMode();
    void stopAutoMode();
    // QMutex m_confirmationMutex;
    // QWaitCondition m_confirmationWaitCondition;
    // bool m_confirmationReceived;
    void setUIEnabled(bool enabled);


    int parseHexOrDec(const QString &str);

};

class AutoModeThread : public QThread
{
    Q_OBJECT
public:
    explicit AutoModeThread(QObject *parent = nullptr);
    ~AutoModeThread();

    void stop();

signals:
    void requestConfirmation(); // 请求用户确认
    void operationCompleted();  // 自动模式完成
    void requestShowMotorMap(); // 请求显示电机映射（如果需要）

public slots:
    void receiveConfirmation(bool confirmed); // 接收用户确认结果

protected:
    void run() override;

private:
    std::atomic<bool> m_stopFlag;
    QMutex m_mutex;
    QWaitCondition m_waitCondition;
    bool m_confirmed;

    // 禁用拷贝构造和赋值
    AutoModeThread(const AutoModeThread &) = delete;
    AutoModeThread &operator=(const AutoModeThread &) = delete;
};

#endif // ZMOTIONPAGE_H
