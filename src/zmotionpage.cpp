#include "inc/zmotionpage.h"
#include "inc/Global.h"
#include "ui_zmotionpage.h"

// 电机映射表，EtherCAT的映射关系
// 使用常量定义每个电机的默认映射
int MotorMap[10] = {
    0,  // MOTOR_IDX_ROTATION (旋转切割电机)
    1,  // MOTOR_IDX_PERCUSSION (冲击电机)
    2,  // MOTOR_IDX_PENETRATION (进给电机)
    3,  // MOTOR_IDX_DOWNCLAMP (下夹紧电机)
    4,  // MOTOR_IDX_ROBOTCLAMP (机械手夹紧电机)
    5,  // MOTOR_IDX_ROBOTROTATION (机械手旋转电机)
    6,  // MOTOR_IDX_ROBOTEXTENSION (机械手移动电机)
    7,  // MOTOR_IDX_STORAGE (存储电机)
    8,  // M8
    9   // M9
};
float fAxisNum;                                         // 总线上的轴数量

// 电机模式常量
const int POSITION_MODE = 65;                         // 位置模式类型码
const int TORQUE_MODE = 67;                           // 力矩模式类型码
const int VELOCITY_MODE = 66;                         // 速度模式类型码

// 机械手夹爪相关常量
const int ROBOTARM_CLAMP_TORQUE_MODE = TORQUE_MODE;   // 力矩模式类型码
const int ROBOTARM_CLAMP_POSITION_MODE = POSITION_MODE; // 位置模式类型码
const float ROBOTARM_CLAMP_INIT_DAC = 10.0f;          // 初始DAC值
const float ROBOTARM_CLAMP_MAX_DAC = 80.0f;           // 最大DAC值
const float ROBOTARM_CLAMP_DAC_INCREMENT = 10.0f;     // DAC增量
const float ROBOTARM_CLAMP_STABLE_THRESHOLD = 1.0f;   // 位置稳定阈值
const int ROBOTARM_CLAMP_STABLE_COUNT = 5;            // 位置稳定计数
const int ROBOTARM_CLAMP_MONITOR_INTERVAL = 500;      // 监控间隔(毫秒)

// 下夹紧初始化相关常量
const float DOWNCLAMP_INIT_START_DAC = -10.0f;        // 初始DAC值(负值)
const float DOWNCLAMP_INIT_MIN_DAC = -50.0f;          // 最小DAC值(负值)
const float DOWNCLAMP_INIT_DAC_STEP = -10.0f;         // DAC步进值(负值)
const float DOWNCLAMP_POSITION_TOLERANCE = 1.0f;      // 位置稳定阈值
const int DOWNCLAMP_STABLE_COUNT = 5;                 // 位置稳定计数
const int INIT_TIMER_INTERVAL = 500;                  // 监控间隔(毫秒)
const float DOWNCLAMP_DEFAULT_SPEED = 2000.0f;        // 下夹紧默认速度
const float ACCEL_RATIO = 5.0f;                       // 加速度比例(相对于速度)
const float DECEL_RATIO = 5.0f;                       // 减速度比例(相对于速度)

// 一键对接相关常量
const float CONNECT_FAST_MIN_POSITION = 7500000.0f;    // 最小进给位置
const float CONNECT_FAST_PENETRATION_SPEED = 13596.0f; // 进给速度
const int CONNECT_FAST_ROTATION_TORQUE_MODE = VELOCITY_MODE; // 旋转电机力矩模式
const float CONNECT_FAST_ROTATION_DAC = 90.0f;         // 旋转电机DAC值
const float CONNECT_FAST_PENETRATION_DISTANCE = 600000.0f; // 进给距离
const float CONNECT_FAST_POSITION_TOLERANCE = 100.0f;  // 位置公差
const float POSITION_REPORT_THRESHOLD = 1000.0f;       // 位置报告阈值
const int CONNECT_FAST_MONITOR_INTERVAL = 100;         // 监控间隔(毫秒)

/**
 * @brief 构造函数 - 初始化界面及所有控制组件
 * @param parent 父窗口
 */
