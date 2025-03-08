// autodrilling.cpp
#include "inc/autodrilling.h"
#include "inc/DrillingParameters.h"
#include "motioncontroller.h"
#include <QDebug>
#include <QThread>

// 定义静态常量
const QString AutoDrillingStateMachine::STATE_SYSTEM_STARTUP = "SYSTEM_STARTUP";
const QString AutoDrillingStateMachine::STATE_DEFAULT_POSITION = "DEFAULT_POSITION";
const QString AutoDrillingStateMachine::STATE_READY = "READY";
const QString AutoDrillingStateMachine::STATE_FIRST_TOOL_INSTALLATION = "FIRST_TOOL_INSTALLATION";
const QString AutoDrillingStateMachine::STATE_FIRST_DRILLING = "FIRST_DRILLING";
const QString AutoDrillingStateMachine::STATE_PIPE_INSTALLATION_LOOP = "PIPE_INSTALLATION_LOOP";
const QString AutoDrillingStateMachine::STATE_PIPE_REMOVAL_LOOP = "PIPE_REMOVAL_LOOP";
const QString AutoDrillingStateMachine::STATE_FIRST_TOOL_RECOVERY = "FIRST_TOOL_RECOVERY";
const QString AutoDrillingStateMachine::STATE_SYSTEM_RESET = "SYSTEM_RESET";
const QString AutoDrillingStateMachine::STATE_OPERATION_COMPLETE = "OPERATION_COMPLETE";

// 定义电机ID常量
const int StorageUnitStateMachine::STORAGE_MOTOR_ID = 0;
const int RobotArmStateMachine::ROTATION_MOTOR_ID = 1;
const int RobotArmStateMachine::EXTENSION_MOTOR_ID = 2;
const int RobotArmStateMachine::CLAMP_MOTOR_ID = 3;
const int DrillingMechanismStateMachine::DRILL_MOTOR_ID = 4;
const int DrillingMechanismStateMachine::PERCUSSION_MOTOR_ID = 5;
const int PenetrationMechanismStateMachine::PENETRATION_MOTOR_ID = 6;
const int ClampMechanismStateMachine::CLAMP_MOTOR_ID = 7;
const int ConnectionMechanismStateMachine::CONNECTION_MOTOR_ID = 8;


/**
 * @brief 自动钻进状态机构造函数
 * @param parent 父对象
 */
AutoDrillingStateMachine::AutoDrillingStateMachine(QObject *parent)
    : StateMachine(parent)
    , m_motionController(nullptr)
    , m_pipeCount(0)
    , m_percussionEnabled(false)
    , m_percussionFrequency(0.0)
    , m_drillMode(CONSTANT_SPEED)
    , m_drillParameter(0.0)
    , m_deltaThread(0.1)  // 默认值，应从配置中读取
    , m_deltaTool(0.2)
    , m_deltaPipe(0.3)
    , m_deltaDrill(1.0)
    , m_v1(0.01)  // 钻进速度
    , m_v2(0.05)  // 钻管接卸速度
    , m_v3(0.1)   // 空行程速度
    , m_omega(60.0)  // 正常钻进旋转速度
    , m_omega_s(10.0) // 低速对接/断开旋转速度
{
    // 创建组件状态机
    m_storageUnit = std::make_shared<StorageUnitStateMachine>();
    m_robotArm = std::make_shared<RobotArmStateMachine>();
    m_drillingMechanism = std::make_shared<DrillingMechanismStateMachine>();
    m_penetrationMechanism = std::make_shared<PenetrationMechanismStateMachine>();
    m_clampMechanism = std::make_shared<ClampMechanismStateMachine>();
    m_connectionMechanism = std::make_shared<ConnectionMechanismStateMachine>();
    
    // 创建主状态
    createMainStates();
    
    logInfo("自动钻进状态机已创建");
}

/**
 * @brief 自动钻进状态机析构函数
 */
AutoDrillingStateMachine::~AutoDrillingStateMachine() {
    logInfo("自动钻进状态机已销毁");
}

/**
 * @brief 初始化自动钻进状态机
 */
void AutoDrillingStateMachine::initialize() {
    // 检查运动控制器
    if (!m_motionController) {
        logError("初始化失败：未设置运动控制器");
        return;
    }
    
    // 设置所有组件为调试模式
    bool isDebugMode = m_motionController->isDebugMode();
    m_storageUnit->setDebugMode(isDebugMode);
    m_robotArm->setDebugMode(isDebugMode);
    m_drillingMechanism->setDebugMode(isDebugMode);
    m_penetrationMechanism->setDebugMode(isDebugMode);
    m_clampMechanism->setDebugMode(isDebugMode);
    m_connectionMechanism->setDebugMode(isDebugMode);
    
    // 初始化所有组件状态机
    if (!m_storageUnit->initialize()) {
        logError("存储单元初始化失败");
    }
    
    if (!m_robotArm->initialize()) {
        logError("机械手初始化失败");
    }
    
    if (!m_drillingMechanism->initialize()) {
        logError("钻进机构初始化失败");
    }
    
    if (!m_penetrationMechanism->initialize()) {
        logError("进给机构初始化失败");
    }
    
    if (!m_clampMechanism->initialize()) {
        logError("下夹紧机构初始化失败");
    }
    
    if (!m_connectionMechanism->initialize()) {
        logError("对接机构初始化失败");
    }
    
    // 重置钻管计数和其他参数
    m_pipeCount = 0;
    m_percussionEnabled = false;
    m_percussionFrequency = 0.0;
    
    // 设置默认钻进模式
    m_drillMode = CONSTANT_SPEED;
    m_drillParameter = 0.0;
    
    // 设置初始状态并启动状态机
    setInitialState(STATE_SYSTEM_STARTUP);
    start();
    
    logInfo("自动钻进状态机初始化完成");
}

