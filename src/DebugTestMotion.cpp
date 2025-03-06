#include "DebugTestMotion.h"
#include "inc/DrillingController.h"
#include "inc/DrillingParameters.h"
#include <QDebug>
#include <QThread>
#include <QEventLoop>
#include <QTimer>
#include <iostream>
#include <iomanip>

#ifdef ENABLE_DEBUG_TEST_MOTION

// 全局调试测试实例
DebugTestMotion* g_debugTest = nullptr;

// 构造函数
DebugTestMotion::DebugTestMotion(QObject *parent)
    : QObject(parent)
    , m_controller(nullptr)
    , m_currentSequenceIndex(0)
    , m_isStepMode(true)
    , m_isRunning(false)
{
    // 创建钻进控制器
    m_controller = new DrillingController(this);

    // 初始化测试序列
    initTestSequence();

    // 设置全局实例
    g_debugTest = this;

    qDebug() << "========================================";
    qDebug() << " 调试测试模块已创建, 可使用 DEBUG_PRINT_STATE() 和 DEBUG_STEP() 宏进行调试";
    qDebug() << "========================================";
}

// 析构函数
DebugTestMotion::~DebugTestMotion()
{
    if (m_isRunning) {
        stopTest();
    }

    if (g_debugTest == this) {
        g_debugTest = nullptr;
    }

    // 释放资源
    if (m_controller) {
        m_controller->release();
    }

    qDebug() << "调试测试模块已销毁";
}

// 初始化测试环境
bool DebugTestMotion::initialize()
{
    if (!m_controller) {
        qDebug() << "错误: 未创建钻进控制器";
        return false;
    }

    // 在调试模式下初始化控制器 - 不连接实际控制器
    bool success = m_controller->initialize("", true); // 传递空字符串和调试模式标志
    if (!success) {
        qDebug() << "错误: 钻进控制器初始化失败";
        return false;
    }

    // 获取状态机指针
    AutoDrillingStateMachine* stateMachine = m_controller->getStateMachine();
    if (!stateMachine) {
        qDebug() << "错误: 状态机未创建";
        return false;
    }
    
    // 设置钻进参数
    // 设置速度参数 - 这些值应该从配置文件中读取
    DrillingParameters::getInstance()->setParameter("速度", "钻进速度", 0.01); // v1
    DrillingParameters::getInstance()->setParameter("速度", "对接速度", 0.05); // v2
    DrillingParameters::getInstance()->setParameter("速度", "空行程速度", 0.1); // v3
    
    // 设置旋转速度参数
    DrillingParameters::getInstance()->setParameter("旋转", "工作转速", 120.0); // omega
    DrillingParameters::getInstance()->setParameter("旋转", "对接转速", 60.0); // omega_s
    
    // 设置冲击参数
    DrillingParameters::getInstance()->setParameter("冲击", "工作频率", 10.0);
    
    // 设置增量参数
    DrillingParameters::getInstance()->setParameter("增量", "螺旋接口标准高度增量", 0.1); // delta_thread
    DrillingParameters::getInstance()->setParameter("增量", "钻具螺旋接口增量", 0.2); // delta_tool
    DrillingParameters::getInstance()->setParameter("增量", "钻管螺旋接口增量", 0.3); // delta_pipe
    DrillingParameters::getInstance()->setParameter("增量", "钻进行程距离", 1.0); // delta_drill

    // 连接信号
    connect(m_controller, &DrillingController::currentStepChanged, this, &DebugTestMotion::onStateChanged);
    connect(m_controller, &DrillingController::stateMachineStarted, this, &DebugTestMotion::onMachineStarted);
    connect(m_controller, &DrillingController::stateMachineStopped, this, &DebugTestMotion::onMachineStopped);
    connect(m_controller, &DrillingController::stateMachinePaused, this, &DebugTestMotion::onMachinePaused);
    connect(m_controller, &DrillingController::stateMachineResumed, this, &DebugTestMotion::onMachineResumed);
    connect(m_controller, &DrillingController::motorStatusUpdated, this, &DebugTestMotion::onMotorStatusUpdated);
    connect(m_controller, &DrillingController::commandResponseReceived, this, &DebugTestMotion::onCommandResponse);

    // 获取状态机对象并连接钻管计数变化信号
    connect(stateMachine, &AutoDrillingStateMachine::pipeCountChanged, this, &DebugTestMotion::onPipeCountChanged);
    
    qDebug() << "调试测试模块初始化成功";
    return true;
}

// 启动测试
void DebugTestMotion::startTest()
{
    if (m_isRunning) {
        qDebug() << "测试已在运行中";
        return;
    }

    if (!m_controller) {
        qDebug() << "错误: 未创建钻进控制器";
        return;
    }

    m_isRunning = true;
    m_currentSequenceIndex = 0;

    qDebug() << "========================================";
    qDebug() << "开始测试序列";
    qDebug() << "========================================";

    if (!m_isStepMode) {
        // 非步进模式，自动执行整个测试序列
        testStateSequence(m_testSequence);
    } else {
        // 步进模式，执行第一步
        stepForward();
    }
}

