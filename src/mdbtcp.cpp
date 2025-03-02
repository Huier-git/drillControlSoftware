#include "inc/mdbtcp.h"
#include "ui_mdbtcp.h"

float downForce = 0;

MdbTCP::MdbTCP(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MdbTCP)
{
    ui->setupUi(this);
    ui->btn_readStart->setEnabled(false);

    // 设置小数点后的位数为2
    ui->lcd_position->setDigitCount(5); // 小数点后2位 + 小数点 + 整数位 = 5位数字
    // 设置显示模式为浮点数
    ui->lcd_position->setMode(QLCDNumber::Dec); // 小数点模式

    // 连接数据库
    InitDB("/home/hui/workdir/VK701_Demo/db/mdbsqlite.db");
    //currentRoundID = 0;
    // 设置表头自适应
    QHeaderView *header = ui->tb_Force->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Stretch);
    header = ui->tb_Torque->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Stretch);
    header = ui->tb_Position->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Stretch);

    // 设置表格的大小策略为 Expanding
    ui->tb_Force->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->tb_Torque->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->tb_Position->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->tb_Force->setSortingEnabled(true);  //启动排序

    connect(ui->btn_cleancmd, &QPushButton::clicked, [=](){
        ui->tb_cmdWindow->clear();
    });

    mdbThread = new QThread();
    mdbworker = new mdbprocess();
    mdbworker->moveToThread(mdbThread);
    mdbThread->start();

    //QThread <=> QObject
    connect(mdbThread, SIGNAL(finished()), mdbworker, SLOT(deleteLater()));
    connect(mdbThread, SIGNAL(finished()), mdbThread, SLOT(deleteLater()));

    //QObject <=> mdbTCP
    // 拉力传感器数值显示到LCD上
    connect(mdbworker, &mdbprocess::tractionLCDshow, this, &MdbTCP::ShowLCDtraction);
    // 扭矩传感器数值显示到LCD上
    connect(mdbworker, &mdbprocess::torqueLCDshow, this, &MdbTCP::ShowLCDtorque);
    // 拉力传感器数值显示到LCD上
    connect(mdbworker, &mdbprocess::positionLCDshow, this, &MdbTCP::ShowLCDposition);


    // 连接modbus网关
    connect(ui->btn_connect, &QPushButton::clicked, mdbworker, [=](){
        if(ui->btn_connect->text() == "Connect")
        {
            QString addr = ui->le_mdbIP->text();
            int port = ui->le_mdbPort->text().toInt();
            //QMetaObject::invokeMethod(mdbworker, "TCPConnect",
                                      //Qt::QueuedConnection, Q_ARG(int, port), Q_ARG(QString, addr));
            mdbworker->TCPConnect(port, addr);
            if(mdbworker->connectStatus == true)
            {
                ui->btn_connect->setText("Disconnect");
                ui->btn_readStart->setEnabled(true);
            }
        }
        else
        {
            mdbworker->TCPDisconnect();             // 断开连接
            ui->btn_connect->setText("Connect");
            ui->btn_readStart->setEnabled(false);
        }
    });
    // 暂停定时器
    connect(ui->btn_connect, &QPushButton::clicked, this, [=](){
        if(ui->btn_connect->text() != "Connect")
        {
            mdbworker->setReadtractionTimer(false, 100);
        }
    });
    // 测试用
    connect(ui->btn_test, &QPushButton::clicked, mdbworker, [=](){
        if(mdbworker->connectStatus == false)
        {
            return;
        }
    });

    // 设置modbusPort和传感器的映射
    connect(ui->btn_readStart, &QPushButton::clicked, this, [=](){
        if(ui->btn_readStart->text() == "Start")
        {
            ui->btn_readStart->setText("Stop");
            mdbworker->Forcemdbport = ui->le_forceCh->text().toInt();       //拉力的PortID
            mdbworker->Torquemdbport = ui->le_torqueCh->text().toInt();     //扭矩的PortID
            mdbworker->Poitionmdbport = ui->le_postionCh->text().toInt();   //位置的PortID

            if(ui->cb_traON->isChecked())
                mdbworker->setReadtractionTimer(true, timeTract);
            if(ui->cb_torON->isChecked())
                mdbworker->setReadtorqueTimer(true, timeTorque);
            if(ui->cb_posON->isChecked())
                mdbworker->setReadpositionTimer(true, timePosition);

            if(ui->cb_traON->isChecked() || ui->cb_torON->isChecked() || ui->cb_posON->isChecked()){
                startTime = QDateTime::currentDateTime();
                currentRoundID++;
            }
        }
        else
        {
            ui->btn_readStart->setText("Start");
            mdbworker->setReadtractionTimer(false, timeTract);
            mdbworker->setReadtorqueTimer(false, timeTorque);
            mdbworker->setReadpositionTimer(false, timePosition);

            if(!(ui->cb_traON->isChecked() || ui->cb_torON->isChecked() || ui->cb_posON->isChecked()))
            {
                ui->tb_cmdWindow->append("No param checked.");
                return;
            }
            // 记录停止的时间
            stopTime = QDateTime::currentDateTime();
            qint64 intervalTimeMS = startTime.msecsTo(stopTime);

            if(AllRecordStart == true)
            {
                // 插入TimeRecord表
                QSqlQuery query(dbModbus);
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
            }

        }
    });

    // 设置零点功能
    connect(ui->btn_setzero, &QPushButton::clicked, this, [=](){
        SetZero();
    });
}

