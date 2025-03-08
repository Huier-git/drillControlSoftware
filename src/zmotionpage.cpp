#include "inc/zmotionpage.h"
#include "ui_zmotionpage.h"

ZMC_HANDLE g_handle = NULL;                             // motion controller
int MotorMap[10] = {9,8,0,1,2,3,4,5,6,7};               // EtherCAT的映射表
int MotorMapbuckup[10] = {9,8,0,1,2,3,4,5,6,7};         // EtherCAT的映射表
float fAxisNum;                                         // 总线上的轴数量

zmotionpage::zmotionpage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::zmotionpage)
    , m_autoModeThread(nullptr)
    , m_isAutoModeRunning(false)
    , m_rotationMotorID(2)    // 摇臂旋转电机ID为2
    , m_extentMotorID(3)      // 摇臂伸缩电机ID为3
    , m_clampMotorID(4)       // 钻杆夹紧电机ID为4
    , m_rotationOffset(0.0f)
    , m_extentOffset(0.0f)
    , m_robotArmStatusTimer(nullptr)
    , m_penetrationMotorID(3)
    , m_penetrationSpeed(PENETRATION_DEFAULT_SPEED)
    , m_penetrationTargetDepth(0.0)
    , m_penetrationOffset(0.0)
    , m_storageCurrentPosition(0)  // 初始化为第一个位置
    , m_storageOffset(0.0f)        // 初始化零点偏移量
    , m_rotationSpeed(DEFAULT_ROTATION_SPEED)  // 初始化旋转速度
    , m_percussionFrequency(DEFAULT_PERCUSSION_FREQ)  // 初始化冲击频率
    , m_isRotating(false)          // 初始化为未旋转状态
    , m_isPercussing(false)        // 初始化为未冲击状态
    , initflag(false)
{
    // 首先设置UI
    ui->setupUi(this);

    // 创建运动控制器实例
    m_motionController = new MotionController(this);

    // 创建并初始化定时器
    basicInfoTimer = new QTimer(this);
    advanceInfoTimer = new QTimer(this);
    realtimeParmTimer = new QTimer(this);

    // 连接定时器信号
    if (basicInfoTimer) {
        connect(basicInfoTimer, &QTimer::timeout, this, &zmotionpage::basicInfoRefresh);
    }
    
    if (advanceInfoTimer) {
        connect(advanceInfoTimer, &QTimer::timeout, this, &zmotionpage::advanceInfoRefreash);
    }

    if (realtimeParmTimer) {
        connect(realtimeParmTimer, &QTimer::timeout, this, &zmotionpage::RefreshTableRealTimeContent);
    }

    // 初始化 Modbus 处理器
    mdbProcessor = new mdbprocess(this);
    if (mdbProcessor) {
        connect(mdbProcessor, &mdbprocess::dataReceived, this, &zmotionpage::handleReceivedData);
    }

    // 初始化机械手状态更新定时器
    m_robotArmStatusTimer = new QTimer(this);
    if (m_robotArmStatusTimer) {
        connect(m_robotArmStatusTimer, &QTimer::timeout, this, [this]() {
            updateRotationAngle();
            updateExtentLength();
            updateClampStatus();
            updatePenetrationDepth();
            updateStorageStatus();
            updateRotationStatus();
            updatePercussionStatus();
        });
        m_robotArmStatusTimer->start(500);
    }

    // 初始化UI组件
    initializeUI();

    // 初始化模式选择
    initModeSelection();

    // 设置初始状态
    ui->btn_BusInit->setEnabled(false);
    ui->cb_modeManual->setChecked(true);

    // 连接其他信号槽
    connectSignalsAndSlots();
}

void zmotionpage::connectSignalsAndSlots()
{
    // 连接自动更新复选框
    connect(ui->CB_AutoUpdate, &QCheckBox::stateChanged, [this](int state) {
        if (advanceInfoTimer) {
            if (state == Qt::Checked) {
                advanceInfoTimer->start(1000);
            } else {
                advanceInfoTimer->stop();
            }
        }
    });

    // 连接实时参数刷新复选框
    connect(ui->cb_motorRtRefrsh, &QCheckBox::stateChanged, [this](int state) {
        if (realtimeParmTimer) {
            if (state == Qt::Checked) {
                realtimeParmTimer->start(500);
            } else {
                realtimeParmTimer->stop();
            }
        }
    });

    // 连接更新按钮
    connect(ui->Btn_Update, &QPushButton::clicked, this, &zmotionpage::advanceInfoRefreash);
    connect(ui->Btn_Update, &QPushButton::clicked, [this]() {
        if (ui->CB_AxisNum->count() == fAxisNum)
            return;
        ui->CB_AxisNum->clear();
        for(int i = 0; i < fAxisNum; ++i) {
            ui->CB_AxisNum->addItem(QString::number(i));
        }
    });

    // 连接机械手控制相关的信号和槽
    connect(ui->btn_rotation_reset, &QPushButton::clicked, this, &zmotionpage::on_btn_rotation_reset_clicked);
    connect(ui->btn_drill_position, &QPushButton::clicked, this, &zmotionpage::on_btn_drill_position_clicked);
    connect(ui->btn_storage_position, &QPushButton::clicked, this, &zmotionpage::on_btn_storage_position_clicked);
    connect(ui->le_set_rotation_angle, &QLineEdit::editingFinished, this, &zmotionpage::on_le_set_rotation_angle_editingFinished);

    // 连接钻管存储机构控制按钮
    connect(ui->btn_storage_backward, &QPushButton::clicked, this, &zmotionpage::on_btn_storage_backward_clicked);
    connect(ui->btn_storage_forward, &QPushButton::clicked, this, &zmotionpage::on_btn_storage_forward_clicked);

    // 连接旋转和冲击控制按钮
    connect(ui->btn_rotation, &QPushButton::clicked, this, &zmotionpage::on_btn_rotation_clicked);
    connect(ui->btn_rotation_stop, &QPushButton::clicked, this, &zmotionpage::on_btn_rotation_stop_clicked);
    connect(ui->btn_percussion, &QPushButton::clicked, this, &zmotionpage::on_btn_percussion_clicked);
    connect(ui->btn_percussion_stop, &QPushButton::clicked, this, &zmotionpage::on_btn_percussion_stop_clicked);
    connect(ui->le_rotation, &QLineEdit::editingFinished, this, &zmotionpage::on_le_rotation_editingFinished);
    connect(ui->le_percussion, &QLineEdit::editingFinished, this, &zmotionpage::on_le_percussion_editingFinished);

    // 其他信号槽连接...
    // 连接夹爪控制信号
    connect(ui->btn_downclamp_open, &QPushButton::clicked, this, &zmotionpage::on_btn_downclamp_open_clicked);
    connect(ui->btn_downclamp_close, &QPushButton::clicked, this, &zmotionpage::on_btn_downclamp_close_clicked);
    connect(ui->le_downclamp_DAC, &QLineEdit::editingFinished, this, &zmotionpage::on_le_downclamp_DAC_editingFinished);
}

zmotionpage::~zmotionpage()
{
    delete ui;
    stopAutoMode();
}


/**
 * @brief 把字符串添加时间辍
 * @param cmdbuffAck
 * @return
 */
QString toCmdWindow(QString cmdbuffAck) {
  QDateTime dateTime = QDateTime::currentDateTime();
  QString timeStr = dateTime.toString("yyyy-MM-dd hh:mm:ss");
  return "[" + timeStr + "] " + cmdbuffAck;
}

/**
 * @brief 简单阻塞睡眠函数
 * @param msec
 */
void sleep(unsigned int msec){
    QTime reachTime = QTime::currentTime().addMSecs(msec);
    while(QTime::currentTime()<reachTime){
        QApplication::processEvents(QEventLoop::AllEvents,100);
    }
}

/**
 * @brief 封装创建 QTableWidgetItem 对象的函数
 * @param text
 * @return
 */
QTableWidgetItem* createTableWidgetItem(const QString& text) {
    return new QTableWidgetItem(text);
}

/**
 * @brief 检测是不是数字
 * @param str
 * @return
 */
bool isNumeric(const QString &str) {
    QRegularExpression re("^-?\\d*\\.?\\d+$"); // 匹配整数或浮点数的正则表达式
    return re.match(str).hasMatch();
}

/**
 * @brief PauseAllAxis 暂停所有的轴的运动
 */
void zmotionpage::PauseAllAxis()
{
    if(g_handle == NULL)
    {
        return;
    }
    char  cmdbuff[2048];
    char  cmdbuffAck[2048];
    //生成命令
    sprintf(cmdbuff, "MOVE_PAUSE");
    //调用命令执行函数
    int ret = ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048);
    if(ret == 1)
    {
        qDebug()<<"Pause All!";
    }
}

void zmotionpage::ResumeAllAxis()
{
    if(g_handle == NULL)
    {
        return;
    }
    for(int i = 0; i < fAxisNum; i++)
    {
        int ret = ZAux_Direct_MoveResume(g_handle, i);            // 恢复所有轴的当前运动
        if(ret == 0)
        {
            qDebug() << "Axis " << i << "Resumed.";
        }
        else
        {
            qDebug() << "Axis " << i << "Resumed Error." << ret;
        }
        float IDLE;
        ZAux_Direct_GetParam(g_handle, "IDLE", i, &IDLE);
        qDebug() << "Axis IDLE " << i << ":" << IDLE;
    }
}

/**
 * @brief 扫描IP按钮
 */
void zmotionpage::on_btn_IP_Scan_clicked()
{
    qDebug() << "Scan";
    char buffer[4096+256+1];
    int32 iresult;
    const char* ipaddress;
    iresult = ZAux_SearchEthlist(buffer, 4096+256+1, 100);          //调用扫描控制器IP的函数，返回错误码信息
    if(iresult != ERR_OK)                                           //如果返回值不是ERR_OK，则表示出现错误
    {
        qDebug() <<"ERR";                                           //打印错误信息
        return;                                                     //结束函数
    }
    qDebug() << "ire" << iresult;
    qDebug() << "buf" << buffer;
    //将扫描到的所有IP地址进行分割，并按照IP地址格式显示在下拉列表中
    int ipos = 0;
    const char* pstring = buffer;                                   //指向扫描到的IP地址列表的首地址
    char buffer2[256];
    buffer2[0] = '\0';
    for(int j = 0; j < 20; j++)                                    //最多20个IP列表
    {
        while(' ' == pstring[0])                                    //如果当前字符为空格，则跳过
        {
            pstring++;
        }
        memset(buffer2, 0, sizeof(buffer2));
        ipos = sscanf(pstring, "%s", &buffer2);                     //将pstring指向的地址处的字符串解析到buffer2中

        if(EOF == ipos)                                             //如果解析结束，则退出循环
        {
            break;
        }
        //跳过字符
        while((' ' != pstring[0]) && ('\t' != pstring[0]) && ('\0' != pstring[0]))  //如果当前字符不是空格、制表符和空字符，则跳过
        {
            pstring++;
        }
        if(0 == strcmp(buffer2, ipaddress))                         //如果扫描到的IP地址与指定的IP地址相同，则退出循环
        {
            return;
        }
        ui->cb_IP_List->addItem(buffer2);                           //将扫描到的IP地址添加到下拉列表中
    }
}

/**
 * @brief 选择IP连接
 */