/**
 * @brief 设置运动控制器
 * @param controller 运动控制器指针
 */
void AutoDrillingStateMachine::setMotionController(MotionController* controller) {
    m_motionController = controller;
    
    // 将控制器传递给所有组件状态机
    m_storageUnit->setMotionController(controller);
    m_robotArm->setMotionController(controller);
    m_drillingMechanism->setMotionController(controller);
    m_penetrationMechanism->setMotionController(controller);
    m_clampMechanism->setMotionController(controller);
    m_connectionMechanism->setMotionController(controller);
    
    logInfo("已设置运动控制器");
}

/**
 * @brief 设置钻进模式
 * @param mode 模式 (CONSTANT_SPEED=66 或 CONSTANT_TORQUE=67)
 * @param value 参数值
 */
void AutoDrillingStateMachine::setDrillMode(DrillMode mode, double value) {
    m_drillMode = mode;
    m_drillParameter = value;
    
    // 应用模式到钻进机构
    static_cast<DrillingMechanismStateMachine*>(m_drillingMechanism.get())->setDrillMode(static_cast<int>(mode), value);
    
    // 发送信号以更新GUI
    emit drillingModeChanged(mode, value);
    
    logInfo(QString("钻进模式已设置: 模式=%1, 参数值=%2").arg(mode).arg(value));
}

/**
 * @brief 启用/禁用冲击功能
 * @param enable true启用，false禁用
 */
void AutoDrillingStateMachine::enablePercussion(bool enable) {
    m_percussionEnabled = enable;
    
    // 应用到钻进机构
    if (enable) {
        static_cast<DrillingMechanismStateMachine*>(m_drillingMechanism.get())->setPercussionFrequency(m_percussionFrequency);
    } else {
        static_cast<DrillingMechanismStateMachine*>(m_drillingMechanism.get())->setPercussionFrequency(0.0);
    }
    
    // 发送信号以更新GUI
    emit percussionStatusChanged(m_percussionEnabled, m_percussionFrequency);
    
    logInfo(QString("冲击功能已%1").arg(enable ? "启用" : "禁用"));
}

/**
 * @brief 设置冲击频率
 * @param frequency 冲击频率 (Hz)
 */
void AutoDrillingStateMachine::setPercussionFrequency(double frequency) {
    m_percussionFrequency = frequency;
    
    // 如果冲击功能已启用，应用新频率
    if (m_percussionEnabled) {
        static_cast<DrillingMechanismStateMachine*>(m_drillingMechanism.get())->setPercussionFrequency(frequency);
    }
    
    // 发送信号以更新GUI
    emit percussionStatusChanged(m_percussionEnabled, m_percussionFrequency);
    
    logInfo(QString("冲击频率已设置: %1 Hz").arg(frequency));
}

/**
 * @brief 开始操作
 */
void AutoDrillingStateMachine::startOperation() {
    if (!isRunning()) {
        // 确保状态机处于正确的初始状态
        if (getCurrentStateName().isEmpty()) {
            setInitialState(STATE_SYSTEM_STARTUP);
        }
        start();
        logInfo("状态机已启动");
    } else {
        logWarning("自动钻进状态机已在运行中");
    }
}

/**
 * @brief 停止操作
 */
void AutoDrillingStateMachine::stopOperation() {
    if (isRunning()) {
        // 如果在PIPE_INSTALLATION_LOOP中，切换到PIPE_REMOVAL_LOOP
        if (getCurrentStateName() == STATE_PIPE_INSTALLATION_LOOP) {
            changeState(STATE_PIPE_REMOVAL_LOOP);
        } else {
            stop();
        }
    } else {
        logWarning("自动钻进状态机未在运行中");
    }
}

/**
 * @brief 暂停操作
 */
void AutoDrillingStateMachine::pauseOperation() {
    if (isRunning() && !isPaused()) {
        pause();
    } else {
        logWarning("自动钻进状态机无法暂停");
    }
}

/**
 * @brief 恢复操作
 */
void AutoDrillingStateMachine::resumeOperation() {
    if (isRunning() && isPaused()) {
        resume();
    } else {
        logWarning("自动钻进状态机无法恢复");
    }
}

/**
 * @brief 获取当前钻管计数
 * @return 钻管计数
 */
int AutoDrillingStateMachine::getCurrentPipeCount() const {
    return m_pipeCount;
}

/**
 * @brief 状态切换前的处理
 * @param oldState 当前状态
 * @param newState 目标状态
 * @return true允许切换，false阻止切换
 */
bool AutoDrillingStateMachine::beforeStateChange(const QString& oldState, const QString& newState) {
    logInfo(QString("状态即将从 %1 切换到 %2").arg(oldState).arg(newState));
    return true;
}

/**
 * @brief 状态切换后的处理
 * @param oldState 之前的状态
 * @param newState 当前状态
 */
void AutoDrillingStateMachine::afterStateChange(const QString& oldState, const QString& newState) {
    logInfo(QString("状态已从 %1 切换到 %2").arg(oldState).arg(newState));
    emit currentStepChanged(QString("当前状态: %1").arg(newState));
}

/**
 * @brief 创建主状态机状态
 */
void AutoDrillingStateMachine::createMainStates() {
    // 创建所有状态
    addState(STATE_SYSTEM_STARTUP, std::make_shared<SystemStartupState>(this));
    addState(STATE_DEFAULT_POSITION, std::make_shared<DefaultPositionState>(this));
    addState(STATE_READY, std::make_shared<ReadyState>(this));
    addState(STATE_FIRST_TOOL_INSTALLATION, std::make_shared<FirstToolInstallationState>(this));
    addState(STATE_FIRST_DRILLING, std::make_shared<FirstDrillingState>(this));
    addState(STATE_PIPE_INSTALLATION_LOOP, std::make_shared<PipeInstallationLoopState>(this));
    addState(STATE_PIPE_REMOVAL_LOOP, std::make_shared<PipeRemovalLoopState>(this));
    addState(STATE_FIRST_TOOL_RECOVERY, std::make_shared<FirstToolRecoveryState>(this));
    addState(STATE_SYSTEM_RESET, std::make_shared<SystemResetState>(this));
    addState(STATE_OPERATION_COMPLETE, std::make_shared<OperationCompleteState>(this));
    
    // 设置初始状态
    setInitialState(STATE_SYSTEM_STARTUP);
    
    // 记录状态创建信息
    logInfo("所有状态已创建完成");
    logInfo(QString("初始状态设置为: %1").arg(STATE_SYSTEM_STARTUP));
}