zmotionpage::zmotionpage(QWidget *parent)
    : QWidget(parent)
    , initflag(false)
    , m_autoModeThread(nullptr)
    , m_isAutoModeRunning(false)
    , m_isPercussing(false)
    , ui(new Ui::zmotionpage)
    , m_rotationMotorID(MOTOR_IDX_ROBOTROTATION)    // 机械手旋转电机ID
    , m_extentMotorID(MOTOR_IDX_ROBOTEXTENSION)     // 机械手移动电机ID
    , m_clampMotorID(MOTOR_IDX_ROBOTCLAMP)          // 机械手夹紧电机ID
    , m_rotationOffset(0.0f)
    , m_extentOffset(0.0f)
    , m_robotArmStatusTimer(nullptr)
    , m_penetrationMotorID(MOTOR_IDX_PENETRATION)   // 进给电机ID
    , m_penetrationSpeed(PENETRATION_DEFAULT_SPEED)
    , m_penetrationTargetDepth(0.0)
    , m_penetrationOffset(0.0)
    , m_storageCurrentPosition(0)  // 初始化为第一个位置
    , m_storageOffset(0.0f)        // 初始化零点偏移量
    , m_rotationSpeed(DEFAULT_ROTATION_SPEED)  // 初始化旋转速度
    , m_percussionFrequency(DEFAULT_PERCUSSION_FREQ)  // 初始化冲击频率
    , m_isRotating(false)          // 初始化为未旋转状态
{
    // 首先设置UI
    ui->setupUi(this);

    // 创建运动控制器实例
    m_motionController = new MotionController(this);

    // 初始化所有定时器
    initializeTimers();

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
                         
/**
 * @brief 连接信号和槽
 */
void zmotionpage::connectSignalsAndSlots()
{
    // 连接自动更新复选框
    connect(ui->CB_AutoUpdate, &QCheckBox::stateChanged, [this](int state) {
        if (state == Qt::Checked) {
            if (!m_advanceInfoTimer->isActive()) {
                m_advanceInfoTimer->start();
            }
        } else {
            if (m_advanceInfoTimer->isActive()) {
                m_advanceInfoTimer->stop();
            }
        }
    });

    // 连接实时参数刷新复选框
    connect(ui->cb_motorRtRefrsh, &QCheckBox::stateChanged, [this](int state) {
        if (state == Qt::Checked) {
            if (!m_realtimeParmTimer->isActive()) {
                m_realtimeParmTimer->start();
            }
        } else {
            if (m_realtimeParmTimer->isActive()) {
                m_realtimeParmTimer->stop();
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

    // 连接夹爪控制信号
    connect(ui->btn_downclamp_open, &QPushButton::clicked, this, &zmotionpage::on_btn_downclamp_open_clicked);
    connect(ui->btn_downclamp_close, &QPushButton::clicked, this, &zmotionpage::on_btn_downclamp_close_clicked);
    
    // 下夹紧初始化按钮连接
    connect(ui->btn_DOWNCLAMP_init, &QPushButton::clicked, this, &zmotionpage::on_btn_DOWNCLAMP_init_clicked);
    
    // 机械手移动初始化按钮连接 - 检查按钮是否存在
    connect(ui->btn_ROBOTEXTENSION_init, &QPushButton::clicked, this, &zmotionpage::on_btn_ROBOTEXTENSION_init_clicked);
    
    // 机械手夹爪初始化按钮连接
    connect(ui->btn_ROBOTCLAMP_init, &QPushButton::clicked, this, &zmotionpage::on_btn_ROBOTCLAMP_init_clicked);
    
    // 一键对接按钮连接
    connect(ui->btn_connect_fast, &QPushButton::clicked, this, &zmotionpage::on_btn_connect_fast_clicked);
}

zmotionpage::~zmotionpage()
{
    // 确保停止所有定时器
    stopMonitoringTimers();
    
    delete ui;
    stopAutoMode();
}

/* ===================================== 工具函数 ===================================== */
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

/* ========================================================================================= */
/*                                       操作页面一的函数                                       */
/* ========================================================================================= */
/* ===================================== 基本按钮功能函数 ===================================== */
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

/**
 * @brief 恢复所有轴的运动
 */
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
        ipos = sscanf(pstring, "%255s", buffer2);  // 限制最大读取长度并使用正确的指针类型

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
        m_basicInfoTimer->stop();
        m_advanceInfoTimer->stop();
        m_realtimeParmTimer->stop();
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
        m_motionController->setControllerHandle(g_handle);          // 设置控制器句柄
    }
    else                                                            // 连接失败
    {
        qDebug() << "[C] Controller connection failed.";
        ui->tb_cmdWindow->append("错误：控制器连接失败");
    }

    // 启动定时器
    if (initflag) {
        if (m_basicInfoTimer) {
            m_basicInfoTimer->start(TIMER_BASIC_INFO_INTERVAL);
        }
        
        // 启动机械手状态更新定时器
        if (m_robotArmStatusTimer) {
            m_robotArmStatusTimer->start(TIMER_ROBOTARM_STATUS_INTERVAL);
        }
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

    m_basicInfoTimer->start(TIMER_BASIC_INFO_INTERVAL);                                     // 开始基础的定时器，使用定义的时间间隔
    qDebug() << "[C] Controller running task.";
    
    // 如果机器人状态更新定时器尚未启动，询问是否要启动
    if (m_robotArmStatusTimer && !m_robotArmStatusTimer->isActive()) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("启动模式选择");
        msgBox.setText("是否启动自动模式？");
        msgBox.setInformativeText("自动模式将自动刷新机械手状态信息");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        int ret = msgBox.exec();
        
        if (ret == QMessageBox::Yes) {
            // 用户选择启动自动模式
            m_robotArmStatusTimer->start(500);
        }
    }
}
/* ===================================== 信息刷新函数 ===================================== */
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

/* ===================================== 命令行功能函数 ===================================== */
/**
 * @brief 处理Modbus命令，用于发送命令
 */
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

/**
 * @brief 解析十六进制或十进制字符串
 * @param str 输入字符串
 * @return 解析后的整数值
 */
int zmotionpage::parseHexOrDec(const QString& str)
{
    bool ok;
    int value = str.toInt(&ok, 16);
    if (!ok) {
        value = str.toInt(&ok, 10);
    }
    return value;
}

/**
 * @brief 处理接收到的数据
 * @param data 接收到的数据
 * @param startReg 起始寄存器地址
 */
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
/**
 * @brief 处理Zmotion命令
 * @param cmd 输入的命令
 */
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

/* ===================================== 电机参数表格函数 ===================================== */
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

    // 启动定时器
    m_basicInfoTimer->start(TIMER_BASIC_INFO_INTERVAL);
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
/* ===================================== 基础电机控制函数 ===================================== */
/**
 * @brief 启用或禁用电机
 */ 
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
    
    // 停止机械手移动初始化定时器
    if (m_robotExtensionInitTimer) {
        m_robotExtensionInitTimer->stop();
        ui->tb_cmdWindow_2->append("[紧急停止] 停止机械手移动初始化过程");
        qDebug() << "[紧急停止] 停止机械手移动初始化过程";
        
        // 重置电机状态
        int mappedMotorID = MotorMap[MOTOR_IDX_ROBOTEXTENSION];
        ZAux_Direct_SetDAC(g_handle, mappedMotorID, 0); // 停止力矩输出
        ZAux_Direct_SetAtype(g_handle, mappedMotorID, 65); // 设回位置模式
        
        m_robotExtensionInitTimer->deleteLater();
        m_robotExtensionInitTimer = nullptr;
    }
    
    // 停止机械手夹爪初始化定时器
    if (m_robotClampInitTimer) {
        m_robotClampInitTimer->stop();
        ui->tb_cmdWindow_2->append("[紧急停止] 停止机械手夹爪初始化过程");
        qDebug() << "[紧急停止] 停止机械手夹爪初始化过程";
        
        // 重置电机状态
        int mappedMotorID = MotorMap[MOTOR_IDX_ROBOTCLAMP];
        ZAux_Direct_SetDAC(g_handle, mappedMotorID, 0); // 停止力矩输出
        ZAux_Direct_SetAtype(g_handle, mappedMotorID, 65); // 设回位置模式
        
        m_robotClampInitTimer->deleteLater();
        m_robotClampInitTimer = nullptr;
    }
    
    // 停止下夹紧初始化定时器
    if (m_downclampInitTimer) {
        m_downclampInitTimer->stop();
        ui->tb_cmdWindow_2->append("[紧急停止] 停止下夹紧初始化过程");
        qDebug() << "[紧急停止] 停止下夹紧初始化过程";
        
        // 重置电机状态
        int mappedMotorID = MotorMap[MOTOR_IDX_DOWNCLAMP];
        ZAux_Direct_SetDAC(g_handle, mappedMotorID, 0); // 停止力矩输出
        ZAux_Direct_SetAtype(g_handle, mappedMotorID, 65); // 设回位置模式
        
        m_downclampInitTimer->deleteLater();
        m_downclampInitTimer = nullptr;
    }
    
    // 停止一键对接操作
    if (m_connectFastRunning) {
        m_connectFastRunning = false;
        ui->tb_cmdWindow_2->append("[紧急停止] 停止一键对接操作");
        qDebug() << "[紧急停止] 停止一键对接操作";
        
        // 重置相关电机状态
        ZAux_Direct_Single_Cancel(g_handle, MotorMap[MOTOR_IDX_PENETRATION], 0); // 停止进给电机
        ZAux_Direct_Single_Cancel(g_handle, MotorMap[MOTOR_IDX_ROTATION], 0); // 停止旋转电机
        ZAux_Direct_SetDAC(g_handle, MotorMap[MOTOR_IDX_ROTATION], 0); // 停止力矩输出
    }
    
    if(ret == 0) {
        qDebug() << "[Waring] Radpid Stop!";
        ui->tb_cmdWindow->append("[Waring] Radpid Stop!");
    } else {
        qDebug() << "[Error] Radpid Stop Failed! Error code:" << ret;
        ui->tb_cmdWindow->append(QString("[Error] Radpid Stop Failed! Error code: %1").arg(ret));
    }
}

/**
 * @brief 显示电机映射
 */
void zmotionpage::ShowMotorMap()
{
    qDebug() << "Motor map:" << MotorMap;
    
    // 格式化映射显示，使其更直观
    QString mapInfo = "[电机映射] 当前映射:\n";
    for (int i = 0; i < 10; ++i) {
        mapInfo += QString("M%1 -> %2\n").arg(i).arg(MotorMap[i]);
    }
    ui->tb_cmdWindow->append(mapInfo);
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

/* ===================================== 页面一的自动模式函数 ===================================== */
// ==== 简易的自动模式，主要就是自动进给+冲击，测试用的 =====
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
// 进给电机 (MOTOR_IDX_PENETRATION) 相关常量
const int PENETRATION_UP_SPEED = 109785;       // 进给电机上升速度 500mm/min
const int PENETRATION_UP_POSITION = 13100000;  // 进给电机顶部位置
const int PENETRATION_DOWN_SPEED = 5489;       // 进给电机下降速度
const int PENETRATION_DIRT_POS = 10100000;     // 进给电机泥土位置

const int DOWN_FORCE_THRESHOLD = 400;          // 下压力阈值
const int ROTATION_DAC_VALUE = 850000;         // 旋转电机DAC值
const int PERCUSSION_DAC_VALUE = -1075;        // 冲击电机DAC值  -419430 -1075
const float POSITION_TOLERANCE = 1000.0;       // 位置容忍
const float MIN_SPEED_THRESHOLD = 0.1;         // 最小速度阈值 0.1
const int SLEEP_DURATION = 100;                // 睡眠时长 ms
const int DONE_WAIT_DURATION = 5000;           // 完成等待时长 ms

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

        // 进给电机上升到最高点
        if (ZAux_Direct_SetSpeed(g_handle, MotorMap[MOTOR_IDX_PENETRATION], PENETRATION_UP_SPEED) != 0 ||
            ZAux_Direct_Single_MoveAbs(g_handle, MotorMap[MOTOR_IDX_PENETRATION], PENETRATION_UP_POSITION) != 0)
        {
            qDebug() << "设置速度或移动到最高点失败";
            emit operationCompleted();
            return;
        }

        // 等待进给电机到达目标位置
        while (!m_stopFlag.load())
        {
            float currentPosition;
            ZAux_Direct_GetMpos(g_handle, MotorMap[MOTOR_IDX_PENETRATION], &currentPosition);
            qDebug() << "当前进给电机位置:" << currentPosition; // 新增日志
            if (std::abs(currentPosition - PENETRATION_UP_POSITION) < POSITION_TOLERANCE)
            {
                qDebug() << "进给电机已到达最高点";
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

        // 进给电机下降
        qDebug()<<"DOWN";
        int ret = ZAux_Direct_SetSpeed(g_handle, MotorMap[MOTOR_IDX_PENETRATION], PENETRATION_UP_SPEED);
        if (ret!= 0) // 检查返回值
        {
            qDebug() << "设置进给电机下降速度失败";
            emit operationCompleted();
            return;
        }
        // 进给电机降到指定位置
        qDebug()<<"DOWN DONE";
        ret = ZAux_Direct_Single_MoveAbs(g_handle, MotorMap[MOTOR_IDX_PENETRATION], PENETRATION_DIRT_POS);
        if ( ret!= 0)
        {
            qDebug() << "移动到指定位置失败";
            emit operationCompleted();
            return;
        }

        // 等待进给电机到达目标位置
        while (!m_stopFlag.load())
        {
            float currentPosition;
            ZAux_Direct_GetMpos(g_handle, MotorMap[MOTOR_IDX_PENETRATION], &currentPosition);
            //qDebug() << "当前进给电机位置:" << currentPosition; // 新增日志
            if (std::abs(currentPosition - PENETRATION_DIRT_POS) < POSITION_TOLERANCE)
            {
                qDebug() << "进给电机已到达指定位置";
                break;
            }
            msleep(SLEEP_DURATION);
        }

        ret = ZAux_Direct_SetSpeed(g_handle, MotorMap[MOTOR_IDX_PENETRATION], PENETRATION_DOWN_SPEED);
        if ( ret!= 0) // 检查返回值
        {
            qDebug() << "设置进给电机下降速度失败";
            emit operationCompleted();
            return;
        }

        // 设置旋转电机和冲击电机
#ifdef Motor2useHall
        ZAux_Direct_SetAccel(g_handle, MotorMap[MOTOR_IDX_PERCUSSION], Motor2acc);
        ZAux_Direct_SetDecel(g_handle, MotorMap[MOTOR_IDX_PERCUSSION], Motor2acc);
#endif
        ZAux_Direct_SetAtype(g_handle, MotorMap[MOTOR_IDX_ROTATION], 66);
        msleep(10);
        ZAux_Direct_SetAtype(g_handle, MotorMap[MOTOR_IDX_PERCUSSION], 66);
        msleep(10);
        ZAux_Direct_SetDAC(g_handle, MotorMap[MOTOR_IDX_PERCUSSION], PERCUSSION_DAC_VALUE);
        msleep(10);
        ZAux_Direct_SetAxisEnable(g_handle, MotorMap[MOTOR_IDX_PERCUSSION], 1);
        ZAux_Direct_SetDAC(g_handle, MotorMap[MOTOR_IDX_ROTATION], ROTATION_DAC_VALUE);
        msleep(100);

        ZAux_Direct_Single_MoveAbs(g_handle, MotorMap[MOTOR_IDX_PENETRATION], 0);

        // 监控下降过程
        while (!m_stopFlag.load())
        {
            float currentPosition, currentSpeed;
            ZAux_Direct_GetMpos(g_handle, MotorMap[MOTOR_IDX_PENETRATION], &currentPosition);
            msleep(10);
            ZAux_Direct_GetMspeed(g_handle, MotorMap[MOTOR_IDX_ROTATION], &currentSpeed);
            //qDebug() << "当前进给电机位置:" << currentPosition << " 速度:" << currentSpeed; // 新增日志
            //qDebug() << "(int)(downForce)" << (int)(downForce) << "DOWN_FORCE_THRESHOLD" << DOWN_FORCE_THRESHOLD;
            if (std::abs(currentSpeed) < MIN_SPEED_THRESHOLD || currentPosition <= 0 || (int)(downForce) >= DOWN_FORCE_THRESHOLD)
            {
                qDebug() << "条件满足，停止冲击和旋转"; // 新增日志
                // 停止冲击和旋转

                ZAux_Direct_SetDAC(g_handle, MotorMap[MOTOR_IDX_ROTATION], 0);
                msleep(10);
                ZAux_Direct_SetAxisEnable(g_handle, MotorMap[MOTOR_IDX_PERCUSSION], 0);
                ZAux_Direct_SetDAC(g_handle, MotorMap[MOTOR_IDX_PERCUSSION], 0);

                //

                ZAux_Direct_Single_Cancel(g_handle, MotorMap[MOTOR_IDX_PENETRATION], 2);

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
        if (ZAux_Direct_SetSpeed(g_handle, MotorMap[MOTOR_IDX_PENETRATION], PENETRATION_UP_SPEED) != 0 ||
            ZAux_Direct_Single_MoveAbs(g_handle, MotorMap[MOTOR_IDX_PENETRATION], PENETRATION_UP_POSITION) != 0)
        {
            qDebug() << "返回到最高点失败";
            emit operationCompleted();
            return;
        }

        // 等待进给电机到达最高点
        while (!m_stopFlag.load())
        {
            float currentPosition;
            ZAux_Direct_GetMpos(g_handle, MotorMap[MOTOR_IDX_PENETRATION], &currentPosition);
            //qDebug() << "当前进给电机位置:" << currentPosition; // 新增日志
            if (std::abs(currentPosition - PENETRATION_UP_POSITION) < POSITION_TOLERANCE)
            {
                qDebug() << "进给电机已返回到最高点";
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
        
        // 确保状态监控定时器在自动模式下运行
        startMonitoringTimers();
    }
    else if (ui->cb_modeManual->isChecked())
    {
        QString info = "切换到手动模式。";
        qDebug() << info;
        ui->tb_cmdWindow->append(info);
        stopAutoMode();
        
        // 在手动模式下，也需要状态信息，但可能频率不同
        // 可以考虑调整定时器间隔，但不要完全停止
        if (!m_basicInfoTimer->isActive()) {
            m_basicInfoTimer->start();
        }
        
        if (!m_advanceInfoTimer->isActive() && ui->CB_AutoUpdate->isChecked()) {
            m_advanceInfoTimer->start();
        }
        
        if (!m_realtimeParmTimer->isActive() && ui->cb_motorRtRefrsh->isChecked()) {
            m_realtimeParmTimer->start();
        }
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
    
    // 开始读取状态
    mdbProcessor->ReadValue(CONNECTION_MODBUS_DEVICE_INDEX, 
                           CONNECTION_MODBUS_SLAVE_ID, 
                           0x00, 1, 4);
    
    // 启动状态监控定时器
    if (!m_connectionStatusTimer->isActive()) {
        m_connectionStatusTimer->start();
    }
    
    qDebug() << "[推杆] 已连接，开始监控状态";
    ui->tb_cmdWindow_2->append("[推杆] 已连接，开始监控状态");
}

// 推杆推出按钮（旧按钮）
void zmotionpage::on_btn_pipePush_clicked()
{
    on_btn_connection_extent_clicked();
}

// 推杆收回按钮（旧按钮）
void zmotionpage::on_btn_pipeRecover_clicked()
{
    on_btn_connection_retract_clicked();
}

// 推杆复位按钮（旧按钮）
void zmotionpage::on_btn_pipeReset_clicked()
{
    on_btn_connection_init_clicked();
}

/* ========================================================================================= */
/*                                       操作页面二的函数                                       */
/* ========================================================================================= */

/* ===================================== 模式选择界面函数 ===================================== */
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
 * @brief 手动模式切换
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
 * @brief 全自动模式切换（状态机自动实现）
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
 * @brief 半自动模式实现（手动切换状态机）
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

/* ===================================== 控制机构运动函数 ===================================== */

// 机械手控制相关函数实现
/**
 * @brief 设置自定义旋转角度
 */
void zmotionpage::on_le_set_rotation_angle_editingFinished()
{
    if (!m_motionController || !m_motionController->isConnected())
    {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 获取用户输入的目标角度
    bool ok;
    float targetAngle = ui->le_set_rotation_angle->text().toFloat(&ok);
    if (!ok)
    {
        ui->tb_cmdWindow_2->append("错误: 无效的角度值");
        return;
    }

    QString msg = QString("[旋转] 已设置目标角度: %1°").arg(targetAngle, 0, 'f', 2);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
}

/**
 * @brief 设置自定义机械手的伸出长度
 */
void zmotionpage::on_le_set_extent_length_editingFinished()
{
    if (!m_motionController || !m_motionController->isConnected())
    {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 获取用户输入的目标长度
    bool ok;
    float targetLength = ui->le_set_extent_length->text().toFloat(&ok);
    if (!ok)
    {
        ui->tb_cmdWindow_2->append("错误: 无效的长度值");
        return;
    }

    QString msg = QString("[伸缩] 已设置目标长度: %1 mm").arg(targetLength, 0, 'f', 2);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
}

/**
 * @brief 重置旋转起点
 */
void zmotionpage::on_btn_rotation_reset_clicked()
{
    if (!g_handle)
    {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 明确打印当前使用的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_ROBOTROTATION]; // 机械手旋转电机

    QString debugMsg = QString("[旋转] 使用电机: MotorMap[%1] = %2").arg(MOTOR_IDX_ROBOTROTATION).arg(mappedMotorID);
    qDebug() << debugMsg;
    ui->tb_cmdWindow_2->append(debugMsg);

    // 设置当前位置为零点
    int ret = ZAux_Direct_SetDpos(g_handle, mappedMotorID, 0);
    if (ret == 0)
    {
        // 记录当前位置作为偏移量
        float currentPos = 0.0f;
        ZAux_Direct_GetMpos(g_handle, mappedMotorID, &currentPos);
        m_rotationOffset = currentPos;

        QString msg = QString("[旋转] 已重置起点，当前位置设为零点");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
    else
    {
        QString msg = QString("[旋转] 错误: 设置零点失败，错误码: %1").arg(ret);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
}

/**
 * @brief 机械手旋转到钻机位置
 */
void zmotionpage::on_btn_drill_position_clicked()
{
    if (!g_handle)
    {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 诊断: 记录操作前的电机状态
    diagnosticPrintAllMotorsState("旋转操作前");

    // 明确当前使用的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_ROBOTROTATION]; // 机械手旋转电机

    QString debugMsg = QString("[旋转] 正在使用电机: MotorMap[%1] = %2").arg(MOTOR_IDX_ROBOTROTATION).arg(mappedMotorID);
    qDebug() << debugMsg;
    ui->tb_cmdWindow_2->append(debugMsg);

    // 诊断: 确认控制器的BASE设置
    char cmdbuffBase[2048];
    char cmdbuffAckBase[2048];
    sprintf(cmdbuffBase, "?BASE");
    if (ZAux_DirectCommand(g_handle, cmdbuffBase, cmdbuffAckBase, 2048) == 0)
    {
        QString baseMsg = QString("[诊断] 当前BASE设置: %1").arg(cmdbuffAckBase);
        qDebug() << baseMsg;
        ui->tb_cmdWindow_2->append(baseMsg);
    }

    // 设置正确的速度参数
    qDebug() << "[旋转] 设置电机参数: 速度=" << ROBOTARM_ROTATION_SPEED;
    ZAux_Direct_SetSpeed(g_handle, mappedMotorID, ROBOTARM_ROTATION_SPEED);
    ZAux_Direct_SetAccel(g_handle, mappedMotorID, ROBOTARM_ROTATION_ACCEL);
    ZAux_Direct_SetDecel(g_handle, mappedMotorID, ROBOTARM_ROTATION_DECEL);

    // 诊断: 确认没有使用多轴同步指令
    sprintf(cmdbuffBase, "BASE(%d)", mappedMotorID);
    ZAux_DirectCommand(g_handle, cmdbuffBase, cmdbuffAckBase, 2048);
    ui->tb_cmdWindow_2->append(QString("[诊断] 已设置BASE为单轴: %1").arg(mappedMotorID));

    // 移动到0度位置
    float targetPosition = 0.0f - m_rotationOffset;

    // 使用绝对移动命令，明确指定只移动一个轴
    int ret = ZAux_Direct_Single_MoveAbs(g_handle, mappedMotorID, targetPosition);

    if (ret == 0)
    {
        QString msg = QString("[旋转] 移动到钻机位置 (0°)，使用电机ID: %1").arg(mappedMotorID);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);

        // 短暂延时后再次检查电机状态
        QTimer::singleShot(500, this, [this]()
                           { diagnosticPrintAllMotorsState("旋转操作后"); });
    }
    else
    {
        QString msg = QString("[旋转] 错误: 移动失败，错误码: %1").arg(ret);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
}

/**
 * @brief 辅助诊断函数来打印所有电机状态
 */
void zmotionpage::diagnosticPrintAllMotorsState(const QString &stage)
{
    if (!g_handle)
        return;

    QString diagMsg = QString("[诊断] %1 电机状态:\n").arg(stage);

    for (int i = 0; i < 8; i++)
    {
        float pos = 0.0f, speed = 0.0f;
        int idle = 0, atype = 0;

        ZAux_Direct_GetMpos(g_handle, MotorMap[i], &pos);
        ZAux_Direct_GetMspeed(g_handle, MotorMap[i], &speed);
        ZAux_Direct_GetIfIdle(g_handle, MotorMap[i], &idle);
        ZAux_Direct_GetAtype(g_handle, MotorMap[i], &atype);

        diagMsg += QString("MotorMap[%1]=%2: 位置=%3, 速度=%4, 运动状态=%5, Atype=%6\n")
                       .arg(i)
                       .arg(MotorMap[i])
                       .arg(pos, 0, 'f', 2)
                       .arg(speed, 0, 'f', 2)
                       .arg(idle == 0 ? "运动中" : "停止")
                       .arg(atype);
    }

    qDebug() << diagMsg;
    ui->tb_cmdWindow_2->append(diagMsg);

    // 检查是否有SYNC_MOTION定义
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    sprintf(cmdbuff, "?SYNC_MOTION");
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0)
    {
        QString syncMsg = QString("[诊断] SYNC_MOTION设置: %1").arg(cmdbuffAck);
        qDebug() << syncMsg;
        ui->tb_cmdWindow_2->append(syncMsg);
    }
}

/**
 * @brief 移动到存储位置
 */
void zmotionpage::on_btn_storage_position_clicked()
{
    if (!g_handle)
    {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 诊断: 记录操作前的电机状态
    diagnosticPrintAllMotorsState("存储位置操作前");

    // 确认使用的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_ROBOTROTATION]; // 机械手旋转电机

    QString debugMsg = QString("[旋转] 正在使用电机: MotorMap[%1] = %2").arg(MOTOR_IDX_ROBOTROTATION).arg(mappedMotorID);
    qDebug() << debugMsg;
    ui->tb_cmdWindow_2->append(debugMsg);

    // 诊断: 确认控制器的BASE设置
    char cmdbuffBase[2048];
    char cmdbuffAckBase[2048];
    sprintf(cmdbuffBase, "?BASE");
    ZAux_DirectCommand(g_handle, cmdbuffBase, cmdbuffAckBase, 2048);
    ui->tb_cmdWindow_2->append(QString("[诊断] 当前BASE设置: %1").arg(cmdbuffAckBase));

    // 明确设置BASE为单轴
    sprintf(cmdbuffBase, "BASE(%d)", mappedMotorID);
    ZAux_DirectCommand(g_handle, cmdbuffBase, cmdbuffAckBase, 2048);
    ui->tb_cmdWindow_2->append(QString("[诊断] 已设置BASE为单轴: %1").arg(mappedMotorID));

    // 设置运动参数
    qDebug() << "[旋转] 设置电机参数: 速度=" << ROBOTARM_ROTATION_SPEED;
    ZAux_Direct_SetSpeed(g_handle, mappedMotorID, ROBOTARM_ROTATION_SPEED);
    ZAux_Direct_SetAccel(g_handle, mappedMotorID, ROBOTARM_ROTATION_ACCEL);
    ZAux_Direct_SetDecel(g_handle, mappedMotorID, ROBOTARM_ROTATION_DECEL);

    // 计算存储位置的脉冲值 (90度对应的脉冲数)
    float targetPulse = (90.0f / 360.0f) * 294912.0f - m_rotationOffset;

    // 使用绝对移动命令，明确指定只移动一个轴
    int ret = ZAux_Direct_Single_MoveAbs(g_handle, mappedMotorID, targetPulse);

    if (ret == 0)
    {
        QString msg = QString("[旋转] 移动到存储位置 (90°)，使用电机ID: %1").arg(mappedMotorID);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);

        // 短暂延时后再次检查电机状态
        QTimer::singleShot(500, this, [this]()
                           { diagnosticPrintAllMotorsState("存储位置操作后"); });
    }
    else
    {
        QString msg = QString("[旋转] 错误: 移动失败，错误码: %1").arg(ret);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
}

/**
 * @brief 重置机械手伸出起点
 */
void zmotionpage::on_btn_robotarm_reset_clicked()
{
    if (!g_handle)
    {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 确认使用的电机ID，并添加调试信息
    int mappedMotorID = MotorMap[MOTOR_IDX_ROBOTEXTENSION]; // 机械手移动电机

    QString debugMsg = QString("[伸缩] 使用电机: MotorMap[%1] = %2").arg(MOTOR_IDX_ROBOTEXTENSION).arg(mappedMotorID);
    qDebug() << debugMsg;
    ui->tb_cmdWindow_2->append(debugMsg);

    // 设置当前位置为零点
    int ret = ZAux_Direct_SetDpos(g_handle, mappedMotorID, 0);
    if (ret == 0)
    {
        // 记录当前位置作为偏移量
        float currentPos = 0.0f;
        ZAux_Direct_GetMpos(g_handle, mappedMotorID, &currentPos);
        m_extentOffset = currentPos;

        QString msg = QString("[伸缩] 已重置起点，当前位置设为零点");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
    else
    {
        QString msg = QString("[伸缩] 错误: 设置零点失败，错误码: %1").arg(ret);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
}

/**
 * @brief 伸出机械手函数
 */
void zmotionpage::on_btn_robotarm_extent_clicked()
{
    if (!g_handle)
    {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 确认使用的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_ROBOTEXTENSION]; // 机械手移动电机

    QString debugMsg = QString("[伸缩] 正在使用电机: MotorMap[%1] = %2").arg(MOTOR_IDX_ROBOTEXTENSION).arg(mappedMotorID);
    qDebug() << debugMsg;
    ui->tb_cmdWindow_2->append(debugMsg);

    // 获取目标长度
    bool ok;
    float targetLength = ui->le_set_extent_length->text().toFloat(&ok);
    if (!ok)
    {
        ui->tb_cmdWindow_2->append("错误: 请设置有效的伸出长度");
        return;
    }

    // 明确设置BASE仅为机械手移动电机
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    sprintf(cmdbuff, "BASE(%d)", mappedMotorID);
    ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048);

    QString baseMsg = QString("[伸缩] 设置BASE仅为机械手移动电机: %1").arg(mappedMotorID);
    qDebug() << baseMsg;
    ui->tb_cmdWindow_2->append(baseMsg);

    // 确保机械手移动电机处于位置模式
    ZAux_Direct_SetAtype(g_handle, mappedMotorID, 65); // 设置为位置模式
    QThread::msleep(10);

    // 设置运动参数 - 确保使用正确的速度
    qDebug() << "[伸缩] 设置电机参数: 速度=" << ROBOTARM_EXTENT_SPEED;
    ZAux_Direct_SetSpeed(g_handle, mappedMotorID, ROBOTARM_EXTENT_SPEED);
    ZAux_Direct_SetAccel(g_handle, mappedMotorID, ROBOTARM_EXTENT_ACCEL);
    ZAux_Direct_SetDecel(g_handle, mappedMotorID, ROBOTARM_EXTENT_DECEL);

    // 计算目标脉冲位置
    float targetPulse = (targetLength * 212992.0f) / 91.1035f - m_extentOffset;

    // 打印参数信息
    QString infoMsg = QString("[伸缩] 目标长度: %1 mm, 脉冲: %2, 偏移: %3")
                          .arg(targetLength, 0, 'f', 2)
                          .arg(targetPulse, 0, 'f', 0)
                          .arg(m_extentOffset, 0, 'f', 0);
    qDebug() << infoMsg;
    ui->tb_cmdWindow_2->append(infoMsg);

    // 使用单轴绝对移动命令
    int ret = ZAux_Direct_Single_MoveAbs(g_handle, mappedMotorID, targetPulse);

    if (ret == 0)
    {
        QString msg = QString("[伸缩] 伸出到指定长度: %1 mm (脉冲: %2)")
                          .arg(targetLength, 0, 'f', 2)
                          .arg(targetPulse, 0, 'f', 0);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
    else
    {
        QString msg = QString("[伸缩] 错误: 移动失败，错误码: %1").arg(ret);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
}

/**
 * @brief 收回机械手函数
 */
void zmotionpage::on_btn_robotarm_retract_clicked()
{
    if (!g_handle)
    {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 确认使用的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_ROBOTEXTENSION]; // 机械手移动电机

    QString debugMsg = QString("[伸缩] 回收操作: 使用电机: MotorMap[%1] = %2").arg(MOTOR_IDX_ROBOTEXTENSION).arg(mappedMotorID);
    qDebug() << debugMsg;
    ui->tb_cmdWindow_2->append(debugMsg);

    // 明确设置BASE仅为机械手移动电机
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    sprintf(cmdbuff, "BASE(%d)", mappedMotorID);
    ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048);

    QString baseMsg = QString("[伸缩] 设置BASE仅为机械手移动电机: %1").arg(mappedMotorID);
    qDebug() << baseMsg;
    ui->tb_cmdWindow_2->append(baseMsg);

    // 确保机械手移动电机处于位置模式
    ZAux_Direct_SetAtype(g_handle, mappedMotorID, 65); // 设置为位置模式
    QThread::msleep(10);

    // 设置运动参数 - 使用高速度参数
    qDebug() << "[伸缩] 设置电机参数: 速度=" << ROBOTARM_EXTENT_SPEED;
    ZAux_Direct_SetSpeed(g_handle, mappedMotorID, ROBOTARM_EXTENT_SPEED);
    ZAux_Direct_SetAccel(g_handle, mappedMotorID, ROBOTARM_EXTENT_ACCEL);
    ZAux_Direct_SetDecel(g_handle, mappedMotorID, ROBOTARM_EXTENT_DECEL);

    // 移动到零点位置（考虑偏移量）
    float targetPosition = 0.0f - m_extentOffset;

    // 打印目标信息
    QString infoMsg = QString("[伸缩] 回收到原点: 目标位置=%1，偏移量=%2")
                          .arg(targetPosition)
                          .arg(m_extentOffset);
    qDebug() << infoMsg;
    ui->tb_cmdWindow_2->append(infoMsg);

    // 使用单轴绝对移动命令，确保只影响当前电机
    int ret = ZAux_Direct_Single_MoveAbs(g_handle, mappedMotorID, targetPosition);

    if (ret == 0)
    {
        QString msg = QString("[伸缩] 开始回收到原点位置");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);

        // 短暂延时后打印状态
        QTimer::singleShot(500, this, [this, mappedMotorID]()
                           {
            float pos = 0.0f, speed = 0.0f;
            ZAux_Direct_GetMpos(g_handle, mappedMotorID, &pos);
            ZAux_Direct_GetMspeed(g_handle, mappedMotorID, &speed);
            
            QString statusMsg = QString("[伸缩] 回收中: 当前位置=%1, 速度=%2")
                               .arg(pos)
                               .arg(speed);
            qDebug() << statusMsg;
            ui->tb_cmdWindow_2->append(statusMsg); });
    }
    else
    {
        QString msg = QString("[伸缩] 错误: 回收操作失败，错误码: %1").arg(ret);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);

        // 如果API调用失败，尝试使用字符串命令
        sprintf(cmdbuff, "MOVEABS(%d,%f)", mappedMotorID, targetPosition);
        if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0)
        {
            QString cmdMsg = QString("[伸缩] 使用命令方式开始回收");
            qDebug() << cmdMsg;
            ui->tb_cmdWindow_2->append(cmdMsg);
        }
        else
        {
            QString errMsg = QString("[伸缩] 命令执行失败: %1").arg(cmdbuffAck);
            qDebug() << errMsg;
            ui->tb_cmdWindow_2->append(errMsg);
        }
    }
}

/**
 * @brief 机械手夹紧电机打开
 */
void zmotionpage::on_btn_robotarm_clamp_open_clicked()
{
    if (!g_handle)
    {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 确认使用的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_ROBOTCLAMP]; // 机械手夹紧电机

    QString debugMsg = QString("[机械手夹紧] 使用电机: MotorMap[%1] = %2").arg(MOTOR_IDX_ROBOTCLAMP).arg(mappedMotorID);
    qDebug() << debugMsg;
    ui->tb_cmdWindow_2->append(debugMsg);

    // 设置力矩控制模式
    int ret = ZAux_Direct_SetAtype(g_handle, mappedMotorID, 67); // 设置为力矩模式
    if (ret != 0)
    {
        QString msg = QString("[机械手夹紧] 错误: 设置力矩模式失败，错误码: %1").arg(ret);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 设置更强的负力矩值使夹爪张开 (从-30改为-40)
    float openDac = -40.0f;
    ret = ZAux_Direct_SetDAC(g_handle, mappedMotorID, openDac);
    if (ret != 0)
    {
        QString msg = QString("[机械手夹紧] 错误: 设置DAC值失败，错误码: %1").arg(ret);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    QString msg = QString("[机械手夹紧] 开始张开 (力矩值: %1)").arg(openDac);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);

    // 设置状态并启动监控
    m_isClampOpening = true;
    m_speedCheckCount = 0;

    // 如果定时器已经运行，先停止
    if (m_clampMonitorTimer->isActive())
    {
        m_clampMonitorTimer->stop();
    }

    // 启动定时器监控
    m_clampMonitorTimer->start();
}

/**
 * @brief 机械手夹紧电机闭合
 */
void zmotionpage::on_btn_robotarm_clamp_close_clicked()
{
    if (!g_handle)
    {
        ui->tb_cmdWindow_2->append("错误: 控制器未连接");
        return;
    }

    // 确认使用的电机ID，并添加调试信息
    int mappedMotorID = MotorMap[MOTOR_IDX_ROBOTCLAMP]; // 机械手夹紧电机

    QString debugMsg = QString("[机械手夹紧] 使用电机: MotorMap[%1] = %2").arg(MOTOR_IDX_ROBOTCLAMP).arg(mappedMotorID);
    qDebug() << debugMsg;
    ui->tb_cmdWindow_2->append(debugMsg);

    // 获取用户设定的力矩值
    bool ok;
    float dac = ui->le_robotarm_clamp_DAC->text().toFloat(&ok);
    if (!ok || dac <= 0)
    {
        dac = ROBOTARM_CLAMP_DEFAULT_DAC;
        ui->le_robotarm_clamp_DAC->setText(QString::number(dac, 'f', 1));
    }

    // 设置力矩控制模式
    char cmdbuff[2048];
    char cmdbuffAck[2048];

    // 设置为力矩模式 (Atype=67)
    sprintf(cmdbuff, "ATYPE(%d)=67", mappedMotorID);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0)
    {
        QString msg = QString("[机械手夹紧] 错误: 设置力矩模式失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 设置正力矩值使夹爪闭合
    sprintf(cmdbuff, "DAC(%d)=%f", mappedMotorID, dac);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0)
    {
        QString msg = QString("[机械手夹紧] 错误: 设置DAC值失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    QString msg = QString("[机械手夹紧] 闭合 (力矩值: %1)").arg(dac);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);

    // 设置状态并启动监控
    m_isClampOpening = false;
    m_speedCheckCount = 0;

    // 如果定时器已经运行，先停止
    if (m_clampMonitorTimer->isActive())
    {
        m_clampMonitorTimer->stop();
    }

    // 启动定时器监控
    m_clampMonitorTimer->start();
}

/**
 * @brief 机械手夹紧监控函数
 */
void zmotionpage::monitorClampSpeed()
{
    if (!g_handle)
    {
        m_clampMonitorTimer->stop();
        return;
    }

    // 使用映射后的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_ROBOTCLAMP]; // 机械手夹紧电机

    static QElapsedTimer elapsedTimer;
    static bool timerStarted = false;
    static int stallCounter = 0;
    static float lastPosition = 0.0f;

    if (!timerStarted)
    {
        elapsedTimer.start();
        timerStarted = true;
        stallCounter = 0;

        // 获取初始位置
        ZAux_Direct_GetMpos(g_handle, mappedMotorID, &lastPosition);
        qDebug() << "[夹紧监控] 初始位置:" << lastPosition;
    }

    // 获取当前速度和位置
    float currentSpeed = 0.0f;
    float currentPosition = 0.0f;

    ZAux_Direct_GetMspeed(g_handle, mappedMotorID, &currentSpeed);
    ZAux_Direct_GetMpos(g_handle, mappedMotorID, &currentPosition);

    // 更新UI显示（实时显示角度）
    if (ui->le_robotarm_clamp_pos)
    {
        float angleDegrees = (currentPosition / 212992.0f) * 360.0f;
        ui->le_robotarm_clamp_pos->setText(QString::number(angleDegrees, 'f', 2));
    }

    // 详细调试信息
    float positionDelta = currentPosition - lastPosition;
    qDebug() << "[夹紧监控] 速度:" << currentSpeed << "位置:" << currentPosition
             << "变化量:" << positionDelta << "计数:" << stallCounter;

    // 堵转检测：速度很小且位置几乎不变
    bool isStalled = (std::abs(currentSpeed) < 5.0) && (std::abs(positionDelta) < 20.0);

    if (isStalled)
    {
        stallCounter++;

        // 连续3次检测到堵转状态才认为真正堵转
        if (stallCounter >= 3)
        {
            // 停止力矩输出
            ZAux_Direct_SetDAC(g_handle, mappedMotorID, 0);
            QThread::msleep(100);

            // 切换到位置模式并保持当前位置
            ZAux_Direct_SetAtype(g_handle, mappedMotorID, 65);
            ZAux_Direct_Single_MoveAbs(g_handle, mappedMotorID, currentPosition);

            QString action = m_isClampOpening ? "张开" : "闭合";
            QString msg = QString("[机械手夹紧] %1检测到堵转，切换到位置模式保持在: %2")
                              .arg(action)
                              .arg(currentPosition, 0, 'f', 2);
            qDebug() << msg;
            ui->tb_cmdWindow_2->append(msg);

            // 停止监控
            m_clampMonitorTimer->stop();
            timerStarted = false;
            return;
        }
    }
    else
    {
        // 如果不满足堵转条件，重置计数器
        stallCounter = 0;
    }

    // 位置变化大的检测和超时检测
    const int MAX_POSITION_CHANGE = 500000; // 位置变化阈值
    const int EXTENDED_TIMEOUT = 15000;     // 延长超时时间到15秒

    bool reachedLimit = std::abs(currentPosition - lastPosition) > MAX_POSITION_CHANGE;

    if (reachedLimit || elapsedTimer.elapsed() > EXTENDED_TIMEOUT)
    {
        // 停止力矩输出
        ZAux_Direct_SetDAC(g_handle, mappedMotorID, 0);
        QThread::msleep(100);

        // 切换到位置模式并保持当前位置
        ZAux_Direct_SetAtype(g_handle, mappedMotorID, 65);
        ZAux_Direct_Single_MoveAbs(g_handle, mappedMotorID, currentPosition);

        QString reason = reachedLimit ? "位置变化过大" : "操作超时";
        QString msg = QString("[机械手夹紧] %1，切换到位置模式保持当前位置: %2")
                          .arg(reason)
                          .arg(currentPosition, 0, 'f', 2);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);

        // 停止监控
        m_clampMonitorTimer->stop();
        timerStarted = false;
    }

    // 更新上一次位置
    lastPosition = currentPosition;
}

/**
 * @brief 更新机械手夹紧状态显示
 */
void zmotionpage::updateClampStatus()
{
    if (!g_handle)
    {
        return;
    }

    // 使用映射后的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_ROBOTCLAMP]; // 机械手夹紧电机

    // 获取当前夹爪位置
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    sprintf(cmdbuff, "?MPOS(%d)", mappedMotorID);

    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0)
    {
        float currentPosition = atof(cmdbuffAck);

        // 将脉冲数转换为角度
        float angleDegrees = (currentPosition / 212992.0f) * 360.0f;

        // 显示角度
        if (ui->le_robotarm_clamp_pos)
        {
            ui->le_robotarm_clamp_pos->setText(QString::number(angleDegrees, 'f', 2));
        }

        // 仅在自动更新模式下输出调试信息
        if (m_robotArmStatusTimer && m_robotArmStatusTimer->isActive())
        {
            QString msg = QString("[夹紧] 当前角度: %1° (脉冲: %2)")
                              .arg(angleDegrees, 0, 'f', 2)
                              .arg(currentPosition, 0, 'f', 0);
            qDebug() << msg;
            ui->tb_cmdWindow_2->append(msg);
        }
    }
}

/* ===================================== 机械手的电机显示函数 ===================================== */

/**
 * @brief 机械手旋转电机更新旋转角度显示
 */
void zmotionpage::updateRotationAngle()
{
    if (!g_handle)
    {
        return;
    }

    // 使用映射后的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_ROBOTROTATION]; // 机械手旋转电机

    // 获取当前旋转位置
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    sprintf(cmdbuff, "?MPOS(%d)", mappedMotorID);

    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0)
    {
        float currentPosition = atof(cmdbuffAck);

        // 将脉冲数转换为角度
        float angleDegrees = (currentPosition / 294912.0f) * 360.0f;

        // 显示角度
        if (ui->le_rotation_angle)
        {
            ui->le_rotation_angle->setText(QString::number(angleDegrees, 'f', 2));
        }

        // 仅在自动更新模式下输出调试信息
        if (m_robotArmStatusTimer && m_robotArmStatusTimer->isActive())
        {
            QString msg = QString("[旋转] 当前角度: %1° (脉冲: %2)")
                              .arg(angleDegrees, 0, 'f', 2)
                              .arg(currentPosition, 0, 'f', 0);
            qDebug() << msg;
            ui->tb_cmdWindow_2->append(msg);
        }
    }
}

/**
 * @brief 更新伸出长度显示
 */
void zmotionpage::updateExtentLength()
{
    if (!g_handle)
    {
        return;
    }

    // 使用映射后的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_ROBOTEXTENSION]; // 机械手移动电机

    // 获取当前位置
    float currentPosition = 0.0f;
    int ret = ZAux_Direct_GetMpos(g_handle, mappedMotorID, &currentPosition);

    if (ret == 0)
    {
        // 转换为毫米 (手臂一圈212992脉冲对应91.1035mm)
        float lengthMm = (currentPosition / 212992.0f) * 91.1035f;

        // 更新UI显示
        if (ui->le_extent_length)
        {
            ui->le_extent_length->setText(QString::number(lengthMm, 'f', 2));
        }

        // 如果在自动更新模式，输出调试信息
        if (m_robotArmStatusTimer && m_robotArmStatusTimer->isActive())
        {
            QString msg = QString("[伸缩] 当前长度: %1 mm (脉冲: %2)")
                              .arg(lengthMm, 0, 'f', 2)
                              .arg(currentPosition, 0, 'f', 0);
            qDebug() << msg;
            ui->tb_cmdWindow_2->append(msg);
        }
    }
    else
    {
        qDebug() << "[伸缩] 获取位置失败，错误码:" << ret;
    }
}

/**
 * @brief 打印当前的电机映射
 */
void zmotionpage::printMotorMapping()
{
    QString mapInfo = "========= 当前电机映射关系 =========\n";
    for (int i = 0; i < 10; ++i)
    {
        mapInfo += QString("MotorMap[%1] = %2\n").arg(i).arg(MotorMap[i]);
    }
    qDebug() << mapInfo;
    ui->tb_cmdWindow_2->append(mapInfo);
}

/* ===================================== 钻进电机控制和显示函数 ===================================== */
/**
 * @brief 更新当前钻进深度显示，该函数将电机脉冲数转换为毫米
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
    
    // 只有在自动模式下才输出调试信息
    if (m_robotArmStatusTimer && m_robotArmStatusTimer->isActive()) {
    QString msg = QString("[进给] 当前深度: %1 mm (脉冲位置: %2)").arg(currentDepth, 0, 'f', 2).arg(currentPulse);
    qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
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
    
    // 读取UI中设置的速度值
    bool speedOk;
    float speed = ui->le_penetration_speed->text().toFloat(&speedOk);
    if (!speedOk || speed <= 0) {
        // 如果读取失败或无效，使用初始化时的默认速度
        speed = PENETRATION_DEFAULT_SPEED;
        ui->le_penetration_speed->setText(QString::number(speed, 'f', 2));
        QString msg = QString("[进给] 使用默认速度: %1").arg(speed);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
    
    // 更新成员变量中的速度值
    m_penetrationSpeed = speed;
    
    // 获取当前位置
    float currentPulse = m_motionController->getCurrentPosition(m_penetrationMotorID);
    
    // 计算目标位置（脉冲）
    double targetPulse = (PENETRATION_MAX_HEIGHT - m_penetrationTargetDepth) * PENETRATION_PULSE_PER_MM;
    
    // 获取电机的映射ID
    int mappedMotorID = MotorMap[m_penetrationMotorID];
    
    // 详细记录映射关系 - 添加这行日志
    QString mappingInfo = QString("[进给] 电机映射: 使用m_penetrationMotorID=%1 -> 映射到实际电机ID=%2")
                          .arg(m_penetrationMotorID)
                          .arg(mappedMotorID);
    qDebug() << mappingInfo;
    ui->tb_cmdWindow_2->append(mappingInfo);
    
    // 使用API函数直接设置速度、加速度和减速度
    if (ZAux_Direct_SetSpeed(g_handle, mappedMotorID, m_penetrationSpeed) != 0) {
        QString msg = QString("[进给] 错误: 设置速度失败 (API方式)");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        
        // 尝试将速度直接写入到字符串命令中
        char cmdbuff[2048];
        char cmdbuffAck[2048];
        sprintf(cmdbuff, "MOVE_SPEED=%f", m_penetrationSpeed);
        
        if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
            msg = QString("[进给] 错误: 替代设置速度方法也失败");
            qDebug() << msg;
            ui->tb_cmdWindow_2->append(msg);
            // 但继续执行，尝试用默认速度移动
        }
    } else {
        QString msg = QString("[进给] 速度设置成功: %1").arg(m_penetrationSpeed);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    }
    
    // 设置加速度和减速度
    ZAux_Direct_SetAccel(g_handle, mappedMotorID, m_penetrationSpeed * ACCEL_RATIO);
    ZAux_Direct_SetDecel(g_handle, mappedMotorID, m_penetrationSpeed * DECEL_RATIO);
    
    // 使用fixed格式而不是科学计数法
    QString formattedPulse = QString::number(targetPulse, 'f', 0);
    
    QString msg = QString("[进给] 开始运动 - 目标深度: %1 mm (当前: %2 mm, 目标脉冲: %3, 速度: %4, 映射电机ID: %5)")
                    .arg(m_penetrationTargetDepth, 0, 'f', 2)
                    .arg(PENETRATION_MAX_HEIGHT - (currentPulse / PENETRATION_PULSE_PER_MM), 0, 'f', 2)
                   .arg(formattedPulse)
                   .arg(m_penetrationSpeed)
                   .arg(mappedMotorID);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
    
    // 执行绝对位置运动 - 直接使用API而不是字符串命令
    if (ZAux_Direct_Single_MoveAbs(g_handle, mappedMotorID, targetPulse) == 0) {
        msg = QString("[进给] 运动已启动");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    } else {
        char cmdbuff[2048];
        char cmdbuffAck[2048];
        
        // 如果API函数失败，尝试字符串命令作为备选
        sprintf(cmdbuff, "MOVEABS(%d,%s)", mappedMotorID, formattedPulse.toUtf8().constData());
        
        if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0) {
            msg = QString("[进给] 运动已启动 (命令方式)");
            qDebug() << msg;
            ui->tb_cmdWindow_2->append(msg);
        } else {
            msg = QString("[进给] 错误: 启动运动失败 - %1").arg(cmdbuffAck);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        }
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
    
    // 获取电机的映射ID 
    int mappedMotorID = MotorMap[m_penetrationMotorID];
    
    QString mappingInfo = QString("[进给取消] 电机映射: 使用m_penetrationMotorID=%1 -> 映射到实际电机ID=%2")
                          .arg(m_penetrationMotorID)
                          .arg(mappedMotorID);
    qDebug() << mappingInfo;
    ui->tb_cmdWindow_2->append(mappingInfo);
    
    // 使用API直接停止电机
    bool stopSuccess = false;
    
    // 尝试使用API函数直接停止
    if (ZAux_Direct_Single_Cancel(g_handle, mappedMotorID, 0) == 0) {
        stopSuccess = true;
    } else {
        // 如果API函数失败，尝试字符串命令
        char cmdbuff[2048];
        char cmdbuffAck[2048];
        
        // 使用CANCEL命令停止运动
        sprintf(cmdbuff, "CANCEL(%d)", mappedMotorID);
        
        if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0) {
            stopSuccess = true;
        }
    }
    
    if (stopSuccess) {
        QString msg = QString("[进给] 已取消运动 (电机ID: %1)").arg(mappedMotorID);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
    } else {
        QString msg = QString("[进给] 错误: 停止运动失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        
        // 尝试紧急停止模式
        char cmdbuff[2048];
        char cmdbuffAck[2048];
        sprintf(cmdbuff, "RAPIDSTOP AXIS(%d)", mappedMotorID);
        
        if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0) {
            msg = QString("[进给] 已使用紧急停止命令");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        }
    }
    
    // 更新当前深度显示
    updatePenetrationDepth();
}
/* ===================================== 初始化UI函数 ===================================== */
/**
 * @brief 初始化UI
 */
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
        // 恢复特定的映射顺序，M7映射到M1
        MotorMap[MOTOR_IDX_ROTATION] = 0;         // 旋转切割
        MotorMap[MOTOR_IDX_PERCUSSION] = 7;       // 冲击电机
        MotorMap[MOTOR_IDX_PENETRATION] = 1;      // 进给电机
        MotorMap[MOTOR_IDX_DOWNCLAMP] = 2;        // 下夹紧
        MotorMap[MOTOR_IDX_ROBOTCLAMP] = 3;       // 机械手夹紧
        MotorMap[MOTOR_IDX_ROBOTROTATION] = 4;    // 机械手旋转
        MotorMap[MOTOR_IDX_ROBOTEXTENSION] = 5;   // 机械手移动
        MotorMap[MOTOR_IDX_STORAGE] = 6;          // 存储电机
        MotorMap[8] = 8;                          // M8 -> 8
        MotorMap[9] = 9;                          // M9 -> 9
        
        // 显示当前映射
        ShowMotorMap();
        
        // 设置每个电机的参数
        if (m_motionController && g_handle) {
            QString settingsMsg = "设置电机控制模式：\n";
            
            // 旋转切割电机设置为速度模式 (Atype=66)
            if (m_motionController->setMotorParameter(MotorMap[MOTOR_IDX_ROTATION], "Atype", 66)) {
                settingsMsg += "电机" + QString::number(MOTOR_IDX_ROTATION) + "(旋转切割): 速度模式(Atype=66)\n";
            } else {
                settingsMsg += "电机" + QString::number(MOTOR_IDX_ROTATION) + "(旋转切割): 设置速度模式失败\n";
            }
            
            // 下夹紧电机设置为力矩模式 (Atype=67)
            if (m_motionController->setMotorParameter(MotorMap[MOTOR_IDX_DOWNCLAMP], "Atype", 67)) {
                settingsMsg += "电机" + QString::number(MOTOR_IDX_DOWNCLAMP) + "(下夹紧): 力矩模式(Atype=67)\n";
            } else {
                settingsMsg += "电机" + QString::number(MOTOR_IDX_DOWNCLAMP) + "(下夹紧): 设置力矩模式失败\n";
            }
            
            // 机械手夹紧电机设置为力矩模式 (Atype=67)
            if (m_motionController->setMotorParameter(MotorMap[MOTOR_IDX_ROBOTCLAMP], "Atype", 67)) {
                settingsMsg += "电机" + QString::number(MOTOR_IDX_ROBOTCLAMP) + "(机械手夹紧): 力矩模式(Atype=67)\n";
            } else {
                settingsMsg += "电机" + QString::number(MOTOR_IDX_ROBOTCLAMP) + "(机械手夹紧): 设置力矩模式失败\n";
            }
            
            ui->tb_cmdWindow->append(settingsMsg);
            
            // 定义电机速度数组
            const float motorSpeeds[] = {
                DRILL_DEFAULT_SPEED,        // 旋转切割电机
                PERCUSSION_DEFAULT_SPEED,   // 冲击电机
                PENETRATION_DEFAULT_SPEED,  // 进给电机
                DOWNCLAMP_DEFAULT_SPEED,    // 下夹紧电机
                ROBOTCLAMP_DEFAULT_SPEED,   // 机械手夹紧电机
                ROBOTROTATION_DEFAULT_SPEED, // 机械手旋转电机
                ROBOTEXTENSION_DEFAULT_SPEED, // 机械手移动电机
                STORAGE_DEFAULT_SPEED       // 存储电机
            };
            
            QString speedMsg = "已设置电机参数：\n";
            
            // 为每个电机设置速度、加速度和减速度
            for (int i = 0; i < 8; i++) {
                // 计算每个电机的参数
                float speed = motorSpeeds[i];
                float accel = speed * ACCEL_RATIO;
                float decel = speed * DECEL_RATIO;
                float fastDec = speed * STOP_SPEED_RATIO;
                
                // 设置电机参数
                if (i == 3 || i == 4) {
                    // 对于力矩模式的电机，设置DAC值而不是速度
                    m_motionController->setMotorParameter(MotorMap[i], "DAC", 0);  // 初始化力矩为0
                    speedMsg += QString("电机%1: 初始力矩=0\n").arg(i);
                } else {
                    // 对于速度模式的电机，设置速度和加减速参数
                    m_motionController->setMotorParameter(MotorMap[i], "Vel", speed);  // 速度
                    m_motionController->setMotorParameter(MotorMap[i], "Acc", accel);  // 加速度
                    m_motionController->setMotorParameter(MotorMap[i], "Dec", decel);  // 减速度
                    m_motionController->setMotorParameter(MotorMap[i], "FastDec", fastDec); // 快速停止减速度
                    
                    // 添加日志信息
                    speedMsg += QString("电机%1: 速度=%2, 加速度=%3, 减速度=%4, 快停=%5\n")
                        .arg(i)
                        .arg(speed, 0, 'f', 1)
                        .arg(accel, 0, 'f', 1)
                        .arg(decel, 0, 'f', 1)
                        .arg(fastDec, 0, 'f', 1);
                }
            }
            
            ui->tb_cmdWindow->append(speedMsg);
            qDebug() << "[电机参数] 已设置所有电机参数";
        } else {
            QString errMsg = "错误：未连接到控制器或运动控制器未初始化，无法设置电机参数";
            ui->tb_cmdWindow->append(errMsg);
            qDebug() << errMsg;
        }
        
        // 提示映射已重置
        QString msg = QString("[电机映射] 已重置为特定映射并设置电机参数");
        qDebug() << msg;
        ui->tb_cmdWindow->append(msg);
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
    m_downclampTimer->setInterval(TIMER_DOWNCLAMP_STATUS_INTERVAL);
    connect(m_downclampTimer, &QTimer::timeout, this, &zmotionpage::updateDownclampStatus);

    // 添加这段代码，打印当前的电机映射关系
    QString mapInfo = "初始化时的电机映射:\n";
    for (int i = 0; i < 10; ++i)
    {
        mapInfo += QString("MotorMap[%1] = %2\n").arg(i).arg(MotorMap[i]);
    }
    qDebug() << mapInfo;
    ui->tb_cmdWindow_2->append(mapInfo);

    // 初始化夹紧监控定时器
    m_clampMonitorTimer = new QTimer(this);
    m_clampMonitorTimer->setInterval(100); // 100ms检查一次
    connect(m_clampMonitorTimer, &QTimer::timeout, this, &zmotionpage::monitorClampSpeed);
    m_isClampOpening = false;
    m_speedCheckCount = 0;

    // 创建推杆状态检查定时器
    m_connectionStatusTimer = new QTimer(this);
    m_connectionStatusTimer->setInterval(CONNECTION_STATUS_CHECK_INTERVAL);
    connect(m_connectionStatusTimer, &QTimer::timeout, this, &zmotionpage::updateConnectionStatus);
    
    // 初始化推杆状态变量
    m_connectionInitialized = false;
}

/* ===================================== 存储机构电机控制和显示函数 ===================================== */
/**
 * @brief 更新存储机构状态显示
 */
void zmotionpage::updateStorageStatus()
{
    if (!m_motionController || !m_motionController->isConnected()) {
        return;
    }

    // 获取映射后的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_STORAGE];  // 存储电机

    // 获取当前电机位置
    float currentPosition = 0.0f;
    
    // 使用 ZAux_Direct_GetDpos 获取当前位置
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    sprintf(cmdbuff, "?DPOS(%d)", mappedMotorID);
    
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0) {
        currentPosition = atof(cmdbuffAck);
        
        // 计算当前存储位置索引
        m_storageCurrentPosition = qRound((currentPosition - m_storageOffset) / GLOBAL_STORAGE_PULSES_PER_POSITION) % GLOBAL_STORAGE_POSITIONS;
        
        // 确保索引为非负数
        if (m_storageCurrentPosition < 0) {
            m_storageCurrentPosition += GLOBAL_STORAGE_POSITIONS;
        }
        
        // 计算角度 - 如果需要显示角度
        float currentAngle = (float)m_storageCurrentPosition * GLOBAL_STORAGE_ANGLE_PER_POSITION;
        
        // 更新UI显示
        if (ui->le_stroage_status) {
            ui->le_stroage_status->setText(QString("位置 %1 / %2 (%3°)").arg(m_storageCurrentPosition + 1)
                                                                     .arg(GLOBAL_STORAGE_POSITIONS)
                                                                      .arg(currentAngle, 0, 'f', 1));
        }
        
        // 只有在自动模式下才输出调试信息
        if (m_robotArmStatusTimer && m_robotArmStatusTimer->isActive()) {
            QString msg = QString("[存储机构] 当前位置: %1 / %2 (角度: %3°, 脉冲: %4)")
                          .arg(m_storageCurrentPosition + 1)
                          .arg(GLOBAL_STORAGE_POSITIONS)
                          .arg(currentAngle, 0, 'f', 1)
                          .arg(currentPosition, 0, 'f', 0);
            qDebug() << msg;
            ui->tb_cmdWindow_2->append(msg);
        }
    }
}

/**
 * @brief 存储机构向后转位
 */
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

    // 使用映射后的电机ID - 修改关键点
    int mappedMotorID = MotorMap[MOTOR_IDX_STORAGE];  // 存储电机
    
    QString mappingInfo = QString("[存储机构] 电机映射: 使用STORAGE_MOTOR_ID=%1 -> 映射到实际电机ID=%2")
                          .arg(MOTOR_IDX_STORAGE)
                          .arg(mappedMotorID);
    qDebug() << mappingInfo;
    ui->tb_cmdWindow_2->append(mappingInfo);

    // 计算目标位置索引（向后转位）
    int targetPosition = (m_storageCurrentPosition - 1 + GLOBAL_STORAGE_POSITIONS) % GLOBAL_STORAGE_POSITIONS;
    
    // 设置运动参数
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    
    // 设置速度和加减速参数
    if (!setStorageMotionParameters(mappedMotorID)) {
        return;
    }
    
    // 关键修改：使用增量运动而不是绝对运动
    // 每次移动固定的脉冲数，但方向相反（负值）
    float movePulses = -GLOBAL_STORAGE_PULSES_PER_POSITION;  // 每个位置的脉冲数，负值表示反向
    
    // 执行运动 - 显示更多诊断信息
    QString msg = QString("[存储机构] 向后转位: 从位置 %1 到位置 %2 (增量脉冲: %3, 电机ID: %4)")
                      .arg(m_storageCurrentPosition + 1)
                      .arg(targetPosition + 1)
                      .arg(movePulses, 0, 'f', 0)
                      .arg(mappedMotorID);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
    
    // 使用固定数字格式而不是科学计数法
    QString formattedPulses = QString::number(movePulses, 'f', 0);
    
    // 尝试使用API函数 - 使用相对运动
    if (ZAux_Direct_Single_Move(g_handle, mappedMotorID, movePulses) == 0) {
        QString successMsg = QString("[存储机构] 向后转位命令已发送 (API方式)");
        qDebug() << successMsg;
        ui->tb_cmdWindow_2->append(successMsg);
        
        // 电机应该已经启动运动，添加一个延时等待运动完成
        QTimer::singleShot(500, this, [this, targetPosition]() {
            m_storageCurrentPosition = targetPosition;
            updateStorageStatus();
        });
    } else {
        // 如果API函数失败，尝试字符串命令
        sprintf(cmdbuff, "MOVE(%d,%s)", mappedMotorID, formattedPulses.toUtf8().constData());
        
        if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0) {
            QString successMsg = QString("[存储机构] 向后转位命令已发送 (命令方式)");
            qDebug() << successMsg;
            ui->tb_cmdWindow_2->append(successMsg);
            
            // 电机应该已经启动运动，添加一个延时等待运动完成
            QTimer::singleShot(500, this, [this, targetPosition]() {
                m_storageCurrentPosition = targetPosition;
                updateStorageStatus();
            });
        } else {
            QString errMsg = QString("[存储机构] 错误: 执行运动失败 - %1").arg(cmdbuffAck);
        qDebug() << errMsg;
        ui->tb_cmdWindow_2->append(errMsg);
        return;
    }
    }
}

/**
 * @brief 存储机构向前转位
 */
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

    // 使用映射后的电机ID - 修改关键点
    int mappedMotorID = MotorMap[MOTOR_IDX_STORAGE];  // 存储电机
    
    QString mappingInfo = QString("[存储机构] 电机映射: 使用STORAGE_MOTOR_ID=%1 -> 映射到实际电机ID=%2")
                          .arg(MOTOR_IDX_STORAGE)
                          .arg(mappedMotorID);
    qDebug() << mappingInfo;
    ui->tb_cmdWindow_2->append(mappingInfo);

    // 计算目标位置索引（向前转位）
    int targetPosition = (m_storageCurrentPosition + 1) % GLOBAL_STORAGE_POSITIONS;
    
    // 设置运动参数
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    
    // 设置速度和加减速参数
    if (!setStorageMotionParameters(mappedMotorID)) {
        return;
    }
    
    // 关键修改：使用增量运动而不是绝对运动
    // 每次移动固定的脉冲数，而不是计算绝对位置
    float movePulses = GLOBAL_STORAGE_PULSES_PER_POSITION;  // 每个位置的脉冲数
    
    // 执行运动 - 显示更多诊断信息
    QString msg = QString("[存储机构] 向前转位: 从位置 %1 到位置 %2 (增量脉冲: %3, 电机ID: %4)")
                      .arg(m_storageCurrentPosition + 1)
                      .arg(targetPosition + 1)
                      .arg(movePulses, 0, 'f', 0)
                      .arg(mappedMotorID);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
    
    // 使用固定数字格式而不是科学计数法
    QString formattedPulses = QString::number(movePulses, 'f', 0);
    
    // 尝试使用API函数 - 使用相对运动
    if (ZAux_Direct_Single_Move(g_handle, mappedMotorID, movePulses) == 0) {
        QString successMsg = QString("[存储机构] 向前转位命令已发送 (API方式)");
        qDebug() << successMsg;
        ui->tb_cmdWindow_2->append(successMsg);
        
        // 电机应该已经启动运动，添加一个延时等待运动完成
        QTimer::singleShot(500, this, [this, targetPosition]() {
            m_storageCurrentPosition = targetPosition;
            updateStorageStatus();
        });
    } else {
        // 如果API函数失败，尝试字符串命令
        sprintf(cmdbuff, "MOVE(%d,%s)", mappedMotorID, formattedPulses.toUtf8().constData());
        
        if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0) {
            QString successMsg = QString("[存储机构] 向前转位命令已发送 (命令方式)");
            qDebug() << successMsg;
            ui->tb_cmdWindow_2->append(successMsg);
            
            // 电机应该已经启动运动，添加一个延时等待运动完成
            QTimer::singleShot(500, this, [this, targetPosition]() {
                m_storageCurrentPosition = targetPosition;
                updateStorageStatus();
            });
        } else {
            QString errMsg = QString("[存储机构] 错误: 执行运动失败 - %1").arg(cmdbuffAck);
        qDebug() << errMsg;
        ui->tb_cmdWindow_2->append(errMsg);
        return;
        }
    }
}

/**
 * @brief 设置存储机构运动参数
 */
bool zmotionpage::setStorageMotionParameters(int motorID)
{
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    
    // 设置速度
    sprintf(cmdbuff, "SPEED(%d)=%f", motorID, STORAGE_DEFAULT_SPEED);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[存储机构] 错误: 设置速度失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return false;
    }
    
    // 设置加速度
    sprintf(cmdbuff, "ACCEL(%d)=%f", motorID, STORAGE_DEFAULT_SPEED * ACCEL_RATIO);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[存储机构] 错误: 设置加速度失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return false;
    }
    
    // 设置减速度
    sprintf(cmdbuff, "DECEL(%d)=%f", motorID, STORAGE_DEFAULT_SPEED * DECEL_RATIO);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[存储机构] 错误: 设置减速度失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return false;
    }
    
    return true;
}

/**
 * @brief 更新旋转状态
 */
void zmotionpage::updateRotationStatus()
{
    if (!g_handle)
    {
        return;
    }

    // 使用映射后的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_ROTATION]; // 旋转电机

    // 获取当前电机状态
    float currentSpeed = 0.0f;
    ZAux_Direct_GetMspeed(g_handle, mappedMotorID, &currentSpeed);

    // 获取DAC值
    float currentDAC = 0.0f;
    ZAux_Direct_GetDAC(g_handle, mappedMotorID, &currentDAC);

    // 如果已经手动停止但状态没更新，不要在这里覆盖状态
    if (!m_isRotating && std::abs(currentSpeed) < 1.0 && std::abs(currentDAC) < 1.0)
    {
        return;
    }

    // 如果电机实际已停止但状态为旋转中，则更新状态
    if (m_isRotating && std::abs(currentSpeed) < 1.0 && std::abs(currentDAC) < 1.0)
    {
        m_isRotating = false;

        // 只有在自动模式下才输出调试信息
        if (m_robotArmStatusTimer && m_robotArmStatusTimer->isActive())
        {
            QString msg = QString("[旋转] 旋转已停止 (自动检测)");
            qDebug() << msg;
            ui->tb_cmdWindow_2->append(msg);
        }
    }
}
/* ===================================== 冲击电机控制和显示函数 ===================================== */
/**
 * @brief 更新冲击状态
 */
void zmotionpage::updatePercussionStatus()
{
    if (!g_handle)
    {
        return;
    }

    // 使用映射后的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_PERCUSSION]; // 冲击电机

    // 获取当前电机状态
    float currentSpeed = 0.0f;
    ZAux_Direct_GetMspeed(g_handle, mappedMotorID, &currentSpeed);

    // 获取DAC值
    float currentDAC = 0.0f;
    ZAux_Direct_GetDAC(g_handle, mappedMotorID, &currentDAC);

    // 如果已经手动停止但状态没更新，不要在这里覆盖状态
    if (!m_isPercussing && std::abs(currentSpeed) < 1.0 && std::abs(currentDAC) < 1.0)
    {
        return;
    }

    // 如果电机实际已停止但状态为冲击中，则更新状态
    if (m_isPercussing && std::abs(currentSpeed) < 1.0 && std::abs(currentDAC) < 1.0)
    {
        m_isPercussing = false;

        // 只有在自动模式下才输出调试信息
        if (m_robotArmStatusTimer && m_robotArmStatusTimer->isActive())
        {
            QString msg = QString("[冲击] 冲击已停止 (自动检测)");
            qDebug() << msg;
            ui->tb_cmdWindow_2->append(msg);
        }
    }
}

/**
 * @brief 设置旋转切割电机的速度
 */
void zmotionpage::on_le_rotation_editingFinished()
{
    bool ok;
    float speed_rpm = ui->le_rotation->text().toFloat(&ok);
    
    // 验证速度范围 (10rpm-190rpm)
    if (!ok || speed_rpm < 10 || speed_rpm > 190) {
        QString msg = QString("[旋转] 错误: 无效的速度值，应在10-190rpm范围内");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        
        // 修正速度到有效范围
        if (!ok || speed_rpm < 10) speed_rpm = 10;
        if (speed_rpm > 190) speed_rpm = 190;
        
        // 更新显示
        ui->le_rotation->setText(QString::number(speed_rpm, 'f', 1));
    }
    
    // 保存设定速度
    m_rotationSpeed = speed_rpm;
    
    QString msg = QString("[旋转] 已设置旋转速度: %1 rpm")
                .arg(speed_rpm, 0, 'f', 1);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
    
    // 如果正在旋转，则更新当前速度
    if (m_isRotating && g_handle) {
        // 计算DAC值 (0-1000，对应0-100%)
        float dac_value = (speed_rpm * 15.5  / 3000.0f) * 1000.0f;
        
        // 更新DAC值
        char cmdbuff[2048];
        char cmdbuffAck[2048];
        sprintf(cmdbuff, "DAC(%d)=%f", MotorMap[MOTOR_IDX_PERCUSSION], dac_value);
        
        if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) == 0) {
            QString updateMsg = QString("[旋转] 已更新运行速度: %1 rpm (DAC值: %2)")
                        .arg(speed_rpm, 0, 'f', 1)
                        .arg(dac_value, 0, 'f', 1);
            qDebug() << updateMsg;
            ui->tb_cmdWindow_2->append(updateMsg);
        }
    }
}

/**
 * @brief 设置冲击频率，把频率转化成脉冲
 */
void zmotionpage::on_le_percussion_editingFinished()
{
    bool ok;
    float freq = ui->le_percussion->text().toFloat(&ok);
    
    // 验证频率范围 (-1.33Hz到1.33Hz)
    if (!ok || freq < -1.33f || freq > 1.33f) {
        QString msg = QString("[冲击] 错误: 无效的频率值，应在-1.33到1.33Hz范围内");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        
        // 重置为0
        freq = 0.0f;
        ui->le_percussion->setText("0.0");
    }
    
    // 计算脉冲频率: f × 663,552
    m_percussionFrequency = freq;
    float pulseFreq = freq * 663552.0f;
    
    QString msg = QString("[冲击] 已设置冲击频率: %1 Hz (脉冲频率: %2)")
                .arg(freq, 0, 'f', 2)
                .arg(pulseFreq, 0, 'f', 0);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
    
    // 如果正在冲击，则更新当前频率
    if (m_isPercussing) {
        on_btn_percussion_clicked();
    }
}

/**
 * @brief 开始旋转切割
 */
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

    // 使用映射后的电机ID - 用户指定旋转电机为MotorMap[0] = 0
    int mappedMotorID = MotorMap[MOTOR_IDX_ROTATION];  // 旋转电机
    
    QString mappingInfo = QString("[旋转] 电机映射: 使用ROTATION_MOTOR_ID=0 -> 映射到实际电机ID=%1")
                        .arg(mappedMotorID);
    qDebug() << mappingInfo;
    ui->tb_cmdWindow_2->append(mappingInfo);

    // 读取旋转速度并确保在有效范围内(10-190rpm)
    bool ok;
    float speed_rpm = ui->le_rotation->text().toFloat(&ok);
    
    // 验证速度范围
    if (!ok || speed_rpm < 10 || speed_rpm > 190) {
        QString msg = QString("[旋转] 错误: 无效的速度值，应在10-190rpm范围内");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        
        // 修正速度到有效范围
        if (!ok || speed_rpm < 10) speed_rpm = 10;
        if (speed_rpm > 190) speed_rpm = 190;
        
        // 更新显示
        ui->le_rotation->setText(QString::number(speed_rpm, 'f', 1));
    }
    
    // 计算旋转DAC值 
    float dac_value = (speed_rpm / 192.0f) * 600.0f;
    
    // 设置Atype确保电机处于速度模式
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    
    // 设置为速度模式 (Atype=66)
    sprintf(cmdbuff, "ATYPE(%d)=66", mappedMotorID);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[旋转] 错误: 设置速度模式失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 使用DAC设置速度
    sprintf(cmdbuff, "DAC(%d)=%f", mappedMotorID, dac_value);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[旋转] 错误: 设置DAC值失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 更新状态
    m_isRotating = true;
    m_rotationSpeed = speed_rpm;
    
    QString msg = QString("[旋转] 开始旋转，速度: %1 rpm (DAC值: %2)")
                .arg(speed_rpm, 0, 'f', 1)
                .arg(dac_value, 0, 'f', 1);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
}

/**
 * @brief 停止旋转切割
 */
void zmotionpage::on_btn_rotation_stop_clicked()
{
    // 检查控制器连接状态
    if (!g_handle) {
        QString msg = QString("[旋转] 错误: 控制器未连接");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 使用映射后的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_ROTATION];  // 旋转电机

    // 如果不在旋转状态，则不需要停止
    if (!m_isRotating) {
        QString msg = QString("[旋转] 当前未在旋转");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 执行停止命令 - 将DAC设为0
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    sprintf(cmdbuff, "DAC(%d)=0", mappedMotorID);
    
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

/**
 * @brief 开始冲击功能
 */
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

    // 使用映射后的电机ID - 用户指定冲击电机为MotorMap[1]
    int mappedMotorID = MotorMap[MOTOR_IDX_PERCUSSION];  // 冲击电机
    
    QString mappingInfo = QString("[冲击] 电机映射: 使用PERCUSSION_MOTOR_ID=1 -> 映射到实际电机ID=%1")
                        .arg(mappedMotorID);
    qDebug() << mappingInfo;
    ui->tb_cmdWindow_2->append(mappingInfo);

    // 读取冲击频率并确保在有效范围内(-1.33Hz到1.33Hz)
    bool ok;
    float freq = ui->le_percussion->text().toFloat(&ok);
    
    // 验证频率范围
    if (!ok || freq < -1.33f || freq > 1.33f) {
        QString msg = QString("[冲击] 错误: 无效的频率值，应在-1.33到1.33Hz范围内");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        
        // 重置为0
        freq = 0.0f;
        ui->le_percussion->setText("0.0");
    }
    
    // 计算DAC值 (0-1000，对应0-100%)
    // 频率正负决定DAC正负，频率大小决定DAC比例
    // 当|freq|=1.33时，使用最大DAC值1000
    float dac_value = (freq / 1.33f) * 882005.0f;
    
    // 设置Atype确保电机处于速度模式
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    
    // 设置为速度模式 (Atype=66)
    sprintf(cmdbuff, "ATYPE(%d)=66", mappedMotorID);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[冲击] 错误: 设置速度模式失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 使用DAC设置速度
    sprintf(cmdbuff, "DAC(%d)=%f", mappedMotorID, dac_value);
    if (ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048) != 0) {
        QString msg = QString("[冲击] 错误: 设置DAC值失败");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }
    
    // 更新状态
    m_isPercussing = true;
    m_percussionFrequency = freq;
    
    QString msg = QString("[冲击] 开始冲击，频率: %1 Hz (DAC值: %2)")
                .arg(freq, 0, 'f', 2)
                .arg(dac_value, 0, 'f', 1);
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);
}

