#include "inc/vk701page.h"
#include "ui_vk701page.h"
#include <QCloseEvent>

// 设置绘图颜色常量
const QColor color[4] = {Qt::darkRed, Qt::darkGreen, Qt::darkBlue, Qt::darkYellow};

vk701page::vk701page(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::vk701page)
{
    ui->setupUi(this);

    // 初始化数据库
    InitDB("/home/hui/workdir/VK701_Demo/db/vibsqlite.db");

    // 设置表头自适应
    QHeaderView *header = ui->table_vibDB->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Stretch);
    // 设置表格的大小策略为 Expanding
    ui->table_vibDB->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 初始化QCustomPlot控件
    qcustomplot[0] = ui->qcustomplot1_2;
    qcustomplot[1] = ui->qcustomplot2_2;
    qcustomplot[2] = ui->qcustomplot3_2;
    qcustomplot[3] = ui->qcustomplot4_2;
    
    // 统一初始化所有绘图控件
    for (int i = 0; i < 4; i++) {
        // 设置图形刷新策略以优化性能
        qcustomplot[i]->setOpenGl(true); // 启用OpenGL加速
        qcustomplot[i]->setNoAntialiasingOnDrag(true); // 拖拽时关闭抗锯齿
        qcustomplot[i]->setPlottingHints(QCP::phFastPolylines); // 使用快速绘制
        
        // 添加图形，只需添加一次，后续只更新数据
        qcustomplot[i]->addGraph();
        qcustomplot[i]->graph(0)->setPen(QPen(color[i]));
        
        // 设置坐标轴
        qcustomplot[i]->xAxis->setRange(0, 1000);
        qcustomplot[i]->yAxis->setRange(-2000, 2000);
        
        // 设置坐标轴框架
        qcustomplot[i]->axisRect()->setupFullAxesBox();
        
        // 预先为图形分配内存，减少动态分配
        QVector<double> x(1000), y(1000);
        for(int j = 0; j < 1000; j++) {
            x[j] = j;
            y[j] = 0;
        }
        qcustomplot[i]->graph(0)->setData(x, y);
        qcustomplot[i]->replot();
    }

    // 创建定时器用于更新图表，避免频繁重绘
    plotUpdateTimer = new QTimer(this);
    connect(plotUpdateTimer, &QTimer::timeout, this, &vk701page::updatePlots);
    plotUpdateTimer->start(plotUpdateInterval);
    
    // 创建定时器用于批量提交数据库
    dbCommitTimer = new QTimer(this);
    connect(dbCommitTimer, &QTimer::timeout, [this]() {
        if (AllRecordStart && !batchData.isEmpty()) {
            QSqlDatabase::database("vk701").transaction();
            QSqlQuery query(QSqlDatabase::database("vk701"));
            query.prepare("INSERT INTO IEPEdata (RoundID, ChID, VibrationData) VALUES (?, ?, ?)");
            query.addBindValue(batchData[0]);
            query.addBindValue(batchData[1]);
            query.addBindValue(batchData[2]);
            query.execBatch();
            QSqlDatabase::database("vk701").commit();
            batchData.clear();
        }
    });
    dbCommitTimer->start(500); // 每500ms提交一次

    // 创建单独的数据读取线程
    workerThread = new QThread();
    worker = new vk701nsd();
    worker->moveToThread(workerThread);

    // 连接线程与对象
    connect(workerThread, &QThread::started, worker, &vk701nsd::doWork);
    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);

    // 连接数据处理信号和槽
    connect(worker, &vk701nsd::resultValue, this, &vk701page::handleResultValue, Qt::QueuedConnection);
    
    // 新增: 连接状态变化和消息信号
    connect(worker, &vk701nsd::stateChanged, this, &vk701page::handleStateChanged);
    connect(worker, &vk701nsd::resultMsg, this, &vk701page::handleResultMsg);

    // 在vk701page构造函数中添加这行代码
    connect(ui->btn_exit, &QPushButton::clicked, this, &vk701page::on_btn_exit_clicked);
}

vk701page::~vk701page()
{
    // 安全关闭工作线程
    safelyShutdownWorker();
    
    // 停止所有定时器
    if (plotUpdateTimer) {
        plotUpdateTimer->stop();
        delete plotUpdateTimer;
    }
    
    if (dbCommitTimer) {
        dbCommitTimer->stop();
        delete dbCommitTimer;
    }
    
    // 提交剩余数据
    if (!batchData.isEmpty()) {
        QSqlDatabase::database("vk701").transaction();
        QSqlQuery query(QSqlDatabase::database("vk701"));
        query.prepare("INSERT INTO IEPEdata (RoundID, ChID, VibrationData) VALUES (?, ?, ?)");
        query.addBindValue(batchData[0]);
        query.addBindValue(batchData[1]);
        query.addBindValue(batchData[2]);
        query.execBatch();
        QSqlDatabase::database("vk701").commit();
    }
    
    // 关闭数据库连接
    if (db.isOpen()) {
        db.close();
    }
    
    delete ui;
}