MdbTCP::~MdbTCP()
{
    mdbThread->quit();
    mdbThread->wait();
    delete ui;
}

void MdbTCP::SetZero()
{
    traction1zero = ui->lcd_top->value();
    traction2zero = ui->lcd_down->value();
    torqueZero    = ui->lcd_torque->value();
    positionZero  = ui->lcd_position->value();
    startTime = QDateTime::currentDateTime();
    qDebug()<< "set zero" ;
}


/**
* @brief 拉力显示到LCD上
* @param data
* @param reg
*/
void MdbTCP::ShowLCDtraction(long data, int reg)
{
    QString values;
    double data1 = data * 0.00981;
    if(reg == 450)
    {
        ui->lcd_top->display(data1-traction1zero);
        values = QString("(%1, %2, %3)").arg(currentRoundID).arg(1).arg(data1-traction1zero);
    }else if(reg == 452)
    {
        ui->lcd_down->display(data1-traction2zero);
        values = QString("(%1, %2, %3)").arg(currentRoundID).arg(2).arg(data1-traction2zero);
        downForce = data1;
        //qDebug()<<"DOWN"<<downForce;
    }
    else
    {
        return;
    }
    if(AllRecordStart != true)
    {
        return;
    }
    QSqlQuery query(dbModbus);
    QString insertQuery = "INSERT INTO Forcedata (RoundID, ChID, ForceData) VALUES ";
    // 拼接完整的 SQL 插入语句
    insertQuery += values;
    // 执行 SQL 插入操作
    if (!query.exec(insertQuery))
    {
        qDebug() << "Error inserting [F]data into database:" << query.lastError().text();
        return;
    }
}

/**
 * @brief 扭矩显示到LCD上
 * @param data
 * @param reg
 */
void MdbTCP::ShowLCDtorque(long data, int reg)
{

    QString values;
    float data1 = data * 0.01;
    if(reg == 0x00)         //扭矩值
    {
        ui->lcd_torque->display(data1-torqueZero);
        values = QString("(%1, %2)").arg(currentRoundID).arg(data1-torqueZero);
    }
    else
    {
        return;
    }
    if(AllRecordStart != true)
    {
        return;
    }
    QSqlQuery query(dbModbus);
    QString insertQuery = "INSERT INTO Torquedata (RoundID, TorData) VALUES ";
    // 拼接完整的 SQL 插入语句
    insertQuery += values;
    // 执行 SQL 插入操作
    if (!query.exec(insertQuery))
    {
        qDebug() << "Error inserting [T]data into database:" << query.lastError().text();
        return;
    }
}