void zmotionpage::on_btn_IP_Connect_clicked()
{
    if (!m_motionController) {
        ui->tb_cmdWindow->append("错误：运动控制器未初始化");
        return;
    }

    int32 iresult;
    QString str = ui->cb_IP_List->currentText();                    // 获取当前选中的IP地址
    QByteArray ba = str.toLatin1();                                 // 转换为 char* 格式
    char *ipaddress = ba.data();                                    // 获取 char* 格式的 IP 地址

    if(strlen(ipaddress) == 0)                                      // 如果扫不到IP，则手动添加
    {
       QString ip = ui->le_IP->text();
       ipaddress = ip.toLatin1().data();
       qDebug() << ipaddress;
    }

    if(g_handle != NULL)                                            // 如果控制器已经连接，则关闭当前连接
    {
        ZAux_Close(g_handle);
        g_handle = NULL;
        qDebug() << "[C] Controller disconnected.";
        basicInfoTimer->stop();
        advanceInfoTimer->stop();
        realtimeParmTimer->stop();
        ui->btn_IP_Connect->setText("Connect");
        ui->btn_BusInit->setEnabled(false);
        m_motionController->setControllerHandle(NULL);  // 断开 MotionController 的连接
        return;
    }

    iresult = ZAux_OpenEth(ipaddress, &g_handle);                   // 与控制器建立连接
    if (iresult == ERR_OK)                                          // 连接成功 ERR_OK
    {
        qDebug() << "[C] Controller connected.";
        ui->btn_BusInit->setEnabled(true);                          // 连接成功，可以初始化，解锁BusInit按钮
        ui->btn_IP_Connect->setText("Disconnect");
        m_motionController->setControllerHandle(g_handle);  // 设置控制器句柄
    }
    else                                                            // 连接失败
    {
        qDebug() << "[C] Controller connection failed.";
        ui->tb_cmdWindow->append("错误：控制器连接失败");
    }
}

/**
 * @brief 运动控制器总线初始化
 */
void zmotionpage::on_btn_BusInit_clicked()
{
    char cmdbuffAck[2048];
    int ret = ZAux_Execute(g_handle,"RUNTASK 1,ECAT_Init",cmdbuffAck,2048);  // 任务1重新运行运动控制器里面BAS中的初始化函数
    //ret = ZAux_Execute(g_handle,"RUNTASK 2,Sub_SetNodePara",NULL,0);// 任务2重新分配节点信息
    ui->tb_cmdWindow->append(toCmdWindow(cmdbuffAck));

    if(ret != 0)                                                     // 如果任务执行失败
    {
       ui->le_BusInitStatus->setText("Init failed");
       qDebug() << "[C] Controller can not run tasks.";
       return ;
    }

    basicInfoTimer->start(3000);                                     // 开始基础的定时器，3s刷新一次
    qDebug() << "[C] Controller running task.";
}

/**
 * @brief 基础信息的刷新
 */
void zmotionpage::basicInfoRefresh()
{
    int ret;
    char Bus_InitStatus[]="ECAT_InitEnable",Bus_TotalAxisnum[]="BusAxis_Num";
    ret = ZAux_Direct_GetUserVar(g_handle, Bus_InitStatus, &fInitStatus);         //读取BAS文件中的变量判断总线初始化完成状态
    //qDebug()<< "Bus_InitStatus" << ret;
    ret += ZAux_BusCmd_GetNodeNum(g_handle, 0, &iNodeNum);                        //读取槽位0上节点个数。
    //qDebug()<< "iNodeNum" << ret;
    ret += ZAux_Direct_GetUserVar(g_handle, Bus_TotalAxisnum, &fAxisNum);         //读取BAS文件中的变量判断扫描的总轴数
    //qDebug()<< "Bus_TotalAxisnum" << ret;

    if(ret != 0)                                                                // 如果读取失败
    {
       ui->le_BusInitStatus->setText("Init failed");
       qDebug() << "[C] Controller can not get var.";
       return ;
    }

    ui->le_BusInitStatus->setText("Init done");
    initflag = true;                                                            //初始化标志位置1
    ui->le_BusNodeNum->setText(QString::number(iNodeNum, 'f', 1));              //显示总线上节点数量
    ui->le_AxisNum->setText(QString::number(fAxisNum, 'f', 1));                 //显示总线上轴数量
}

/**
 * @brief 高级信息的刷新
 */
void zmotionpage::advanceInfoRefreash()
{
    //qDebug()<<"HEY";
    if(g_handle == NULL || initflag == false)                                   // 如果没连接直接返回
    {
        return;
    }

    int selectindex = ui->CB_AxisNum->currentIndex();                           // 获取当前选中的轴号
    if(selectindex == -1)
    {
       ui->tb_cmdWindow->append(toCmdWindow("Index Error."));
       return;
    }
    //刷新轴号，获取更新轴当前实时反馈的运动参数
    int m_atype,m_AxisStatus,m_Idle;
    float m_units,m_speed,m_accel,m_decel,m_fMpos,m_fDpos;

    //轴状态更新,可选手动更新的参数
    ZAux_Direct_GetAtype(g_handle, MotorMap[selectindex], &m_atype);                        // 轴类型
    ZAux_Direct_GetUnits(g_handle, MotorMap[selectindex], &m_units);                        // 单位
    ZAux_Direct_GetSpeed(g_handle, MotorMap[selectindex], &m_speed);                        // 速度
    ZAux_Direct_GetAccel(g_handle, MotorMap[selectindex], &m_accel);                        // 加速度
    ZAux_Direct_GetDecel(g_handle, MotorMap[selectindex], &m_decel);                        // 减速度

    ui->LE_Atype->setText(QString ("%2").arg (m_atype));
    ui->LE_PulseEquivalent->setText(QString ("%2").arg (m_units));
    ui->LE_Speed->setText(QString ("%2").arg (m_speed));
    ui->LE_Accel->setText(QString ("%2").arg (m_accel));
    ui->LE_Decel->setText(QString ("%2").arg (m_decel));

    ZAux_Direct_GetMpos(g_handle, MotorMap[selectindex], &m_fMpos);                         //轴编码器反馈位置
    ZAux_Direct_GetDpos(g_handle, MotorMap[selectindex], &m_fDpos);                         //轴指令位置
    ZAux_Direct_GetAxisStatus(g_handle, MotorMap[selectindex], &m_AxisStatus);              //轴状态
    ZAux_Direct_GetIfIdle(g_handle, MotorMap[selectindex], &m_Idle);                        //轴是否在运动

    ui->LE_DirectAxisPos->setText(QString ("%2").arg (m_fDpos));
    ui->LE_CurrentAxisPos->setText(QString ("%2").arg (m_fMpos));
    ui->LE_AxisStatus->setText(QString ("%2").arg(m_AxisStatus));
    ui->LE_IfIdle->setText(m_Idle == 0 ? "Going" : (m_Idle == -1 ? "Done" : ""));

    int m_bAxisEnable;
    ZAux_Direct_GetAxisEnable(g_handle, MotorMap[selectindex], &m_bAxisEnable); // 获取轴使能状态 0 表示关闭 1 表示打开

    ui->Btn_Enable->setText(m_bAxisEnable ? "Disable" : "Enable");
    ui->LE_EableStatus->setText(m_bAxisEnable ? "on" : "off");

}

void zmotionpage::handleModbusCommand(const QString& cmd)
{
    QStringList parts = cmd.split(" ", Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        ui->tb_cmdWindow->append("Invalid Modbus command");
        return;
    }

    QString operation = parts[0].toLower();
    if (operation == "read") {
        if (parts.size() != 4) {
            ui->tb_cmdWindow->append("Invalid read command. Use: read <slave> <address> <count>");
            return;
        }
        int slave = parts[1].toInt();
        int address = parseHexOrDec(parts[2]);
        int count = parseHexOrDec(parts[3]);

        // 限制读取的寄存器数量为1或2
        if (count != 1 && count != 2) {
            ui->tb_cmdWindow->append("Invalid count. Use 1 or 2 for the number of registers to read.");
            return;
        }

        mdbProcessor->ReadValue(0, slave, address, count, 4); // 使用功能码 3
    }
    else if (operation == "write") {
        if (parts.size() < 5) {
            ui->tb_cmdWindow->append("Invalid write command. Use: write <slave> <start_address> <num_registers> <value1> [value2] ...");
            return;
        }

        int slave = parts[1].toInt();  // Slave ID
        int startAddress = parseHexOrDec(parts[2]);  // Start address
        int numRegisters = parts[3].toInt();  // Number of registers

        QVector<quint16> values;
        for (int i = 4; i < parts.size(); ++i) {
            values.append(parseHexOrDec(parts[i]));  // Convert each value from hex/dec to integer
        }

        if (values.size() != numRegisters) {
            ui->tb_cmdWindow->append("Number of values does not match the number of registers specified.");
            return;
        }

        mdbProcessor->WriteValue(0, slave, startAddress, values);  // Send the command
    } else {
        ui->tb_cmdWindow->append("Unknown Modbus command: " + operation);
    }

}

int zmotionpage::parseHexOrDec(const QString& str)
{
    bool ok;
    int value = str.toInt(&ok, 16);
    if (!ok) {
        value = str.toInt(&ok, 10);
    }
    return value;
}

void zmotionpage::handleReceivedData(const QVector<quint16>& data, int startReg)
{
    for (int i = 0; i < data.size(); ++i) {
        int reg = startReg + i; // 加1以转换回1-based地址
        QString hexReg = QString("0x%1").arg(reg, 4, 16, QChar('0')).toUpper();
        QString hexData = QString("0x%1").arg(data[i], 4, 16, QChar('0')).toUpper();
        ui->tb_cmdWindow->append(QString("Received data from register %1: %2 (Hex: %3)")
                                 .arg(hexReg)
                                 .arg(data[i])
                                 .arg(hexData));

        // 检测是否读取的是0x0000地址，并且返回值为0
        if (reg == 0x0000 && data[i] == 0) {
            // 更改按钮文本为"Disconnect"
            ui->btn_pipeConnect->setText("Disconnect");
        }
    }

}
void zmotionpage::handleZmotion(const QString& cmd)
{
    if (g_handle == NULL)
        return;

    if (cmd == "?Map") {
        ShowMotorMap();
        return;
    }

    QByteArray cmdData = cmd.toLatin1();
    const char* cmdPtr = cmdData.constData();

    char respond[2048];

    int32 result = ZAux_Execute(g_handle, cmdPtr, respond, sizeof(respond));
    if (result != ERR_OK) {
        qDebug() << "Failed to send command: " << result;
        return;
    }
    ui->tb_cmdWindow->append(toCmdWindow(respond));
}

/**
 * @brief 发送命令
 */
void zmotionpage::on_btn_sendCmd_clicked()
{
    QString cmd = ui->tx_cmd->toPlainText().trimmed();
        if (cmd.isEmpty())
            return;

        if (cmd.startsWith("mdb:")) {
            handleModbusCommand(cmd.mid(4).trimmed());
        } else {
            handleZmotion(cmd);
        }
}

/**
 * @brief 初始化电机参数表格
 */
void zmotionpage::initMotorTable()
{
    if(g_handle == NULL || initflag != true)
    {
        qDebug() << "[C] Cannot get motor parm.";
        return;
    }

    // 设置行数和列数
    int n = 10;
    //int n = fAxisNum;                                                          // 行数等于轴数
    int columnCount = 10;                                                         // 需要显示，编辑的8个参数
    ui->tb_motor->setRowCount(n);
    ui->tb_motor->setColumnCount(columnCount);
    // 设置表头标签
    QStringList horizontalHeaderLabels;
    horizontalHeaderLabels << "EN" << "MPos" << "Pos" << "MVel" << "Vel" << "DAC" << "Atype" << "Unit" << "Acc" << "Dec";
    ui->tb_motor->setHorizontalHeaderLabels(horizontalHeaderLabels);
    // 设置MPos和MVel为斜体
    QFont italicFont;
    italicFont.setItalic(true);
    QTableWidgetItem *mPosHeaderItem = new QTableWidgetItem("MPos");
    mPosHeaderItem->setFont(italicFont);
    ui->tb_motor->setHorizontalHeaderItem(1, mPosHeaderItem);

    QTableWidgetItem *mVelHeaderItem = new QTableWidgetItem("MVel");
    mVelHeaderItem->setFont(italicFont);
    ui->tb_motor->setHorizontalHeaderItem(3, mVelHeaderItem);
    // 设置表格内容
    QStringList verticalHeaderLabels;
    for (int i = 0; i < n; ++i) {
        verticalHeaderLabels << QString("M%1").arg(i);
    }
    ui->tb_motor->setVerticalHeaderLabels(verticalHeaderLabels);
    // 初始化表格内容并设置第二列不可编辑
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < columnCount; ++j) {
            QTableWidgetItem *item = new QTableWidgetItem();
            if (j == 1 || j == 3) {
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
            ui->tb_motor->setItem(i, j, item);
        }
    }