/**
 * @brief 增加钻管计数
 */
void AutoDrillingStateMachine::incrementPipeCount() {
    m_pipeCount++;
    emit pipeCountChanged(m_pipeCount);
    logInfo(QString("钻管计数增加到: %1").arg(m_pipeCount));
}

/**
 * @brief 减少钻管计数
 */
void AutoDrillingStateMachine::decrementPipeCount() {
    if (m_pipeCount > 0) {
        m_pipeCount--;
        emit pipeCountChanged(m_pipeCount);
        logInfo(QString("钻管计数减少到: %1").arg(m_pipeCount));
    }
}

//================ ComponentStateMachine 实现 ================

/**
 * @brief 组件状态机构造函数
 * @param name 组件名称
 * @param parent 父对象
 */
ComponentStateMachine::ComponentStateMachine(const QString& name, QObject *parent)
    : StateMachine(parent)
    , m_componentName(name)
    , m_motionController(nullptr)
    , m_isDebugMode(false)
{
    logDebug(QString("组件状态机创建: %1").arg(name));
}

/**
 * @brief 组件状态机析构函数
 */
ComponentStateMachine::~ComponentStateMachine() {
    logDebug(QString("组件状态机销毁: %1").arg(m_componentName));
}

/**
 * @brief 获取组件名称
 * @return 组件名称
 */
QString ComponentStateMachine::getComponentName() const {
    return m_componentName;
}

/**
 * @brief 设置运动控制器
 * @param controller 运动控制器指针
 */
void ComponentStateMachine::setMotionController(MotionController* controller) {
    m_motionController = controller;
}

void ComponentStateMachine::setDebugMode(bool debug) {
    m_isDebugMode = debug;
    logInfo(QString("%1: %2调试模式").arg(m_componentName).arg(debug ? "启用" : "禁用"));
}

bool ComponentStateMachine::isDebugMode() const {
    return m_isDebugMode;
}

//================ StorageUnitStateMachine 实现 ================

/**
 * @brief 存储单元状态机构造函数
 */
StorageUnitStateMachine::StorageUnitStateMachine(QObject *parent)
    : ComponentStateMachine("存储单元", parent)
    , m_currentPosition(0)
{
    // 初始化存储单元状态机
}

/**
 * @brief 存储单元状态机析构函数
 */
StorageUnitStateMachine::~StorageUnitStateMachine() {
    // 清理资源
}

/**
 * @brief 初始化存储单元
 */
bool StorageUnitStateMachine::initialize() {
    if (isDebugMode()) {
        logInfo(QString("[调试模式] %1初始化").arg(getComponentName()));
        m_currentPosition = 0;
        return true;
    }
    
    if (!m_motionController) {
        logError("初始化失败：未设置运动控制器");
        return false;
    }
    
    // 设置存储单元参数
    m_motionController->setMotorParameter(STORAGE_MOTOR_ID, "Atype", 66);  // 恒速度模式
    m_motionController->setMotorParameter(STORAGE_MOTOR_ID, "EN", 1);      // 使能电机
    m_motionController->setMotorParameter(STORAGE_MOTOR_ID, "Vel", 10.0);  // 旋转速度
    
    // 设置初始位置
    return rotateToPosition(0);
}

/**
 * @brief 重置存储单元
 */
void StorageUnitStateMachine::reset() {
    rotateToPosition(0);
}

/**
 * @brief 旋转到指定位置
 * @param position 目标位置 (0-13)
 * @return 是否成功
 */
bool StorageUnitStateMachine::rotateToPosition(int position) {
    if (isDebugMode()) {
        int oldPosition = m_currentPosition;
        m_currentPosition = position;
        logInfo(QString("[调试模式] 存储单元电机(ID=%1) 旋转: %2° → %3° (位置: %4 → %5)")
            .arg(STORAGE_MOTOR_ID)
            .arg(oldPosition * 360.0f / MAX_POSITIONS)
            .arg(position * 360.0f / MAX_POSITIONS)
            .arg(oldPosition)
            .arg(position));
        return true;
    }
    
    if (!m_motionController) {
        logError("旋转失败：未设置运动控制器");
        return false;
    }
    
    if (position < 0 || position >= MAX_POSITIONS) {
        logError(QString("无效的存储位置: %1").arg(position));
        return false;
    }
    
    // 计算角度 - 每个位置对应的角度为 (position/MAX_POSITIONS) * 360
    float angle = (float)position / MAX_POSITIONS * 360.0f;
    
    // 使用运动控制器执行旋转
    if (!m_motionController->moveMotorAbsolute(STORAGE_MOTOR_ID, angle)) {
        logError(QString("旋转到位置 %1 失败").arg(position));
        return false;
    }
    
    m_currentPosition = position;
    logInfo(QString("存储单元旋转到位置: %1").arg(position));
    return true;
}

/**
 * @brief 获取当前位置
 * @return 当前位置 (0-13)
 */
int StorageUnitStateMachine::getCurrentPosition() const {
    return m_currentPosition;
}

//================ RobotArmStateMachine 实现 ================

/**
 * @brief 机械手状态机构造函数
 */
