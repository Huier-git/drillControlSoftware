#include "inc/motorpage.h"
#include "ui_motorpage.h"
#include "zmcaux.h"
#include <QDebug>
#include <QThread>
#include <vector>  // 为 std::vector 添加头文件

// 定义全局变量
ZMC_HANDLE g_handle = nullptr;

motorpage::motorpage(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::motorpage)
{
    ui->setupUi(this);

    qRegisterMetaType<QVector<float>>("QVector<float>");

    // 连接数据库
    InitDB("/home/hui/workdir/VK701_Demo/db/motorsqlite.db");

    // 设置读取全部电机三环参数的定时器
    ReadAlldataTimer = new QTimer();
    ReadAlldataTimer->setInterval(timeReadAll);

    // 创建 ReadParamThread 实例
    ReadParamThread *workthread = new ReadParamThread();
    workthread->start();

    // 连接信号到槽函数，用于处理从线程中读取到的参数数据
    connect(workthread, &ReadParamThread::paramsRead, this, &motorpage::onParamsRead);

    for (int i = 0; i < 10; ++i) {
        qcustomplot[i] = this->findChild<QCustomPlot*>(QString("qcustomplot%1").arg(i+1));
        //这里可以对每个 QCustomPlot 对象进行其他初始化操作
        //设置X轴文字标注
        //qcustomplot[i]->xAxis->setLabel("Time");
        //设置Y轴文字标注
        //qcustomplot[i]->yAxis->setLabel("Current");
        qcustomplot[i]->yAxis->setNumberPrecision(2);     //显示位数
        qcustomplot[i]->addGraph();
    }

    connect(ui->btn_test1, &QPushButton::clicked, this, [=](){
        ReadMotorParam(g_handle, 0, 1);
        ReadMotorParam(g_handle, 0, 2);
        ReadMotorParam(g_handle, 0, 3);
    });

    connect(ui->btn_test2, &QPushButton::clicked, this, [=](){
        ReadMotorParam2(g_handle, 0, 1);
        ReadMotorParam2(g_handle, 0, 2);
        ReadMotorParam2(g_handle, 0, 3);
    });

    connect(ReadAlldataTimer, &QTimer::timeout, this, [=](){
        DrawMotorParamSelect();
    });

    connect(ui->btn_testReadAll, &QPushButton::clicked, this, [=](){
        if(ui->btn_testReadAll->text() == "TestReadAll")
        {
            ui->btn_testReadAll->setText("Stop");
            ReadAlldataTimer->start();

            workthread->runStart = true;

            startTime = QDateTime::currentDateTime();
            currentRoundID++;

            qDebug() << "Read All Start.";
        }
        else
        {
            ui->btn_testReadAll->setText("TestReadAll");
            workthread->runStart = false;
            ReadAlldataTimer->stop();
            //workthread->exit();

            // 记录停止的时间
            stopTime = QDateTime::currentDateTime();
            qint64 intervalTimeMS = startTime.msecsTo(stopTime);

            if(AllRecordStart == true)
            {
                // 插入TimeRecord表
                QSqlQuery query(dbMotor);
                QString insertStatement = "INSERT INTO TimeRecord (RoundID, TimeDiff) VALUES (:round, :timeDiff)";
                query.prepare(insertStatement);
                // 绑定参数
                query.bindValue(":round", currentRoundID);           // 插入的 Round 值
                query.bindValue(":timeDiff", intervalTimeMS);        // 插入的 TimeDiff 值
                // 执行 SQL 插入操作
                if (!query.exec())
                {
                    qDebug() << "Error inserting data into TimeRecord table:" << query.lastError().text();
                }
                else
                {
                    qDebug() << "Data inserted into TimeRecord table successfully.";
                }
                qDebug() << "Read All Stop";
            }

        }
    });

    // 设置界面上的 小曲线的 开关
    for (int i = 1; i <= 10; ++i) {
        QPushButton* currentButton = findChild<QPushButton*>(QString("btn_plot%1").arg(i));
        QLineEdit* currentLineEdit = findChild<QLineEdit*>(QString("le_plot%1").arg(i));
        if (currentButton && currentLineEdit)
        {
            connect(currentButton, &QPushButton::clicked, this, [=]() {
                int buttonIndex = currentButton->objectName().mid(8).toInt(); // 获取按钮的序号
                if (currentButton->text() == "Show")
                {
                    //获得currentButton的序号
                    plotswitch[buttonIndex - 1] = true;
                    plottype[buttonIndex - 1] = currentLineEdit->text().toInt();
                    if(lastplottype[buttonIndex - 1] != plottype[buttonIndex - 1])
                        Xstep[buttonIndex - 1] = 0;
                    lastplottype[buttonIndex - 1] = plottype[buttonIndex - 1];
                    currentButton->setText("Stop");
                } else {
                    //获得currentButton的序号
                    plotswitch[buttonIndex - 1] = false;
                    currentButton->setText("Show");
                }
            });
        }
    }

    // 读取数据库最后一行中的RoundID，并赋值给currentRoundID，保持连贯性
    QSqlQuery query("SELECT MAX(RoundID) FROM TimeRecord", dbMotor);
    if (query.exec() && query.first())
    {
        currentRoundID = query.value(0).toInt();
        qDebug() << "Last Max RoundID is " << currentRoundID;
    }
    else
    {
        qDebug() << "Failed to retrieve RoundID:" << query.lastError().text();
    }

    // 初始化电机参数数组
    m_motorParams.resize(m_axisNum, std::vector<float>(3, 0.0f));  // 3种参数类型
}