// 停止测试
void DebugTestMotion::stopTest()
{
    if (!m_isRunning) {
        return;
    }

    m_isRunning = false;
    if (m_controller && m_controller->isRunning()) {
        m_controller->stopStateMachine();
    }

    qDebug() << "测试已停止";
}

// 暂停测试
void DebugTestMotion::pauseTest()
{
    if (!m_isRunning || !m_controller) {
        return;
    }

    m_controller->pauseStateMachine();
    qDebug() << "测试已暂停";
}

// 恢复测试
void DebugTestMotion::resumeTest()
{
    if (!m_isRunning || !m_controller) {
        return;
    }

    m_controller->resumeStateMachine();
    qDebug() << "测试已恢复";
}

// 逐步执行
void DebugTestMotion::stepForward()
{
    if (!m_isRunning) {
        startTest();
        return;
    }

    if (m_currentSequenceIndex >= m_testSequence.size()) {
        qDebug() << "\n=== 测试序列已完成 ===";
        stopTest();
        return;
    }

    QString currentState = m_testSequence[m_currentSequenceIndex];
    QString stateDesc = getStateDescription(currentState);

    qDebug() << "\n========================================";
    qDebug() << "执行步骤" << (m_currentSequenceIndex + 1) << "/" << m_testSequence.size();
    qDebug() << "目标状态: " << stateDesc << " (" << currentState << ")";
    qDebug() << "========================================";

    // 执行状态转换
    if (m_currentSequenceIndex == 0) {
        qDebug() << "准备启动状态机...";
        qDebug() << "按Enter继续...";
        std::string input;
        std::getline(std::cin, input);
        
        m_controller->startStateMachine();
        waitForOperation(2000);
    } else {
        transitionToState(currentState);
    }

    // 输出转换后的状态
    printCurrentState();

    // 前进到下一步
    m_currentSequenceIndex++;

    if (m_currentSequenceIndex >= m_testSequence.size()) {
        qDebug() << "\n=== 测试序列已全部完成 ===";
    }
}

// 打印当前状态
void DebugTestMotion::printCurrentState()
{
    if (!m_controller) {
        std::cerr << "\n[错误]\t未创建钻进控制器" << std::endl;
        return;
    }

    AutoDrillingStateMachine* stateMachine = m_controller->getStateMachine();
    if (!stateMachine) {
        std::cerr << "\n[错误]\t无法获取状态机" << std::endl;
        return;
    }

    QString currentStateName = stateMachine->getCurrentStateName();
    QString stateDesc = getStateDescription(currentStateName);

    // 系统状态信息
    std::cout << "\n================================================" << std::endl;
    std::cout << "                系统状态信息                    " << std::endl;
    std::cout << "================================================" << std::endl;
    std::cout << std::left << std::setw(15) << "当前状态: " << stateDesc.toStdString() << std::endl;
    std::cout << std::left << std::setw(15) << "状态代码: " << currentStateName.toStdString() << std::endl;
    std::cout << std::left << std::setw(15) << "运行状态: " << (m_controller->isRunning() ? "运行中" : "已停止") << std::endl;
    std::cout << std::left << std::setw(15) << "钻管计数: " << stateMachine->getCurrentPipeCount() << std::endl;

    // 机构状态信息
    std::cout << "\n================================================" << std::endl;
    std::cout << "                机构状态信息                    " << std::endl;
    std::cout << "================================================" << std::endl;

    // 1. 存储单元
    std::cout << "\n[存储单元]" << std::endl;
    printMechanismStatus(MotorID::STORAGE);

    // 2. 机械手
    std::cout << "\n[机械手]" << std::endl;
    printMechanismStatus(MotorID::ROBOT_ROTATION);
    printMechanismStatus(MotorID::ROBOT_EXTENSION);
    printMechanismStatus(MotorID::ROBOT_CLAMP);

    // 3. 钻进机构
    std::cout << "\n[钻进机构]" << std::endl;
    printMechanismStatus(MotorID::DRILL);
    printMechanismStatus(MotorID::PERCUSSION);

    // 4. 其他机构
    std::cout << "\n[其他机构]" << std::endl;
    printMechanismStatus(MotorID::PENETRATION);
    printMechanismStatus(MotorID::CLAMP);
    printMechanismStatus(MotorID::CONNECTION);

    // 运动流程信息
    std::cout << "\n================================================" << std::endl;
    std::cout << "                运动流程信息                    " << std::endl;
    std::cout << "================================================" << std::endl;
    printMotionSequence(currentStateName);
}

