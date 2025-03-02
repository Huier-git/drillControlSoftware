#ifndef MDBTCP_H
#define MDBTCP_H

#include <QWidget>
#include <QDebug>
#include <QThread>
#include <QTimer>
// 添加Sqlite 数据库
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QtSql>
#include <QTimer>
#include <QTableWidget>
#include <QProcess>
#include "inc/mdbprocess.h"
#include "inc/Global.h"

//解决 Python 和 Qt 的关键词 slots 冲突
#pragma push_macro("slots")
#undef slots
#include <Python.h>
#pragma pop_macro("slots")

namespace Ui {
class MdbTCP;
}

class MdbTCP : public QWidget
{
    Q_OBJECT

public:
    explicit MdbTCP(QWidget *parent = nullptr);
    ~MdbTCP();
    int  timeTract      = 100;       //默认100ms
    int  timeTorque     = 100;
    int  timePosition   = 100;
private slots:
    void ShowLCDtraction(long data, int reg);

    void ShowLCDtorque(long data, int reg);

    void ShowLCDposition(long data, int reg);

    void on_btn_nuke_clicked();

    void on_btn_showDB_clicked();

    void on_btn_deleteData_clicked();

    void on_btn_mdbShow_clicked();

signals:
    //void tractionLCDshow(int64_t data, int reg);

private:
    int portPressure;
    int portTorque;
    //void ReadValue(int mdbport, int mdbID, int reg, int num, bool is2complement);
    void InitDB(const QString &fileName);       // 初始化数据库
    void closeEvent(QCloseEvent *event);
    void SetZero();

private:
    Ui::MdbTCP *ui;
    //QModbusClient *modbusDevices[4] = {nullptr};
    QThread *mdbThread;
    mdbprocess *mdbworker;
    int currentRoundID = 1;
    QSqlDatabase dbModbus;                // 存储数据的数据库
    QDateTime startTime;            // 开始时间
    QDateTime stopTime;             // 结束时间

    double traction1zero = 0;
    double traction2zero = 0;
    double torqueZero    = 0;
    double positionZero  = 0;
};

#endif // MDBTCP_H