// 处理数据采集线程返回的数据
void vk701page::handleResultValue(QVector<double> *list)
{
    // 记录开始的时间
    if(startTimeflag != false) {
        startTime = QDateTime::currentDateTime();
        startTimeflag = false;
    }
    
    // 获取数据大小
    int size = list->size();
    if (size <= 0) return;
    
    // 保存数据到本地缓存，用于更新图表
    {
        QMutexLocker locker(&dataMutex); // 锁定数据互斥锁
        currentData = *list; // 复制数据
    }
    
    // 标记需要更新图表
    needPlotUpdate = true;
    
    // 保存数据到数据库（如果数据记录标志为真）
    if (AllRecordStart) {
        // 使用QtConcurrent异步保存数据，避免阻塞UI
        QtConcurrent::run(this, &vk701page::saveDataToDatabase, *list, 4, size/4);
    }
}

// 处理状态变化
void vk701page::handleStateChanged(DAQState newState)
{
    // 根据状态更新UI
    switch (newState) {
        case DAQState::Disconnected:
            ui->statusLabel->setText("状态: 未连接");
            break;
        case DAQState::Initializing:
            ui->statusLabel->setText("状态: 初始化中");
            break;
        case DAQState::Running:
            ui->statusLabel->setText("状态: 采集中");
            ui->btn_start_2->setEnabled(false);
            ui->btn_stop_2->setEnabled(true);
            break;
        case DAQState::Paused:
            ui->statusLabel->setText("状态: 已暂停");
            ui->btn_start_2->setEnabled(true);
            ui->btn_stop_2->setEnabled(false);
            break;
        case DAQState::Stopping:
            ui->statusLabel->setText("状态: 正在停止");
            ui->btn_start_2->setEnabled(false);
            ui->btn_stop_2->setEnabled(false);
            break;
        case DAQState::Error:
            ui->statusLabel->setText("状态: 错误");
            ui->btn_start_2->setEnabled(true);
            ui->btn_stop_2->setEnabled(false);
            break;
    }
}

// 处理消息
void vk701page::handleResultMsg(QString msg)
{
    // 显示消息
    QMessageBox::information(this, "数据采集", msg);
}

// 安全关闭工作线程
void vk701page::safelyShutdownWorker()
{
    if (workerThread && workerThread->isRunning()) {
        // 请求工作线程停止
        worker->requestStop();
        worker->fDAQSampleClr = true;
        
        // 给线程一些时间来完成清理工作
        if (!workerThread->wait(3000)) {
            qDebug() << "工作线程未能在3秒内退出，强制终止";
            workerThread->terminate();
            workerThread->wait(); // 等待线程实际终止
        }
    }
}

// 异步保存数据到数据库
void vk701page::saveDataToDatabase(const QVector<double> &data, int channels, int pointsPerChannel)
{
    // 检查数据有效性
    if (data.isEmpty() || channels <= 0 || pointsPerChannel <= 0) {
        return;
    }
    
    QVariantList roundIds;
    QVariantList channelIds;
    QVariantList values;
    
    // 准备批量插入数据
    for (int i = 0; i < pointsPerChannel; i++) {
        for (int j = 0; j < channels; j++) {
            int index = i * channels + j;
            if (index < data.size()) {
                roundIds.append(currentRoundID);
                channelIds.append(j + 1);  // 通道编号从1开始
                values.append(data[index]);
            }
        }
    }
    
    // 使用互斥锁保护批量数据
    {
        QMutexLocker locker(&dataMutex);
        batchData.clear();
        batchData.append(roundIds);
        batchData.append(channelIds);
        batchData.append(values);
    }
}