// 打印机构状态
void DebugTestMotion::printMechanismStatus(int motorId) const
{
    if (!m_controller) {
        std::cerr << "  [错误] 控制器未初始化" << std::endl;
        return;
    }
    
    // 获取电机参数
    if (!m_motorStatus.contains(motorId)) {
        std::cerr << "  [错误] 电机 " << motorId << " 状态未知" << std::endl;
        return;
    }
    
    const QMap<QString, float>& params = m_motorStatus[motorId];
    
    // 获取电机名称
    QString name = m_controller->getMotionController()->getMotorName(motorId);
    
    // 获取电机参数
    double position = params.value("Pos", 0.0);
    double velocity = params.value("Vel", 0.0);
    int mode = params.value("Atype", 0);
    bool enabled = params.value("EN", 0) > 0;
    
    QString modeStr;
    switch (mode) {
        case 65: modeStr = "位置模式"; break;
        case 66: modeStr = "速度模式"; break;
        case 67: modeStr = "力矩模式"; break;
        default: modeStr = QString::number(mode); break;
    }
    
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "│  %-12s %2d    %-10s %8.2f %8.2f    %-4s │",
             name.toStdString().c_str(),
             motorId,
             modeStr.toStdString().c_str(),
             position,
             velocity,
             enabled ? "启用" : "停用");
    std::cout << buffer << std::endl;
}