RobotArmStateMachine::RobotArmStateMachine(QObject *parent)
    : ComponentStateMachine("机械手", parent)
    , m_rotationPosition(0)
    , m_extension(0)
    , m_clamp(0)
{
    // 初始化机械手状态机
}

/**
 * @brief 机械手状态机析构函数
 */
RobotArmStateMachine::~RobotArmStateMachine() {
    // 清理资源
}

/**
 * @brief 初始化机械手
 */
bool RobotArmStateMachine::initialize() {
    if (isDebugMode()) {
        logInfo(QString("[调试模式] %1初始化").arg(getComponentName()));
        m_rotationPosition = 0;  // 对准钻进机构
        m_extension = 0;         // 缩回
        m_clamp = 0;            // 未夹持
        return true;
    }
    
    if (!m_motionController) {
        logError("初始化失败：未设置运动控制器");
        return false;
    }
    
    // 设置旋转电机参数
    m_motionController->setMotorParameter(ROTATION_MOTOR_ID, "Atype", 66);
    m_motionController->setMotorParameter(ROTATION_MOTOR_ID, "EN", 1);
    m_motionController->setMotorParameter(ROTATION_MOTOR_ID, "Vel", 15.0);
    
    // 设置伸缩电机参数
    m_motionController->setMotorParameter(EXTENSION_MOTOR_ID, "Atype", 66);
    m_motionController->setMotorParameter(EXTENSION_MOTOR_ID, "EN", 1);
    m_motionController->setMotorParameter(EXTENSION_MOTOR_ID, "Vel", 10.0);
    
    // 设置夹持电机参数
    m_motionController->setMotorParameter(CLAMP_MOTOR_ID, "Atype", 66);
    m_motionController->setMotorParameter(CLAMP_MOTOR_ID, "EN", 1);
    m_motionController->setMotorParameter(CLAMP_MOTOR_ID, "Vel", 5.0);
    
    // 重置状态
    m_rotationPosition = 0;
    m_extension = 0;
    m_clamp = 0;
    
    // 移动到初始位置
    bool success = true;
    success &= setRotationPosition(0);
    success &= setExtension(0);
    success &= setClamp(0);
    
    return success;
}

/**
 * @brief 重置机械手
 */
void RobotArmStateMachine::reset() {
    setRotationPosition(0);
    setExtension(0);
    setClamp(0);
}

/**
 * @brief 设置旋转位置
 * @param position 目标位置 (0=对准钻进机构, 1=对准存储单元)
 * @return 是否成功
 */
bool RobotArmStateMachine::setRotationPosition(int position) {
    if (isDebugMode()) {
        int oldPosition = m_rotationPosition;
        m_rotationPosition = position;
        float oldAngle = (oldPosition == 0) ? 
            DrillingParameters::RobotPosition::DRILL_POSITION : 
            DrillingParameters::RobotPosition::STORAGE_POSITION;
        float newAngle = (position == 0) ? 
            DrillingParameters::RobotPosition::DRILL_POSITION : 
            DrillingParameters::RobotPosition::STORAGE_POSITION;
        logInfo(QString("[调试模式] 机械手旋转电机(ID=%1) 旋转: %2° → %3° (%4 → %5)")
            .arg(ROTATION_MOTOR_ID)
            .arg(oldAngle)
            .arg(newAngle)
            .arg(oldPosition == 0 ? "对准钻进机构" : "对准存储单元")
            .arg(position == 0 ? "对准钻进机构" : "对准存储单元"));
        return true;
    }
    
    if (!m_motionController) {
        logError("设置旋转位置失败：未设置运动控制器");
        return false;
    }
    
    if (position != 0 && position != 1) {
        logError(QString("无效的旋转位置: %1").arg(position));
        return false;
    }
    
    // 计算旋转角度：0度表示对准钻进机构，90度表示对准存储单元
    float angle = (position == 0) ? 
        DrillingParameters::RobotPosition::DRILL_POSITION : 
        DrillingParameters::RobotPosition::STORAGE_POSITION;
    
    // 使用运动控制器执行旋转
    if (!m_motionController->moveMotorAbsolute(ROTATION_MOTOR_ID, angle)) {
        logError(QString("设置旋转位置 %1 失败").arg(position));
        return false;
    }
    
    m_rotationPosition = position;
    logInfo(QString("机械手旋转位置设置为: %1").arg(position == 0 ? "对准钻进机构" : "对准存储单元"));
    return true;
}

/**
 * @brief 设置伸缩状态
 * @param extension 目标状态 (0=缩回, 1=伸出)
 * @return 是否成功
 */
bool RobotArmStateMachine::setExtension(int extension) {
    if (isDebugMode()) {
        int oldExtension = m_extension;
        m_extension = extension;
        logInfo(QString("[调试模式] 机械手伸缩电机(ID=%1) 移动: %2mm → %3mm (%4 → %5)")
            .arg(EXTENSION_MOTOR_ID)
            .arg(oldExtension * 100.0f)
            .arg(extension * 100.0f)
            .arg(oldExtension == 0 ? "缩回" : "伸出")
            .arg(extension == 0 ? "缩回" : "伸出"));
        return true;
    }
    
    if (!m_motionController) {
        logError("设置伸缩状态失败：未设置运动控制器");
        return false;
    }
    
    if (extension != 0 && extension != 1) {
        logError(QString("无效的伸缩状态: %1").arg(extension));
        return false;
    }
    
    // 计算伸缩位置：0表示缩回，100表示伸出
    float position = extension * 100.0f;
    
    // 使用运动控制器执行伸缩
    if (!m_motionController->moveMotorAbsolute(EXTENSION_MOTOR_ID, position)) {
        logError(QString("设置伸缩状态 %1 失败").arg(extension));
        return false;
    }
    
    m_extension = extension;
    logInfo(QString("机械手伸缩状态设置为: %1").arg(extension == 0 ? "缩回" : "伸出"));
    return true;
}