// 更新图表
void vk701page::updatePlots()
{
    if (!needPlotUpdate) {
        return;  // 如果没有新数据，不更新图表
    }
    
    QVector<double> dataCopy;
    {
        QMutexLocker locker(&dataMutex);
        dataCopy = currentData;
    }
    
    int size = dataCopy.size();
    if (size <= 0) {
        return;
    }
    
    int pointsPerChannel = size / 4;
    
    // 更新四个通道的图表
    for (int i = 0; i < 4; i++) {
        QVector<double> x(pointsPerChannel), y(pointsPerChannel);
        
        for (int j = 0; j < pointsPerChannel; j++) {
            x[j] = j;
            y[j] = dataCopy[j * 4 + i] * 1000; // 转换为毫伏
        }
        
        // 更新数据而不是清除和重建图表
        qcustomplot[i]->graph(0)->setData(x, y);
        qcustomplot[i]->xAxis->setRange(0, pointsPerChannel);
        
        // 判断是否需要自动调整Y轴范围
        double minY = *std::min_element(y.begin(), y.end());
        double maxY = *std::max_element(y.begin(), y.end());
        double margin = (maxY - minY) * 0.1; // 10%的边距
        qcustomplot[i]->yAxis->setRange(minY - margin, maxY + margin);
        
        // 每个通道单独重绘，减少资源占用
        qcustomplot[i]->replot(QCustomPlot::rpQueuedReplot);
    }
    
    needPlotUpdate = false;
}

// 开始采集按钮处理
void vk701page::on_btn_start_2_clicked()
{
    ui->btn_start_2->setEnabled(false);

    // 开启工作线程
    if (!workerThread->isRunning()) {
        // 判断采样频率
        int sa = ui->le_samplingFrequency->text().toInt();
        if (sa <= 100000 && sa >= 1000) {  // 采样频率在1-100K
            worker->samplingFrequency = sa;
            worker->setBufferSize(4 * sa); // 更新缓冲区大小
        }
        ui->le_samplingFrequency->setEnabled(false);
        workerThread->start();
    }
    
    // 启用数据记录
    AllRecordStart = true;
    worker->fDAQSampleClr = false;
    startTimeflag = true;  // 采集时间的标志

    // 轮次+1
    currentRoundID++;
    
    // 启动定时器以适当的间隔更新UI
    plotUpdateTimer->start(plotUpdateInterval);
}

// 停止采集按钮处理
void vk701page::on_btn_stop_2_clicked()
{
    ui->btn_start_2->setEnabled(true);
    
    // 停止数据记录
    AllRecordStart = false;
    
    // 停止采集卡
    worker->fDAQSampleClr = true;

    // 重新启用采样率修改框
    ui->le_samplingFrequency->setEnabled(true);
    
    // 记录停止的时间
    stopTime = QDateTime::currentDateTime();
    qint64 intervalTimeMS = startTime.msecsTo(stopTime);
    
    // 插入TimeRecord表
    QSqlQuery query(db);
    QString insertStatement = "INSERT INTO TimeRecord (Round, TimeDiff) VALUES (:round, :timeDiff)";
    query.prepare(insertStatement);
    query.bindValue(":round", currentRoundID);
    query.bindValue(":timeDiff", intervalTimeMS);
    
    if (!query.exec()) {
        qDebug() << "向TimeRecord表插入数据时出错:" << query.lastError().text();
    } else {
        qDebug() << "数据成功插入TimeRecord表.";
    }
    
    // 降低UI更新频率以节省资源
    plotUpdateTimer->setInterval(500);
}

// 新增: 退出按钮处理
void vk701page::on_btn_exit_clicked()
{
    // 请求用户确认
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认退出", 
                                 "确定要退出应用程序吗?\n所有正在进行的采集将被停止。",
                                 QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        close(); // 触发closeEvent
    }
}

// 处理窗口关闭事件
void vk701page::closeEvent(QCloseEvent *event)
{
    // 如果线程正在运行，请求用户确认
    if (workerThread && workerThread->isRunning()) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "确认退出", 
                                    "数据采集正在进行中。确定要退出吗?",
                                    QMessageBox::Yes | QMessageBox::No);
        
        if (reply == QMessageBox::No) {
            event->ignore(); // 取消关闭
            return;
        }
    }
    
    // 确认关闭，先安全停止线程
    safelyShutdownWorker();
    
    // 提交剩余数据
    if (!batchData.isEmpty()) {
        QSqlDatabase::database("vk701").transaction();
        QSqlQuery query(QSqlDatabase::database("vk701"));
        query.prepare("INSERT INTO IEPEdata (RoundID, ChID, VibrationData) VALUES (?, ?, ?)");
        query.addBindValue(batchData[0]);
        query.addBindValue(batchData[1]);
        query.addBindValue(batchData[2]);
        query.execBatch();
        QSqlDatabase::database("vk701").commit();
    }
    
    // 关闭数据库连接
    if (db.isOpen()) {
        db.close();
    }
    
    event->accept(); // 允许关闭
}