/**
 * @brief 位置显示到LCD上
 * @param data
 * @param reg
 */
void MdbTCP::ShowLCDposition(long data, int reg)
{
    QString values;
    float data1;
    if(data < 0)
    {
        data1 = 2*32767 + data; //修正负数
    }
    else
    {
        data1 = data;
    }
    data1 = data1 * 150/4096;
    if(reg == 0x00)         //位置值
    {
        ui->lcd_position->display(data1-positionZero);
        values = QString("(%1, %2)").arg(currentRoundID).arg(data1-positionZero);
    }
    else
    {
        return;
    }
    if(AllRecordStart != true)
    {
        return;
    }
    QSqlQuery query(dbModbus);
    QString insertQuery = "INSERT INTO Positiondata (RoundID, PosData) VALUES ";
    // 拼接完整的 SQL 插入语句
    insertQuery += values;
    // 执行 SQL 插入操作
    if (!query.exec(insertQuery))
    {
        qDebug() << "Error inserting [P]data into database:" << query.lastError().text();
        return;
    }
}

/**
 * @brief 连接数据库
 * @param fileName
 */
void MdbTCP::InitDB(const QString &fileName)
{
    // 判断数据库是否存在
    bool dbExists = QFile::exists(fileName);
    // 如果数据库不存在，则创建数据库并初始化
    if (!dbExists)
    {
        QSqlDatabase dbModbus = QSqlDatabase::addDatabase("QSQLITE","mdbtcp"); // 获取当前数据库连接
        dbModbus.setDatabaseName(fileName);
        if (!dbModbus.open())
        {
            qDebug() << "Error: Failed to connect database." << dbModbus.lastError();
            return;
        }
    }
    dbModbus = QSqlDatabase::addDatabase("QSQLITE","mdbtcp"); // 获取当前数据库连接
    dbModbus.setDatabaseName(fileName);
    if (!dbModbus.open())
    {
        qDebug() << "Error: Failed to connect database." << dbModbus.lastError();
        return;
    }
    qDebug() << "Connect to mdbtcp database.";

    // 读取数据库最后一行中的RoundID，并赋值给currentRoundID，保持连贯性
    QSqlQuery query("SELECT MAX(RoundID) FROM TimeRecord", dbModbus);
    if (query.exec() && query.first())
    {
        currentRoundID = query.value(0).toInt();
        qDebug() << "Last Max RoundID is " << currentRoundID;
    }
    else
    {
        qDebug() << "Failed to retrieve RoundID:" << query.lastError().text();
    }
}





void MdbTCP::on_btn_nuke_clicked()
{
    // 使用 prepared statements 防止 SQL 注入攻击
    QSqlQuery query(dbModbus);
    query.prepare("DELETE FROM Forcedata");
    query.exec();

    query.prepare("DELETE FROM Torquedata");
    query.exec();

    query.prepare("DELETE FROM TimeRecord");
    query.exec();

    query.prepare("DELETE FROM Positiondata");
    query.exec();

    query.prepare("DELETE FROM sqlite_sequence WHERE name='Forcedata'");
    query.exec();

    query.prepare("DELETE FROM sqlite_sequence WHERE name='Torquedata'");
    query.exec();

    query.prepare("DELETE FROM sqlite_sequence WHERE name='TimeRecord'");
    query.exec();

    query.prepare("DELETE FROM sqlite_sequence WHERE name='Positiondata'");
    query.exec();

    // 提交删除操作，然后执行 VACUUM
    dbModbus.commit();

    // 执行 VACUUM 命令
    query.prepare("VACUUM");
    query.exec();

    // 判断 SQL 操作是否成功
    if (query.lastError().isValid()) {
        qDebug() << "Failed to delete all data:" << query.lastError().text();
        return;
    }
    currentRoundID = 0;
    qDebug() << "All data deleted successfully.";
}