motorpage::~motorpage()
{
    delete ui;
}


/**
 * @brief 连接数据库
 * @param fileName
 */
void motorpage::InitDB(const QString &fileName)
{
    // 判断数据库是否存在
    bool dbExists = QFile::exists(fileName);
    // 如果数据库不存在，则创建数据库并初始化
    if (!dbExists)
    {
        QSqlDatabase dbMotor = QSqlDatabase::addDatabase("QSQLITE","motordata"); // 获取当前数据库连接
        dbMotor.setDatabaseName(fileName);
        if (!dbMotor.open())
        {
            qDebug() << "Error: Failed to connect database." << dbMotor.lastError();
            return;
        }
    }
    dbMotor = QSqlDatabase::addDatabase("QSQLITE","motordata"); // 获取当前数据库连接
    dbMotor.setDatabaseName(fileName);
    if (!dbMotor.open())
    {
        qDebug() << "Error: Failed to connect motordata database." << dbMotor.lastError();
        return;
    }
    qDebug() << "Connect to motordata database.";

    // 读取数据库最后一行中的RoundID，并赋值给currentRoundID，保持连贯性
    QSqlQuery query("SELECT MAX(RoundID) FROM TimeRecord", dbMotor);
    if (query.exec() && query.first())
    {
        currentRoundID = query.value(0).toInt();
        qDebug() << "Last Max Motordatabase RoundID is " << currentRoundID;
    }
    else
    {
        qDebug() << "Failed to retrieve RoundID:" << query.lastError().text();
    }
}


/**
 * @brief motorpage::SetAutoUp 上报参数的设置（读取命令式）
 * @param handle
 */
void motorpage::ReadMotorParam(void* handle, int axis, int type)
{
    if (!handle) return;
    
    float value = 0;
    switch(type) {
        case 0:
            ZAux_Direct_GetDpos(handle, axis, &value);
            break;
        case 1:
            ZAux_Direct_GetMpos(handle, axis, &value);
            break;
        case 2:
            ZAux_Direct_GetSpeed(handle, axis, &value);
            break;
    }
    // 确保索引在有效范围内
    if (axis < m_axisNum && type < 3) {
        m_motorParams[axis][type] = value;
    }
}

void motorpage::ReadMotorParam2(ZMC_HANDLE handle, int motorID, int type)
{
    int ret;
    float mspeed,mpos,torque;
    if(type == 1)
    {
        ret = ZAux_Direct_GetParam(handle, "DRIVE_TORQUE", MotorMap[motorID], &torque);
        qDebug() << "torque2:" << torque;
    }
    else if(type == 2)
    {
        ret = ZAux_Direct_GetParam(handle, "MSPEED", MotorMap[motorID], &mspeed);
        qDebug() << "mspeed2:" << mspeed;
    }
    else if(type == 3)
    {
        ret = ZAux_Direct_GetParam(handle, "MPOS", MotorMap[motorID], &mpos);
        qDebug() << "mpos2:" << mpos;
    }
    else
    {return;}
    qDebug() << "read2 type:" << type << "ret:" << ret;
}