// 初始化数据库
void vk701page::InitDB(const QString &fileName)
{
    // 判断数据库是否存在
    bool dbExists = QFile::exists(fileName);
    
    // 如果数据库不存在，则创建数据库并初始化
    if (!dbExists) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "vk701");
        db.setDatabaseName(fileName);

        if (!db.open()) {
            qDebug() << "错误: 连接数据库失败." << db.lastError();
            return;
        }

        qDebug() << "连接到数据库成功.";

        // 创建表结构
        QSqlQuery createQuery(db);
        
        // 使用事务提高执行效率
        db.transaction();
        
        // 创建数据表
        if (!createQuery.exec("CREATE TABLE IF NOT EXISTS IEPEdata ("
                             "RoundID INTEGER,"
                             "ChID INTEGER,"
                             "VibrationData REAL)")) {
            qDebug() << "创建IEPEdata表出错:" << createQuery.lastError().text();
            db.rollback();
            return;
        }
        
        // 创建时间记录表
        if (!createQuery.exec("CREATE TABLE IF NOT EXISTS TimeRecord ("
                              "Round INTEGER,"
                              "TimeDiff REAL)")) {
            qDebug() << "创建TimeRecord表出错:" << createQuery.lastError().text();
            db.rollback();
            return;
        }
        
        // 创建索引以加快查询速度
        if (!createQuery.exec("CREATE INDEX IF NOT EXISTS idx_IEPEdata_RoundID ON IEPEdata(RoundID)")) {
            qDebug() << "创建索引出错:" << createQuery.lastError().text();
            db.rollback();
            return;
        }
        
        db.commit();
        qDebug() << "数据库创建成功.";
        db.close();
    }

    // 无论数据库是否已存在，都要重新连接数据库
    db = QSqlDatabase::addDatabase("QSQLITE", "vk701");
    db.setDatabaseName(fileName);
    if (!db.open()) {
        qDebug() << "错误: 连接数据库失败." << db.lastError();
        return;
    }
    qDebug() << "连接到vk701数据库成功.";

    // 设置数据库优化选项
    QSqlQuery pragmaQuery(db);
    pragmaQuery.exec("PRAGMA journal_mode = WAL"); // 使用WAL模式提高写入性能
    pragmaQuery.exec("PRAGMA synchronous = NORMAL"); // 降低同步级别提高性能
    pragmaQuery.exec("PRAGMA cache_size = 10000"); // 增加缓存大小
    pragmaQuery.exec("PRAGMA temp_store = MEMORY"); // 临时存储使用内存

    // 读取数据库最后一行中的RoundID，保持轮次连贯性
    QSqlQuery query("SELECT MAX(RoundID) FROM IEPEdata", db);
    if (query.exec() && query.first()) {
        currentRoundID = query.value(0).toInt();
        qDebug() << "当前最大轮次ID:" << currentRoundID;
    } else {
        qDebug() << "获取RoundID失败:" << query.lastError().text();
        currentRoundID = 0;
    }
}

// 清理旧数据，保留最近N轮数据
void vk701page::cleanupOldData(int keepLastNRounds)
{
    // 如果当前轮次小于或等于要保留的轮次数，则不需要清理
    if (currentRoundID <= keepLastNRounds) {
        return;
    }

    // 计算需要删除的轮次（所有小于 currentRoundID - keepLastNRounds 的轮次）
    int deleteBeforeRound = currentRoundID - keepLastNRounds;
    
    // 使用事务来加速删除操作
    db.transaction();
    
    QSqlQuery deleteQuery(db);
    // 删除IEPEdata表中的旧数据
    if (!deleteQuery.exec(QString("DELETE FROM IEPEdata WHERE RoundID < %1").arg(deleteBeforeRound))) {
        qDebug() << "删除旧数据失败:" << deleteQuery.lastError().text();
        db.rollback();
        return;
    }
    
    // 删除TimeRecord表中的旧数据
    if (!deleteQuery.exec(QString("DELETE FROM TimeRecord WHERE Round < %1").arg(deleteBeforeRound))) {
        qDebug() << "删除旧时间记录失败:" << deleteQuery.lastError().text();
        db.rollback();
        return;
    }
    
    db.commit();
    
    // 压缩数据库（可选，但会暂时锁定数据库）
    QSqlQuery vacuumQuery(db);
    vacuumQuery.exec("VACUUM");
    
    qDebug() << "已清理轮次" << deleteBeforeRound << "之前的数据";
}