void MdbTCP::on_btn_showDB_clicked()
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

    /// [1] Forcedata ///
    // 1.输入对象前先清空已经有的数据
    ui->tb_Force->setRowCount(0);

    // 2.根据范围，将符合范围的数据全部显示
    int row = 0;
    QSqlQuery query(dbModbus);
    if (query.exec(QString("SELECT * FROM Forcedata WHERE rowid BETWEEN %1 AND %2").arg(start).arg(end)))            // 查询数据库的命令
    {
        QSqlRecord record = query.record();
        int count = record.count();
        qDebug() << "record.count:" << count;
        ui->tb_Force->setColumnCount(count);
        ui->tb_Force->setHorizontalHeaderLabels({"RoundID" , "ChID" , "ForceData"}); // 设置表头
        // 输出查询结果
        while (query.next())
        {
            ui->tb_Force->insertRow(row);
            for(int var = 0; var < count; ++var)
            {
                ui->tb_Force->setItem(row,var,new QTableWidgetItem(query.value(var).toString()));
            }
            row++;
        }
    }
    else
    {
        qDebug() << "Query failed." << query.lastError().text();
    }
    // 3.查询所有的数量并显示
    query.prepare("SELECT COUNT(*) FROM Forcedata");
    if (query.exec()) {
        if (query.next()) {
            int rowCount = query.value(0).toInt();
            qDebug() << "Number of rows in the table:" << rowCount;
            ui->label_ForceNum->setText(QString::number(rowCount));;
        }
    } else {
        qDebug() << "Query failed:" << query.lastError().text();
    }

    /// [2] Torquedata ///
    // 1.输入对象前先清空
    ui->tb_Torque->setRowCount(0);
    row = 0;
    // 2.根据范围，将符合范围的数据全部显示
    if (query.exec(QString("SELECT * FROM Torquedata WHERE rowid BETWEEN %1 AND %2").arg(start).arg(end)))            // 查询数据库的命令
    {
        QSqlRecord record = query.record();
        int count = record.count();
        qDebug() << "record.count:" << count;
        ui->tb_Torque->setColumnCount(count);
        ui->tb_Torque->setHorizontalHeaderLabels({"RoundID" , "TorData"}); // 设置表头
        // 输出查询结果
        while (query.next())
        {
            ui->tb_Torque->insertRow(row);
            for(int var = 0; var < count; ++var)
            {
                ui->tb_Torque->setItem(row,var,new QTableWidgetItem(query.value(var).toString()));
            }
            row++;
        }
    }
    else
    {
        qDebug() << "Query failed." << query.lastError().text();
    }
    // 3.查询所有的数量并显示
    query.prepare("SELECT COUNT(*) FROM Torquedata");
    if (query.exec()) {
        if (query.next()) {
            int rowCount = query.value(0).toInt();
            qDebug() << "Number of rows in the table:" << rowCount;
            ui->label_TorqueNum->setText(QString::number(rowCount));;
        }
    } else {
        qDebug() << "Query failed:" << query.lastError().text();
    }

    /// [3] Positiondata ///
    // 1.输入对象前先清空
    ui->tb_Position->setRowCount(0);
    row = 0;
    // 2.根据范围，将符合范围的数据全部显示
    if (query.exec(QString("SELECT * FROM Positiondata WHERE rowid BETWEEN %1 AND %2").arg(start).arg(end)))            // 查询数据库的命令
    {
        QSqlRecord record = query.record();
        int count = record.count();
        qDebug() << "record.count:" << count;
        ui->tb_Position->setColumnCount(count);
        ui->tb_Position->setHorizontalHeaderLabels({"RoundID" , "PosData"}); // 设置表头
        // 输出查询结果
        while (query.next())
        {
            ui->tb_Position->insertRow(row);
            for(int var = 0; var < count; ++var)
            {
                ui->tb_Position->setItem(row,var,new QTableWidgetItem(query.value(var).toString()));
            }
            row++;
        }
    }
    else
    {
        qDebug() << "Query failed." << query.lastError().text();
    }
    // 3.查询所有的数量并显示
    query.prepare("SELECT COUNT(*) FROM Positiondata");
    if (query.exec()) {\
        if (query.next()) {
            int rowCount = query.value(0).toInt();
            qDebug() << "Number of rows in the table:" << rowCount;
            ui->label_PositonNum->setText(QString::number(rowCount));;
        }
    } else {
        qDebug() << "Query failed:" << query.lastError().text();
    }
}