//    // 设置水平表头列宽自适应
//    QHeaderView *header = ui->tb_motor->horizontalHeader();
//    header->setSectionResizeMode(QHeaderView::Stretch);

    // 设置水平表头列宽为可调整
    QHeaderView *header = ui->tb_motor->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    // 设置表格的大小策略为 Expanding
    //ui->tb_motor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // 启用水平滚动条
    ui->tb_motor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // 设置表格单元格的宽度
    ui->tb_motor->horizontalHeader()->setDefaultSectionSize(100);  //设置默认宽度
    ui->tb_motor->setColumnWidth(0,50);
    ui->tb_motor->setColumnWidth(5,50);
    ui->tb_motor->setColumnWidth(6,50);
    ui->tb_motor->setColumnWidth(7,50);
}

/**
 * @brief 刷新表格的参数
 */
void zmotionpage::RefreshTableContent()
{
    int iEN, iAType;
    float fMPos, fDPos, fMVel, fDVel, fDAC, fUnit, fAcc, fDec;
    int n = fAxisNum;                                                          // 行数等于轴数
    // 读取电机的参数
    for (int i = 0; i < n; ++i)                                 // 0-9
    {
        // 读取参数
        int ret;
        ret =  ZAux_Direct_GetAtype(g_handle, MotorMap[i], &iAType);             // 获取轴类型
        ret += ZAux_Direct_GetAxisEnable(g_handle, MotorMap[i], &iEN);           // 获取轴使能状态
        ret += ZAux_Direct_GetDpos(g_handle, MotorMap[i], &fDPos);               // 获取轴设定位置
        ret += ZAux_Direct_GetMpos(g_handle, MotorMap[i], &fMPos);               // 获取轴反馈位置
        ret += ZAux_Direct_GetSpeed(g_handle, MotorMap[i], &fDVel);              // 获取轴设定速度
        ret += ZAux_Direct_GetMspeed(g_handle, MotorMap[i], &fMVel);             // 获取轴反馈速度
        ret += ZAux_Direct_GetUnits(g_handle, MotorMap[i], &fUnit);              // 获取轴设定脉冲单位
        ret += ZAux_Direct_GetAccel(g_handle, MotorMap[i], &fAcc);               // 获取轴设定加速度
        ret += ZAux_Direct_GetDecel(g_handle, MotorMap[i], &fDec);               // 获取轴设定减速度
        if(iAType == 65) // 保证位置模式下的DAC是0
        {
            ret += ZAux_Direct_SetDAC(g_handle, MotorMap[i], 0);
        }
        ret += ZAux_Direct_GetDAC(g_handle, MotorMap[i], &fDAC);

        if(ret != 0)
        {
            QString str = QString("[M] M%1 parm cannot read. Error: %2").arg(i).arg(ret);
            QByteArray byteArray = str.toUtf8();
            qDebug() << byteArray;
            ui->tb_cmdWindow->append(toCmdWindow(byteArray));
            //return;
        }
        // 显示到Table上面
        // 内容"EN" << "MPos" << "Pos" << "MVel" << "Vel" << "DAC" << "Atype" << "Unit" << "Acc" << "Dec";
        ui->tb_motor->setItem(i, 0, createTableWidgetItem(QString("%1").arg(iEN)));
        ui->tb_motor->setItem(i, 1, createTableWidgetItem(QString("%1").arg(fMPos)));
        ui->tb_motor->setItem(i, 2, createTableWidgetItem(QString("%1").arg(fDPos)));
        ui->tb_motor->setItem(i, 3, createTableWidgetItem(QString("%1").arg(fMVel)));
        ui->tb_motor->setItem(i, 4, createTableWidgetItem(QString("%1").arg(fDVel)));
        ui->tb_motor->setItem(i, 5, createTableWidgetItem(QString("%1").arg(fDAC)));
        ui->tb_motor->setItem(i, 6, createTableWidgetItem(QString("%1").arg(iAType)));
        ui->tb_motor->setItem(i, 7, createTableWidgetItem(QString("%1").arg(fUnit)));
        ui->tb_motor->setItem(i, 8, createTableWidgetItem(QString("%1").arg(fAcc)));
        ui->tb_motor->setItem(i, 9, createTableWidgetItem(QString("%1").arg(fDec)));
    }
    // 创建完成后，释放存储在容器中的 QTableWidgetItem 对象的内存
    qDeleteAll(tableItems.begin(), tableItems.end());
}

void zmotionpage::RefreshTableRealTimeContent()
{
    int n = fAxisNum;                                                          // 行数等于轴数
    float fMPos, fMVel;
    for (int i = 0; i < n; ++i) {
        ui->tb_motor->setItem(i, 1, createTableWidgetItem(QString(" ")));
        ZAux_Direct_GetMpos(g_handle, MotorMap[i], &fMPos);               // 获取轴反馈位置
        ui->tb_motor->setItem(i, 1, createTableWidgetItem(QString::number(fMPos)));
        ui->tb_motor->setItem(i, 3, createTableWidgetItem(QString(" ")));
        ZAux_Direct_GetMspeed(g_handle, MotorMap[i], &fMVel);             // 获取轴反馈速度
        ui->tb_motor->setItem(i, 3, createTableWidgetItem(QString::number(fMVel)));
    }

}


void zmotionpage::unmodifyMotorTable(int row, int column)
{
    oldCellValue = ui->tb_motor->item(row, column)->text();
    // 保存单元格的坐标
    oldRow = row;
    oldCol = column;
    qDebug() << "Choose row:" << oldRow << " col:" << oldCol << " Value:" << oldCellValue;
}
/**
 * @brief 电机参数表格的参数修改
 * @param item
 */
void zmotionpage::modifyMotorTable(QTableWidgetItem *item)
{
    if (item && item->tableWidget() == ui->tb_motor)
    {
        int row = item->row();
        int col = item->column();
        // 忽略第二列（列号1）和第四列（列号3）的变化
        if (col == 1 || col == 3)
        {
            return;
        }
        QString newCellValue = item->text();
        if (newCellValue == oldCellValue)                   // 如果新内容与旧内容相同，则无需执行任何操作，防止重复触发
            return;
        // 如果是数字就设置，不是就恢复原来的
        isNumeric(newCellValue) ? setMotorParm(row, col, newCellValue) : item->setText(oldCellValue);
    }
}

/**
 * @brief zmotionpage::setMotorParm
 * @param row
 * @param col
 * @param value
 */
void zmotionpage::setMotorParm(int row, int col, QString value)
{
    if(g_handle == NULL)
        return;
    int ret = 1;

    // 内容"EN" << "MPos" << "Pos" << "MVel" << "Vel" << "DAC" << "Atype" << "Unit" << "Acc" << "Dec";
    switch(col)
    {
    case 0: //设置使能
        ret = ZAux_Direct_SetAxisEnable(g_handle, MotorMap[row], value.toFloat());
        break;
    case 2: //设置位置
            // 取消之前的运动
        ZAux_Direct_Single_Cancel(g_handle, MotorMap[row], 0);
        sleep(10);
        if(ui->cb_motorPosABS->isChecked())
        {   // 使用绝对值式的
            ret = ZAux_Direct_Single_MoveAbs(g_handle, MotorMap[row], value.toFloat());
        }else
        {   // 使用相对值式的
            ret = ZAux_Direct_Single_Move(g_handle, MotorMap[row], value.toFloat());
        }
        break;
    case 4: //设置速度
        ret = ZAux_Direct_SetSpeed(g_handle, MotorMap[row], value.toFloat());
        break;
    case 5: //设置DAC
        int iAType;
        ZAux_Direct_GetAtype(g_handle, MotorMap[row], &iAType);
        if(iAType == 66 || iAType == 67)
            ret = ZAux_Direct_SetDAC(g_handle, MotorMap[row], value.toFloat());
        else
            ret = ZAux_Direct_SetDAC(g_handle, MotorMap[row], 0);
        break;
    case 6: //设置AType
        ret = ZAux_Direct_SetAtype(g_handle, MotorMap[row], value.toFloat());
        break;
    case 7: //设置脉冲
        ret = ZAux_Direct_SetUnits(g_handle, MotorMap[row], value.toFloat());
        break;
    case 8: //设置加速度
        ret = ZAux_Direct_SetAccel(g_handle, MotorMap[row], value.toFloat());
        break;
    case 9: //设置减速度
        ret = ZAux_Direct_SetDecel(g_handle, MotorMap[row], value.toFloat());
        break;
    default:
        break;
    }
    if(ret == 0)
    {
        qDebug() << "[M] Modified value at row:" << row << "column:" << col << "New Value:" << value.toFloat() << " to M" << MotorMap[row];
        ui->tb_cmdWindow->append("Done!");
    }
    else
    {
        qDebug() << "[M] Can not modify value at row:" << row << "column:" << col << "New Value:" << value.toFloat() << " to M" << MotorMap[row];
    }
}


void zmotionpage::on_Btn_Enable_clicked()
{
    int enable = -1;
    int index = ui->CB_AxisNum->currentIndex();
    ZAux_Direct_GetAxisEnable(g_handle, MotorMap[index], &enable);
    if(enable == 1)
    {
        ZAux_Direct_SetAxisEnable(g_handle, MotorMap[index], false);
        qDebug() << "Disable Axis" << MotorMap[index];
    }
    else if(enable == 0)
    {
        ZAux_Direct_SetAxisEnable(g_handle, MotorMap[index], true);
        qDebug() << "Enable Axis" << MotorMap[index];
    }
}

/**
 * @brief 清除警报
 */
void zmotionpage::on_Btn_ClearAlm_clicked()
{
    int index = ui->CB_AxisNum->currentIndex();
    int ret;
    ret = ZAux_BusCmd_DriveClear(g_handle, MotorMap[index], 0);
    if(ret == 0)
        qDebug() << "Alarm Cleared.";
}

/**
 * @brief zmotionpage::设置当前的位置为新的零点
 */
void zmotionpage::on_Btn_setCurrZero_clicked()
{
    int ret;
    int index = ui->CB_AxisNum->currentIndex();
    ret = ZAux_Direct_SetMpos(g_handle, MotorMap[index], 0);
    qDebug() << "Set Axis " << index << "to Zero:" << ret;
}

/**
 * @brief zmotionpage::紧急停止-停止全部轴
 */
void zmotionpage::on_btn_rapidStop_clicked()
{
    
    int ret;
    ret = ZAux_Direct_Rapidstop(g_handle, 2);
    stopAutoMode();//强行停止
    if(ret == 0)
        qDebug() << "[Waring] Radpid Stop!";
    ui->tb_cmdWindow->append("[Waring] Radpid Stop!");

}

void zmotionpage::ShowMotorMap()
{
    qDebug() << "Motor map:" << MotorMap;
    QStringList motorValues;
    for (int i = 0; i < 10; ++i) {
        motorValues.append(QString::number(MotorMap[i]));
    }
    QString motorString = motorValues.join(", ");
    ui->tb_cmdWindow->append(motorString);
}


