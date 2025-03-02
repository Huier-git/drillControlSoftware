#include "zmotionpage.h"
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
{
    ui->setupUi(this);

    initflag = false;                                               //初始化标志位
    ui->btn_BusInit->setEnabled(false);                             //关闭初始化按钮
    //===基础信息刷新定时器===
    basicInfoTimer = new QTimer(this);                              //创建基础信息刷新的定时器，定时器在Businit后开始
    connect(basicInfoTimer, &QTimer::timeout, this, &zmotionpage::basicInfoRefresh);
    //===高级信息刷新定时器===
    advanceInfoTimer = new QTimer(this);                            //创建高级信息刷新的定时器
    connect(advanceInfoTimer, &QTimer::timeout, this, &zmotionpage::advanceInfoRefreash);
    connect(ui->CB_AutoUpdate, &QCheckBox::stateChanged, [=](int state) {
        if (state == Qt::Checked) {
            advanceInfoTimer->start(1000);                           // 启动定时器
        } else {
            advanceInfoTimer->stop();                               // 停止定时器
        }
    });
    //===表格信息刷新定时器===
    //当处于Edit模式的时候需要暂停
    realtimeParmTimer = new QTimer(this);
    connect(realtimeParmTimer, &QTimer::timeout, this, &zmotionpage::RefreshTableRealTimeContent);
    connect(ui->cb_motorRtRefrsh, &QCheckBox::stateChanged, [=](int state){
        if (state == Qt::Checked) {
            realtimeParmTimer->start(500);  // 启动定时器
            qDebug()<<"3";
        } else {
            realtimeParmTimer->stop();      // 停止定时器
            qDebug()<<"4";
        }
    });
    //===高级信息手动刷新按钮===
    connect(ui->Btn_Update, &QPushButton::clicked, this, &zmotionpage::advanceInfoRefreash);        // 普通参数刷新按钮
    connect(ui->Btn_Update, &QPushButton::clicked, [=](){
        if(ui->CB_AxisNum->count() == fAxisNum)
            return;
        ui->CB_AxisNum->clear();
        for(int i = 0; i < fAxisNum; ++i)                                           // 更新combox里的轴
        {
            ui->CB_AxisNum->addItem(QString::number(i));
        }
    });
    //===手动刷新电机表格===
    connect(ui->btn_motorParmUpdate, &QPushButton::clicked, [=]{    // 电机参数表格刷新按钮
        initMotorTable();//初始化表格
        RefreshTableContent();//刷新表格的内容
    });
    //===开启电机表格编辑模式===
    connect(ui->cb_motorParmEdit, &QCheckBox::stateChanged, [=](int state) {                        // 开启编辑的checkbox
        if (state == Qt::Checked) {
            ui->btn_motorParmUpdate->setEnabled(false);
            //ui->cb_motorRtRefrsh->setChecked(false);
            //ui->cb_motorRtRefrsh->setCheckable(false);
            //PauseAllAxis();                                                                                 // 编辑模式下暂停所有轴的运动
            connect(ui->tb_motor, &QTableWidget::cellDoubleClicked, this,&zmotionpage::unmodifyMotorTable);
            connect(ui->tb_motor, &QTableWidget::itemChanged, this, &zmotionpage::modifyMotorTable);        // 开启找出编辑的内容
            //disconnect(ui->btn_moveResume, &QPushButton::clicked, this, &zmotionpage::ResumeAllAxis);       // 编辑模式下不能直接恢复运动
        } else {
            ui->btn_motorParmUpdate->setEnabled(true);
            //ui->cb_motorRtRefrsh->setCheckable(true);
            disconnect(ui->tb_motor, &QTableWidget::cellDoubleClicked, this,&zmotionpage::unmodifyMotorTable);
            disconnect(ui->tb_motor, &QTableWidget::itemChanged, this, &zmotionpage::modifyMotorTable);     // 关闭找出编辑的内容
            //connect(ui->btn_moveResume, &QPushButton::clicked, this, &zmotionpage::ResumeAllAxis);          // 退出编辑模式后允许恢复运动
            initMotorTable();       //结束后自动刷新一下表格
            RefreshTableContent();
        }
    });
    //===手动暂停轴的运动===
    connect(ui->btn_movePause, &QPushButton::clicked, this, [=]{
        char  cmdbuff[2048];
        char  cmdbuffAck[2048];
        //生成命令
        sprintf(cmdbuff, "MOVE_PAUSE");
        //调用命令执行函数
        if(ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0)
        {
            qDebug()<< "Pause All!";
        }
    });
    //===手动恢复轴的运动===
    connect(ui->btn_moveResume, &QPushButton::clicked, this, [=]{
        char  cmdbuff[2048];
        char  cmdbuffAck[2048];
        //生成命令
        sprintf(cmdbuff, "MOVE_RESUME");
        //调用命令执行函数
        if(ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0)
        {
            qDebug()<< "Resume All!";
        }
    });
    //===EtherCAT序号重新映射===
    connect(ui->btn_MapMotor, &QPushButton::clicked, this, [=](){
        for(int i = 0; i < 10; i++)
        {
            MotorMap[i] = MotorMapbuckup[i];
        }
        if(fAxisNum == 1)
        {
            MotorMap[0] = 0;
        }
        else if(fAxisNum >= 2)
        {
            MotorMap[0] = fAxisNum - 1;
            MotorMap[1] = fAxisNum - 2;
        }
        ShowMotorMap();
    });

    //===自动模式和手动模式的选框的初始化
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
    // 设置cb_modeManual为默认选中
    ui->cb_modeManual->setChecked(true);
    // 确定按钮绑定函数
    connect(ui->btn_modeConfirm, &QPushButton::clicked, this, &zmotionpage::runningMode);

    mdbProcessor = new mdbprocess(this);
    connect(mdbProcessor, &mdbprocess::dataReceived, this, &zmotionpage::handleReceivedData);

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
    //char *ipaddress = "192.168.1.222";
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
        return;
    }
    iresult = ZAux_OpenEth(ipaddress, &g_handle);                   // 与控制器建立连接
    if (iresult == ERR_OK)                                          // 连接成功 ERR_OK
    {
        qDebug() << "[C] Controller connected.";
        ui->btn_BusInit->setEnabled(true);                          // 连接成功，可以初始化，解锁BusInit按钮
        ui->btn_IP_Connect->setText("Disconnect");
    }
    else                                                            // 连接失败
    {
        qDebug() << "[C] Controller connection failed.";
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
        qDebug() << "自动模式线程启动";

        // 示例：请求显示电机映射
        emit requestShowMotorMap();

//        ZAux_Direct_SetAxisEnable(g_handle,MotorMap[0],1);
//        sleep(10);
//        ZAux_Direct_SetAxisEnable(g_handle,MotorMap[1],1);
//        sleep(10);
//        ZAux_Direct_SetAxisEnable(g_handle,MotorMap[3],1);
//        sleep(10);
//        ZAux_Direct_SetAxisEnable(g_handle,MotorMap[4],1);


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
        connect(m_autoModeThread, &AutoModeThread::requestShowMotorMap, this, [this]()
                {
                    // 实现显示电机映射的逻辑
                    ui->tb_cmdWindow->append("显示电机映射");
                    ShowMotorMap();
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