/**
 * @brief 设置夹持状态
 * @param clamp 目标状态 (0=未夹持, 1=夹紧)
 * @return 是否成功
 */
bool RobotArmStateMachine::setClamp(int clamp) {
    if (isDebugMode()) {
        int oldClamp = m_clamp;
        m_clamp = clamp;
        logInfo(QString("[调试模式] 机械手夹持电机(ID=%1) 移动: %2mm → %3mm (%4 → %5)")
            .arg(CLAMP_MOTOR_ID)
            .arg(oldClamp * 50.0f)
            .arg(clamp * 50.0f)
            .arg(oldClamp == 0 ? "未夹持" : "夹紧")
            .arg(clamp == 0 ? "未夹持" : "夹紧"));
        return true;
    }
    
    if (!m_motionController) {
        logError("设置夹持状态失败：未设置运动控制器");
        return false;
    }
    
    if (clamp != 0 && clamp != 1) {
        logError(QString("无效的夹持状态: %1").arg(clamp));
        return false;
    }
    
    // 计算夹持位置：0表示未夹持，50表示夹紧
    float position = clamp * 50.0f;
    
    // 使用运动控制器执行夹持
    if (!m_motionController->moveMotorAbsolute(CLAMP_MOTOR_ID, position)) {
        logError(QString("设置夹持状态 %1 失败").arg(clamp));
        return false;
    }
    
    m_clamp = clamp;
    logInfo(QString("机械手夹持状态设置为: %1").arg(clamp == 0 ? "未夹持" : "夹紧"));
    return true;
}

/**
 * @brief 获取旋转位置
 * @return 旋转位置 (0=对准钻进机构, 1=对准存储单元)
 */
int RobotArmStateMachine::getRotationPosition() const {
    return m_rotationPosition;
}

/**
 * @brief 获取伸缩状态
 * @return 伸缩状态 (0=缩回, 1=伸出)
 */
int RobotArmStateMachine::getExtension() const {
    return m_extension;
}

/**
 * @brief 获取夹持状态
 * @return 夹持状态 (0=未夹持, 1=夹紧)
 */
int RobotArmStateMachine::getClamp() const {
    return m_clamp;
}

//================ DrillingMechanismStateMachine 实现 ================

/**
 * @brief 钻进机构状态机构造函数
 */
DrillingMechanismStateMachine::DrillingMechanismStateMachine(QObject *parent)
    : ComponentStateMachine("钻进机构", parent)
    , m_rotationSpeed(0.0)
    , m_percussionFrequency(0.0)
    , m_connected(false)
    , m_drillMode(66)  // 默认恒速度模式
    , m_drillValue(0.0)
{
    // 初始化钻进机构状态机
}

/**
 * @brief 钻进机构状态机析构函数
 */
DrillingMechanismStateMachine::~DrillingMechanismStateMachine() {
    // 清理资源
}

/**
 * @brief 初始化钻进机构
 */
bool DrillingMechanismStateMachine::initialize() {
    if (isDebugMode()) {
        logInfo(QString("[调试模式] %1初始化").arg(getComponentName()));
        m_rotationSpeed = 0.0;
        m_percussionFrequency = 0.0;
        m_connected = false;
        m_drillMode = 66;  // 恒速度模式
        m_drillValue = 0.0;
        return true;
    }
    
    if (!m_motionController) {
        logError("初始化失败：未设置运动控制器");
        return false;
    }
    
    // 设置钻进电机参数
    m_motionController->setMotorParameter(DRILL_MOTOR_ID, "Atype", m_drillMode);
    m_motionController->setMotorParameter(DRILL_MOTOR_ID, "EN", 1);
    
    // 设置冲击电机参数
    m_motionController->setMotorParameter(PERCUSSION_MOTOR_ID, "Atype", 66);
    m_motionController->setMotorParameter(PERCUSSION_MOTOR_ID, "EN", 1);
    
    // 停止钻进和冲击
    setRotationSpeed(0.0);
    setPercussionFrequency(0.0);
    
    // 设置连接状态为未连接
    setConnectionStatus(false);
    
    return true;
}

/**
 * @brief 重置钻进机构
 */
void DrillingMechanismStateMachine::reset() {
    setRotationSpeed(0.0);
    setPercussionFrequency(0.0);
    setConnectionStatus(false);
}

/**
 * @brief 设置旋转速度
 * @param speed 旋转速度 (rad/s)
 * @return 是否成功
 */
bool DrillingMechanismStateMachine::setRotationSpeed(double speed) {
    if (isDebugMode()) {
        double oldSpeed = m_rotationSpeed;
        m_rotationSpeed = speed;
        logInfo(QString("[调试模式] 钻进电机(ID=%1) 速度: %2 rad/s → %3 rad/s (模式: %4)")
            .arg(DRILL_MOTOR_ID)
            .arg(oldSpeed)
            .arg(speed)
            .arg(m_drillMode == 66 ? "恒速度" : "恒力矩"));
        return true;
    }
    
    if (!m_motionController) {
        logError("设置旋转速度失败：未设置运动控制器");
        return false;
    }
    
    if (m_drillMode == 66) {  // 恒速度模式
        if (!m_motionController->setMotorParameter(DRILL_MOTOR_ID, "Vel", speed)) {
            logError(QString("设置旋转速度 %1 失败").arg(speed));
            return false;
        }
    } else if (m_drillMode == 67) {  // 恒力矩模式
        if (!m_motionController->setMotorParameter(DRILL_MOTOR_ID, "DAC", speed)) {
            logError(QString("设置旋转力矩 %1 失败").arg(speed));
            return false;
        }
    }
    
    m_rotationSpeed = speed;
    logInfo(QString("钻进机构旋转速度设置为: %1").arg(speed));
    return true;
}

/**
 * @brief 设置冲击频率
 * @param frequency 冲击频率 (Hz)
 * @return 是否成功
 */
