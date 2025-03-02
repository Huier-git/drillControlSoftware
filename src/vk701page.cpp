#include "inc/vk701page.h"
#include "ui_vk701page.h"


const QColor color[4] = {Qt::darkRed, Qt::darkGreen, Qt::darkBlue, Qt::darkYellow};     // Plot color

vk701page::vk701page(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::vk701page)
{
    ui->setupUi(this);

    // 连接数据库
    InitDB("/home/hui/workdir/VK701_Demo/db/vibsqlite.db");

    // 设置表头自适应
    QHeaderView *header = ui->table_vibDB->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Stretch);
    // 设置表格的大小策略为 Expanding
    ui->table_vibDB->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 初始化qcustomplot控件
    qcustomplot[0] = ui->qcustomplot1_2;
    qcustomplot[1] = ui->qcustomplot2_2;
    qcustomplot[2] = ui->qcustomplot3_2;
    qcustomplot[3] = ui->qcustomplot4_2;
    for (int i = 0; i < 4; i++)
    {
        QVector<double> x(1000), y(1000);
        for(int j = 0; j < 1000; j++)
        {
            x[j] = j;
            y[j] = 0;
        }
        //添加图形
        qcustomplot[i]->addGraph();
        //设置画笔
        qcustomplot[i]->graph(0)->setPen(QPen(color[i]));
        //传入数据，setData的两个参数类型为double
        qcustomplot[i]->graph(0)->setData(x, y);
        //设置X轴文字标注
        qcustomplot[i]->xAxis->setLabel("mSec");
        //设置Y轴文字标注
        qcustomplot[i]->yAxis->setLabel("mV");
        //设置X轴坐标范围
        qcustomplot[i]->xAxis->setRange(0, 1000);
        //设置Y轴坐标范围
        qcustomplot[i]->yAxis->setRange(-2000, 2000);
        //在坐标轴右侧和上方画线，和X/Y轴一起形成一个矩形
        qcustomplot[i]->axisRect()->setupFullAxesBox();
        qcustomplot[i]->replot();
    }

    // 创建单独的数据读取线程
    workerThread = new QThread();
    worker = new vk701nsd();
    worker->moveToThread(workerThread);

    //QThread <=> QObject
    connect(workerThread, SIGNAL(started()), worker, SLOT(doWork()) );
    connect(workerThread, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(workerThread, SIGNAL(finished()), workerThread, SLOT(deleteLater()));

    //QObject <=> vk701page
    connect(worker, &vk701nsd::resultValue, this, &vk701page::handleResultValue);
}

vk701page::~vk701page()
{
    workerThread->quit();
    workerThread->wait();
    delete ui;
}

void vk701page::handleResultValue(QList<double> *list)
{
    // 记录开始的时间
    if(startTimeflag != false)
    {
        startTime = QDateTime::currentDateTime();
        startTimeflag = false;
    }
    int size = list->size();

    std::unique_ptr<double[][4]> channel(new double[size / 4][4]);          // 使用智能指针代替new的操作
    //double (*channel)[4] = new double[size / 4][4];

    for (int i = 0; i < size / 4; i++)
    {
        channel[i][0] =list->value(4*i+0);
        channel[i][1] =list->value(4*i+1);
        channel[i][2] =list->value(4*i+2);
        channel[i][3] =list->value(4*i+3);
    }

    list->clear();

    if(AllRecordStart == true)//当全局采集标志打开则保存数据！
    {
    /// 存入sqlite数据库
    // 准备插入数据的 SQL 语句
        QSqlQuery query(db);
        QString insertQuery = "INSERT INTO IEPEdata (RoundID, ChID, VibrationData) VALUES ";
        QString values;

        for (int i = 0; i < size / 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                // 获取与开始的时间的时间差
                //timeDiff = QDateTime::currentDateTime();
                //QString time = timeDiff.toString("hh.mm.ss.zzz");
                values += QString("(%1, %2, %3),")
                              .arg(currentRoundID) // 这里是当前轮次的标识符
                              .arg(j + 1)          // 通道编号从 1 开始
                              .arg(channel[i][j]);
                              //.arg(time);
            }
        }
        // 删除最后一个逗号
        values.chop(1);
        // 拼接完整的 SQL 插入语句
        insertQuery += values;
        // 执行 SQL 插入操作
        if (!query.exec(insertQuery))
        {
            qDebug() << "Error inserting data into database:" << query.lastError().text();
            return;
        }
    }
    /// 绘图代码
    for (int i = 0; i < 4; i++)
    {
        QVector<double> x(size / 4), y(size / 4);
        for(int j = 0; j < size / 4; j++)
        {
            x[j] = j;
            y[j] = channel[j][i] * 1000;
        }
        qcustomplot[i]->clearGraphs();
        //添加图形
        qcustomplot[i]->addGraph();
        //设置画笔
        qcustomplot[i]->graph(0)->setPen(QPen(color[i]));
        //传入数据，setData的两个参数类型为double
        qcustomplot[i]->graph(0)->setData(x, y);
        //设置X轴坐标范围
        qcustomplot[i]->xAxis->setRange(0, size / 4);
        qcustomplot[i]->replot();
        //printf("CH%d: %4d  ",i+1,y[i]);
        x.clear();
        y.clear();

    }
    //printf("\n");
    //delete [] channel;        // 使用智能指针就不需要手动清除了
}