void DebugTestMotion::printMotionSequence(const QString& currentState) const
{
    std::cout << "\n[当前状态]: " << currentState.toStdString() << std::endl;
    
    if (currentState == AutoDrillingStateMachine::STATE_SYSTEM_STARTUP) {
        std::cout << "\n[系统启动流程]" << std::endl;
        std::cout << "1. 初始化所有电机" << std::endl;
        std::cout << "   - 所有电机: 设置为启用状态" << std::endl;
        std::cout << "2. 检查系统状态" << std::endl;
        std::cout << "3. 等待操作指令" << std::endl;
    }
    else if (currentState == AutoDrillingStateMachine::STATE_DEFAULT_POSITION) {
        std::cout << "\n[默认位置流程]" << std::endl;
        std::cout << "1. 机械手回到安全位置" << std::endl;
        std::cout << "   - 机械手旋转: 0度位置 (安全区)" << std::endl;
        std::cout << "   - 机械手伸缩: 0mm位置 (完全收回)" << std::endl;
        std::cout << "   - 机械手夹持: 0位置 (完全松开)" << std::endl;
        std::cout << "2. 钻进机构停止" << std::endl;
        std::cout << "   - 钻进旋转: 0RPM (停止)" << std::endl;
        std::cout << "   - 钻进冲击: 0Hz (停止)" << std::endl;
        std::cout << "3. 所有夹持机构松开" << std::endl;
        std::cout << "   - 下夹紧机构: 0位置 (完全松开)" << std::endl;
        std::cout << "   - 对接机构: 0位置 (完全分离)" << std::endl;
        std::cout << "4. 进给机构回零" << std::endl;
        std::cout << "   - 进给机构: 0mm位置 (零位)" << std::endl;
    }
    else if (currentState == AutoDrillingStateMachine::STATE_READY) {
        std::cout << "\n[就绪状态流程]" << std::endl;
        std::cout << "1. 确认所有机构就位" << std::endl;
        std::cout << "   - 所有电机: 已启用，处于默认位置" << std::endl;
        std::cout << "2. 等待开始指令" << std::endl;
    }
    else if (currentState == AutoDrillingStateMachine::STATE_FIRST_TOOL_INSTALLATION) {
        std::cout << "\n[首根钻具安装流程]" << std::endl;
        std::cout << "1. 机械手移动到存储区" << std::endl;
        std::cout << "   - 机械手旋转: 0度 -> 90度 (存储区位置)" << std::endl;
        std::cout << "   - 进给机构: 0mm -> 1315mm (工作位置)(空行程速度v3)" << std::endl;
        std::cout << "2. 机械手夹持钻具" << std::endl;
        std::cout << "   - 机械手伸缩: 0mm -> 200mm (伸出)" << std::endl;
        std::cout << "   - 机械手夹持: 0 -> 100 (完全夹紧)" << std::endl;
        std::cout << "   - 机械手伸缩: 200mm -> 0mm (回收)" << std::endl;
        std::cout << "3. 机械手移动到钻台" << std::endl;
        std::cout << "   - 机械手旋转: 90度 -> 0度 (钻台位置)" << std::endl;
        std::cout << "   - 机械手伸缩: 0mm -> 250mm (对准钻台)" << std::endl;
        std::cout << "4. 进给机构移动" << std::endl;
        std::cout << "   - 进给机构: 1315mm -> 610mm (钻具安装起始位置 580+30)" << std::endl;
        std::cout << "5. 执行连接操作" << std::endl;
        std::cout << "   - 钻进旋转速度: 0 -> 60rpm (低速对接旋转)" << std::endl;
        std::cout << "   - 进给机构缓慢下移: 610mm -> 580mm (对接速度v2)" << std::endl;
        std::cout << "   - 到达580mm后停止旋转和进给" << std::endl;
        std::cout << "6. 对接机构" << std::endl;
        std::cout << "   - 对接机构伸出: 0mm -> 15mm (卡住钻具)" << std::endl;
        std::cout << "7. 机械手松开" << std::endl;
        std::cout << "   - 机械手夹持: 100 -> 0 (完全松开)" << std::endl;
        std::cout << "   - 机械手伸缩: 250mm -> 0mm (完全收回)" << std::endl;
    }
    else if (currentState == AutoDrillingStateMachine::STATE_FIRST_DRILLING) {
        std::cout << "\n[首次钻进流程]" << std::endl;
        std::cout << "1. 钻进电机、冲击电机和进给机构同时启动" << std::endl;
        std::cout << "   - 钻进旋转: 0RPM -> 120RPM (工作转速)" << std::endl;
        std::cout << "   - 钻进冲击: 0Hz -> 10Hz (工作频率)" << std::endl;
        std::cout << "   - 进给机构: 580mm -> 0mm (工作位置)(钻进速度v1)" << std::endl;
        std::cout << "2. 监测钻进参数" << std::endl;
        std::cout << "   - 监测转速、扭矩、推力等参数" << std::endl;
        std::cout << "3. 达到预定深度后停止" << std::endl;
        std::cout << "   - 钻进旋转: 120RPM -> 0RPM" << std::endl;
        std::cout << "   - 钻进冲击: 10Hz -> 0Hz" << std::endl;
        std::cout << "4. 下夹紧机构夹紧" << std::endl;
        std::cout << "   - 下夹紧机构: 0 -> 100 (完全夹紧)" << std::endl;
        std::cout << "5. 对接机构" << std::endl;
        std::cout << "   - 对接机构回收: 15mm -> 0mm (解锁钻具)" << std::endl;
        std::cout << "6. 执行断开操作" << std::endl;
        std::cout << "   - 钻进旋转速度: 0 -> -60rpm (低速对接旋转)" << std::endl;
        std::cout << "   - 进给机构缓慢上移: 0mm -> 30mm (对接速度v2)" << std::endl;
        std::cout << "   - 到达30mm后停止旋转和进给" << std::endl;
        std::cout << "7. 进给机构" << std::endl;
        std::cout << "   - 进给机构上升: 30mm -> 650mm (空行程速度v3)" << std::endl;
    }
    else if (currentState == AutoDrillingStateMachine::STATE_PIPE_INSTALLATION_LOOP) {
        std::cout << "\n[钻管安装循环流程]" << std::endl;
        std::cout << "1. 机械手取钻管" << std::endl;
        std::cout << "   - 机械手旋转: 0度 -> 90度 (存储区位置)" << std::endl;
        std::cout << "   - 机械手伸缩: 0mm -> 200mm (伸出)" << std::endl;
        std::cout << "   - 机械手夹持: 0 -> 100 (完全夹紧)" << std::endl;
        std::cout << "   - 机械手缩回: 200mm -> 0mm (缩回)" << std::endl;
        std::cout << "   - 机械手旋转: 90度 -> 0度 (钻台位置)" << std::endl;
        std::cout << "   - 机械手伸缩: 0mm -> 200mm (伸出)" << std::endl;
        std::cout << "2. 进给机构" << std::endl;
        std::cout << "   - 进给机构下降: 650mm -> 580mm (钻管安装高度)(空行程速度v3)" << std::endl;
        std::cout << "3. 执行连接操作" << std::endl;
        std::cout << "   - 钻进旋转速度: 0 -> 60rpm (低速对接旋转)" << std::endl;
        std::cout << "   - 进给机构缓慢下移: 580mm -> 550mm (对接速度v2)" << std::endl;
        std::cout << "   - 到达550mm时停止旋转和进给" << std::endl;
        std::cout << "4. 对接机构推出" << std::endl;
        std::cout << "   - 对接机构推出: 0mm -> 15mm (锁住钻管)" << std::endl;
        std::cout << "5. 机械手收回" << std::endl;
        std::cout << "   - 机械手夹持: 100 -> 0 (松开)" << std::endl;
        std::cout << "   - 机械手伸缩: 200mm -> 0mm (收回)" << std::endl;
        std::cout << "6. 钻管之间的对接" << std::endl;
        std::cout << "   - 钻进旋转速度: 0 -> 60rpm (低速对接旋转)" << std::endl;
        std::cout << "   - 进给机构缓慢下降: 550mm -> 520mm (对接速度v2)" << std::endl;
        std::cout << "   - 下降到520mm时钻具和钻管对接" << std::endl;
        std::cout << "7. 下夹紧机构" << std::endl;
        std::cout << "   - 下夹紧机构松开: 100 -> 0" << std::endl;
        std::cout << "8. 钻机钻进" << std::endl;
        std::cout << "   - 钻进旋转: 0RPM -> 120RPM (工作转速)" << std::endl;
        std::cout << "   - 钻进冲击: 0Hz -> 10Hz (工作频率)" << std::endl;
        std::cout << "   - 进给机构: 520mm -> 0mm (工作位置)(钻进速度v1)" << std::endl;
        std::cout << "   - 到达0mm位置停止旋转、冲击和进给" << std::endl;
        std::cout << "9. 下夹紧机构" << std::endl;
        std::cout << "   - 下夹紧机构夹紧: 0 -> 100" << std::endl;
        std::cout << "10. 执行断开操作" << std::endl;
        std::cout << "   - 对接机构回收: 15mm -> 0mm" << std::endl;
        std::cout << "   - 钻进旋转速度: 0 -> -60rpm (低速对接旋转)" << std::endl;
        std::cout << "   - 进给机构缓慢上移: 0mm -> 30mm (对接速度v2)" << std::endl;
        std::cout << "   - 到达30mm后停止旋转和进给" << std::endl;
        std::cout << "11. 进给机构" << std::endl;
        std::cout << "   - 进给机构上升: 30mm -> 650mm (空行程速度v3)" << std::endl;
    }
    else if (currentState == AutoDrillingStateMachine::STATE_PIPE_REMOVAL_LOOP) {
        std::cout << "\n[钻管拆卸循环流程]" << std::endl;
        std::cout << "1. 进给机构" << std::endl;
        std::cout << "   - 进给机构下降: 650mm -> 30mm (空行程速度v3)" << std::endl;
        std::cout << "   - 执行连接操作:" << std::endl;
        std::cout << "   - 钻进旋转速度: 0 -> 60rpm (低速对接旋转)" << std::endl;
        std::cout << "   - 进给机构缓慢下移: 30mm -> 0mm (对接速度v2)" << std::endl;
        std::cout << "   - 到达0mm后停止旋转和进给" << std::endl;
        std::cout << "   - 对接机构推出: 0mm -> 15mm" << std::endl;
        std::cout << "2. 下夹紧机构" << std::endl;
        std::cout << "   - 下夹紧机构松开: 100 -> 0" << std::endl;
        std::cout << "3. 进给机构" << std::endl;
        std::cout << "   - 进给机构上升: 0mm -> 550mm (空行程速度v3)" << std::endl;
        std::cout << "4. 下夹紧机构" << std::endl;
        std::cout << "   - 下夹紧机构夹紧: 0 -> 100" << std::endl;
        std::cout << "5. 断开钻管间连接" << std::endl;
        std::cout << "   - 钻进旋转速度: 0 -> -60rpm (低速对接旋转)" << std::endl;
        std::cout << "   - 进给机构缓慢上移: 550mm -> 580mm (对接速度v2)" << std::endl;
        std::cout << "   - 到达580mm后停止旋转和进给" << std::endl;
        std::cout << "6. 机械手抓取钻管" << std::endl;
        std::cout << "   - 机械手伸缩: 0mm -> 250mm (伸出)" << std::endl;
        std::cout << "   - 机械手夹持: 0 -> 100 (完全夹紧)" << std::endl;
        std::cout << "7. 对接机构回收" << std::endl;
        std::cout << "   - 对接机构回收: 15mm -> 0mm (解锁钻进机构的钻管)" << std::endl;
        std::cout << "8. 断开钻管与钻进机构连接" << std::endl;
        std::cout << "   - 钻进旋转速度: 0 -> -60rpm (低速对接旋转)" << std::endl;
        std::cout << "   - 进给机构缓慢上移: 580mm -> 610mm (对接速度v2)" << std::endl;
        std::cout << "   - 到达610mm后停止旋转和进给" << std::endl;
        std::cout << "9. 存储机构旋转到空位" << std::endl;
        std::cout << "   - 存储位置: M_n-i" << std::endl;
        std::cout << "10. 机械手回收" << std::endl;
        std::cout << "   - 机械手伸缩: 250mm -> 0mm (缩回)" << std::endl;
        std::cout << "   - 机械手旋转: 0度 -> 90度" << std::endl;
        std::cout << "   - 机械手伸缩: 0mm -> 250mm (伸出)" << std::endl;
        std::cout << "   - 机械手夹持: 100 -> 0 (完全松开)" << std::endl;
        std::cout << "   - 机械手伸缩: 250mm -> 0mm (缩回)" << std::endl;
        std::cout << "   - 机械手旋转: 90度 -> 0度" << std::endl;
        std::cout << "11. 进给机构" << std::endl;
        std::cout << "   - 进给机构上升: 610mm -> 650mm (待机)" << std::endl;
    }
    else if (currentState == AutoDrillingStateMachine::STATE_FIRST_TOOL_RECOVERY) {
        std::cout << "\n[首根钻具回收流程]" << std::endl;
        std::cout << "1. 进给机构下降" << std::endl;
        std::cout << "   - 进给机构下降: 650mm -> 30mm (空行程速度v3)" << std::endl;
        std::cout << "   - 执行连接操作:" << std::endl;
        std::cout << "   - 钻进旋转速度: 0 -> 60rpm (低速对接旋转)" << std::endl;
        std::cout << "   - 进给机构缓慢下移: 30mm -> 0mm (对接速度v2)" << std::endl;
        std::cout << "   - 到达0mm后停止旋转和进给" << std::endl;
        std::cout << "   - 对接机构推出: 0mm -> 15mm" << std::endl;
        std::cout << "2. 下夹紧松开" << std::endl;
        std::cout << "   - 下夹紧机构: 100 -> 0" << std::endl;
        std::cout << "3. 进给机构上升" << std::endl;
        std::cout << "   - 进给机构上升: 0mm -> 610mm" << std::endl;
        std::cout << "4. 机械手抓取钻管" << std::endl;
        std::cout << "   - 机械手伸缩: 0mm -> 250mm (伸出)" << std::endl;
        std::cout << "   - 机械手夹持: 0 -> 100 (完全夹紧)" << std::endl;
        std::cout << "5. 对接机构回收" << std::endl;
        std::cout << "   - 对接机构回收: 15mm -> 0mm (解锁钻进机构的钻管)" << std::endl;
        std::cout << "6. 断开钻管与钻进机构连接" << std::endl;
        std::cout << "   - 钻进旋转速度: 0 -> -60rpm (低速对接旋转)" << std::endl;
        std::cout << "   - 进给机构缓慢上移: 610mm -> 640mm (对接速度v2)" << std::endl;
        std::cout << "   - 到达640mm后停止旋转和进给" << std::endl;
        std::cout << "7. 存储机构旋转到空位" << std::endl;
        std::cout << "   - 存储位置: M_n-i" << std::endl;
        std::cout << "8. 机械手回收" << std::endl;
        std::cout << "   - 机械手伸缩: 250mm -> 0mm (缩回)" << std::endl;
        std::cout << "   - 机械手旋转: 0度 -> 90度" << std::endl;
        std::cout << "   - 机械手伸缩: 0mm -> 250mm (伸出)" << std::endl;
        std::cout << "   - 机械手夹持: 100 -> 0 (完全松开)" << std::endl;
        std::cout << "   - 机械手伸缩: 250mm -> 0mm (缩回)" << std::endl;
        std::cout << "   - 机械手旋转: 90度 -> 0度" << std::endl;
        std::cout << "9. 进给机构" << std::endl;
        std::cout << "   - 进给机构下降: 640mm -> 0mm (待机)" << std::endl;
    }
    else if (currentState == AutoDrillingStateMachine::STATE_SYSTEM_RESET) {
        std::cout << "\n[系统重置流程]" << std::endl;
        std::cout << "1. 所有电机停止" << std::endl;
        std::cout << "   - 钻进旋转: 停止" << std::endl;
        std::cout << "   - 钻进冲击: 停止" << std::endl;
        std::cout << "2. 机构回到安全位置" << std::endl;
        std::cout << "   - 机械手: 返回安全位置" << std::endl;
        std::cout << "   - 进给机构: 返回零位" << std::endl;
        std::cout << "   - 所有夹持装置: 松开" << std::endl;
        std::cout << "3. 重置系统参数" << std::endl;
        std::cout << "   - 钻管计数: 重置为0" << std::endl;
    }
    else if (currentState == AutoDrillingStateMachine::STATE_OPERATION_COMPLETE) {
        std::cout << "\n[操作完成流程]" << std::endl;
        std::cout << "1. 确认所有机构安全" << std::endl;
        std::cout << "   - 所有机构: 回到安全位置" << std::endl;
        std::cout << "2. 保存操作数据" << std::endl;
        std::cout << "   - 记录钻进深度、钻管数量等数据" << std::endl;
        std::cout << "3. 等待新的指令" << std::endl;
    }
    else {
        std::cout << "\n[未知状态]" << std::endl;
        std::cout << "没有可用的流程信息" << std::endl;
    }
}