void motorpage::ReadMotorParamAll(ZMC_HANDLE handle)
{
    int ret;
    ret = ZAux_Direct_GetAllAxisPara(handle, "DRIVE_TORQUE", 10, torqueAll);
    ret += ZAux_Direct_GetAllAxisPara(handle, "MSPEED", 10, speedAll);
    ret += ZAux_Direct_GetAllAxisPara(handle, "MPOS", 10, positionAll);
    //qDebug() << "Read All Param:" << ret;    //DEBug
}

void motorpage::DrawMotorParamSelect()
{
    //ReadMotorParamAll(handle);      // 读取一次全部轴的数据
    for (int i = 0; i < 10; ++i) {
        if (plotswitch[i] == 0) // 如果该plot开关关闭，则跳过
            continue;
        switch (plottype[i]) {
            case 1: // 力矩
                DrawMotorParam(i, 1);
                break;
            case 2: // 速度
                DrawMotorParam(i, 2);
                break;
            case 3: // 位置
                DrawMotorParam(i, 3);
                break;
            default:
                break;
        }
    }
}

void motorpage::DrawMotorParam(int motorID, int paramType)
{
    const int maxDataPoints = 100; // 设置最大数据点数量
        // 获取要添加的数据
        float paramValue = 0.0f;
        switch(paramType) {
            case 1: // 力矩
                paramValue = torqueAll[MotorMap[motorID]];
                //paramValue = sin(Xstep);
                break;
            case 2: // 速度
                paramValue = speedAll[MotorMap[motorID]];
                break;
            case 3: // 位置
                paramValue = positionAll[MotorMap[motorID]];
                //paramValue = sin(Xstep[motorID]);
                break;
            default:
                return; // 如果参数类型无效，直接返回
        }

    /// 获取该图表的数据向量
    QVector<double>& x = xData[motorID];
    QVector<double>& y = yData[motorID];

    // 如果数据点数量已经达到最大数量，移除最早的数据点
    if (x.size() >= maxDataPoints) {
        x.removeFirst();
        y.removeFirst();
    }

    // 将新数据添加到现有数据的末尾
    x.append(Xstep[motorID]);
    y.append((double)paramValue);

    if(Xstep[motorID] == 0)
    {
        x.clear();
        y.clear();
    }

    // 更新计数器
    Xstep[motorID]++;

    // 重新添加数据到图表中
    qcustomplot[motorID]->graph(0)->setData(x, y);

    // 设置X轴的范围，只显示最近的100个数据点
    qcustomplot[motorID]->xAxis->setRange(Xstep[motorID] - std::min((int)Xstep[motorID], maxDataPoints), (int)Xstep[motorID]);

    // 设置Y轴的范围
    double minY = *std::min_element(y.constBegin(), y.constEnd());
    double maxY = *std::max_element(y.constBegin(), y.constEnd());
    double margin = (maxY - minY) * 0.1; // 10% 的边距
    qcustomplot[motorID]->yAxis->setRange(minY - margin, maxY + margin);
    // 重新绘制图表
    qcustomplot[motorID]->replot();
}

void motorpage::debugShowParaAll()
{
    size_t torqueSize = sizeof(torqueAll) / sizeof(torqueAll[0]);
    for (size_t i = 0; i < torqueSize; ++i) {
        qDebug() << "torqueAll[" << i << "]:" << torqueAll[i];
    }

    size_t speedSize = sizeof(speedAll) / sizeof(speedAll[0]);
    for (size_t i = 0; i < speedSize; ++i) {
        qDebug() << "speedAll[" << i << "]:" << speedAll[i];
    }

    size_t positionSize = sizeof(positionAll) / sizeof(positionAll[0]);
    for (size_t i = 0; i < positionSize; ++i) {
        qDebug() << "positionAll[" << i << "]:" << positionAll[i];
    }
}

void motorpage::onParamsRead(QVector<float> dpos, QVector<float> mpos, QVector<float> speed)
{
    // 使用实际的轴数量
    for(int i = 0; i < m_axisNum && i < dpos.size(); i++) {
        updateMotorDisplay(i, dpos[i], mpos[i], speed[i]);
    }
}