void vk701page::on_btn_start_2_clicked()
{
    ui->btn_start_2->setEnabled(false);

    // 开启工作线程
    if (!workerThread->isRunning())
    {
        // 判断采样频率
        int sa = ui->le_samplingFrequency->text().toInt();
        if(sa <= 100000 && sa >= 1000)                       // 采样频率在1-100K
        {
            worker->samplingFrequency = sa;
        }
        ui->le_samplingFrequency->setEnabled(false);
        workerThread->start();
    }
    worker->fDAQSampleClr = false;
    startTimeflag = true;                   // 采集时间的标志

    currentRoundID++;                       // 按下一次开始，轮次+1
}

void vk701page::on_btn_stop_2_clicked()
{
    ui->btn_start_2->setEnabled(true);
    // 停止采集卡
    worker->fDAQSampleClr = true;
    // 记录停止的时间
    stopTime = QDateTime::currentDateTime();
    qint64 intervalTimeMS = startTime.msecsTo(stopTime);
    // 插入TimeRecord表
    QSqlQuery query(db);
    QString insertStatement = "INSERT INTO TimeRecord (Round, TimeDiff) VALUES (:round, :timeDiff)";
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

// 连接数据库，并且初始化数据库
void vk701page::InitDB(const QString &fileName)
{
    // 判断数据库是否存在
    bool dbExists = QFile::exists(fileName);
    // 如果数据库不存在，则创建数据库并初始化
   if (!dbExists)
   {
       QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE","vk701"); // 获取当前数据库连接
       db.setDatabaseName(fileName);

       if (!db.open())
       {
           qDebug() << "Error: Failed to connect database." << db.lastError();
           return;
       }

       qDebug() << "Connect to database.";

       QSqlQuery createQuery(db);
       if (!createQuery.exec("CREATE TABLE IF NOT EXISTS IEPEdata ("
                             "RoundID INTEGER,"
                             "ChID INTEGER,"
                             "VibrationData REAL)"))
        {
           qDebug() << "Error creating IEPEdata table:" << createQuery.lastError().text();
           return;
        }
       if (!createQuery.exec("CREATE TABLE IF NOT EXISTS TimeRecord ("
                                          "Round INTEGER,"
                                          "TimeDiff REAL)"))
        {
           qDebug() << "Error creating TimeRecord table:" << createQuery.lastError().text();
           return;
        }


       qDebug() << "Database created successfully.";

       db.close();
   }

    db = QSqlDatabase::addDatabase("QSQLITE","vk701"); // 获取当前数据库连接
        db.setDatabaseName(fileName);
            if (!db.open())
            {
                qDebug() << "Error: Failed to connect database." << db.lastError();
                return;
            }
    qDebug() << "Connect to vk701 database.";

    // 读取数据库最后一行中的RoundID，并赋值给currentRoundID，保持连贯性
    QSqlQuery query("SELECT MAX(RoundID) FROM IEPEdata", db);
    if (query.exec() && query.first())
    {
        currentRoundID = query.value(0).toInt();
    }
    else
    {
        qDebug() << "Failed to retrieve RoundID:" << query.lastError().text();
    }

}

// 显示范围内的数据
void vk701page::on_btn_showDB_clicked()
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
    // 1.输入对象前先清空 table_vibDB 已经有的数据
    ui->table_vibDB->setRowCount(0);

    // 2.根据范围，将符合范围的数据全部显示到 table_vibDB 上
    int row = 0;
    QSqlQuery query(db);
    if (query.exec(QString("SELECT * FROM IEPEdata WHERE rowid BETWEEN %1 AND %2").arg(start).arg(end)))            // 查询数据库的命令
    {
        QSqlRecord record = query.record();
        int count = record.count();
        qDebug() << "record.count:" << count;
        ui->table_vibDB->setColumnCount(count);
        ui->table_vibDB->setHorizontalHeaderLabels({"RoundID" , "ChID" , "VibrationData"}); // 设置表头
        // 输出查询结果
        while (query.next())
        {
            ui->table_vibDB->insertRow(row);
            for(int var = 0; var < count; ++var)
            {
                ui->table_vibDB->setItem(row,var,new QTableWidgetItem(query.value(var).toString()));
            }
            row++;
        }
    }
    else
    {
        qDebug() << "Query failed." << query.lastError().text();
    }
    // 3.查询所有的数量并显示
    query.prepare("SELECT COUNT(*) FROM IEPEdata");
    if (query.exec()) {
        if (query.next()) {
            int rowCount = query.value(0).toInt();
            qDebug() << "Number of rows in the table:" << rowCount;
            ui->le_totalDataNum->setText(QString::number(rowCount));;
        }
    } else {
        qDebug() << "Query failed:" << query.lastError().text();
    }

}


void vk701page::on_btn_deleteData_clicked()
{
    // 提取需要清空的论次
    int round = ui->spinBox_round->value();
    // 1.输入对象前先清空 table_vibDB 已经有的数据
    ui->table_vibDB->setRowCount(0);
    // 2.删除选中范围内的数据
    QSqlQuery query(db);
    // 构造删除数据的 SQL 语句
    QString deleteQuery = QString("DELETE FROM IEPEdata WHERE RoundID = %1").arg(round);
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

// 删除所有数据库的数据
void vk701page::on_btn_nuke_clicked()
{
    db.transaction();

    // 使用 prepared statements 防止 SQL 注入攻击
    QSqlQuery query(db);
    query.prepare("DELETE FROM IEPEdata");
    query.exec();

    query.prepare("DELETE FROM TimeRecord");
    query.exec();

    query.prepare("DELETE FROM sqlite_sequence WHERE name='IEPEdata'");
    query.exec();

    query.prepare("DELETE FROM sqlite_sequence WHERE name='TimeRecord'");
    query.exec();

    // 提交删除操作，然后执行 VACUUM
    db.commit();

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