/**
 * @brief 停止冲击功能
 */
void zmotionpage::on_btn_percussion_stop_clicked()
{
    // 检查控制器连接状态
    if (!g_handle) {
        QString msg = QString("[冲击] 错误: 控制器未连接");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 使用映射后的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_PERCUSSION];  // 冲击电机

    // 如果不在冲击状态，则不需要停止
    if (!m_isPercussing) {
        QString msg = QString("[冲击] 当前未在冲击");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 执行停止命令 - 将DAC设为0
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    sprintf(cmdbuff, "DAC(%d)=0", mappedMotorID);
    
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
/* ===================================== 下夹紧电机的控制和显示函数 ===================================== */
/**
 * @brief 打开下夹紧的夹爪
 */
void zmotionpage::on_btn_downclamp_open_clicked()
{
    if (!g_handle)
    {
        QString msg = QString("[夹爪] 错误: 控制器未连接");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 确认使用的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_DOWNCLAMP]; // 下夹紧电机

    // 记录下夹紧电机当前位置
    float moveMotorPos = 0.0f;
    ZAux_Direct_GetMpos(g_handle, mappedMotorID, &moveMotorPos);

    QString posMsg = QString("[夹爪] 操作前下夹紧电机位置: %1").arg(moveMotorPos);
    qDebug() << posMsg;
    ui->tb_cmdWindow_2->append(posMsg);

    // 明确设置BASE仅为下夹紧电机
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    sprintf(cmdbuff, "BASE(%d)", mappedMotorID);
    ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048);

    QString debugMsg = QString("[夹爪] 设置BASE仅为下夹紧电机: %1").arg(mappedMotorID);
    qDebug() << debugMsg;
    ui->tb_cmdWindow_2->append(debugMsg);

    // 设置位置控制模式
    int ret = ZAux_Direct_SetAtype(g_handle, mappedMotorID, POSITION_MODE); // 切换到位置模式
    if (ret != 0)
    {
        QString msg = QString("[夹爪] 错误: 设置位置模式失败，错误码: %1").arg(ret);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 设置适当的速度参数
    ZAux_Direct_SetSpeed(g_handle, mappedMotorID, DOWNCLAMP_DEFAULT_SPEED);
    ZAux_Direct_SetAccel(g_handle, mappedMotorID, DOWNCLAMP_DEFAULT_SPEED * ACCEL_RATIO);
    ZAux_Direct_SetDecel(g_handle, mappedMotorID, DOWNCLAMP_DEFAULT_SPEED * DECEL_RATIO);

    // 移动到0点位置（打开夹爪）
    ret = ZAux_Direct_Single_MoveAbs(g_handle, mappedMotorID, 0);
    if (ret != 0)
    {
        QString msg = QString("[夹爪] 错误: 移动到0点位置失败，错误码: %1").arg(ret);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    QString msg = QString("[夹爪] 正在打开 (位置模式，移动到0点)");
    qDebug() << msg;
    ui->tb_cmdWindow_2->append(msg);

    m_isDownclamping = true;
    m_downclampTimer->start(); // 开始监控位置变化
}

/**
 * @brief 关闭下夹紧的夹爪
 */
void zmotionpage::on_btn_downclamp_close_clicked()
{
    if (!g_handle)
    {
        QString msg = QString("[夹爪] 错误: 控制器未连接");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 确认使用的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_DOWNCLAMP]; // 下夹紧电机

    // 记录下夹紧电机当前位置
    float moveMotorPos = 0.0f;
    ZAux_Direct_GetMpos(g_handle, mappedMotorID, &moveMotorPos);

    QString posMsg = QString("[夹爪] 操作前下夹紧电机位置: %1").arg(moveMotorPos);
    qDebug() << posMsg;
    ui->tb_cmdWindow_2->append(posMsg);

    // 明确设置BASE仅为下夹紧电机
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    sprintf(cmdbuff, "BASE(%d)", mappedMotorID);
    ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048);

    QString debugMsg = QString("[夹爪] 设置BASE仅为下夹紧电机: %1").arg(mappedMotorID);
    qDebug() << debugMsg;
    ui->tb_cmdWindow_2->append(debugMsg);

    // 设置力矩控制模式
    int ret = ZAux_Direct_SetAtype(g_handle, mappedMotorID, TORQUE_MODE); // 设置为力矩模式
    if (ret != 0)
    {
        QString msg = QString("[夹爪] 错误: 设置力矩模式失败，错误码: %1").arg(ret);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 先停止之前可能存在的定时器
    if (m_downclampInitTimer) {
        m_downclampInitTimer->stop();
        m_downclampInitTimer->deleteLater();
        m_downclampInitTimer = nullptr;
    }

    // 创建一个状态对象，用于定时器内部使用
    class ClampStatus {
    public:
        float lastPosition = 0.0f;
        int stableCount = 0;
        float currentDAC = ROBOTARM_CLAMP_INIT_DAC;  // 初始值为10
        bool completed = false;
    };
    
    // 使用智能指针管理状态对象
    QSharedPointer<ClampStatus> status(new ClampStatus());
    
    // 步骤1: 初始DAC设置为小值，逐渐增大
    ret = ZAux_Direct_SetDAC(g_handle, mappedMotorID, status->currentDAC);
    if (ret != 0)
    {
        QString errorMsg = QString("[夹爪] 错误: 设置DAC失败，错误码: %1").arg(ret);
        qDebug() << errorMsg;
        ui->tb_cmdWindow_2->append(errorMsg);
        return;
    }
    
    QString dacMsg = QString("[夹爪] 开始关闭，初始DAC=%1（将逐渐增大到%2）")
        .arg(status->currentDAC)
        .arg(ROBOTARM_CLAMP_MAX_DAC);
    qDebug() << dacMsg;
    ui->tb_cmdWindow_2->append(dacMsg);

    // 获取当前位置作为初始位置
    ZAux_Direct_GetMpos(g_handle, mappedMotorID, &status->lastPosition);
    
    // 创建新的定时器
    m_downclampInitTimer = new QTimer(this);
    
    connect(m_downclampInitTimer, &QTimer::timeout, this, [this, mappedMotorID, status]() {
        // 如果已经完成，停止定时器
        if (status->completed) {
            m_downclampInitTimer->stop();
            m_downclampInitTimer->deleteLater();
            m_downclampInitTimer = nullptr;
            return;
        }

        float currentPosition = 0.0f;
        ZAux_Direct_GetMpos(g_handle, mappedMotorID, &currentPosition);
        
        QString posMsg = QString("[夹爪] 当前位置: %1, DAC: %2")
                        .arg(currentPosition, 0, 'f', 2)
                        .arg(status->currentDAC, 0, 'f', 1);
        qDebug() << posMsg;
        ui->tb_cmdWindow_2->append(posMsg);
        
        // 判断位置是否稳定（变化小于设定阈值）
        if (std::abs(currentPosition - status->lastPosition) < ROBOTARM_CLAMP_STABLE_THRESHOLD) {
            status->stableCount++;
            
            QString stableMsg = QString("[夹爪] 位置稳定计数: %1/%2")
                               .arg(status->stableCount)
                               .arg(ROBOTARM_CLAMP_STABLE_COUNT);
            qDebug() << stableMsg;
            ui->tb_cmdWindow_2->append(stableMsg);
            
            // 如果连续多次读取位置变化小于阈值，认为位置稳定
            if (status->stableCount >= ROBOTARM_CLAMP_STABLE_COUNT) {
                // 如果当前DAC值小于最大值，继续增大DAC值
                if (status->currentDAC < ROBOTARM_CLAMP_MAX_DAC) {
                    status->currentDAC += ROBOTARM_CLAMP_DAC_INCREMENT; 
                    
                    int ret = ZAux_Direct_SetDAC(g_handle, mappedMotorID, status->currentDAC);
                    if (ret != 0) {
                        QString errorMsg = QString("[夹爪] 错误: 增大DAC值失败，错误码: %1").arg(ret);
                        qDebug() << errorMsg;
                        ui->tb_cmdWindow_2->append(errorMsg);
                        // 继续执行，不返回
                    }
                    
                    QString increaseDacMsg = QString("[夹爪] 增大DAC值到: %1").arg(status->currentDAC);
                    qDebug() << increaseDacMsg;
                    ui->tb_cmdWindow_2->append(increaseDacMsg);
                    
                    // 重置计数器
                    status->stableCount = 0;
                } 
                // 如果已经达到最大DAC值，完成夹紧过程
                else {
                    QString completedMsg = QString("[夹爪] 夹紧完成，最终DAC值: %1").arg(status->currentDAC);
                    qDebug() << completedMsg;
                    ui->tb_cmdWindow_2->append(completedMsg);
                    
                    status->completed = true;
                }
            }
        } else {
            // 位置变化大于阈值，说明夹爪正在移动
            status->stableCount = 0;
            status->lastPosition = currentPosition;
        }
    });
    
    // 开始监测位置，每500毫秒检查一次
    m_downclampInitTimer->start(ROBOTARM_CLAMP_MONITOR_INTERVAL);
    
    m_isDownclamping = true;
    m_downclampTimer->start(); // 同时启动常规的位置监控定时器
}

/**
 * @brief 设置下夹紧机构夹爪的力矩
 */
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

/**
 * @brief 更新下夹紧机构夹爪的状态
 */ 
void zmotionpage::updateDownclampStatus()
{
    if (!g_handle || !m_isDownclamping)
    {
        m_downclampTimer->stop();
        return;
    }

    // 使用映射后的电机ID
    int mappedMotorID = MotorMap[MOTOR_IDX_DOWNCLAMP]; // 下夹紧电机

    // 确保BASE仅设置为当前电机
    char cmdbuff[2048];
    char cmdbuffAck[2048];
    sprintf(cmdbuff, "BASE(%d)", mappedMotorID);
    ZAux_DirectCommand(g_handle, cmdbuff, cmdbuffAck, 2048);

    static float lastPosition = 0.0f;
    static float lastSpeed = 0.0f;
    static QElapsedTimer m_elapsedTimer;
    static bool timerStarted = false;

    if (!timerStarted)
    {
        m_elapsedTimer.start();
        timerStarted = true;
        ZAux_Direct_GetMpos(g_handle, mappedMotorID, &lastPosition);
        ZAux_Direct_GetMspeed(g_handle, mappedMotorID, &lastSpeed);
    }

    // 获取当前位置和速度
    float currentPosition = 0.0f;
    float currentSpeed = 0.0f;
    ZAux_Direct_GetMpos(g_handle, mappedMotorID, &currentPosition);
    ZAux_Direct_GetMspeed(g_handle, mappedMotorID, &currentSpeed);

    ui->le_downclamp_status->setText(QString::number(currentPosition, 'f', 2));

    // 检查速度变化是否很小（堵转检测）
    if (abs(currentSpeed - lastSpeed) < 0.1 && abs(currentSpeed) < 0.2)
    {
        if (m_elapsedTimer.elapsed() > 500)
        { // 如果速度连续0.5秒几乎不变
            // 停止力矩输出，切换到位置模式
            ZAux_Direct_SetDAC(g_handle, mappedMotorID, 0);
            QThread::msleep(100);

            // 切换到位置模式并保持当前位置
            ZAux_Direct_SetAtype(g_handle, mappedMotorID, 65);
            ZAux_Direct_Single_MoveAbs(g_handle, mappedMotorID, currentPosition);

            QString msg = QString("[夹爪] 检测到堵转，切换到位置模式保持当前位置: %1").arg(currentPosition, 0, 'f', 2);
            qDebug() << msg;
            ui->tb_cmdWindow_2->append(msg);

            m_isDownclamping = false;
            m_downclampTimer->stop();
            timerStarted = false;
            return;
        }
    }
    else
    {
        // 如果速度有变化，重置计时器
        m_elapsedTimer.restart();
    }

    lastPosition = currentPosition;
    lastSpeed = currentSpeed;

    // 检查是否超时
    if (m_elapsedTimer.elapsed() > DOWNCLAMP_TIMEOUT)
    {
        // 超时停止
        ZAux_Direct_SetDAC(g_handle, mappedMotorID, 0);
        QThread::msleep(100);

        // 切换到位置模式并保持当前位置
        ZAux_Direct_SetAtype(g_handle, mappedMotorID, 65);
        ZAux_Direct_Single_MoveAbs(g_handle, mappedMotorID, currentPosition);

        QString msg = QString("[夹爪] 操作超时，切换到位置模式保持当前位置: %1").arg(currentPosition, 0, 'f', 2);
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);

        m_isDownclamping = false;
        m_downclampTimer->stop();
        timerStarted = false;
    }
}

/**
 * @brief 初始化所有定时器
 */
void zmotionpage::initializeTimers()
{
    // 创建基础信息刷新定时器
    m_basicInfoTimer = new QTimer(this);
    m_basicInfoTimer->setInterval(TIMER_BASIC_INFO_INTERVAL);
    connect(m_basicInfoTimer, &QTimer::timeout, this, &zmotionpage::basicInfoRefresh);

    // 创建高级信息刷新定时器
    m_advanceInfoTimer = new QTimer(this);
    m_advanceInfoTimer->setInterval(TIMER_ADVANCE_INFO_INTERVAL);
    connect(m_advanceInfoTimer, &QTimer::timeout, this, &zmotionpage::advanceInfoRefreash);

    // 创建实时参数刷新定时器
    m_realtimeParmTimer = new QTimer(this);
    m_realtimeParmTimer->setInterval(TIMER_REALTIME_PARAM_INTERVAL);
    connect(m_realtimeParmTimer, &QTimer::timeout, this, &zmotionpage::RefreshTableRealTimeContent);

    // 创建夹紧监控定时器
    m_clampMonitorTimer = new QTimer(this);
    m_clampMonitorTimer->setInterval(TIMER_CLAMP_MONITOR_INTERVAL);
    connect(m_clampMonitorTimer, &QTimer::timeout, this, &zmotionpage::monitorClampSpeed);
    
    // 创建机械手状态更新定时器
    m_robotArmStatusTimer = new QTimer(this);
    m_robotArmStatusTimer->setInterval(TIMER_ROBOTARM_STATUS_INTERVAL);
    connect(m_robotArmStatusTimer, &QTimer::timeout, this, [this]() {
        updateRotationAngle();
        updateExtentLength();
        updateClampStatus();
        updatePenetrationDepth();
        updateStorageStatus();
        updateRotationStatus();
        updatePercussionStatus();
        updateDownclampStatus();
    });

    // 创建夹爪状态监控定时器
    m_downclampTimer = new QTimer(this);
    m_downclampTimer->setInterval(TIMER_DOWNCLAMP_STATUS_INTERVAL);
    connect(m_downclampTimer, &QTimer::timeout, this, &zmotionpage::updateDownclampStatus);
}

/**
 * @brief 启动监控类定时器
 */
void zmotionpage::startMonitoringTimers()
{
    // 仅在总线已初始化的情况下启动定时器
    if (initflag) {
        // 启动信息刷新定时器
        if (!m_basicInfoTimer->isActive()) {
            m_basicInfoTimer->start();
        }
        
        if (!m_advanceInfoTimer->isActive()) {
            m_advanceInfoTimer->start();
        }
        
        if (!m_realtimeParmTimer->isActive() && ui->CB_AutoUpdate->isChecked()) {
            m_realtimeParmTimer->start();
        }
        
        // 启动状态监控定时器
        if (!m_robotArmStatusTimer->isActive()) {
            m_robotArmStatusTimer->start();
        }
    }
}

/**
 * @brief 停止监控类定时器
 */
void zmotionpage::stopMonitoringTimers()
{
    // 停止所有监控定时器
    if (m_basicInfoTimer->isActive()) {
        m_basicInfoTimer->stop();
    }
    
    if (m_advanceInfoTimer->isActive()) {
        m_advanceInfoTimer->stop();
    }
    
    if (m_realtimeParmTimer->isActive()) {
        m_realtimeParmTimer->stop();
    }
    
    if (m_robotArmStatusTimer->isActive()) {
        m_robotArmStatusTimer->stop();
    }
    
    if (m_clampMonitorTimer->isActive()) {
        m_clampMonitorTimer->stop();
    }
    
    if (m_downclampTimer->isActive()) {
        m_downclampTimer->stop();
    }
    
    if (m_connectionStatusTimer->isActive()) {
        m_connectionStatusTimer->stop();
    }
}

/**
 * @brief 机械手移动初始化
 * 
 * 该函数执行以下步骤：
 * 1. 将电机设置为力矩模式(Atype=67)
 * 2. 设置DAC为-50回收，监测位置直到稳定
 * 3. 将当前位置设为零点，DAC设为0
 * 4. 将电机设置回位置模式(Atype=65)
 */
void zmotionpage::on_btn_ROBOTEXTENSION_init_clicked()
{
    if (!g_handle)
    {
        QString msg = QString("[机械手移动初始化] 错误: 控制器未连接");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 使用电机索引常量
    int mappedMotorID = MotorMap[MOTOR_IDX_ROBOTEXTENSION]; // 机械手移动电机
    
    QString startMsg = QString("[机械手移动初始化] 开始初始化电机 MotorMap[%1]=%2")
                      .arg(MOTOR_IDX_ROBOTEXTENSION)
                      .arg(mappedMotorID);
    qDebug() << startMsg;
    ui->tb_cmdWindow_2->append(startMsg);
    
    // 步骤1: 设置为力矩模式
    int ret = ZAux_Direct_SetAtype(g_handle, mappedMotorID, TORQUE_MODE);
    if (ret != 0)
    {
        QString errorMsg = QString("[机械手移动初始化] 错误: 设置力矩模式失败，错误码: %1").arg(ret);
        qDebug() << errorMsg;
        ui->tb_cmdWindow_2->append(errorMsg);
        return;
    }
    
    QString modeMsg = QString("[机械手移动初始化] 已设置为力矩模式(Atype=%1)").arg(TORQUE_MODE);
    qDebug() << modeMsg;
    ui->tb_cmdWindow_2->append(modeMsg);
    
    // 步骤2: 设置DAC为初始化DAC值以回收
    ret = ZAux_Direct_SetDAC(g_handle, mappedMotorID, ROBOTEXTENSION_INIT_DAC);
    if (ret != 0)
    {
        QString errorMsg = QString("[机械手移动初始化] 错误: 设置DAC失败，错误码: %1").arg(ret);
        qDebug() << errorMsg;
        ui->tb_cmdWindow_2->append(errorMsg);
        return;
    }
    
    QString dacMsg = QString("[机械手移动初始化] 已设置DAC=%1开始回收").arg(ROBOTEXTENSION_INIT_DAC);
    qDebug() << dacMsg;
    ui->tb_cmdWindow_2->append(dacMsg);
    
    // 先停止之前可能存在的定时器
    if (m_robotExtensionInitTimer) {
        m_robotExtensionInitTimer->stop();
        m_robotExtensionInitTimer->deleteLater();
    }
    
    // 创建新的定时器
    m_robotExtensionInitTimer = new QTimer(this);
    float lastPosition = 0.0f;
    int stableCount = 0;
    
    connect(m_robotExtensionInitTimer, &QTimer::timeout, this, [this, mappedMotorID, lastPosition, stableCount]() mutable {
        float currentPosition = 0.0f;
        ZAux_Direct_GetMpos(g_handle, mappedMotorID, &currentPosition);
        
        QString posMsg = QString("[机械手移动初始化] 当前位置: %1").arg(currentPosition, 0, 'f', 2);
        qDebug() << posMsg;
        ui->tb_cmdWindow_2->append(posMsg);
        
        // 判断位置是否稳定
        if (std::abs(currentPosition - lastPosition) < ROBOTEXTENSION_POSITION_TOLERANCE) {
            stableCount++;
            
            QString stableMsg = QString("[机械手移动初始化] 位置稳定计数: %1/%2").arg(stableCount).arg(ROBOTEXTENSION_STABLE_COUNT);
            qDebug() << stableMsg;
            ui->tb_cmdWindow_2->append(stableMsg);
            
            // 如果连续N次读取位置变化小于阈值，认为位置已稳定
            if (stableCount >= ROBOTEXTENSION_STABLE_COUNT) {
                m_robotExtensionInitTimer->stop();
                
                // 步骤3: 将DAC设为0
                ZAux_Direct_SetDAC(g_handle, mappedMotorID, 0);
                
                // 将当前位置设为零点 - 先设置DPOS再设置MPOS
                ZAux_Direct_SetDpos(g_handle, mappedMotorID, 0);
                ZAux_Direct_SetMpos(g_handle, mappedMotorID, 0);
                
                QString resetMsg = QString("[机械手移动初始化] 位置已稳定，已设置当前位置为零点，DAC=0");
                qDebug() << resetMsg;
                ui->tb_cmdWindow_2->append(resetMsg);
                
                // 步骤4: 设置回位置模式
                ZAux_Direct_SetAtype(g_handle, mappedMotorID, POSITION_MODE);
                
                QString completedMsg = QString("[机械手移动初始化] 初始化完成，已设置为位置模式(Atype=%1)").arg(POSITION_MODE);
                qDebug() << completedMsg;
                ui->tb_cmdWindow_2->append(completedMsg);
                
                // 清理定时器
                m_robotExtensionInitTimer->deleteLater();
                m_robotExtensionInitTimer = nullptr;
            }
        } else {
            // 位置不稳定，重置计数
            stableCount = 0;
            lastPosition = currentPosition;
        }
    });
    
    // 开始监测位置，每500毫秒检查一次
    m_robotExtensionInitTimer->start(500);
}

/**
 * @brief 机械手夹爪初始化
 * 
 * 该函数执行以下步骤：
 * 1. 将电机设置为力矩模式(Atype=67)
 * 2. DAC设置成10，监测电机的位置，逐步增加DAC值(每次+10，最大80)
 * 3. 当位置停止变化，认为夹爪张开到最大
 * 4. 将当前位置设为零点，DAC设为0
 * 5. 将电机设置回位置模式(Atype=65)
 */
void zmotionpage::on_btn_ROBOTCLAMP_init_clicked()
{
    if (!g_handle)
    {
        QString msg = QString("[机械手夹爪初始化] 错误: 控制器未连接");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 使用电机索引常量
    int mappedMotorID = MotorMap[MOTOR_IDX_ROBOTCLAMP]; // 机械手夹紧电机
    
    QString startMsg = QString("[机械手夹爪初始化] 开始初始化电机 MotorMap[%1]=%2")
                      .arg(MOTOR_IDX_ROBOTCLAMP)
                      .arg(mappedMotorID);
    qDebug() << startMsg;
    ui->tb_cmdWindow_2->append(startMsg);
    
    // 步骤1: 设置为力矩模式
    int ret = ZAux_Direct_SetAtype(g_handle, mappedMotorID, ROBOTARM_CLAMP_TORQUE_MODE);
    if (ret != 0)
    {
        QString errorMsg = QString("[机械手夹爪初始化] 错误: 设置力矩模式失败，错误码: %1").arg(ret);
        qDebug() << errorMsg;
        ui->tb_cmdWindow_2->append(errorMsg);
        return;
    }
    
    QString modeMsg = QString("[机械手夹爪初始化] 已设置为力矩模式(Atype=%1)").arg(ROBOTARM_CLAMP_TORQUE_MODE);
    qDebug() << modeMsg;
    ui->tb_cmdWindow_2->append(modeMsg);
    
    // 先停止之前可能存在的定时器
    if (m_robotClampInitTimer) {
        m_robotClampInitTimer->stop();
        m_robotClampInitTimer->deleteLater();
    }
    
    // 创建一个状态对象，用于定时器内部使用
    class InitStatus {
    public:
        float lastPosition = 0.0f;
        int stableCount = 0;
        float currentDAC = ROBOTARM_CLAMP_INIT_DAC;  // 机械手夹爪初始DAC为正值，用于张开夹爪
        bool increasedDAC = false;
    };
    
    // 使用智能指针管理状态对象
    QSharedPointer<InitStatus> status(new InitStatus());
    
    // 步骤2: 初始DAC设置为负值，用于张开夹爪
    ret = ZAux_Direct_SetDAC(g_handle, mappedMotorID, status->currentDAC);
    if (ret != 0)
    {
        QString errorMsg = QString("[机械手夹爪初始化] 错误: 设置DAC失败，错误码: %1").arg(ret);
        qDebug() << errorMsg;
        ui->tb_cmdWindow_2->append(errorMsg);
        return;
    }
    
    QString dacMsg = QString("[机械手夹爪初始化] 已设置初始DAC=%1 (正值张开夹爪)").arg(status->currentDAC);
    qDebug() << dacMsg;
    ui->tb_cmdWindow_2->append(dacMsg);
    
    // 获取当前位置作为初始位置
    ZAux_Direct_GetMpos(g_handle, mappedMotorID, &status->lastPosition);
    
    // 创建新的定时器
    m_robotClampInitTimer = new QTimer(this);
    
    connect(m_robotClampInitTimer, &QTimer::timeout, this, [this, mappedMotorID, status]() {
        float currentPosition = 0.0f;
        ZAux_Direct_GetMpos(g_handle, mappedMotorID, &currentPosition);
        
        QString posMsg = QString("[机械手夹爪初始化] 当前位置: %1, DAC: %2")
                         .arg(currentPosition, 0, 'f', 2)
                         .arg(status->currentDAC, 0, 'f', 1);
        qDebug() << posMsg;
        ui->tb_cmdWindow_2->append(posMsg);
        
        // 判断位置是否稳定（变化小于稳定阈值）
        if (std::abs(currentPosition - status->lastPosition) < ROBOTARM_CLAMP_STABLE_THRESHOLD) {
            status->stableCount++;
            
            QString stableMsg = QString("[机械手夹爪初始化] 位置稳定计数: %1/%2").arg(status->stableCount).arg(ROBOTARM_CLAMP_STABLE_COUNT);
            qDebug() << stableMsg;
            ui->tb_cmdWindow_2->append(stableMsg);
            
            // 如果连续N次读取位置变化小于阈值，认为位置暂时稳定
            if (status->stableCount >= ROBOTARM_CLAMP_STABLE_COUNT) {

                if (status->currentDAC < ROBOTARM_CLAMP_MAX_DAC) {
                    status->currentDAC += ROBOTARM_CLAMP_DAC_INCREMENT;  // 加上增量
                    status->increasedDAC = true;
                    
                    int ret = ZAux_Direct_SetDAC(g_handle, mappedMotorID, status->currentDAC);
                    if (ret != 0) {
                        QString errorMsg = QString("[机械手夹爪初始化] 错误: 增加DAC值失败，错误码: %1").arg(ret);
                        qDebug() << errorMsg;
                        ui->tb_cmdWindow_2->append(errorMsg);
                        // 继续执行，不返回
                    }
                    
                    QString increaseDacMsg = QString("[机械手夹爪初始化] 增加DAC值到: %1").arg(status->currentDAC);
                    qDebug() << increaseDacMsg;
                    ui->tb_cmdWindow_2->append(increaseDacMsg);
                    
                    // 重置计数器
                    status->stableCount = 0;
                } 
                // 如果已经是最大DAC值或者已经增加过DAC值但位置依然稳定，则认为夹爪已完全张开
                else {
                    m_robotClampInitTimer->stop();
                    
                    // 步骤3: 将DAC设为0
                    ZAux_Direct_SetDAC(g_handle, mappedMotorID, 0);
                    
                    // 将当前位置设为零点
                    ZAux_Direct_SetDpos(g_handle, mappedMotorID, 0);
                    ZAux_Direct_SetMpos(g_handle, mappedMotorID, 0);
                    
                    QString resetMsg = QString("[机械手夹爪初始化] 位置已稳定，已设置当前位置为零点，DAC=0");
                    qDebug() << resetMsg;
                    ui->tb_cmdWindow_2->append(resetMsg);
                    
                    // 步骤4: 设置回位置模式
                    ZAux_Direct_SetAtype(g_handle, mappedMotorID, ROBOTARM_CLAMP_POSITION_MODE);
                    
                    QString completedMsg = QString("[机械手夹爪初始化] 初始化完成，已设置为位置模式(Atype=%1)").arg(ROBOTARM_CLAMP_POSITION_MODE);
                    qDebug() << completedMsg;
                    ui->tb_cmdWindow_2->append(completedMsg);
                    
                    // 清理定时器
                    m_robotClampInitTimer->deleteLater();
                    m_robotClampInitTimer = nullptr;
                }
            }
        } else {
            // 位置变化大于阈值，说明夹爪正在移动
            status->stableCount = 0;
            status->lastPosition = currentPosition;
            status->increasedDAC = false;
        }
    });
    
    // 开始监测位置，每指定毫秒检查一次
    m_robotClampInitTimer->start(ROBOTARM_CLAMP_MONITOR_INTERVAL);
}

// 一键对接功能实现
void zmotionpage::on_btn_connect_fast_clicked()
{
    // 检查是否已经初始化
    if (!initflag) {
        QMessageBox::warning(this, "操作失败", "请先初始化总线！");
        return;
    }
    
    // 如果已经在运行中，不再重复执行
    if (m_connectFastRunning) {
        QMessageBox::warning(this, "操作无效", "一键对接操作正在执行中！");
        return;
    }
    
    // 获取进给电机当前位置
    float penetrationPos = 0.0f;
    int result = ZAux_Direct_GetParam(g_handle, "DPOS", MotorMap[MOTOR_IDX_PENETRATION], &penetrationPos);
    if (result != 0) {
        QMessageBox::warning(this, "操作失败", "获取进给电机位置失败！");
        qDebug() << "一键对接失败: 获取进给电机位置失败, 错误码:" << result;
        return;
    }
    
    // 检查进给电机位置是否满足条件（大于7500000）
    if (penetrationPos <= CONNECT_FAST_MIN_POSITION) {
        QMessageBox::warning(this, "操作失败", QString("进给电机位置必须大于%1才能执行对接操作！").arg(CONNECT_FAST_MIN_POSITION));
        qDebug() << "一键对接失败: 进给电机位置不满足条件，当前位置:" << penetrationPos;
        return;
    }
    
    qDebug() << "开始执行一键对接操作，进给电机当前位置:" << penetrationPos;
    ui->tb_cmdWindow_2->append(QString("开始执行一键对接操作，进给电机当前位置: %1").arg(penetrationPos));
    
    // 设置标志位，表示对接操作正在进行
    m_connectFastRunning = true;
    
    // 设置进给电机速度
    ZAux_Direct_SetParam(g_handle, "SPEED", MotorMap[MOTOR_IDX_PENETRATION], CONNECT_FAST_PENETRATION_SPEED);
    qDebug() << QString("进给电机速度设置为%1").arg(CONNECT_FAST_PENETRATION_SPEED);
    ui->tb_cmdWindow_2->append(QString("进给电机速度设置为%1").arg(CONNECT_FAST_PENETRATION_SPEED));
    
    
    // 设置旋转切割电机为力矩模式(Atype=66)，DAC设为恒定值
    ZAux_Direct_SetParam(g_handle, "ATYPE", MotorMap[MOTOR_IDX_ROTATION], CONNECT_FAST_ROTATION_TORQUE_MODE);
    ZAux_Direct_SetParam(g_handle, "DAC", MotorMap[MOTOR_IDX_ROTATION], CONNECT_FAST_ROTATION_DAC);
    qDebug() << QString("旋转切割电机设置为力矩模式(Atype=%1)，DAC=%2").arg(CONNECT_FAST_ROTATION_TORQUE_MODE).arg(CONNECT_FAST_ROTATION_DAC);
    ui->tb_cmdWindow_2->append(QString("旋转切割电机设置为力矩模式(Atype=%1)，DAC=%2").arg(CONNECT_FAST_ROTATION_TORQUE_MODE).arg(CONNECT_FAST_ROTATION_DAC));
    
    // 计算进给电机的目标位置（当前位置减去设定距离）
    float targetPos = penetrationPos - CONNECT_FAST_PENETRATION_DISTANCE;
    
    // 首先启动旋转切割电机
    ZAux_Direct_Single_Move(g_handle, MotorMap[MOTOR_IDX_ROTATION], 1.0f);
    qDebug() << "启动旋转切割电机";
    ui->tb_cmdWindow_2->append("启动旋转切割电机");
    
    // 然后启动进给电机，向下移动到目标位置
    ZAux_Direct_Single_MoveAbs(g_handle, MotorMap[MOTOR_IDX_PENETRATION], targetPos);
    qDebug() << "启动进给电机，向下移动到目标位置:" << targetPos;
    ui->tb_cmdWindow_2->append(QString("启动进给电机，向下移动到目标位置: %1").arg(targetPos));
    
    // 创建一个定时器来监控进度，这样不会阻塞UI线程
    QTimer* monitorTimer = new QTimer(this);
    connect(monitorTimer, &QTimer::timeout, this, [this, monitorTimer, targetPos]() {
        // 如果不再运行（可能被紧急停止），就不继续
        if (!m_connectFastRunning) {
            monitorTimer->stop();
            monitorTimer->deleteLater();
            return;
        }
        
        // 获取当前位置
        float currentPos = 0.0f;
        int result = ZAux_Direct_GetParam(g_handle, "DPOS", MotorMap[MOTOR_IDX_PENETRATION], &currentPos);
        if (result != 0) {
            qDebug() << "获取进给电机位置失败，错误码:" << result;
            ui->tb_cmdWindow_2->append(QString("获取进给电机位置失败，错误码: %1").arg(result));
            m_connectFastRunning = false;
            monitorTimer->stop();
            monitorTimer->deleteLater();
            return;
        }
        
        // 输出当前位置信息（每设定阈值个单位）
        static float lastReportedPos = 0;
        if (std::abs(currentPos - lastReportedPos) > POSITION_REPORT_THRESHOLD) {
            qDebug() << "当前进给位置:" << currentPos << "，目标位置:" << targetPos;
            ui->tb_cmdWindow_2->append(QString("当前进给位置: %1，目标位置: %2").arg(currentPos).arg(targetPos));
            lastReportedPos = currentPos;
        }
        
        // 检查是否接近目标位置
        if (std::abs(currentPos - targetPos) <= CONNECT_FAST_POSITION_TOLERANCE) {
            // 进给完成后，停止旋转切割电机
            ZAux_Direct_Single_Cancel(g_handle, MotorMap[MOTOR_IDX_ROTATION], 0);
            ZAux_Direct_SetParam(g_handle, "DAC", MotorMap[MOTOR_IDX_ROTATION], 0.0f);  // 停止力矩输出
            qDebug() << "进给完成，停止旋转切割电机";
            ui->tb_cmdWindow_2->append("进给完成，停止旋转切割电机");
            
            // 清理资源
            m_connectFastRunning = false;
            monitorTimer->stop();
            monitorTimer->deleteLater();
            
            // 显示操作完成消息
            QMessageBox::information(this, "操作完成", "一键对接操作已完成！");
        }
    });
    
    // 启动监控定时器
    monitorTimer->start(CONNECT_FAST_MONITOR_INTERVAL);
}

/**
 * @brief 下夹紧初始化
 * 
 * 该函数执行以下步骤：
 * 1. 将电机设置为力矩模式(Atype=67)
 * 2. DAC设置成-10，监测电机的位置，逐步减小DAC值(每次-10，最小-50)
 * 3. 当位置停止变化，认为夹爪张开到最大
 * 4. 将当前位置设为零点，DAC设为0
 * 5. 将电机设置回位置模式(Atype=65)
 */
void zmotionpage::on_btn_DOWNCLAMP_init_clicked()
{
    if (!g_handle)
    {
        QString msg = QString("[下夹紧初始化] 错误: 控制器未连接");
        qDebug() << msg;
        ui->tb_cmdWindow_2->append(msg);
        return;
    }

    // 使用电机索引常量
    int mappedMotorID = MotorMap[MOTOR_IDX_DOWNCLAMP]; // 下夹紧电机
    
    QString startMsg = QString("[下夹紧初始化] 开始初始化电机 MotorMap[%1]=%2")
                      .arg(MOTOR_IDX_DOWNCLAMP)
                      .arg(mappedMotorID);
    qDebug() << startMsg;
    ui->tb_cmdWindow_2->append(startMsg);
    
    // 步骤1: 设置为力矩模式
    int ret = ZAux_Direct_SetAtype(g_handle, mappedMotorID, TORQUE_MODE);
    if (ret != 0)
    {
        QString errorMsg = QString("[下夹紧初始化] 错误: 设置力矩模式失败，错误码: %1").arg(ret);
        qDebug() << errorMsg;
        ui->tb_cmdWindow_2->append(errorMsg);
        return;
    }
    
    QString modeMsg = QString("[下夹紧初始化] 已设置为力矩模式(Atype=%1)").arg(TORQUE_MODE);
    qDebug() << modeMsg;
    ui->tb_cmdWindow_2->append(modeMsg);
    
    // 先停止之前可能存在的定时器
    if (m_downclampInitTimer) {
        m_downclampInitTimer->stop();
        m_downclampInitTimer->deleteLater();
    }
    
    // 创建一个状态对象，用于定时器内部使用
    class InitStatus {
    public:
        float lastPosition = 0.0f;
        int stableCount = 0;
        float currentDAC = DOWNCLAMP_INIT_START_DAC;  // 初始值为-10
        bool dacChanged = false;
    };
    
    // 使用智能指针管理状态对象
    QSharedPointer<InitStatus> status(new InitStatus());
    
    // 步骤2: 初始DAC设置为负值，这会使夹爪张开
    ret = ZAux_Direct_SetDAC(g_handle, mappedMotorID, status->currentDAC);
    if (ret != 0)
    {
        QString errorMsg = QString("[下夹紧初始化] 错误: 设置DAC失败，错误码: %1").arg(ret);
        qDebug() << errorMsg;
        ui->tb_cmdWindow_2->append(errorMsg);
        return;
    }
    
    QString dacMsg = QString("[下夹紧初始化] 已设置初始DAC=%1（负值使夹爪张开）").arg(status->currentDAC);
    qDebug() << dacMsg;
    ui->tb_cmdWindow_2->append(dacMsg);
    
    // 获取当前位置作为初始位置
    ZAux_Direct_GetMpos(g_handle, mappedMotorID, &status->lastPosition);
    
    // 创建新的定时器
    m_downclampInitTimer = new QTimer(this);
    
    connect(m_downclampInitTimer, &QTimer::timeout, this, [this, mappedMotorID, status]() {
        float currentPosition = 0.0f;
        ZAux_Direct_GetMpos(g_handle, mappedMotorID, &currentPosition);
        
        QString posMsg = QString("[下夹紧初始化] 当前位置: %1, DAC: %2")
                         .arg(currentPosition, 0, 'f', 2)
                         .arg(status->currentDAC, 0, 'f', 1);
        qDebug() << posMsg;
        ui->tb_cmdWindow_2->append(posMsg);
        
        // 判断位置是否稳定（变化小于设定阈值）
        if (std::abs(currentPosition - status->lastPosition) < DOWNCLAMP_POSITION_TOLERANCE) {
            status->stableCount++;
            
            QString stableMsg = QString("[下夹紧初始化] 位置稳定计数: %1/%2")
                                .arg(status->stableCount)
                                .arg(DOWNCLAMP_STABLE_COUNT);
            qDebug() << stableMsg;
            ui->tb_cmdWindow_2->append(stableMsg);
            
            // 如果连续多次读取位置变化小于阈值，认为位置暂时稳定
            if (status->stableCount >= DOWNCLAMP_STABLE_COUNT) {
                // 如果当前DAC值大于最小值(数值上小于DOWNCLAMP_INIT_MIN_DAC)，继续减小DAC值
                if (status->currentDAC > DOWNCLAMP_INIT_MIN_DAC) {
                    status->currentDAC += DOWNCLAMP_INIT_DAC_STEP;  // 减小DAC值，使其更负（因为DOWNCLAMP_INIT_DAC_STEP是负值）
                    status->dacChanged = true;
                    
                    int ret = ZAux_Direct_SetDAC(g_handle, mappedMotorID, status->currentDAC);
                    if (ret != 0) {
                        QString errorMsg = QString("[下夹紧初始化] 错误: 减小DAC值失败，错误码: %1").arg(ret);
                        qDebug() << errorMsg;
                        ui->tb_cmdWindow_2->append(errorMsg);
                        // 继续执行，不返回
                    }
                    
                    QString decreaseDacMsg = QString("[下夹紧初始化] 减小DAC值到: %1（使夹爪进一步张开）").arg(status->currentDAC);
                    qDebug() << decreaseDacMsg;
                    ui->tb_cmdWindow_2->append(decreaseDacMsg);
                    
                    // 重置计数器
                    status->stableCount = 0;
                } 
                // 如果已经是最小DAC值或者已经减小过DAC值但位置依然稳定，则认为夹爪已完全张开
                else {
                    m_downclampInitTimer->stop();
                    
                    // 步骤3: 将DAC设为0
                    ZAux_Direct_SetDAC(g_handle, mappedMotorID, 0);
                    
                    // 将当前位置设为零点
                    ZAux_Direct_SetDpos(g_handle, mappedMotorID, 0);
                    ZAux_Direct_SetMpos(g_handle, mappedMotorID, 0);
                    
                    QString resetMsg = QString("[下夹紧初始化] 位置已稳定（夹爪已完全张开），已设置当前位置为零点，DAC=0");
                    qDebug() << resetMsg;
                    ui->tb_cmdWindow_2->append(resetMsg);
                    
                    // 步骤4: 设置回位置模式
                    ZAux_Direct_SetAtype(g_handle, mappedMotorID, POSITION_MODE);
                    
                    QString completedMsg = QString("[下夹紧初始化] 初始化完成，已设置为位置模式(Atype=%1)").arg(POSITION_MODE);
                    qDebug() << completedMsg;
                    ui->tb_cmdWindow_2->append(completedMsg);
                    
                    // 清理定时器
                    m_downclampInitTimer->deleteLater();
                    m_downclampInitTimer = nullptr;
                }
            }
        } else {
            // 位置变化大于阈值，说明夹爪正在移动
            status->stableCount = 0;
            status->lastPosition = currentPosition;
            status->dacChanged = false;
        }
    });
    
    // 开始监测位置，每500毫秒检查一次
    m_downclampInitTimer->start(INIT_TIMER_INTERVAL);
}

/**
 * @brief 处理推杆Modbus数据
 * @param data 接收到的数据
 * @param startReg 起始寄存器地址
 */
void zmotionpage::handleConnectionData(const QVector<quint16>& data, int startReg)
{
    // 根据寄存器地址识别这是什么数据
    if (startReg == 0) { // 状态寄存器
        if (data.size() > 0) {
            // 解析电机状态
            quint16 status = data[0];
            QString statusText;
            
            switch (status) {
                case 0:
                    statusText = "停止";
                    break;
                case 1:
                    statusText = "运行中";
                    break;
                case 2:
                    statusText = "碰撞停止";
                    break;
                default:
                    statusText = QString("未知状态(%1)").arg(status);
            }
            
            qDebug() << "[推杆] 状态: " << statusText;
            
            // 如果状态是停止，更新位置
            if (status == 0) {
                // 读取位置值
                mdbProcessor->ReadValue(CONNECTION_MODBUS_DEVICE_INDEX, 
                                       CONNECTION_MODBUS_SLAVE_ID, 
                                       0x01, 2, 4); // 读取位置寄存器
            }
        }
    } else if (startReg == 1) { // 位置寄存器
        if (data.size() >= 2) {
            // 用两个16位值拼接成32位位置值
            int32_t position = (static_cast<int32_t>(data[1]) << 16) | data[0];
            
            qDebug() << "[推杆] 当前位置: " << position;
            
            // 更新UI显示
            QString status;
            if (position == CONNECTION_MOTOR_FULLY_RETRACTED) {
                status = "完全收回";
            } else if (position == CONNECTION_MOTOR_FULLY_EXTENDED) {
                status = "完全推出";
            } else if (position > CONNECTION_MOTOR_FULLY_EXTENDED && position < CONNECTION_MOTOR_FULLY_RETRACTED) {
                status = "中间位置";
            } else {
                status = QString("未定义位置: %1").arg(position);
            }
            
            ui->le_connection_status->setText(status);
        }
    }
}

/**
 * @brief 更新推杆状态
 */
void zmotionpage::updateConnectionStatus()
{
    // 读取推杆状态
    mdbProcessor->ReadValue(CONNECTION_MODBUS_DEVICE_INDEX, 
                           CONNECTION_MODBUS_SLAVE_ID, 
                           0x00, 1, 4); // 读取状态寄存器
}

/**
 * @brief 推杆初始化按钮点击事件处理函数
 */
void zmotionpage::on_btn_connection_init_clicked()
{
    qDebug() << "[推杆] 开始初始化";
    ui->tb_cmdWindow_2->append("[推杆] 开始初始化");

    // 1. 先建立Modbus连接
    if (!mdbProcessor->connectStatus) {
        mdbProcessor->TCPConnect(502, "192.168.1.203");
        QThread::msleep(100);  // 等待连接建立
    }

    // 2. 移动到初始化位置(35000)，速度250rpm
    QVector<quint16> initData;
    int32_t position = CONNECTION_MOTOR_INIT_POSITION;
    int16_t speed = CONNECTION_MOTOR_INIT_SPEED * 10; // 协议要求速度值乘以10
    
    // 位置高16位和低16位
    initData.append(position & 0xFFFF);  // 低16位
    initData.append((position >> 16) & 0xFFFF);  // 高16位
    
    // 参数字节
    initData.append(0);  // 预留参数
    initData.append(0);  // 预留参数
    
    // 速度值高16位和低16位
    initData.append(speed & 0xFFFF);  // 低16位
    initData.append((speed >> 16) & 0xFFFF);  // 高16位

    // 加速时间和误差容忍
    int accelTime = CONNECTION_MOTOR_ACCEL_TIME;  // 250ms
    int errorTolerance = CONNECTION_MOTOR_ERROR_TOLERANCE;  // 100步
    initData.append(accelTime & 0xFFFF);  // 加速时间
    initData.append(errorTolerance & 0xFFFF);  // 误差容忍

    // 发送命令
    mdbProcessor->WriteValue(CONNECTION_MODBUS_DEVICE_INDEX, 
                            CONNECTION_MODBUS_SLAVE_ID,
                            0x10, initData);
    
    qDebug() << "[推杆] 移动到初始化位置: " << CONNECTION_MOTOR_INIT_POSITION
             << ", 速度: " << CONNECTION_MOTOR_INIT_SPEED << "rpm";
    ui->tb_cmdWindow_2->append(QString("[推杆] 移动到初始化位置: %1, 速度: %2rpm")
                              .arg(CONNECTION_MOTOR_INIT_POSITION)
                              .arg(CONNECTION_MOTOR_INIT_SPEED));
    
    // 3. 启动定时器等待电机到达位置
    // 修改状态显示
    ui->le_connection_status->setText("正在初始化...");
    
    // 启动状态监控
    if (!m_connectionStatusTimer->isActive()) {
        m_connectionStatusTimer->start();
    }
    
    // 4. 周期性检查电机状态，等待到达位置
    QTimer::singleShot(3000, this, [this]() {
        // 周期性检查，等待电机停止
        QTimer* checkTimer = new QTimer(this);
        connect(checkTimer, &QTimer::timeout, this, [this, checkTimer]() {
            // 读取电机状态
            mdbProcessor->ReadValue(CONNECTION_MODBUS_DEVICE_INDEX, 
                                   CONNECTION_MODBUS_SLAVE_ID, 
                                   0x00, 1, 4);
            
            // 我们在handleConnectionData函数中处理返回的状态
            // 如果电机停止，我们会收到状态为0的数据
            
            // 检查是否初始化完成
            static int checkCount = 0;
            checkCount++;
            
            // 如果电机状态长时间为0（停止状态），说明已经到达位置
            if (checkCount > 10) {
                checkTimer->stop();
                checkTimer->deleteLater();
                
                // 设置当前位置为零点
                QVector<quint16> zeroData;
                zeroData.append(0);  // 低16位
                zeroData.append(0);  // 高16位
                zeroData.append(0);  // 预留参数
                zeroData.append(0);  // 预留参数
                
                mdbProcessor->WriteValue(CONNECTION_MODBUS_DEVICE_INDEX, 
                                        CONNECTION_MODBUS_SLAVE_ID,
                                        0x01, zeroData);
                
                qDebug() << "[推杆] 将当前位置设置为零点";
                ui->tb_cmdWindow_2->append("[推杆] 将当前位置设置为零点");
                
                // 设置初始化完成标志
                m_connectionInitialized = true;
                
                // 更新状态显示
                ui->le_connection_status->setText("完全收回(零点)");
                
                // 自动执行收回功能（移动到0位置）
                on_btn_connection_retract_clicked();
            }
        });
        checkTimer->start(500);  // 每500毫秒检查一次
    });
}

/**
 * @brief 推杆推出按钮点击事件处理函数
 */
void zmotionpage::on_btn_connection_extent_clicked()
{
    // 检查是否已初始化
    if (!m_connectionInitialized) {
        QMessageBox::warning(this, "操作失败", "请先初始化推杆！");
        return;
    }

    qDebug() << "[推杆] 开始推出";
    ui->tb_cmdWindow_2->append("[推杆] 开始推出");

    // 移动到完全推出位置(-35000)，速度500rpm
    QVector<quint16> moveData;
    int32_t position = CONNECTION_MOTOR_FULLY_EXTENDED;  // -35000
    int16_t speed = CONNECTION_MOTOR_NORMAL_SPEED * 10;  // 500rpm
    
    // 位置高16位和低16位
    moveData.append(position & 0xFFFF);  // 低16位
    moveData.append((position >> 16) & 0xFFFF);  // 高16位
    
    // 参数字节
    moveData.append(0);  // 预留参数
    moveData.append(0);  // 预留参数
    
    // 速度值高16位和低16位
    moveData.append(speed & 0xFFFF);  // 低16位
    moveData.append((speed >> 16) & 0xFFFF);  // 高16位

    // 加速时间和误差容忍
    int accelTime = CONNECTION_MOTOR_ACCEL_TIME;  // 250ms
    int errorTolerance = CONNECTION_MOTOR_ERROR_TOLERANCE;  // 100步
    moveData.append(accelTime & 0xFFFF);  // 加速时间
    moveData.append(errorTolerance & 0xFFFF);  // 误差容忍

    // 发送命令
    mdbProcessor->WriteValue(CONNECTION_MODBUS_DEVICE_INDEX, 
                            CONNECTION_MODBUS_SLAVE_ID,
                            0x10, moveData);
    
    qDebug() << "[推杆] 移动到推出位置: " << CONNECTION_MOTOR_FULLY_EXTENDED
             << ", 速度: " << CONNECTION_MOTOR_NORMAL_SPEED << "rpm";
    ui->tb_cmdWindow_2->append(QString("[推杆] 移动到推出位置: %1, 速度: %2rpm")
                              .arg(CONNECTION_MOTOR_FULLY_EXTENDED)
                              .arg(CONNECTION_MOTOR_NORMAL_SPEED));
    
    // 修改状态显示
    ui->le_connection_status->setText("正在推出...");
    
    // 确保状态监控定时器运行
    if (!m_connectionStatusTimer->isActive()) {
        m_connectionStatusTimer->start();
    }
}

/**
 * @brief 推杆收回按钮点击事件处理函数
 */
void zmotionpage::on_btn_connection_retract_clicked()
{
    // 检查是否已初始化
    if (!m_connectionInitialized) {
        QMessageBox::warning(this, "操作失败", "请先初始化推杆！");
        return;
    }

    qDebug() << "[推杆] 开始收回";
    ui->tb_cmdWindow_2->append("[推杆] 开始收回");

    // 移动到完全收回位置(0)，速度500rpm
    QVector<quint16> moveData;
    int32_t position = CONNECTION_MOTOR_FULLY_RETRACTED;  // 0
    int16_t speed = CONNECTION_MOTOR_NORMAL_SPEED * 10;  // 500rpm
    
    // 位置高16位和低16位
    moveData.append(position & 0xFFFF);  // 低16位
    moveData.append((position >> 16) & 0xFFFF);  // 高16位
    
    // 参数字节
    moveData.append(0);  // 预留参数
    moveData.append(0);  // 预留参数
    
    // 速度值高16位和低16位
    moveData.append(speed & 0xFFFF);  // 低16位
    moveData.append((speed >> 16) & 0xFFFF);  // 高16位

    // 加速时间和误差容忍
    int accelTime = CONNECTION_MOTOR_ACCEL_TIME;  // 250ms
    int errorTolerance = CONNECTION_MOTOR_ERROR_TOLERANCE;  // 100步
    moveData.append(accelTime & 0xFFFF);  // 加速时间
    moveData.append(errorTolerance & 0xFFFF);  // 误差容忍

    // 发送命令
    mdbProcessor->WriteValue(CONNECTION_MODBUS_DEVICE_INDEX, 
                            CONNECTION_MODBUS_SLAVE_ID,
                            0x10, moveData);
    
    qDebug() << "[推杆] 移动到收回位置: " << CONNECTION_MOTOR_FULLY_RETRACTED
             << ", 速度: " << CONNECTION_MOTOR_NORMAL_SPEED << "rpm";
    ui->tb_cmdWindow_2->append(QString("[推杆] 移动到收回位置: %1, 速度: %2rpm")
                              .arg(CONNECTION_MOTOR_FULLY_RETRACTED)
                              .arg(CONNECTION_MOTOR_NORMAL_SPEED));
    
    // 修改状态显示
    ui->le_connection_status->setText("正在收回...");
    
    // 确保状态监控定时器运行
    if (!m_connectionStatusTimer->isActive()) {
        m_connectionStatusTimer->start();
    }
}