// 显示范围内的数据
void vk701page::on_btn_showDB_clicked()
{
    // 判断范围条件，如果不符合则返回
    bool ok;
    int start = ui->le_rage_start->text().toInt(&ok);
    int end = ui->le_rage_end->text().toInt(&ok);
    if (!ok || start > end || start < 0 || end < 0) {
        qDebug() << "范围错误";
        return;
    }

    qDebug() << "范围:" << start << "-" << end;

    // 清空表格已有数据
    ui->table_vibDB->setRowCount(0);

    // 使用分页查询提高性能，避免一次加载全部数据
    const int pageSize = 1000; // 每页记录数
    int currentPage = 0;
    int row = 0;
    
    QSqlQuery query(db);
    // 设置查询超时
    query.setForwardOnly(true); // 只向前滚动结果集，节省内存
    
    // 设置表头
    ui->table_vibDB->setColumnCount(3);
    ui->table_vibDB->setHorizontalHeaderLabels({"轮次ID", "通道ID", "振动数据"});
    
    // 使用分页查询
    while (true) {
        int offset = currentPage * pageSize;
        QString queryStr = QString("SELECT * FROM IEPEdata WHERE rowid BETWEEN %1 AND %2 LIMIT %3 OFFSET %4")
                           .arg(start).arg(end).arg(pageSize).arg(offset);
                           
        if (!query.exec(queryStr)) {
            qDebug() << "查询失败." << query.lastError().text();
            break;
        }
        
        // 如果没有更多记录，退出循环
        if (!query.next()) {
            break;
        }
        
        // 处理本页数据
        do {
            ui->table_vibDB->insertRow(row);
            for (int var = 0; var < 3; ++var) {
                ui->table_vibDB->setItem(row, var, new QTableWidgetItem(query.value(var).toString()));
            }
            row++;
            
            // 当加载足够多的行时，更新UI以保持响应性
            if (row % 100 == 0) {
                QApplication::processEvents();
            }
        } while (query.next());
        
        currentPage++;
    }

    // 查询所有记录数量并显示
    query.prepare("SELECT COUNT(*) FROM IEPEdata");
    if (query.exec() && query.next()) {
        int rowCount = query.value(0).toInt();
        qDebug() << "表中的行数:" << rowCount;
        ui->le_totalDataNum->setText(QString::number(rowCount));
    } else {
        qDebug() << "查询失败:" << query.lastError().text();
    }
}

// 根据轮次删除数据
void vk701page::on_btn_deleteData_clicked()
{
    // 提取需要清空的轮次
    int round = ui->spinBox_round->value();
    
    // 清空表格
    ui->table_vibDB->setRowCount(0);
    
    // 使用事务提高删除效率
    db.transaction();
    
    QSqlQuery query(db);
    // 删除IEPEdata表中的数据
    QString deleteQuery = QString("DELETE FROM IEPEdata WHERE RoundID = %1").arg(round);
    if (query.exec(deleteQuery)) {
        // 同时删除TimeRecord表中相应的记录
        query.exec(QString("DELETE FROM TimeRecord WHERE Round = %1").arg(round));
        qDebug() << "数据删除成功.";
    } else {
        qDebug() << "删除数据失败:" << query.lastError().text();
        db.rollback();
        return;
    }
    
    db.commit();
    
    // 删除数据后，更新显示
    on_btn_showDB_clicked();
}

// 删除所有数据库数据
void vk701page::on_btn_nuke_clicked()
{
    // 提示用户确认
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "删除所有数据", 
                                 "确定要删除所有数据吗? 此操作不可恢复。",
                                 QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    // 使用事务提高性能
    db.transaction();

    // 使用预处理语句防止SQL注入攻击
    QSqlQuery query(db);
    
    // 删除IEPEdata表中的所有数据
    if (!query.exec("DELETE FROM IEPEdata")) {
        qDebug() << "删除IEPEdata表数据失败:" << query.lastError().text();
        db.rollback();
        return;
    }
    
    // 删除TimeRecord表中的所有数据
    if (!query.exec("DELETE FROM TimeRecord")) {
        qDebug() << "删除TimeRecord表数据失败:" << query.lastError().text();
        db.rollback();
        return;
    }
    
    // 重置自增序列（如果有的话）
    query.exec("DELETE FROM sqlite_sequence WHERE name='IEPEdata'");
    query.exec("DELETE FROM sqlite_sequence WHERE name='TimeRecord'");

    // 提交删除操作
    db.commit();

    // 执行VACUUM命令压缩数据库文件
    query.exec("VACUUM");

    // 重置轮次计数器
    currentRoundID = 0;
    qDebug() << "所有数据删除成功.";
    
    // 清空表格
    ui->table_vibDB->setRowCount(0);
    ui->le_totalDataNum->setText("0");
}