bool DrillingMechanismStateMachine::setPercussionFrequency(double frequency) {
    if (isDebugMode()) {
        double oldFrequency = m_percussionFrequency;
        m_percussionFrequency = frequency;
        logInfo(QString("[调试模式] 冲击电机(ID=%1) 频率: %2 Hz → %3 Hz")
            .arg(PERCUSSION_MOTOR_ID)
            .arg(oldFrequency)
            .arg(frequency));
        return true;
    }
    
    if (!m_motionController) {
        logError("设置冲击频率失败：未设置运动控制器");
        return false;
    }
    
    // 设置冲击电机频率
    if (frequency > 0) {
        // 启用冲击
        if (!m_motionController->setMotorParameter(PERCUSSION_MOTOR_ID, "Vel", frequency)) {
            logError(QString("设置冲击频率 %1 失败").arg(frequency));
            return false;
        }
    } else {
        // 停止冲击
        if (!m_motionController->setMotorParameter(PERCUSSION_MOTOR_ID, "Vel", 0)) {
            logError("停止冲击失败");
            return false;
        }
    }
    
    m_percussionFrequency = frequency;
    logInfo(QString("钻进机构冲击频率设置为: %1 Hz").arg(frequency));
    return true;
}

/**
 * @brief 设置连接状态
 * @param connected 连接状态 (true=已对接, false=未对接)
 * @return 是否成功
 */
bool DrillingMechanismStateMachine::setConnectionStatus(bool connected) {
    m_connected = connected;
    logInfo(QString("钻进机构连接状态设置为: %1").arg(connected ? "已对接" : "未对接"));
    return true;
}

/**
 * @brief 获取旋转速度
 * @return 旋转速度 (rad/s)
 */
double DrillingMechanismStateMachine::getRotationSpeed() const {
    return m_rotationSpeed;
}

/**
 * @brief 获取冲击频率
 * @return 冲击频率 (Hz)
 */
double DrillingMechanismStateMachine::getPercussionFrequency() const {
    return m_percussionFrequency;
}

/**
 * @brief 获取连接状态
 * @return 连接状态 (true=已对接, false=未对接)
 */
bool DrillingMechanismStateMachine::getConnectionStatus() const {
    return m_connected;
}

/**
 * @brief 设置钻进模式
 * @param mode 模式 (66=恒速度, 67=恒力矩)
 * @param value 参数值
 * @return 是否成功
 */
bool DrillingMechanismStateMachine::setDrillMode(int mode, double value) {
    if (!m_motionController) {
        logError("设置钻进模式失败：未设置运动控制器");
        return false;
    }
    
    if (mode != 66 && mode != 67) {
        logError(QString("无效的钻进模式: %1").arg(mode));
        return false;
    }
    
    // 设置钻进模式
    if (!m_motionController->setMotorParameter(DRILL_MOTOR_ID, "Atype", mode)) {
        logError(QString("设置钻进模式 %1 失败").arg(mode));
        return false;
    }
    
    // 设置参数值
    QString paramName = (mode == 66) ? "Vel" : "DAC";
    if (!m_motionController->setMotorParameter(DRILL_MOTOR_ID, paramName, value)) {
        logError(QString("设置钻进参数 %1=%2 失败").arg(paramName).arg(value));
        return false;
    }
    
    m_drillMode = mode;
    m_drillValue = value;
    logInfo(QString("钻进模式已设置: 模式=%1, 参数值=%2").arg(mode).arg(value));
    return true;
}

//================ PenetrationMechanismStateMachine 实现 ================

/**
 * @brief 进给机构状态机构造函数
 */
PenetrationMechanismStateMachine::PenetrationMechanismStateMachine(QObject *parent)
    : ComponentStateMachine("进给机构", parent)
    , m_currentPosition(POSITION_D)  // 默认顶部位置
    , m_currentPositionValue(0.0)
{
    // 初始化位置值数组
    m_positionValues[POSITION_A] = 0.0;      // 最底部位置
    m_positionValues[POSITION_A1] = 0.1;     // A + Δ_thread
    m_positionValues[POSITION_B1] = 0.3;     // 钻具安装完成位置
    m_positionValues[POSITION_B2] = 0.5;     // 钻管安装完成位置
    m_positionValues[POSITION_C1] = 0.2;     // 钻具中间安装位置 (B1 - Δ_tool)
    m_positionValues[POSITION_C2] = 0.4;     // 钻管中间安装位置 (B2 - Δ_pipe)
    m_positionValues[POSITION_D] = 1.0;      // 最顶部位置
}

/**
 * @brief 进给机构状态机析构函数
 */
PenetrationMechanismStateMachine::~PenetrationMechanismStateMachine() {
    // 清理资源
}

/**
 * @brief 初始化进给机构
 */
bool PenetrationMechanismStateMachine::initialize() {
    if (isDebugMode()) {
        logInfo(QString("[调试模式] %1初始化").arg(getComponentName()));
        m_currentPosition = POSITION_D;  // 初始化在顶部位置
        m_currentPositionValue = m_positionValues[POSITION_D];
        return true;
    }
    
    if (!m_motionController) {
        logError("初始化失败：未设置运动控制器");
        return false;
    }
    
    // 设置进给电机参数
    m_motionController->setMotorParameter(PENETRATION_MOTOR_ID, "Atype", 66);
    m_motionController->setMotorParameter(PENETRATION_MOTOR_ID, "EN", 1);
    m_motionController->setMotorParameter(PENETRATION_MOTOR_ID, "Vel", 20.0);
    
    // 移动到顶部位置
    return moveToPosition(POSITION_D);
}

/**
 * @brief 重置进给机构
 */
void PenetrationMechanismStateMachine::reset() {
    moveToPosition(POSITION_D);
}

/**
 * @brief 移动到指定位置
 * @param position 目标位置
 * @return 是否成功
 */