// 测试指定状态序列
void DebugTestMotion::testStateSequence(const QStringList& stateSequence)
{
    if (!m_controller) {
        qDebug() << "错误: 未创建钻进控制器";
        return;
    }

    m_isRunning = true;
    m_isStepMode = false;

    for (int i = 0; i < stateSequence.size(); i++) {
        if (!m_isRunning) {
            qDebug() << "测试被中断";
            return;
        }

        QString targetState = stateSequence[i];
        QString stateDesc = getStateDescription(targetState);

        qDebug() << "========================================";
        qDebug() << "执行步骤" << (i + 1) << "/" << stateSequence.size();
        qDebug() << "目标状态: " << stateDesc << " (" << targetState << ")";
        qDebug() << "========================================";

        // 执行状态转换
        if (i == 0) {
            // 第一个状态需要启动状态机
            m_controller->startStateMachine();
        } else {
            if (!transitionToState(targetState)) {
                qDebug() << "状态转换失败，测试中断";
                m_isRunning = false;
                return;
            }
        }

        // 等待状态处理完成
        waitForOperation(1000);

        // 输出转换后的状态
        printCurrentState();
    }

    qDebug() << "========================================";
    qDebug() << "测试序列执行完成";
    qDebug() << "========================================";
    m_isRunning = false;
}

