#ifndef MOTORPAGE_H
#define MOTORPAGE_H

#include <QMainWindow>
#include <QTimer>
#include <cmath>
#include <QThread>
#include "qcustomplot.h"
#include "./inc/zmotion.h"
#include "./inc/zmcaux.h"
#include "inc/Global.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QtSql>

extern ZMC_HANDLE g_handle;

namespace Ui {
class motorpage;
}

class motorpage : public QMainWindow
{
    Q_OBJECT

public:
    explicit motorpage(QWidget *parent = nullptr);
    ~motorpage();
    int timeReadAll = 200;

private slots:
    void ReadMotorParam(ZMC_HANDLE handle, int motorID, int type);
    void ReadMotorParam2(ZMC_HANDLE handle, int motorID, int type);

    void DrawMotorParamSelect();
    void DrawMotorParam(int motorID, int paramType);
    void ReadMotorParamAll(ZMC_HANDLE handle);
    void debugShowParaAll();

    void onParamsRead(QVector<float> torqueAll, QVector<float> speedAll, QVector<float> positionAll);
    void on_btn_nuke_clicked();
    void on_btn_showDB_clicked();

private:
    Ui::motorpage *ui;
    QCustomPlot *qcustomplot[10];
    float torqueAll[10] = {1,2,3,4,5,6,7,8,9,10};
    float positionAll[10] = {2,2,2,2,2,2,2,2,2,2};
    float speedAll[10] = {3,3,3,3,3,3,3,3,3,3};

    bool plotswitch[10];
    int  plottype[10];
    int  lastplottype[10];

    QTimer *ReadAlldataTimer;

    void InitDB(const QString &fileName);       // 初始化数据库
    int currentRoundID = 1;
    QSqlDatabase dbMotor;                // 存储数据的数据库

    // 保存所有图表的数据
    QMap<int, QVector<double>> xData;
    QMap<int, QVector<double>> yData;
    int Xstep[10];

    QDateTime startTime;            // 开始时间
    QDateTime stopTime;             // 结束时间


};

class ReadParamThread : public QThread
{
    Q_OBJECT

public:
    bool runStart = false;
signals:
    void paramsRead(QVector<float> torqueAllData, QVector<float> speedAllData, QVector<float> positionAllData);
protected:
    void run() override {
        while(true){
            while (runStart){
                // 获取循环开始的时间戳
//                auto startTime = std::chrono::high_resolution_clock::now();

                int ret;
                QVector<float> torqueAllData(10);
                QVector<float> speedAllData(10);
                QVector<float> positionAllData(10);

                ret =  ZAux_Direct_GetAllAxisPara(g_handle, "DRIVE_TORQUE", 10, torqueAllData.data());
                ret += ZAux_Direct_GetAllAxisPara(g_handle, "MSPEED",       10, speedAllData.data());
                ret += ZAux_Direct_GetAllAxisPara(g_handle, "MPOS",         10, positionAllData.data());

//                qDebug() << "Read All Param:" << ret;
                emit paramsRead(torqueAllData, speedAllData, positionAllData);
                QThread::msleep(100);
//                // 获取循环结束的时间戳
//                auto endTime = std::chrono::high_resolution_clock::now();

//                // 计算循环执行的时间（毫秒）
//                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
//                qDebug() << "Loop time:" << duration << "ms";
            }
            if(runStart != true)
                QThread::msleep(100);
        }
    }
};

#endif // MOTORPAGE_H