/**
 * @brief zmotionpage::紧急停止-暂停某个值的运动，并且取消运动缓冲
 */
void zmotionpage::on_Btn_StopMove_clicked()
{
    int index = ui->CB_AxisNum->currentIndex();
    int ret;
    ret = ZAux_Direct_Single_Cancel(g_handle, MotorMap[index], 2);
    if(ret == 0)
        qDebug() << "Axis " << MotorMap[index] << " Stop.";
}

//// AutoModeThread 实现

//AutoModeThread::AutoModeThread(zmotionpage *parent) : m_parent(parent), m_stopFlag(false) {}

//void AutoModeThread::run()
//{
//    m_stopFlag = false;
//    // 重新映射
//    m_parent->ShowMotorMap();

//    // 设置初始速度
//    ZAux_Direct_SetSpeed(g_handle, MotorMap[3], 153640);

//    // Motor 4 上升到最高点 13000000
//    ZAux_Direct_Single_MoveAbs(g_handle, MotorMap[3], 1300000);

//    // 等待上升完成
//    while (!m_stopFlag) {
//        float currentPosition;
//        ZAux_Direct_GetMpos(g_handle, MotorMap[3], &currentPosition);
//        if (std::abs(currentPosition - 1300000) < 1000) { // 允许1000的误差
//            break;
//        }
//        QThread::msleep(100);
//    }

//    if (m_stopFlag) return;

//    // 发出确认请求
//    emit requestConfirmation();
//    if (!m_parent->waitForConfirmation() || m_stopFlag) return;

//    // 设置 Motor 1 (冲击电机) 和 Motor 2 (旋转电机)
//    ZAux_Direct_SetAtype(g_handle, MotorMap[1], 66);
//    QThread::msleep(10);
//    ZAux_Direct_SetAtype(g_handle, MotorMap[2], 66);
//    QThread::msleep(10);
//    ZAux_Direct_SetDAC(g_handle, MotorMap[2], -419430);
//    QThread::msleep(10);
//    ZAux_Direct_SetDAC(g_handle, MotorMap[1], 850000);

//    // 设置 Motor 4 的新速度用于下降 6583
//    ZAux_Direct_SetSpeed(g_handle, MotorMap[3], 65830);

//    // Motor 4 下降
//    ZAux_Direct_Single_MoveAbs(g_handle, MotorMap[3], 0);

//    // 监控下降过程
//    while (!m_stopFlag) {
//        float currentPosition, currentSpeed;
//        ZAux_Direct_GetMpos(g_handle, MotorMap[3], &currentPosition);
//        ZAux_Direct_GetMspeed(g_handle, MotorMap[2], &currentSpeed);

//        // 检查是否卡钻 (旋转电机速度为0)
//        if (std::abs(currentSpeed) < 0.1) {
//            // 紧急停止
//            ZAux_Direct_Rapidstop(g_handle, 2);
//            break;
//        }

//        // 检查是否到达底部
//        if (currentPosition <= 0) {
//            break;
//        }

//        QThread::msleep(100);
//    }

//    // 停止冲击和旋转
//    ZAux_Direct_SetDAC(g_handle, MotorMap[1], 0);
//    ZAux_Direct_SetDAC(g_handle, MotorMap[2], 0);

//    // 设置上升速度
//    ZAux_Direct_SetSpeed(g_handle, MotorMap[3], 153640);

//    // 上升回最高点
//    ZAux_Direct_Single_MoveAbs(g_handle, MotorMap[3], 1300000);

//    // 等待上升完成
//    while (!m_stopFlag) {
//        float currentPosition;
//        ZAux_Direct_GetMpos(g_handle, MotorMap[3], &currentPosition);
//        if (std::abs(currentPosition - 1300000) < 1000) { // 允许1000的误差
//            break;
//        }
//        QThread::msleep(100);
//    }

//    emit operationCompleted();
//}

//void AutoModeThread::stop()
//{
//    QMutexLocker locker(&m_mutex);
//    m_stopFlag = true;
//    m_waitCondition.wakeAll();
//}

AutoModeThread::AutoModeThread(QObject *parent)
    : QThread(parent), m_stopFlag(false), m_confirmed(false)
{
}

AutoModeThread::~AutoModeThread()
{
    stop();
    wait();
}
void AutoModeThread::stop()
{
    m_stopFlag.store(true);
}

void AutoModeThread::receiveConfirmation(bool confirmed)
{
    QMutexLocker locker(&m_mutex);
    m_confirmed = confirmed;
    m_waitCondition.wakeAll(); // 唤醒等待的线程
}

// 在顶部定义可变的变量
const int MOTOR_4_UP_SPEED = 109785;        //进给电机速度        500mm/min
const int MOTOR_4_UP_POSITION = 13100000;      //进给电机顶部位置
const int MOTOR_4_DOWN_SPEED = 5489;        //进给电机下降速度
const int MOTOR_4_DIRT_POS = 10100000;
const int DOWN_FORCE_THRESHOLD = 400;
const int MOTOR_1_DAC_VALUE = 850000;       //旋转电机DAC
const int MOTOR_2_DAC_VALUE = -1075;      //冲击电机DAC  -419430 -1075
const float POSITION_TOLERANCE = 1000.0;     //位置容忍
const float MIN_SPEED_THRESHOLD = 0.1;     //0.1
const int SLEEP_DURATION = 100; // ms
const int DONE_WAIT_DURATION = 5000; // ms

#define Motor2useHall 1

#ifdef Motor2useHall

const int Motor2acc = 1000;

#else
const int Motor2acc = 10000;
#endif



void AutoModeThread::run()
{
    while (!m_stopFlag.load())
    {
        QString msg = "自动模式线程启动";
        qDebug() << msg;
        emit messageLogged(msg);

        // 示例：请求显示电机映射
        emit requestShowMotorMap();

        // Motor 4 上升到最高点
        if (ZAux_Direct_SetSpeed(g_handle, MotorMap[3], MOTOR_4_UP_SPEED) != 0 ||
            ZAux_Direct_Single_MoveAbs(g_handle, MotorMap[3], MOTOR_4_UP_POSITION) != 0)
        {
            qDebug() << "设置速度或移动到最高点失败";
            emit operationCompleted();
            return;
        }

        // 等待 Motor 4 到达目标位置
        while (!m_stopFlag.load())
        {
            float currentPosition;
            ZAux_Direct_GetMpos(g_handle, MotorMap[3], &currentPosition);
            qDebug() << "当前 Motor 4 位置:" << currentPosition; // 新增日志
            if (std::abs(currentPosition - MOTOR_4_UP_POSITION) < POSITION_TOLERANCE)
            {
                qDebug() << "Motor 4 已到达最高点";
                break;
            }
            msleep(SLEEP_DURATION);
        }

        // 请求用户确认
        emit requestConfirmation();

        // 等待用户确认
        {
            QMutexLocker locker(&m_mutex);
            // 重置确认结果
            m_confirmed = false;
            // 等待用户确认，直到 receiveConfirmation 被调用并唤醒
            m_waitCondition.wait(&m_mutex);

            if (m_stopFlag.load())
            {
                break; // 检查是否需要停止
            }

            if (m_confirmed)
            {
                qDebug() << "用户确认继续自动模式。";
                // 继续自动模式的操作
            }
            else
            {
                qDebug() << "用户取消自动模式。";
                break; // 退出自动模式
            }
        }

        // Motor 4 下降
        qDebug()<<"DOWN";
        int ret = ZAux_Direct_SetSpeed(g_handle, MotorMap[3], MOTOR_4_UP_SPEED);
        if (ret!= 0) // 检查返回值
        {
            qDebug() << "设置 Motor 4 下降速度失败";
            emit operationCompleted();
            return;
        }
        // Motor 4 降到指定位置
        qDebug()<<"DOWN DONE";
        ret = ZAux_Direct_Single_MoveAbs(g_handle, MotorMap[3], MOTOR_4_DIRT_POS);
        if ( ret!= 0)
        {
            qDebug() << "移动到指定位置失败";
            emit operationCompleted();
            return;
        }

        // 等待 Motor 4 到达目标位置
        while (!m_stopFlag.load())
        {
            float currentPosition;
            ZAux_Direct_GetMpos(g_handle, MotorMap[3], &currentPosition);
            //qDebug() << "当前 Motor 4 位置:" << currentPosition; // 新增日志
            if (std::abs(currentPosition - MOTOR_4_DIRT_POS) < POSITION_TOLERANCE)
            {
                qDebug() << "Motor 4 已到达指定位置";
                break;
            }
            msleep(SLEEP_DURATION);
        }

        ret = ZAux_Direct_SetSpeed(g_handle, MotorMap[3], MOTOR_4_DOWN_SPEED);
        if ( ret!= 0) // 检查返回值
        {
            qDebug() << "设置 Motor 4 下降速度失败";
            emit operationCompleted();
            return;
        }

        // 设置 Motor 1 (旋转电机) 和 Motor 2 (冲击电机)
#ifdef Motor2useHall
        ZAux_Direct_SetAccel(g_handle, MotorMap[2], Motor2acc);
        ZAux_Direct_SetDecel(g_handle, MotorMap[2], Motor2acc);
#endif
        ZAux_Direct_SetAtype(g_handle, MotorMap[1], 66);
        msleep(10);
        ZAux_Direct_SetAtype(g_handle, MotorMap[2], 66);
        msleep(10);
        ZAux_Direct_SetDAC(g_handle, MotorMap[2], MOTOR_2_DAC_VALUE);
        msleep(10);
        ZAux_Direct_SetAxisEnable(g_handle,MotorMap[2],1);
        ZAux_Direct_SetDAC(g_handle, MotorMap[1], MOTOR_1_DAC_VALUE);
        msleep(100);

        ZAux_Direct_Single_MoveAbs(g_handle, MotorMap[3], 0);

        // 监控下降过程
        while (!m_stopFlag.load())
        {
            float currentPosition, currentSpeed;
            ZAux_Direct_GetMpos(g_handle, MotorMap[3], &currentPosition);
            msleep(10);
            ZAux_Direct_GetMspeed(g_handle, MotorMap[1], &currentSpeed);
            //qDebug() << "当前 Motor 4 位置:" << currentPosition << " 速度:" << currentSpeed; // 新增日志
            //qDebug() << "(int)(downForce)" << (int)(downForce) << "DOWN_FORCE_THRESHOLD" << DOWN_FORCE_THRESHOLD;
            if (std::abs(currentSpeed) < MIN_SPEED_THRESHOLD || currentPosition <= 0 || (int)(downForce) >= DOWN_FORCE_THRESHOLD)
            {
                qDebug() << "条件满足，停止冲击和旋转"; // 新增日志
                // 停止冲击和旋转

                ZAux_Direct_SetDAC(g_handle, MotorMap[1], 0);
                msleep(10);
                ZAux_Direct_SetAxisEnable(g_handle,MotorMap[2],0);
                ZAux_Direct_SetDAC(g_handle, MotorMap[2], 0);

                //

                ZAux_Direct_Single_Cancel(g_handle, MotorMap[3], 2);

                int ret;
                ret = ZAux_Direct_Rapidstop(g_handle, 2);
                if(ret == 0)
                    qDebug() << "[Waring] Radpid Stop!";
                break;
            }

            msleep(SLEEP_DURATION);
        }

        if (m_stopFlag.load())
        {
            emit operationCompleted();
            return;
        }

        msleep(DONE_WAIT_DURATION);

        // 上升回最高点
        if (ZAux_Direct_SetSpeed(g_handle, MotorMap[3], MOTOR_4_UP_SPEED) != 0 ||
            ZAux_Direct_Single_MoveAbs(g_handle, MotorMap[3], MOTOR_4_UP_POSITION) != 0)
        {
            qDebug() << "返回到最高点失败";
            emit operationCompleted();
            return;
        }

        // 等待 Motor 4 到达最高点
        while (!m_stopFlag.load())
        {
            float currentPosition;
            ZAux_Direct_GetMpos(g_handle, MotorMap[3], &currentPosition);
            //qDebug() << "当前 Motor 4 位置:" << currentPosition; // 新增日志
            if (std::abs(currentPosition - MOTOR_4_UP_POSITION) < POSITION_TOLERANCE)
            {
                qDebug() << "Motor 4 已返回到最高点";
                break;
            }
            msleep(SLEEP_DURATION);
        }
    }
    emit operationCompleted();
    qDebug() << "自动模式线程结束";
}