bool PenetrationMechanismStateMachine::moveToPosition(Position position) {
    if (isDebugMode()) {
        Position oldPosition = m_currentPosition;
        double oldValue = m_currentPositionValue;
        m_currentPosition = position;
        m_currentPositionValue = m_positionValues[position];
        
        QString oldPosName;
        QString newPosName;
        switch (oldPosition) {
            case POSITION_A: oldPosName = "A (最底部)"; break;
            case POSITION_A1: oldPosName = "A1 (断开/连接位置)"; break;
            case POSITION_B1: oldPosName = "B1 (钻具安装完成)"; break;
            case POSITION_B2: oldPosName = "B2 (钻管安装完成)"; break;
            case POSITION_C1: oldPosName = "C1 (钻具中间安装)"; break;
            case POSITION_C2: oldPosName = "C2 (钻管中间安装)"; break;
            case POSITION_D: oldPosName = "D (最顶部)"; break;
        }
        switch (position) {
            case POSITION_A: newPosName = "A (最底部)"; break;
            case POSITION_A1: newPosName = "A1 (断开/连接位置)"; break;
            case POSITION_B1: newPosName = "B1 (钻具安装完成)"; break;
            case POSITION_B2: newPosName = "B2 (钻管安装完成)"; break;
            case POSITION_C1: newPosName = "C1 (钻具中间安装)"; break;
            case POSITION_C2: newPosName = "C2 (钻管中间安装)"; break;
            case POSITION_D: newPosName = "D (最顶部)"; break;
        }
        
        logInfo(QString("[调试模式] 进给电机(ID=%1) 移动: %2 → %3 (位置值: %4 → %5)")
            .arg(PENETRATION_MOTOR_ID)
            .arg(oldPosName)
            .arg(newPosName)
            .arg(oldValue)
            .arg(m_currentPositionValue));
        return true;
    }
    
    if (!m_motionController) {
        logError("移动失败：未设置运动控制器");
        return false;
    }
    
    if (position < POSITION_A || position > POSITION_D) {
        logError(QString("无效的进给位置: %1").arg(position));
        return false;
    }
    
    // 根据位置设置不同的速度
    double speed;
    if (position == POSITION_A) {
        // 钻进速度
        speed = 10.0;  // v1
    } else if (position == POSITION_D) {
        // 空行程速度
        speed = 50.0;  // v3
    } else {
        // 钻管接卸速度
        speed = 20.0;  // v2
    }
    
    // 设置电机速度
    m_motionController->setMotorParameter(PENETRATION_MOTOR_ID, "Vel", speed);
    
    // 移动到目标位置
    double targetPosition = m_positionValues[position];
    if (!m_motionController->moveMotorAbsolute(PENETRATION_MOTOR_ID, targetPosition)) {
        logError(QString("移动到位置 %1 失败").arg(position));
        return false;
    }
    
    m_currentPosition = position;
    m_currentPositionValue = targetPosition;
    
    QString positionName;
    switch (position) {
        case POSITION_A: positionName = "A (最底部)"; break;
        case POSITION_A1: positionName = "A1 (断开/连接位置)"; break;
        case POSITION_B1: positionName = "B1 (钻具安装完成)"; break;
        case POSITION_B2: positionName = "B2 (钻管安装完成)"; break;
        case POSITION_C1: positionName = "C1 (钻具中间安装)"; break;
        case POSITION_C2: positionName = "C2 (钻管中间安装)"; break;
        case POSITION_D: positionName = "D (最顶部)"; break;
    }
    
    logInfo(QString("进给机构移动到位置: %1").arg(positionName));
    return true;
}

/**
 * @brief 按增量移动
 * @param increment 增量值
 * @return 是否成功
 */
bool PenetrationMechanismStateMachine::moveByIncrement(double increment) {
    if (!m_motionController) {
        logError("增量移动失败：未设置运动控制器");
        return false;
    }
    
    // 移动指定增量
    if (!m_motionController->moveMotorRelative(PENETRATION_MOTOR_ID, increment)) {
        logError(QString("增量移动 %1 失败").arg(increment));
        return false;
    }
    
    // 更新当前位置值
    m_currentPositionValue += increment;
    
    logInfo(QString("进给机构增量移动: %1").arg(increment));
    return true;
}

/**
 * @brief 获取当前位置
 * @return 当前位置枚举
 */
PenetrationMechanismStateMachine::Position PenetrationMechanismStateMachine::getCurrentPosition() const {
    return m_currentPosition;
}

/**
 * @brief 获取当前位置值
 * @return 当前位置值
 */
double PenetrationMechanismStateMachine::getCurrentPositionValue() const {
    return m_currentPositionValue;
}

//================ ClampMechanismStateMachine 实现 ================

/**
 * @brief 下夹紧机构状态机构造函数
 */
ClampMechanismStateMachine::ClampMechanismStateMachine(QObject *parent)
    : ComponentStateMachine("下夹紧机构", parent)
    , m_clampState(OPEN)
{
    // 初始化下夹紧机构状态机
}

/**
 * @brief 下夹紧机构状态机析构函数
 */
ClampMechanismStateMachine::~ClampMechanismStateMachine() {
    // 清理资源
}

/**
 * @brief 初始化下夹紧机构
 */
bool ClampMechanismStateMachine::initialize() {
    if (isDebugMode()) {
        logInfo(QString("[调试模式] %1初始化").arg(getComponentName()));
        m_clampState = OPEN;  // 初始化为张开状态
        return true;
    }
    
    if (!m_motionController) {
        logError("初始化失败：未设置运动控制器");
        return false;
    }
    
    // 设置夹紧电机参数
    m_motionController->setMotorParameter(CLAMP_MOTOR_ID, "Atype", 66);
    m_motionController->setMotorParameter(CLAMP_MOTOR_ID, "EN", 1);
    m_motionController->setMotorParameter(CLAMP_MOTOR_ID, "Vel", 5.0);
    
    // 设置为张开状态
    return setClampState(OPEN);
}