// 初始化测试序列
void DebugTestMotion::initTestSequence()
{
    m_testSequence.clear();
    
    // 添加主要状态序列
    m_testSequence << AutoDrillingStateMachine::STATE_SYSTEM_STARTUP
                   << AutoDrillingStateMachine::STATE_DEFAULT_POSITION
                   << AutoDrillingStateMachine::STATE_READY
                   << AutoDrillingStateMachine::STATE_FIRST_TOOL_INSTALLATION
                   << AutoDrillingStateMachine::STATE_FIRST_DRILLING
                   << AutoDrillingStateMachine::STATE_PIPE_INSTALLATION_LOOP
                   << AutoDrillingStateMachine::STATE_PIPE_REMOVAL_LOOP
                   << AutoDrillingStateMachine::STATE_FIRST_TOOL_RECOVERY
                   << AutoDrillingStateMachine::STATE_SYSTEM_RESET
                   << AutoDrillingStateMachine::STATE_OPERATION_COMPLETE;
    
    // 初始化详细步骤描述映射
    m_stateDescriptions.clear();
    
    // 首根钻具安装流程
    m_stateDescriptions[AutoDrillingStateMachine::STATE_FIRST_TOOL_INSTALLATION] = QStringList()
        << "机械手移动到存储区"
        << "机械手夹持钻具"
        << "机械手移动到钻台"
        << "进给机构移动到钻具安装起始位置"
        << "执行连接操作"
        << "对接机构伸出"
        << "机械手松开";
    
    // 首次钻进流程
    m_stateDescriptions[AutoDrillingStateMachine::STATE_FIRST_DRILLING] = QStringList()
        << "启动钻进和冲击"
        << "进给机构下降"
        << "达到预定深度"
        << "下夹紧机构夹紧"
        << "对接机构回收"
        << "执行断开操作"
        << "进给机构上升";
    
    // 钻管安装循环流程
    m_stateDescriptions[AutoDrillingStateMachine::STATE_PIPE_INSTALLATION_LOOP] = QStringList()
        << "机械手移动到存储区"
        << "机械手夹持钻管"
        << "机械手移动到钻台"
        << "进给机构下降到钻管安装高度"
        << "执行连接操作"
        << "对接机构推出"
        << "机械手松开并收回"
        << "钻管之间的对接"
        << "下夹紧机构松开"
        << "开始钻进"
        << "下夹紧机构夹紧"
        << "对接机构回收"
        << "进给机构上升";
    
    // 钻管拆卸循环流程
    m_stateDescriptions[AutoDrillingStateMachine::STATE_PIPE_REMOVAL_LOOP] = QStringList()
        << "进给机构下降"
        << "执行连接操作"
        << "对接机构推出"
        << "下夹紧机构松开"
        << "进给机构上升"
        << "下夹紧机构夹紧"
        << "断开钻管间连接"
        << "机械手抓取钻管"
        << "对接机构回收"
        << "断开钻管与钻进机构连接"
        << "存储机构旋转到空位"
        << "机械手回收钻管"
        << "进给机构上升到待机位置";
    
    // 首根钻具回收流程
    m_stateDescriptions[AutoDrillingStateMachine::STATE_FIRST_TOOL_RECOVERY] = QStringList()
        << "进给机构下降"
        << "执行连接操作"
        << "对接机构推出"
        << "下夹紧机构松开"
        << "进给机构上升"
        << "机械手抓取钻具"
        << "对接机构回收"
        << "断开钻具与钻进机构连接"
        << "存储机构旋转到空位"
        << "机械手回收钻具"
        << "进给机构下降到待机位置";
}