// 启动自动模式
void zmotionpage::startAutoMode()
{
    if (!m_autoModeThread)
    {
        m_autoModeThread = new AutoModeThread(this);

        // 连接 AutoModeThread 的信号到 zmotionpage 的槽或 lambda
        connect(m_autoModeThread, &AutoModeThread::requestConfirmation, this, &zmotionpage::handleConfirmation);
        connect(m_autoModeThread, &AutoModeThread::operationCompleted, this, &zmotionpage::onAutoModeCompleted);
        connect(m_autoModeThread, &AutoModeThread::requestShowMotorMap, this, [this]() {
            ui->tb_cmdWindow->append("显示电机映射");
            ShowMotorMap();
        });
        connect(m_autoModeThread, &AutoModeThread::messageLogged, this, [this](const QString& message) {
            ui->tb_cmdWindow->append(message);
        });
    }

    if (!m_isAutoModeRunning)
    {
        m_isAutoModeRunning = true;
        setUIEnabled(false);
        m_autoModeThread->start();
        ui->tb_cmdWindow->append("自动模式启动");
    }
}

// 停止自动模式
void zmotionpage::stopAutoMode()
{
    if (m_autoModeThread && m_isAutoModeRunning)
    {
        connect(m_autoModeThread, &QThread::finished, this, [this]()
                {
            // 清理线程对象
            delete m_autoModeThread;
            m_autoModeThread = nullptr;

            m_isAutoModeRunning = false;
            setUIEnabled(true);
            ui->tb_cmdWindow->append("自动模式已停止"); });

        m_autoModeThread->stop(); // 设置停止标志
    }
}

// 设置 UI 元素的启用状态
void zmotionpage::setUIEnabled(bool enabled)
{
    ui->btn_modeConfirm->setEnabled(enabled);
    ui->cb_modeAuto->setEnabled(enabled);
    ui->cb_modeManual->setEnabled(enabled);
    // 其他需要启用或禁用的 UI 元素
}

// 切换运动模式
void zmotionpage::runningMode()
{
    if (ui->cb_modeAuto->isChecked())
    {
        QString info = "切换到自动模式。";
        qDebug() << info;
        ui->tb_cmdWindow->append(info);
        startAutoMode(); // 开始自动模式
    }
    if (ui->cb_modeManual->isChecked())
    {
        QString info = "切换到手动模式。";
        qDebug() << info;
        ui->tb_cmdWindow->append(info);
        stopAutoMode();
    }
}

// 自动模式完成处理
void zmotionpage::onAutoModeCompleted()
{
    m_isAutoModeRunning = false;
    setUIEnabled(true);
    ui->tb_cmdWindow->append("自动模式完成，请按确认按钮开始新一轮钻进");
    // 可以在此处启用模式选择复选框
}

// 处理用户确认
void zmotionpage::handleConfirmation()
{
    // 弹出确认对话框或在界面上提示用户输入 'y' 确认

    // 示例：使用消息框获取确认
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认", "是否继续自动模式？",
                                  QMessageBox::Yes | QMessageBox::No);
    bool confirmed = (reply == QMessageBox::Yes);

    // 将确认结果传递给 AutoModeThread
    if (m_autoModeThread)
    {
        m_autoModeThread->receiveConfirmation(confirmed);
    }

    // 更新界面
    if (confirmed)
    {
        ui->tb_cmdWindow->append("用户确认继续。");
    }
    else
    {
        ui->tb_cmdWindow->append("用户取消自动模式。");
    }
}

// 推杆 Modbus
void zmotionpage::on_btn_pipeConnect_clicked()
{
    // 读取寄存器 0 的值
    // 连接到Modbus设备
    mdbProcessor->TCPConnect(502, "192.168.1.200");  // 使用默认Modbus端口502
    QThread::msleep(100);  // 等待100毫秒
    mdbProcessor->ReadValue(0, 1,  0x00, 1, 4);
    qDebug() << "dd";
}

// 推杆推出
// void zmotionpage::on_btn_pipePush_clicked()
// {
//     if(ui->btn_pipeConnect->text() != "Disconnect")
//     {
//         qDebug()<< "Please connect first";
//         return;
//     }

//     mdbProcessor->WriteValue(0, 1, 16, {0xFFFF, 0x63C0});
// }


// void zmotionpage::on_btn_pipeRecover_clicked()
// {
//     if(ui->btn_pipeConnect->text() != "Disconnect")
//     {
//         qDebug()<< "Please connect first";
//         return;
//     }

//     mdbProcessor->WriteValue(0, 1, 16, {0x0000, 0x0000});
// }

// ===== 模式切换相关函数实现 =====

/**
 * @brief 初始化模式选择按钮组
 */
void zmotionpage::initModeSelection()
{
    // 创建按钮组，确保三个模式按钮互斥
    QButtonGroup* modeGroup = new QButtonGroup(this);
    modeGroup->addButton(ui->rb_manual_mode);
    modeGroup->addButton(ui->rb_automatic_mode);
    modeGroup->addButton(ui->rb_semi_mode);
    
    // 默认选择手动模式
    ui->rb_manual_mode->setChecked(true);
    
    // 连接确认按钮信号
    connect(ui->btn_mode_confirm, &QPushButton::clicked, this, &zmotionpage::onModeConfirm);
}

/**
 * @brief 模式确认按钮点击处理
 */
void zmotionpage::onModeConfirm()
{
    if (ui->rb_manual_mode->isChecked()) {
        runManualMode();
    } else if (ui->rb_automatic_mode->isChecked()) {
        runAutomaticMode();
    } else if (ui->rb_semi_mode->isChecked()) {
        runSemiAutomaticMode();
    }
}

/**
 * @brief 手动模式实现
 */
void zmotionpage::runManualMode()
{
    ui->tb_cmdWindow_2->append("[模式] 切换到手动模式");
    // 启用所有手动控制按钮
    enableManualControls(true);
    // 停止自动和半自动模式的运行
    stopAutoMode();
    stopSemiAutoMode();
    
    qDebug() << "手动模式已启动";
}

/**
 * @brief 全自动模式实现
 */
void zmotionpage::runAutomaticMode()
{
    ui->tb_cmdWindow_2->append("[模式] 切换到全自动模式");
    // 禁用所有手动控制按钮
    enableManualControls(false);
    // 停止其他模式
    stopSemiAutoMode();
    
    qDebug() << "全自动模式已启动";
    
    // 启动自动模式线程
    startAutoMode();
}

/**
 * @brief 半自动模式实现
 */
void zmotionpage::runSemiAutomaticMode()
{
    ui->tb_cmdWindow_2->append("[模式] 切换到半自动模式");
    // 禁用所有手动控制按钮，除了急停按钮
    enableManualControls(false);
    ui->btn_rapidStop->setEnabled(true); // 保持急停按钮可用
    
    // 停止全自动模式
    stopAutoMode();
    
    qDebug() << "半自动模式已启动";
}

/**
 * @brief 启用/禁用手动控制按钮
 */
void zmotionpage::enableManualControls(bool enable)
{
    // 基本控制按钮
    ui->Btn_Enable->setEnabled(enable);
    ui->Btn_ClearAlm->setEnabled(enable);
    ui->Btn_StopMove->setEnabled(enable);
    ui->btn_rapidStop->setEnabled(enable);
    
    // 机械手旋转控制按钮
    ui->btn_rotation_reset->setEnabled(enable);
    ui->btn_drill_position->setEnabled(enable);
    ui->btn_storage_position->setEnabled(enable);
    ui->le_set_rotation_angle->setEnabled(enable);
    
    // 机械手伸缩控制按钮
    ui->btn_robotarm_reset->setEnabled(enable);
    ui->btn_robotarm_extent->setEnabled(enable);
    ui->btn_robotarm_retract->setEnabled(enable);
    ui->le_set_extent_length->setEnabled(enable);
    
    // 机械手夹爪控制按钮
    ui->btn_robotarm_clamp_open->setEnabled(enable);
    ui->btn_robotarm_clamp_close->setEnabled(enable);
    
    // 进给控制按钮
    ui->btn_penetration_start->setEnabled(enable);
    ui->btn_penetration_cancel->setEnabled(enable);
    ui->le_penetration_target->setEnabled(enable);    // 目标深度输入框
    ui->le_penetration_speed->setEnabled(enable);  // 速度输入框
    
    // 钻管存储机构控制
    ui->btn_storage_backward->setEnabled(enable);
    ui->btn_storage_forward->setEnabled(enable);
    
    // 旋转和冲击控制
    ui->btn_rotation->setEnabled(enable);
    ui->btn_rotation_stop->setEnabled(enable);
    ui->btn_percussion->setEnabled(enable);
    ui->btn_percussion_stop->setEnabled(enable);
    ui->le_rotation->setEnabled(enable);
    ui->le_percussion->setEnabled(enable);
    // 夹爪控制
    ui->btn_downclamp_open->setEnabled(enable);
    ui->btn_downclamp_close->setEnabled(enable);
    ui->le_downclamp_DAC->setEnabled(enable);
}

/**
 * @brief 停止半自动模式
 */
void zmotionpage::stopSemiAutoMode()
{
    // TODO: 实现停止半自动模式的逻辑
    qDebug() << "半自动模式已停止";
}

// ===== 机械手控制相关函数实现 =====

/**
 * @brief 更新旋转角度显示
 */