void motorpage::on_btn_nuke_clicked()
{
    // 使用 prepared statements 防止 SQL 注入攻击
    QSqlQuery query(dbMotor);

    for (int i = 0; i < 10; ++i) {
        QString tableName = "Motordata" + QString::number(i);
        query.prepare("DELETE FROM " + tableName);
        query.exec();

        query.prepare("DELETE FROM sqlite_sequence WHERE name='RoundID'");
        query.prepare("DELETE FROM sqlite_sequence WHERE name='Current'");
        query.prepare("DELETE FROM sqlite_sequence WHERE name='Velocity'");
        query.prepare("DELETE FROM sqlite_sequence WHERE name='Position'");
        query.exec();
        // 执行 VACUUM 命令
        query.prepare("VACUUM");
        query.exec();

    }

    // 提交删除操作，然后执行 VACUUM
    dbMotor.commit();

    // 执行 VACUUM 命令
    query.prepare("VACUUM");
    query.exec();

    // 判断 SQL 操作是否成功
    if (query.lastError().isValid()) {
        qDebug() << "Failed to delete all data:" << query.lastError().text();
        return;
    }
    currentRoundID = 0;
    qDebug() << "All motordata deleted successfully.";
}


void motorpage::on_btn_showDB_clicked()
{
    // 判断范围的条件，如果不符合范围的条件直接返回
    bool ok;
    int start = ui->le_rage_start->text().toInt(&ok);
    int end = ui->le_rage_end->text().toInt(&ok);
    if(!ok || start > end || start < 0 || end < 0)
    {
        qDebug() << "Range Error";
        return;
    }

    qDebug() << "Range:" << start << "-" << end;
    // 符合条件则进行SQL的查询

    // 1.输入对象前先清空已经有的数据
    ui->tb_motordata->setRowCount(0);

    // 2.根据范围，将符合范围的数据全部显示
    int row = 0;
    QSqlQuery query(dbMotor);
    int motorid = ui->le_Motor->text().toInt();
    if (query.exec(QString("SELECT * FROM Motordata%1 WHERE rowid BETWEEN %2 AND %3").arg(motorid).arg(start).arg(end)))            // 查询数据库的命令
    {
        QSqlRecord record = query.record();
        int count = record.count();
        qDebug() << "record.count:" << count;
        ui->tb_motordata->setColumnCount(count);
        ui->tb_motordata->setHorizontalHeaderLabels({"RoundID" , "Current" , "Velocity" , "Position"}); // 设置表头
        // 输出查询结果
        while (query.next())
        {
            ui->tb_motordata->insertRow(row);
            for(int var = 0; var < count; ++var)
            {
                ui->tb_motordata->setItem(row,var,new QTableWidgetItem(query.value(var).toString()));
            }
            row++;
        }
    }
    else
    {
        qDebug() << "Query failed." << query.lastError().text();
    }
    // 3.查询所有的数量并显示
    query.prepare(QString("SELECT COUNT(*) FROM Motordata%1").arg(motorid));
    if (query.exec()) {
        if (query.next()) {
            int rowCount = query.value(0).toInt();
            qDebug() << "Number of rows in the table:" << rowCount;
            ui->label_Num->setText(QString::number(rowCount));;
        }
    } else {
        qDebug() << "Query failed:" << query.lastError().text();
    }

}

void motorpage::updateMotorDisplay(int axis, float dpos, float mpos, float speed)
{
    // 实现更新显示的逻辑
    // 例如：更新UI上对应的显示元素
    if (axis >= 0 && axis < m_axisNum) {
        // 这里添加更新UI的代码
        // 例如：
        /*
        QString axisPrefix = QString("le_axis%1_").arg(axis);
        QLineEdit* dposEdit = findChild<QLineEdit*>(axisPrefix + "dpos");
        QLineEdit* mposEdit = findChild<QLineEdit*>(axisPrefix + "mpos");
        QLineEdit* speedEdit = findChild<QLineEdit*>(axisPrefix + "speed");
        
        if (dposEdit) dposEdit->setText(QString::number(dpos));
        if (mposEdit) mposEdit->setText(QString::number(mpos));
        if (speedEdit) speedEdit->setText(QString::number(speed));
        */
    }
}