/**
 * @brief 重置下夹紧机构
 */
void ClampMechanismStateMachine::reset() {
    setClampState(OPEN);
}

/**
 * @brief 设置夹紧状态
 * @param state 目标状态
 * @return 是否成功
 */
bool ClampMechanismStateMachine::setClampState(ClampState state) {
    if (isDebugMode()) {
        ClampState oldState = m_clampState;
        m_clampState = state;
        
        QString oldStateName;
        QString newStateName;
        double oldPosition;
        double newPosition;
        
        switch (oldState) {
            case OPEN: 
                oldStateName = "张开"; 
                oldPosition = 0.0;
                break;
            case LOOSE: 
                oldStateName = "夹紧未成功"; 
                oldPosition = 50.0;
                break;
            case TIGHT: 
                oldStateName = "夹紧成功"; 
                oldPosition = 100.0;
                break;
        }
        
        switch (state) {
            case OPEN: 
                newStateName = "张开"; 
                newPosition = 0.0;
                break;
            case LOOSE: 
                newStateName = "夹紧未成功"; 
                newPosition = 50.0;
                break;
            case TIGHT: 
                newStateName = "夹紧成功"; 
                newPosition = 100.0;
                break;
        }
        
        logInfo(QString("[调试模式] 下夹紧电机(ID=%1) 移动: %2mm → %3mm (%4 → %5)")
            .arg(CLAMP_MOTOR_ID)
            .arg(oldPosition)
            .arg(newPosition)
            .arg(oldStateName)
            .arg(newStateName));
        return true;
    }
    
    if (!m_motionController) {
        logError("设置夹紧状态失败：未设置运动控制器");
        return false;
    }
    
    // 根据状态设置不同的位置
    double position;
    switch (state) {
        case OPEN:
            position = 0.0;
            break;
        case LOOSE:
            position = 50.0;
            break;
        case TIGHT:
            position = 100.0;
            break;
        default:
            logError(QString("无效的夹紧状态: %1").arg(state));
            return false;
    }
    
    // 移动到目标位置
    if (!m_motionController->moveMotorAbsolute(CLAMP_MOTOR_ID, position)) {
        logError(QString("设置夹紧状态 %1 失败").arg(state));
        return false;
    }
    
    m_clampState = state;
    
    QString stateName;
    switch (state) {
        case OPEN: stateName = "张开"; break;
        case LOOSE: stateName = "夹紧未成功"; break;
        case TIGHT: stateName = "夹紧成功"; break;
    }
    
    logInfo(QString("下夹紧机构状态设置为: %1").arg(stateName));
    return true;
}

/**
 * @brief 获取当前夹紧状态
 * @return 夹紧状态
 */
ClampMechanismStateMachine::ClampState ClampMechanismStateMachine::getClampState() const {
    return m_clampState;
}

//================ ConnectionMechanismStateMachine 实现 ================

/**
 * @brief 对接机构状态机构造函数
 */
ConnectionMechanismStateMachine::ConnectionMechanismStateMachine(QObject *parent)
    : ComponentStateMachine("对接机构", parent)
    , m_isExtended(false)
{
    // 初始化对接机构状态机
}

/**
 * @brief 对接机构状态机析构函数
 */
ConnectionMechanismStateMachine::~ConnectionMechanismStateMachine() {
    // 清理资源
}

/**
 * @brief 初始化对接机构
 */
bool ConnectionMechanismStateMachine::initialize() {
    if (isDebugMode()) {
        logInfo(QString("[调试模式] %1初始化").arg(getComponentName()));
        m_isExtended = false;  // 初始化为收回状态
        return true;
    }
    
    if (!m_motionController) {
        logError("初始化失败：未设置运动控制器");
        return false;
    }
    
    // 设置对接电机参数
    m_motionController->setMotorParameter(CONNECTION_MOTOR_ID, "Atype", 66);
    m_motionController->setMotorParameter(CONNECTION_MOTOR_ID, "EN", 1);
    m_motionController->setMotorParameter(CONNECTION_MOTOR_ID, "Vel", 5.0);
    
    // 设置为收回状态
    return setConnectionState(false);
}

/**
 * @brief 重置对接机构
 */
void ConnectionMechanismStateMachine::reset() {
    setConnectionState(false);
}

/**
 * @brief 设置对接状态
 * @param extended 目标状态 (true=推出, false=收回)
 * @return 是否成功
 */
bool ConnectionMechanismStateMachine::setConnectionState(bool extended) {
    if (isDebugMode()) {
        bool oldState = m_isExtended;
        m_isExtended = extended;
        logInfo(QString("[调试模式] 对接电机(ID=%1) 移动: %2mm → %3mm (%4 → %5)")
            .arg(CONNECTION_MOTOR_ID)
            .arg(oldState ? 100.0 : 0.0)
            .arg(extended ? 100.0 : 0.0)
            .arg(oldState ? "推出" : "收回")
            .arg(extended ? "推出" : "收回"));
        return true;
    }
    
    if (!m_motionController) {
        logError("设置对接状态失败：未设置运动控制器");
        return false;
    }
    
    // 设置位置：0表示收回，100表示推出
    double position = extended ? 100.0 : 0.0;
    
    // 移动到目标位置
    if (!m_motionController->moveMotorAbsolute(CONNECTION_MOTOR_ID, position)) {
        logError(QString("设置对接状态 %1 失败").arg(extended ? "推出" : "收回"));
        return false;
    }
    
    m_isExtended = extended;
    logInfo(QString("对接机构状态设置为: %1").arg(extended ? "推出" : "收回"));
    return true;
}

/**
 * @brief 获取当前对接状态
 * @return 对接状态 (true=推出, false=收回)
 */
bool ConnectionMechanismStateMachine::isExtended() const {
    return m_isExtended;
}