void zmotionpage::updateRotationAngle()
{
    if (m_motionController && m_motionController->isConnected()) {
        // 获取当前旋转角度
        float currentAngle = m_motionController->getCurrentPosition(m_rotationMotorID);
        
        // 显示绝对角度（不受重置影响）
        ui->le_rotation_angle->setText(QString::number(currentAngle, 'f', 2));
        QString msg = QString("[旋转] 当前角度: %1").arg(currentAngle);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
}

/**
 * @brief 重置旋转起点
 */
void zmotionpage::on_btn_rotation_reset_clicked()
{
    if (!m_motionController || !m_motionController->isConnected()) {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 设置当前位置为零点
    if (m_motionController->setZeroPosition(m_rotationMotorID)) {
        m_rotationOffset = m_motionController->getCurrentPosition(m_rotationMotorID);
        QString msg = QString("[旋转] 已重置起点，当前位置设为零点");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
}

/**
 * @brief 移动到钻机位置（0度）
 */
void zmotionpage::on_btn_drill_position_clicked()
{
    if (!m_motionController || !m_motionController->isConnected()) {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 设置运动参数
    m_motionController->setMotorParameter(m_rotationMotorID, "Speed", ROBOTARM_ROTATION_SPEED);
    m_motionController->setMotorParameter(m_rotationMotorID, "Acc", ROBOTARM_ROTATION_ACCEL);
    m_motionController->setMotorParameter(m_rotationMotorID, "Dec", ROBOTARM_ROTATION_DECEL);

    // 计算目标位置（考虑偏移量）
    float targetPosition = ROBOTARM_DRILL_POSITION_ANGLE - m_rotationOffset;
    
    // 移动到钻机位置
    if (m_motionController->moveMotorAbsolute(m_rotationMotorID, targetPosition)) {
        QString msg = QString("[旋转] 移动到钻机位置 (目标角度: %1)").arg(ROBOTARM_DRILL_POSITION_ANGLE);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
}

/**
 * @brief 移动到存储位置（90度）
 */
void zmotionpage::on_btn_storage_position_clicked()
{
    if (!m_motionController || !m_motionController->isConnected()) {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 设置运动参数
    m_motionController->setMotorParameter(m_rotationMotorID, "Speed", ROBOTARM_ROTATION_SPEED);
    m_motionController->setMotorParameter(m_rotationMotorID, "Acc", ROBOTARM_ROTATION_ACCEL);
    m_motionController->setMotorParameter(m_rotationMotorID, "Dec", ROBOTARM_ROTATION_DECEL);

    // 计算目标位置（考虑偏移量）
    float targetPosition = ROBOTARM_STORAGE_POSITION_ANGLE - m_rotationOffset;
    
    // 移动到存储位置
    if (m_motionController->moveMotorAbsolute(m_rotationMotorID, targetPosition)) {
        QString msg = QString("[旋转] 移动到存储位置 (目标角度: %1)").arg(ROBOTARM_STORAGE_POSITION_ANGLE);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
}

/**
 * @brief 设置自定义旋转角度
 */
void zmotionpage::on_le_set_rotation_angle_editingFinished()
{
    if (!m_motionController || !m_motionController->isConnected()) {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 获取用户输入的目标角度
    bool ok;
    float targetAngle = ui->le_set_rotation_angle->text().toFloat(&ok);
    if (!ok) {
        ui->tb_cmdWindow_2->append("错误: 无效的角度值");
        return;
    }

    // 设置运动参数
    m_motionController->setMotorParameter(m_rotationMotorID, "Speed", ROBOTARM_ROTATION_SPEED);
    m_motionController->setMotorParameter(m_rotationMotorID, "Acc", ROBOTARM_ROTATION_ACCEL);
    m_motionController->setMotorParameter(m_rotationMotorID, "Dec", ROBOTARM_ROTATION_DECEL);

    // 计算目标位置（考虑偏移量）
    float targetPosition = targetAngle - m_rotationOffset;
    
    // 移动到目标位置
    if (m_motionController->moveMotorAbsolute(m_rotationMotorID, targetPosition)) {
        QString msg = QString("[旋转] 移动到指定角度 (目标: %1)").arg(targetAngle);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
}

/**
 * @brief 更新伸出长度显示
 */
void zmotionpage::updateExtentLength()
{
    if (m_motionController && m_motionController->isConnected()) {
        // 获取当前伸出长度
        float currentLength = m_motionController->getCurrentPosition(m_extentMotorID);
        
        // 显示绝对长度（不受重置影响）
        ui->le_extent_length->setText(QString::number(currentLength, 'f', 2));
        QString msg = QString("[伸缩] 当前长度: %1").arg(currentLength);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
}

/**
 * @brief 重置伸出起点
 */
void zmotionpage::on_btn_robotarm_reset_clicked()
{
    if (!m_motionController || !m_motionController->isConnected()) {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 设置当前位置为零点
    if (m_motionController->setZeroPosition(m_extentMotorID)) {
        m_extentOffset = m_motionController->getCurrentPosition(m_extentMotorID);
        QString msg = QString("[伸缩] 已重置起点，当前位置设为零点");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
}

/**
 * @brief 控制机械手伸出
 */
void zmotionpage::on_btn_robotarm_extent_clicked()
{
    if (!m_motionController || !m_motionController->isConnected()) {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 设置运动参数
    m_motionController->setMotorParameter(m_extentMotorID, "Speed", ROBOTARM_EXTENT_SPEED);
    m_motionController->setMotorParameter(m_extentMotorID, "Acc", ROBOTARM_EXTENT_ACCEL);
    m_motionController->setMotorParameter(m_extentMotorID, "Dec", ROBOTARM_EXTENT_DECEL);

    // 计算目标位置（考虑偏移量）
    float targetPosition = ROBOTARM_EXTENT_POSITION - m_extentOffset;
    
    // 移动到伸出位置
    if (m_motionController->moveMotorAbsolute(m_extentMotorID, targetPosition)) {
        QString msg = QString("[伸缩] 伸出到指定位置 (目标: %1)").arg(ROBOTARM_EXTENT_POSITION);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
}

/**
 * @brief 控制机械手回收
 */
void zmotionpage::on_btn_robotarm_retract_clicked()
{
    if (!m_motionController || !m_motionController->isConnected()) {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 设置运动参数
    m_motionController->setMotorParameter(m_extentMotorID, "Speed", ROBOTARM_EXTENT_SPEED);
    m_motionController->setMotorParameter(m_extentMotorID, "Acc", ROBOTARM_EXTENT_ACCEL);
    m_motionController->setMotorParameter(m_extentMotorID, "Dec", ROBOTARM_EXTENT_DECEL);

    // 计算目标位置（考虑偏移量）
    float targetPosition = ROBOTARM_RETRACT_POSITION - m_extentOffset;
    
    // 移动到回收位置
    if (m_motionController->moveMotorAbsolute(m_extentMotorID, targetPosition)) {
        QString msg = QString("[伸缩] 回收到原位 (目标: %1)").arg(ROBOTARM_RETRACT_POSITION);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
}

/**
 * @brief 设置自定义伸出长度
 */
void zmotionpage::on_le_set_extent_length_editingFinished()
{
    if (!m_motionController || !m_motionController->isConnected()) {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 获取用户输入的目标长度
    bool ok;
    float targetLength = ui->le_set_extent_length->text().toFloat(&ok);
    if (!ok) {
        ui->tb_cmdWindow_2->append("错误: 无效的长度值");
        return;
    }

    // 设置运动参数
    m_motionController->setMotorParameter(m_extentMotorID, "Speed", ROBOTARM_EXTENT_SPEED);
    m_motionController->setMotorParameter(m_extentMotorID, "Acc", ROBOTARM_EXTENT_ACCEL);
    m_motionController->setMotorParameter(m_extentMotorID, "Dec", ROBOTARM_EXTENT_DECEL);

    // 计算目标位置（考虑偏移量）
    float targetPosition = targetLength - m_extentOffset;
    
    // 移动到目标位置
    if (m_motionController->moveMotorAbsolute(m_extentMotorID, targetPosition)) {
        QString msg = QString("[伸缩] 移动到指定长度 (目标: %1)").arg(targetLength);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
}

/**
 * @brief 更新夹爪状态显示
 */
void zmotionpage::updateClampStatus()
{
    if (m_motionController && m_motionController->isConnected()) {
        // 获取当前夹爪力矩值
        float currentDAC = 0.0f;
        QMap<QString, float> params;
        if (m_motionController->getMotorParameters(m_clampMotorID, params)) {
            currentDAC = params["DAC"];
            QString status = (currentDAC > 0) ? "闭合" : "张开";
            QString msg = QString("[夹爪] 当前状态: %1 (力矩值: %2)").arg(status).arg(currentDAC);
            qDebug() << msg;
            ui->tb_cmdWindow_2->append(msg);
        }
    }
}

/**
 * @brief 夹爪张开
 */
void zmotionpage::on_btn_robotarm_clamp_open_clicked()
{
    if (!m_motionController || !m_motionController->isConnected()) {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 设置力矩控制模式
    if (m_motionController->setMotorParameter(m_clampMotorID, "Atype", ROBOTARM_CLAMP_TORQUE_MODE)) {
        // 设置负力矩值使夹爪张开
        if (m_motionController->setMotorParameter(m_clampMotorID, "DAC", -ROBOTARM_CLAMP_DEFAULT_DAC)) {
            QString msg = QString("[夹爪] 张开 (力矩值: %1)").arg(-ROBOTARM_CLAMP_DEFAULT_DAC);
            qDebug() << msg;
            ui->tb_cmdWindow_2->append(msg);
        }
    }
}

/**
 * @brief 夹爪闭合
 */
void zmotionpage::on_btn_robotarm_clamp_close_clicked()
{
    if (!m_motionController || !m_motionController->isConnected()) {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 设置力矩控制模式
    if (m_motionController->setMotorParameter(m_clampMotorID, "Atype", ROBOTARM_CLAMP_TORQUE_MODE)) {
        // 设置正力矩值使夹爪闭合
        if (m_motionController->setMotorParameter(m_clampMotorID, "DAC", ROBOTARM_CLAMP_DEFAULT_DAC)) {
            QString msg = QString("[夹爪] 闭合 (力矩值: %1)").arg(ROBOTARM_CLAMP_DEFAULT_DAC);
            qDebug() << msg;
            ui->tb_cmdWindow_2->append(msg);
        }
    }
}

/**
 * @brief 更新当前钻进深度显示
 */
void zmotionpage::updatePenetrationDepth()
{
    if (!m_motionController || !m_motionController->isConnected()) {
        return;
    }

    // 获取当前电机位置（脉冲数）
    float currentPulse = m_motionController->getCurrentPosition(m_penetrationMotorID);
    
    // 计算当前深度（毫米）- 将脉冲转换为毫米，考虑最大高度是最高点
    double currentDepth = PENETRATION_MAX_HEIGHT - (currentPulse / PENETRATION_PULSE_PER_MM);
    
    // 更新显示
    if (ui->le_penetration_depth) {
        ui->le_penetration_depth->setText(QString::number(currentDepth, 'f', 2));
    }
    
    QString msg = QString("[进给] 当前深度: %1 mm (脉冲位置: %2)").arg(currentDepth, 0, 'f', 2).arg(currentPulse);
    qDebug() << msg;
}

/**
 * @brief 设置目标钻进深度
 */
void zmotionpage::on_le_penetration_target_editingFinished()
{
    bool ok;
    double targetDepth = ui->le_penetration_target->text().toDouble(&ok);
    
    if (!ok) {
        QString msg = QString("[进给] 错误: 无效的目标深度值");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        // 恢复原来的值
        ui->le_penetration_target->setText(QString::number(m_penetrationTargetDepth, 'f', 2));
        return;
    }
    
    // 检查深度范围（0-最大高度）
    if (targetDepth < 0 || targetDepth > PENETRATION_MAX_HEIGHT) {
        QString msg = QString("[进给] 错误: 目标深度超出范围 (0-%1 mm)").arg(PENETRATION_MAX_HEIGHT);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        // 恢复原来的值
        ui->le_penetration_target->setText(QString::number(m_penetrationTargetDepth, 'f', 2));
        return;
    }
    
    m_penetrationTargetDepth = targetDepth;
    QString msg = QString("[进给] 已设置目标深度: %1 mm").arg(targetDepth, 0, 'f', 2);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
}

/**
 * @brief 设置进给速度
 */
void zmotionpage::on_le_penetration_speed_editingFinished()
{
    bool ok;
    float speed = ui->le_penetration_speed->text().toFloat(&ok);
    
    if (!ok || speed <= 0) {
        QString msg = QString("[进给] 错误: 无效的速度值");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        // 恢复原来的值
        ui->le_penetration_speed->setText(QString::number(m_penetrationSpeed, 'f', 2));
        return;
    }
    
    m_penetrationSpeed = speed;
    QString msg = QString("[进给] 已设置进给速度: %1").arg(speed, 0, 'f', 2);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
}

/**
 * @brief 开始进给到目标深度
 */
void zmotionpage::on_btn_penetration_start_clicked()
{
    if (!m_motionController || !m_motionController->isConnected()) {
        QString msg = QString("[进给] 错误: 控制器未连接");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 验证是否处于手动模式
    if (!ui->rb_manual_mode->isChecked()) {
        QString msg = QString("[进给] 错误: 只能在手动模式下操作");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 获取当前位置和目标位置（脉冲）
    float currentPulse = m_motionController->getCurrentPosition(m_penetrationMotorID);
    
    // 计算目标位置（脉冲）
    // 注意：这里需要倒置计算，因为最大高度（1315mm）对应最大脉冲（13100000）
    double targetPulse = (PENETRATION_MAX_HEIGHT - m_penetrationTargetDepth) * PENETRATION_PULSE_PER_MM;
    
    // 设置进给速度和加减速参数
    m_motionController->setMotorParameter(m_penetrationMotorID, "Speed", m_penetrationSpeed);
    m_motionController->setMotorParameter(m_penetrationMotorID, "Acc", PENETRATION_DEFAULT_ACCEL);
    m_motionController->setMotorParameter(m_penetrationMotorID, "Dec", PENETRATION_DEFAULT_DECEL);
    
    QString msg = QString("[进给] 开始运动 - 目标深度: %1 mm (当前: %2 mm, 目标脉冲: %3)")
                    .arg(m_penetrationTargetDepth, 0, 'f', 2)
                    .arg(PENETRATION_MAX_HEIGHT - (currentPulse / PENETRATION_PULSE_PER_MM), 0, 'f', 2)
                    .arg(targetPulse);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
    
    // 执行绝对位置运动
    if (m_motionController->moveMotorAbsolute(m_penetrationMotorID, targetPulse)) {
        msg = QString("[进给] 运动已启动");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    } else {
        msg = QString("[进给] 错误: 启动运动失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
    
    // 更新当前深度显示
    updatePenetrationDepth();
}

/**
 * @brief 取消进给运动
 */
void zmotionpage::on_btn_penetration_cancel_clicked()
{
    if (!m_motionController || !m_motionController->isConnected()) {
        QString msg = QString("[进给] 错误: 控制器未连接");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 停止电机运动（使用平滑停止模式 = 0）
    if (m_motionController->stopMotor(m_penetrationMotorID, 0)) {
        QString msg = QString("[进给] 已取消运动");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    } else {
        QString msg = QString("[进给] 错误: 停止运动失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
    
    // 更新当前深度显示
    updatePenetrationDepth();
}

void zmotionpage::initializeUI()
{
    // 初始化进给深度控制
    if (ui->le_penetration_depth) {
        ui->le_penetration_depth->setReadOnly(true);
    }
    if (ui->le_penetration_target) {
        ui->le_penetration_target->setText(QString::number(m_penetrationTargetDepth, 'f', 2));
    }
    if (ui->le_penetration_speed) {
        ui->le_penetration_speed->setText(QString::number(m_penetrationSpeed, 'f', 2));
    }

    // 初始化电机表格
    connect(ui->btn_motorParmUpdate, &QPushButton::clicked, [this]{
        initMotorTable();
        RefreshTableContent();
    });

    // 初始化电机表格编辑模式
    connect(ui->cb_motorParmEdit, &QCheckBox::stateChanged, [this](int state) {
        if (state == Qt::Checked) {
            ui->btn_motorParmUpdate->setEnabled(false);
            connect(ui->tb_motor, &QTableWidget::cellDoubleClicked, this, &zmotionpage::unmodifyMotorTable);
            connect(ui->tb_motor, &QTableWidget::itemChanged, this, &zmotionpage::modifyMotorTable);
        } else {
            ui->btn_motorParmUpdate->setEnabled(true);
            disconnect(ui->tb_motor, &QTableWidget::cellDoubleClicked, this, &zmotionpage::unmodifyMotorTable);
            disconnect(ui->tb_motor, &QTableWidget::itemChanged, this, &zmotionpage::modifyMotorTable);
            initMotorTable();
            RefreshTableContent();
        }
    });

    // 初始化运动控制按钮
    connect(ui->btn_movePause, &QPushButton::clicked, this, [this]{
        if (g_handle) {
            char cmdbuff[2048];
            char cmdbuffAck[2048];
            sprintf(cmdbuff, "MOVE_PAUSE");
            if(ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0) {
                qDebug() << "Pause All!";
            }
        }
    });

    connect(ui->btn_moveResume, &QPushButton::clicked, this, [this]{
        if (g_handle) {
            char cmdbuff[2048];
            char cmdbuffAck[2048];
            sprintf(cmdbuff, "MOVE_RESUME");
            if(ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0) {
                qDebug() << "Resume All!";
            }
        }
    });

    // 初始化EtherCAT映射按钮
    connect(ui->btn_MapMotor, &QPushButton::clicked, this, [this](){
        for(int i = 0; i < 10; i++) {
            MotorMap[i] = MotorMapbuckup[i];
        }
        if(fAxisNum == 1) {
            MotorMap[0] = 0;
        }
        else if(fAxisNum >= 2) {
            MotorMap[0] = fAxisNum - 1;
            MotorMap[1] = fAxisNum - 2;
        }
        ShowMotorMap();
    });

    // 初始化模式切换按钮
    connect(ui->cb_modeAuto, &QCheckBox::toggled, [this](bool checked) {
        if (checked) {
            ui->cb_modeManual->setChecked(false);
        }
    });

    connect(ui->cb_modeManual, &QCheckBox::toggled, [this](bool checked) {
        if (checked) {
            ui->cb_modeAuto->setChecked(false);
        }
    });

    // 连接模式确认按钮
    connect(ui->btn_modeConfirm, &QPushButton::clicked, this, &zmotionpage::runningMode);

    // 初始化钻管存储机构状态显示
    if (ui->le_stroage_status) {
        ui->le_stroage_status->setReadOnly(true);
        ui->le_stroage_status->setText(QString("位置 %1 / %2 (%3°)").arg(m_storageCurrentPosition + 1)
                                                                  .arg(STORAGE_POSITIONS)
                                                                  .arg(m_storageCurrentPosition * STORAGE_ANGLE_PER_POSITION, 0, 'f', 1));
    }

    // 初始化旋转和冲击控制
    if (ui->le_rotation) {
        ui->le_rotation->setText(QString::number(m_rotationSpeed, 'f', 1));
    }
    if (ui->le_percussion) {
        ui->le_percussion->setText(QString::number(m_percussionFrequency, 'f', 1));
    }

    // 初始化夹爪相关变量
    m_downclampDAC = DOWNCLAMP_DEFAULT_DAC;
    m_isDownclamping = false;
    
    // 设置默认力矩值
    ui->le_downclamp_DAC->setText(QString::number(m_downclampDAC, 'f', 1));
    
    // 创建并配置夹爪状态监控定时器
    m_downclampTimer = new QTimer(this);
    m_downclampTimer->setInterval(DOWNCLAMP_CHECK_INTERVAL);
    connect(m_downclampTimer, &QTimer::timeout, this, &zmotionpage::updateDownclampStatus);
}

// 更新存储机构状态显示
void zmotionpage::updateStorageStatus()
{
    if (!m_motionController || !m_motionController->isConnected()) {
        return;
    }

    // 获取当前电机位置
    float currentPosition = 0.0f;
    
    // 使用 ZAux_Direct_GetDpos 获取当前位置
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    sprintf(cmdbuff, "?DPOS(%d)", STORAGE_MOTOR_ID);
    
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0) {
        currentPosition = atof(cmdbuffAck);
        
        // 计算当前角度（考虑零点偏移）
        float currentAngle = (currentPosition - m_storageOffset) * 360.0f / PULSES_PER_REVOLUTION;
        
        // 规范化角度到0-360范围
        while (currentAngle < 0) {
            currentAngle += 360.0f;
        }
        while (currentAngle >= 360.0f) {
            currentAngle -= 360.0f;
        }
        
        // 计算当前位置索引（0-6）
        m_storageCurrentPosition = qRound(currentAngle / STORAGE_ANGLE_PER_POSITION) % STORAGE_POSITIONS;
        
        // 更新UI显示
        if (ui->le_stroage_status) {
            ui->le_stroage_status->setText(QString("位置 %1 / %2 (%3°)").arg(m_storageCurrentPosition + 1)
                                                                      .arg(STORAGE_POSITIONS)
                                                                      .arg(currentAngle, 0, 'f', 1));
        }
    }
}

// 存储机构向后转位
void zmotionpage::on_btn_storage_backward_clicked()
{
    // 检查是否处于手动模式
    if (!ui->cb_modeManual->isChecked()) {
        QString msg = QString("[存储机构] 错误: 只能在手动模式下操作");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 检查控制器连接状态
    if (!g_handle) {
        QString msg = QString("[存储机构] 错误: 控制器未连接");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 计算目标位置索引（向后转位）
    int targetPosition = (m_storageCurrentPosition - 1 + STORAGE_POSITIONS) % STORAGE_POSITIONS;
    
    // 计算目标角度
    float targetAngle = targetPosition * STORAGE_ANGLE_PER_POSITION;
    
    // 设置运动参数
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    
    // 设置速度
    sprintf(cmdbuff, "SPEED(%d)=%f", STORAGE_MOTOR_ID, STORAGE_SPEED);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[存储机构] 错误: 设置速度失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 设置加速度
    sprintf(cmdbuff, "ACCEL(%d)=%f", STORAGE_MOTOR_ID, STORAGE_ACCEL);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[存储机构] 错误: 设置加速度失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 设置减速度
    sprintf(cmdbuff, "DECEL(%d)=%f", STORAGE_MOTOR_ID, STORAGE_DECEL);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[存储机构] 错误: 设置减速度失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 计算目标脉冲位置
    float targetPulses = m_storageOffset + (targetAngle * PULSES_PER_REVOLUTION / 360.0f);
    
    // 执行运动
    QString msg = QString("[存储机构] 向后转位: 从位置 %1 到位置 %2 (角度: %3°)")
                      .arg(m_storageCurrentPosition + 1)
                      .arg(targetPosition + 1)
                      .arg(targetAngle, 0, 'f', 1);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
    
    // 使用绝对位置移动命令
    sprintf(cmdbuff, "MOVEABS(%d)=%f", STORAGE_MOTOR_ID, targetPulses);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString errMsg = QString("[存储机构] 错误: 执行运动失败");
        qDebug() << errMsg;
        ui->tb_cmdWindow_2->append(errMsg);
        return;
    }
    
    // 更新当前位置
    m_storageCurrentPosition = targetPosition;
    updateStorageStatus();
}

// 存储机构向前转位
void zmotionpage::on_btn_storage_forward_clicked()
{
    // 检查是否处于手动模式
    if (!ui->cb_modeManual->isChecked()) {
        QString msg = QString("[存储机构] 错误: 只能在手动模式下操作");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 检查控制器连接状态
    if (!g_handle) {
        QString msg = QString("[存储机构] 错误: 控制器未连接");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 计算目标位置索引（向前转位）
    int targetPosition = (m_storageCurrentPosition + 1) % STORAGE_POSITIONS;
    
    // 计算目标角度
    float targetAngle = targetPosition * STORAGE_ANGLE_PER_POSITION;
    
    // 设置运动参数
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    
    // 设置速度
    sprintf(cmdbuff, "SPEED(%d)=%f", STORAGE_MOTOR_ID, STORAGE_SPEED);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[存储机构] 错误: 设置速度失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 设置加速度
    sprintf(cmdbuff, "ACCEL(%d)=%f", STORAGE_MOTOR_ID, STORAGE_ACCEL);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[存储机构] 错误: 设置加速度失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 设置减速度
    sprintf(cmdbuff, "DECEL(%d)=%f", STORAGE_MOTOR_ID, STORAGE_DECEL);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[存储机构] 错误: 设置减速度失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 计算目标脉冲位置
    float targetPulses = m_storageOffset + (targetAngle * PULSES_PER_REVOLUTION / 360.0f);
    
    // 执行运动
    QString msg = QString("[存储机构] 向前转位: 从位置 %1 到位置 %2 (角度: %3°)")
                      .arg(m_storageCurrentPosition + 1)
                      .arg(targetPosition + 1)
                      .arg(targetAngle, 0, 'f', 1);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
    
    // 使用绝对位置移动命令
    sprintf(cmdbuff, "MOVEABS(%d)=%f", STORAGE_MOTOR_ID, targetPulses);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString errMsg = QString("[存储机构] 错误: 执行运动失败");
        qDebug() << errMsg;
        ui->tb_cmdWindow_2->append(errMsg);
        return;
    }
    
    // 更新当前位置
    m_storageCurrentPosition = targetPosition;
    updateStorageStatus();
}

// 更新旋转状态
void zmotionpage::updateRotationStatus()
{
    if (!g_handle) {
        return;
    }

    // 获取当前电机状态
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    sprintf(cmdbuff, "?MTYPE(%d)", ROTATION_MOTOR_ID);
    
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0) {
        int motionType = atoi(cmdbuffAck);
        
        // 如果电机停止但状态为旋转中，则更新状态
        if (motionType == 0 && m_isRotating) {
            m_isRotating = false;
            QString msg = QString("[旋转] 旋转已停止");
            qDebug() << msg;
            ui->tb_cmdWindow_2->append(msg);
        }
    }
}

// 更新冲击状态
void zmotionpage::updatePercussionStatus()
{
    if (!g_handle) {
        return;
    }

    // 获取当前电机状态
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    sprintf(cmdbuff, "?MTYPE(%d)", PERCUSSION_MOTOR_ID);
    
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0) {
        int motionType = atoi(cmdbuffAck);
        
        // 如果电机停止但状态为冲击中，则更新状态
        if (motionType == 0 && m_isPercussing) {
            m_isPercussing = false;
            QString msg = QString("[冲击] 冲击已停止");
            qDebug() << msg;
            ui->tb_cmdWindow_2->append(msg);
        }
    }
}

// 设置旋转速度
void zmotionpage::on_le_rotation_editingFinished()
{
    bool ok;
    float speed = ui->le_rotation->text().toFloat(&ok);
    
    if (!ok || speed <= 0) {
        QString msg = QString("[旋转] 错误: 无效的速度值");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        // 恢复原来的值
        ui->le_rotation->setText(QString::number(m_rotationSpeed, 'f', 1));
        return;
    }
    
    m_rotationSpeed = speed;
    QString msg = QString("[旋转] 已设置旋转速度: %1").arg(speed, 0, 'f', 1);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
    
    // 如果正在旋转，则更新当前速度
    if (m_isRotating) {
        on_btn_rotation_clicked();
    }
}

// 设置冲击频率
void zmotionpage::on_le_percussion_editingFinished()
{
    bool ok;
    float freq = ui->le_percussion->text().toFloat(&ok);
    
    if (!ok || freq <= 0) {
        QString msg = QString("[冲击] 错误: 无效的频率值");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        // 恢复原来的值
        ui->le_percussion->setText(QString::number(m_percussionFrequency, 'f', 1));
        return;
    }
    
    m_percussionFrequency = freq;
    QString msg = QString("[冲击] 已设置冲击频率: %1").arg(freq, 0, 'f', 1);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
    
    // 如果正在冲击，则更新当前频率
    if (m_isPercussing) {
        on_btn_percussion_clicked();
    }
}

// 开始旋转
void zmotionpage::on_btn_rotation_clicked()
{
    // 检查是否处于手动模式
    if (!ui->cb_modeManual->isChecked()) {
        QString msg = QString("[旋转] 错误: 只能在手动模式下操作");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 检查控制器连接状态
    if (!g_handle) {
        QString msg = QString("[旋转] 错误: 控制器未连接");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 设置运动参数
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    
    // 设置速度
    sprintf(cmdbuff, "SPEED(%d)=%f", ROTATION_MOTOR_ID, m_rotationSpeed);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[旋转] 错误: 设置速度失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 设置加速度
    sprintf(cmdbuff, "ACCEL(%d)=%f", ROTATION_MOTOR_ID, ROTATION_ACCEL);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[旋转] 错误: 设置加速度失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 设置减速度
    sprintf(cmdbuff, "DECEL(%d)=%f", ROTATION_MOTOR_ID, ROTATION_DECEL);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[旋转] 错误: 设置减速度失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 执行连续运动（正向）
    sprintf(cmdbuff, "FORWARD(%d)", ROTATION_MOTOR_ID);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[旋转] 错误: 启动旋转失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 更新状态
    m_isRotating = true;
    QString msg = QString("[旋转] 开始旋转，速度: %1").arg(m_rotationSpeed, 0, 'f', 1);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
}

// 停止旋转
void zmotionpage::on_btn_rotation_stop_clicked()
{
    // 检查控制器连接状态
    if (!g_handle) {
        QString msg = QString("[旋转] 错误: 控制器未连接");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 如果不在旋转状态，则不需要停止
    if (!m_isRotating) {
        QString msg = QString("[旋转] 当前未在旋转");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 执行停止命令
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    sprintf(cmdbuff, "CANCEL(%d)", ROTATION_MOTOR_ID);
    
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[旋转] 错误: 停止旋转失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 更新状态
    m_isRotating = false;
    QString msg = QString("[旋转] 已停止旋转");
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
}

// 开始冲击
void zmotionpage::on_btn_percussion_clicked()
{
    // 检查是否处于手动模式
    if (!ui->cb_modeManual->isChecked()) {
        QString msg = QString("[冲击] 错误: 只能在手动模式下操作");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 检查控制器连接状态
    if (!g_handle) {
        QString msg = QString("[冲击] 错误: 控制器未连接");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 设置运动参数
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    
    // 设置速度（根据频率计算）
    float speed = m_percussionFrequency * 60.0f; // 将频率(Hz)转换为速度(rpm)
    sprintf(cmdbuff, "SPEED(%d)=%f", PERCUSSION_MOTOR_ID, speed);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[冲击] 错误: 设置速度失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 设置加速度
    sprintf(cmdbuff, "ACCEL(%d)=%f", PERCUSSION_MOTOR_ID, PERCUSSION_ACCEL);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[冲击] 错误: 设置加速度失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 设置减速度
    sprintf(cmdbuff, "DECEL(%d)=%f", PERCUSSION_MOTOR_ID, PERCUSSION_DECEL);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[冲击] 错误: 设置减速度失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 执行连续运动（正向）
    sprintf(cmdbuff, "FORWARD(%d)", PERCUSSION_MOTOR_ID);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[冲击] 错误: 启动冲击失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 更新状态
    m_isPercussing = true;
    QString msg = QString("[冲击] 开始冲击，频率: %1 Hz").arg(m_percussionFrequency, 0, 'f', 1);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
}

// 停止冲击
void zmotionpage::on_btn_percussion_stop_clicked()
{
    // 检查控制器连接状态
    if (!g_handle) {
        QString msg = QString("[冲击] 错误: 控制器未连接");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 如果不在冲击状态，则不需要停止
    if (!m_isPercussing) {
        QString msg = QString("[冲击] 当前未在冲击");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 执行停止命令
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    sprintf(cmdbuff, "CANCEL(%d)", PERCUSSION_MOTOR_ID);
    
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[冲击] 错误: 停止冲击失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 更新状态
    m_isPercussing = false;
    QString msg = QString("[冲击] 已停止冲击");
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
}

// 打开夹爪
void zmotionpage::on_btn_downclamp_open_clicked()
{
    if (!g_handle) {
        QString msg = QString("[夹爪] 错误: 控制器未连接");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    char cmdbuff[2048];
    char cmdbuffAck[2048];

    // 设置力矩模式
    sprintf(cmdbuff, "ATYPE(%d)=67", DOWNCLAMP_MOTOR_ID);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[夹爪] 错误: 设置力矩模式失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 设置负向力矩（打开方向）
    sprintf(cmdbuff, "DAC(%d)=%f", DOWNCLAMP_MOTOR_ID, -m_downclampDAC);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[夹爪] 错误: 设置力矩值失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    QString msg = QString("[夹爪] 开始打开 (力矩值: %1)").arg(-m_downclampDAC);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);

    m_isDownclamping = true;
    m_downclampTimer->start(); // 开始监控位置变化
}

// 关闭夹爪
void zmotionpage::on_btn_downclamp_close_clicked()
{
    if (!g_handle) {
        QString msg = QString("[夹爪] 错误: 控制器未连接");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    char cmdbuff[2048];
    char cmdbuffAck[2048];

    // 设置力矩模式
    sprintf(cmdbuff, "ATYPE(%d)=67", DOWNCLAMP_MOTOR_ID);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[夹爪] 错误: 设置力矩模式失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 设置正向力矩（关闭方向）
    sprintf(cmdbuff, "DAC(%d)=%f", DOWNCLAMP_MOTOR_ID, m_downclampDAC);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[夹爪] 错误: 设置力矩值失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    QString msg = QString("[夹爪] 开始关闭 (力矩值: %1)").arg(m_downclampDAC);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);

    m_isDownclamping = true;
    m_downclampTimer->start(); // 开始监控位置变化
}

// 设置夹爪力矩
void zmotionpage::on_le_downclamp_DAC_editingFinished()
{
    bool ok;
    float dac = ui->le_downclamp_DAC->text().toFloat(&ok);
    
    if (!ok || dac <= 0) {
        QString msg = QString("[夹爪] 错误: 无效的力矩值");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        // 恢复原来的值
        ui->le_downclamp_DAC->setText(QString::number(m_downclampDAC, 'f', 1));
        return;
    }

    m_downclampDAC = dac;
    QString msg = QString("[夹爪] 设置力矩值: %1").arg(m_downclampDAC);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
}

// 更新夹爪状态
void zmotionpage::updateDownclampStatus()
{
    if (!g_handle || !m_isDownclamping) {
        m_downclampTimer->stop();
        return;
    }

    static float lastPosition = 0.0f;
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    static QTime startTime = QTime::currentTime();

    // 获取当前位置
    sprintf(cmdbuff, "MPOS(%d)", DOWNCLAMP_MOTOR_ID);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0) {
        float currentPosition = atof(cmdbuffAck);
        
        // 更新位置显示
        ui->le_downclamp_status->setText(QString::number(currentPosition, 'f', 2));

        // 检查位置是否变化
        if (abs(currentPosition - lastPosition) < DOWNCLAMP_POSITION_THRESHOLD) {
            // 如果位置几乎不变，说明可能到达极限
            if (startTime.elapsed() > DOWNCLAMP_CHECK_INTERVAL) {
                // 停止力矩输出
                sprintf(cmdbuff, "DAC(%d)=0", DOWNCLAMP_MOTOR_ID);
                ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048);
                
                QString msg = QString("[夹爪] 到达极限位置，停止运动");
                qDebug() << msg;
                ui->tb_cmdWindow_2->append(msg);
                
                m_isDownclamping = false;
                m_downclampTimer->stop();
            }
        } else {
            // 位置在变化，重置计时器
            startTime = QTime::currentTime();
        }
        
        lastPosition = currentPosition;
    }

    // 检查是否超时
    if (startTime.elapsed() > DOWNCLAMP_TIMEOUT) {
        // 超时停止
        sprintf(cmdbuff, "DAC(%d)=0", DOWNCLAMP_MOTOR_ID);
        ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048);
        
        QString msg = QString("[夹爪] 运动超时，停止运动");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        
        m_isDownclamping = false;
        m_downclampTimer->stop();
    }
}