// 转换到目标状态
bool DebugTestMotion::transitionToState(const QString& targetState)
{
    if (!m_controller || !m_controller->isRunning()) {
        std::cerr << "\n[错误] 状态机未运行，无法转换状态" << std::endl;
        return false;
    }

    AutoDrillingStateMachine* stateMachine = m_controller->getStateMachine();
    if (!stateMachine) {
        std::cerr << "\n[错误] 无法获取状态机" << std::endl;
        return false;
    }

    QString currentState = stateMachine->getCurrentStateName();
    if (currentState == targetState) {
        std::cout << "\n[信息] 已经处于目标状态: " << targetState.toStdString() << std::endl;
        return true;
    }

    std::cout << "\n[操作] 将状态从 " << currentState.toStdString() 
              << " 转换到 " << targetState.toStdString() << std::endl;
    
    // 执行状态转换
    if (stateMachine->changeState(targetState)) {
        waitForOperation(2000);
        return true;
    } else {
        std::cerr << "\n[错误] 无法转换到目标状态" << std::endl;
        
        // 尝试先回到就绪状态再转换
        if (currentState != AutoDrillingStateMachine::STATE_READY) {
            std::cout << "\n[操作] 尝试先回到就绪状态再转换" << std::endl;
            std::cout << "[操作] 按Enter继续..." << std::endl;
            std::string input;
            std::getline(std::cin, input);
            
            if (stateMachine->changeState(AutoDrillingStateMachine::STATE_READY)) {
                waitForOperation(2000);
                return stateMachine->changeState(targetState);
            }
        }
    }
    
    return false;
}