void MdbTCP::on_btn_deleteData_clicked()
{
    // 提取需要清空的论次
    int round = ui->spinBox_round->value();
    // 1.输入对象前先清空 table_vibDB 已经有的数据
    ui->tb_Force->setRowCount(0);
    ui->tb_Torque->setRowCount(0);
    ui->tb_Position->setRowCount(0);
    // 2.删除选中范围内的数据
    QSqlQuery query (dbModbus);
    // 构造删除数据的 SQL 语句
    QString deleteQuery = QString( "DELETE FROM Forcedata WHERE RoundID = %1; "
                                   "DELETE FROM Torquedata WHERE RoundID = %1; "
                                   "DELETE FROM Positiondata WHERE RoundID = %1").arg(round);
    // 执行 SQL 删除操作
    if (query.exec(deleteQuery))
    {
        qDebug() << "Data deleted successfully.";
    }
    else
    {
        qDebug() << "Failed to delete data:" << query.lastError().text();
    }
    // 删除数据后，更新显示的数据
    on_btn_showDB_clicked();
}


QProcess *pythonProcess = nullptr;
// 调用Python程序实现数据库的可视化
void MdbTCP::on_btn_mdbShow_clicked()
{
    if (pythonProcess && pythonProcess->state() == QProcess::Running) {
        qDebug() << "Python process is already running.";
        return;
    }

    QString pythonExecutable = "python3";  // 或者是您系统中 Python 可执行文件的路径
    QString scriptPath = QDir::currentPath() + "/py/mdb.py";
    int roundID = ui->sb_mdbShow->value();

    QStringList arguments;
    arguments << scriptPath << QString::number(roundID);

    pythonProcess = new QProcess(this);

    connect(pythonProcess, &QProcess::readyReadStandardOutput, [=]() {
        QString output = pythonProcess->readAllStandardOutput();
        qDebug() << "Python output:" << output.trimmed();
    });

    connect(pythonProcess, &QProcess::readyReadStandardError, [=]() {
        QString error = pythonProcess->readAllStandardError();
        qDebug() << "Python error:" << error.trimmed();
    });

    connect(pythonProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [=](int exitCode, QProcess::ExitStatus exitStatus) {
        qDebug() << "Python process finished with exit code:" << exitCode;
        pythonProcess->deleteLater();
        pythonProcess = nullptr;
    });

    qDebug() << "Starting Python process...";
    pythonProcess->start(pythonExecutable, arguments);
}

// 在窗口关闭或应用退出时，确保结束 Python 进程
void MdbTCP::closeEvent(QCloseEvent *event)
{
    if (pythonProcess && pythonProcess->state() == QProcess::Running) {
        qDebug() << "Terminating Python process...";
        pythonProcess->terminate();
        if (!pythonProcess->waitForFinished(3000)) {  // 等待 3 秒
            qDebug() << "Force killing Python process...";
            pythonProcess->kill();  // 如果进程没有及时结束，强制结束它
        }
    }
    QWidget::closeEvent(event);
}