// 等待操作完成
void DebugTestMotion::waitForOperation(int msec)
{
    std::cout << "\n[等待] 操作执行中..." << std::endl;
    QEventLoop loop;
    QTimer::singleShot(msec, [&loop]() {
        std::cout << "[完成] 操作已完成" << std::endl << std::endl;
        loop.quit();
    });
    loop.exec();
}

// 状态名称到描述的转换
QString DebugTestMotion::getStateDescription(const QString& stateName) const
{
    if (stateName == AutoDrillingStateMachine::STATE_SYSTEM_STARTUP)
        return "系统启动";
    else if (stateName == AutoDrillingStateMachine::STATE_DEFAULT_POSITION)
        return "默认位置";
    else if (stateName == AutoDrillingStateMachine::STATE_READY)
        return "就绪状态";
    else if (stateName == AutoDrillingStateMachine::STATE_FIRST_TOOL_INSTALLATION)
        return "首根钻具安装";
    else if (stateName == AutoDrillingStateMachine::STATE_FIRST_DRILLING)
        return "首次钻进";
    else if (stateName == AutoDrillingStateMachine::STATE_PIPE_INSTALLATION_LOOP)
        return "钻管安装循环";
    else if (stateName == AutoDrillingStateMachine::STATE_PIPE_REMOVAL_LOOP)
        return "钻管拆卸循环";
    else if (stateName == AutoDrillingStateMachine::STATE_FIRST_TOOL_RECOVERY)
        return "首根钻具回收";
    else if (stateName == AutoDrillingStateMachine::STATE_SYSTEM_RESET)
        return "系统重置";
    else if (stateName == AutoDrillingStateMachine::STATE_OPERATION_COMPLETE)
        return "操作完成";
    else
        return stateName;
}

// 获取状态的详细步骤描述
void DebugTestMotion::printStateSteps(const QString& stateName) const
{
    if (m_stateDescriptions.contains(stateName)) {
        QStringList steps = m_stateDescriptions[stateName];
        qDebug() << "状态 [" << getStateDescription(stateName) << "] 的详细步骤:";
        for (int i = 0; i < steps.size(); ++i) {
            qDebug() << "  " << (i+1) << ". " << steps[i];
        }
    }
}

// 事件处理器
void DebugTestMotion::onStateChanged(const QString& oldState, const QString& newState)
{
    QString oldDesc = getStateDescription(oldState);
    QString newDesc = getStateDescription(newState);
    qDebug() << "状态变化: " << oldDesc << " (" << oldState << ") -> " 
             << newDesc << " (" << newState << ")";
    
    // 打印新状态的详细步骤
    if (!newState.isEmpty()) {
        printStateSteps(newState);
    }
}

void DebugTestMotion::onMachineStarted()
{
    qDebug() << "状态机已启动";
}

void DebugTestMotion::onMachineStopped()
{
    qDebug() << "状态机已停止";
}

void DebugTestMotion::onMachinePaused()
{
    qDebug() << "状态机已暂停";
}

void DebugTestMotion::onMachineResumed()
{
    qDebug() << "状态机已恢复";
}

void DebugTestMotion::onPipeCountChanged(int count)
{
    qDebug() << "钻管计数已更新: " << count;
}

// 电机状态更新处理函数
void DebugTestMotion::onMotorStatusUpdated(int motorID, const QMap<QString, float>& params)
{
    // 更新电机状态
    m_motorStatus[motorID] = params;
}

// 命令响应处理函数
void DebugTestMotion::onCommandResponse(const QString& response)
{
    qDebug() << "命令响应: " << response;
}

#endif // ENABLE_DEBUG_TEST_MOTION