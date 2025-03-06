#include "inc/autodrilling.h"
#include "inc/DrillingParameters.h"
#include <QDebug>

// 状态机状态名称常量
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

// 电机ID常量定义
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
 * @brief 组件状态机基类构造函数
 */
ComponentStateMachine::ComponentStateMachine(const QString& name, QObject *parent)
    : StateMachine(parent)
    , m_componentName(name)
    , m_motionController(nullptr)
{
}

/**
 * @brief 组件状态机基类析构函数
 */
ComponentStateMachine::~ComponentStateMachine()
{
}

/**
 * @brief 获取组件名称
 */
QString ComponentStateMachine::getComponentName() const
{
    return m_componentName;
}

/**
 * @brief 设置运动控制器
 */
void ComponentStateMachine::setMotionController(MotionController* controller)
{
    m_motionController = controller;
}

/**
 * @brief 自动钻进状态机构造函数
 */
AutoDrillingStateMachine::AutoDrillingStateMachine(QObject *parent)
    : StateMachine(parent)
    , m_motionController(nullptr)
    , m_pipeCount(0)
    , m_percussionEnabled(false)
    , m_percussionFrequency(0.0)
    , m_drillMode(CONSTANT_SPEED)
    , m_drillParameter(0.0)
    , m_deltaThread(0.0)
    , m_deltaTool(0.0)
    , m_deltaPipe(0.0)
    , m_deltaDrill(0.0)
    , m_v1(0.01)  // 钻进速度
    , m_v2(0.05)  // 对接速度
    , m_v3(0.1)   // 空行程速度
    , m_omega(120.0)   // 正常钻进转速
    , m_omega_s(60.0)  // 对接转速
{
    // 创建子状态机
    m_storageUnit = std::make_shared<StorageUnitStateMachine>();
    m_robotArm = std::make_shared<RobotArmStateMachine>();
    m_drillingMechanism = std::make_shared<DrillingMechanismStateMachine>();
    m_penetrationMechanism = std::make_shared<PenetrationMechanismStateMachine>();
    m_clampMechanism = std::make_shared<ClampMechanismStateMachine>();
    m_connectionMechanism = std::make_shared<ConnectionMechanismStateMachine>();
    
    // 创建主状态机状态
    createMainStates();
}

/**
 * @brief 自动钻进状态机析构函数
 */
AutoDrillingStateMachine::~AutoDrillingStateMachine()
{
}

/**
 * @brief 初始化状态机
 */
void AutoDrillingStateMachine::initialize()
{
    // 加载参数
    DrillingParameters* params = DrillingParameters::getInstance();
    
    // 获取增量参数
    m_deltaThread = params->getParameter("增量", "螺旋接口标准高度增量").toDouble();
    m_deltaTool = params->getParameter("增量", "钻具螺旋接口增量").toDouble();
    m_deltaPipe = params->getParameter("增量", "钻管螺旋接口增量").toDouble();
    m_deltaDrill = params->getParameter("增量", "钻进行程距离").toDouble();
    
    // 获取速度参数
    m_v1 = params->getParameter("速度", "钻进速度").toDouble();
    m_v2 = params->getParameter("速度", "对接速度").toDouble();
    m_v3 = params->getParameter("速度", "空行程速度").toDouble();
    
    // 获取旋转速度参数
    m_omega = params->getParameter("旋转", "工作转速").toDouble();
    m_omega_s = params->getParameter("旋转", "对接转速").toDouble();
    
    // 初始化组件状态机
    m_storageUnit->initialize();
    m_robotArm->initialize();
    m_drillingMechanism->initialize();
    m_penetrationMechanism->initialize();
    m_clampMechanism->initialize();
    m_connectionMechanism->initialize();
    
    // 设置初始状态
    setCurrentState(STATE_SYSTEM_STARTUP);
    
    qDebug() << "自动钻进状态机已初始化";
}

/**
 * @brief 设置运动控制器
 */
void AutoDrillingStateMachine::setMotionController(MotionController* controller)
{
    m_motionController = controller;
    
    // 设置子状态机的运动控制器
    m_storageUnit->setMotionController(controller);
    m_robotArm->setMotionController(controller);
    m_drillingMechanism->setMotionController(controller);
    m_penetrationMechanism->setMotionController(controller);
    m_clampMechanism->setMotionController(controller);
    m_connectionMechanism->setMotionController(controller);
}

/**
 * @brief 设置钻进模式
 */
void AutoDrillingStateMachine::setDrillMode(DrillMode mode, double value)
{
    m_drillMode = mode;
    m_drillParameter = value;
    
    if (m_drillingMechanism) {
        m_drillingMechanism->setDrillMode(mode, value);
    }
    
    emit drillingModeChanged(mode, value);
}

/**
 * @brief 启用/禁用冲击功能
 */
void AutoDrillingStateMachine::enablePercussion(bool enable)
{
    m_percussionEnabled = enable;
    
    if (enable && m_drillingMechanism) {
        m_drillingMechanism->setPercussionFrequency(m_percussionFrequency);
    } else if (m_drillingMechanism) {
        m_drillingMechanism->setPercussionFrequency(0.0);
    }
    
    emit percussionStatusChanged(enable, m_percussionFrequency);
}

/**
 * @brief 设置冲击频率
 */
void AutoDrillingStateMachine::setPercussionFrequency(double frequency)
{
    m_percussionFrequency = frequency;
    
    if (m_percussionEnabled && m_drillingMechanism) {
        m_drillingMechanism->setPercussionFrequency(frequency);
    }
    
    emit percussionStatusChanged(m_percussionEnabled, frequency);
}

/**
 * @brief 开始操作
 */
void AutoDrillingStateMachine::startOperation()
{
    if (getCurrentStateName() == STATE_READY) {
        changeState(STATE_FIRST_TOOL_INSTALLATION);
    } else {
        qDebug() << "无法开始操作，状态机未就绪";
    }
}

/**
 * @brief 停止操作
 */
void AutoDrillingStateMachine::stopOperation()
{
    // 停止所有机构
    m_drillingMechanism->setRotationSpeed(0);
    m_drillingMechanism->setPercussionFrequency(0);
    
    // 将状态机转换到系统重置状态
    changeState(STATE_SYSTEM_RESET);
}

/**
 * @brief 暂停操作
 */
void AutoDrillingStateMachine::pauseOperation()
{
    // 实现暂停逻辑
    // 暂停钻进和冲击
    if (m_drillingMechanism) {
        m_drillingMechanism->setRotationSpeed(0);
        m_drillingMechanism->setPercussionFrequency(0);
    }
    
    // 暂停进给
    if (m_penetrationMechanism) {
        // 停止进给机构移动
        int currentPosition = m_penetrationMechanism->getCurrentPositionValue();
        m_penetrationMechanism->moveToPosition(currentPosition, 0);
    }
}

/**
 * @brief 恢复操作
 */
void AutoDrillingStateMachine::resumeOperation()
{
    // 实现恢复逻辑
    QString currentState = getCurrentStateName();
    
    // 根据当前状态恢复相应的操作
    if (currentState == STATE_FIRST_DRILLING || 
        currentState == STATE_PIPE_INSTALLATION_LOOP) {
        // 恢复钻进和冲击
        m_drillingMechanism->setRotationSpeed(m_omega);
        if (m_percussionEnabled) {
            m_drillingMechanism->setPercussionFrequency(m_percussionFrequency);
        }
    }
}

/**
 * @brief 获取当前钻管计数
 */
int AutoDrillingStateMachine::getCurrentPipeCount() const
{
    return m_pipeCount;
}

/**
 * @brief 钻管计数增加
 */
void AutoDrillingStateMachine::incrementPipeCount()
{
    m_pipeCount++;
    emit pipeCountChanged(m_pipeCount);
}

/**
 * @brief 钻管计数减少
 */
void AutoDrillingStateMachine::decrementPipeCount()
{
    if (m_pipeCount > 0) {
        m_pipeCount--;
        emit pipeCountChanged(m_pipeCount);
    }
}

/**
 * @brief 状态变更前的钩子方法
 */
bool AutoDrillingStateMachine::beforeStateChange(const QString& oldState, const QString& newState)
{
    qDebug() << "状态即将从" << oldState << "变更为" << newState;
    
    // 检查状态转换是否有效
    if (!m_motionController) {
        qDebug() << "运动控制器未设置，无法变更状态";
        return false;
    }
    
    // 在这里可以添加状态转换的先决条件检查
    
    return true;
}

/**
 * @brief 状态变更后的钩子方法
 */
void AutoDrillingStateMachine::afterStateChange(const QString& oldState, const QString& newState)
{
    qDebug() << "状态已从" << oldState << "变更为" << newState;
    
    emit currentStepChanged(newState);
    
    // 执行新状态对应的操作
    executeStateActions(newState);
}

/**
 * @brief 执行状态对应的操作
 */
void AutoDrillingStateMachine::executeStateActions(const QString& state)
{
    if (state == STATE_SYSTEM_STARTUP) {
        // 系统启动操作
        // 初始化所有电机
        if (m_motionController) {
            for (int i = 0; i <= 8; i++) {
                m_motionController->enableMotor(i, true);
            }
        }
    }
    else if (state == STATE_DEFAULT_POSITION) {
        // 默认位置操作
        // 将所有机构移动到默认位置
        m_robotArm->setRotationPosition(0);              // 机械手旋转到0度位置
        m_robotArm->setExtension(0);                     // 机械手缩回
        m_robotArm->setClamp(0);                         // 机械手松开
        m_drillingMechanism->setRotationSpeed(0);        // 钻进旋转停止
        m_drillingMechanism->setPercussionFrequency(0);  // 冲击停止
        m_clampMechanism->setClampState(ClampMechanismStateMachine::OPEN);  // 下夹紧机构松开
        m_connectionMechanism->setConnectionState(false); // 对接机构收回
        m_penetrationMechanism->moveToPosition(PenetrationMechanismStateMachine::POSITION_A); // 进给机构回零
    }
    else if (state == STATE_FIRST_TOOL_INSTALLATION) {
        // 首根钻具安装流程
        executeFirstToolInstallation();
    }
    else if (state == STATE_FIRST_DRILLING) {
        // 首次钻进流程
        executeFirstDrilling();
    }
    else if (state == STATE_PIPE_INSTALLATION_LOOP) {
        // 钻管安装循环流程
        executePipeInstallationLoop();
    }
    else if (state == STATE_PIPE_REMOVAL_LOOP) {
        // 钻管拆卸循环流程
        executePipeRemovalLoop();
    }
    else if (state == STATE_FIRST_TOOL_RECOVERY) {
        // 首根钻具回收流程
        executeFirstToolRecovery();
    }
    else if (state == STATE_SYSTEM_RESET) {
        // 系统重置流程
        // 停止所有电机
        m_drillingMechanism->setRotationSpeed(0);
        m_drillingMechanism->setPercussionFrequency(0);
        
        // 所有机构回到安全位置
        m_robotArm->setRotationPosition(0);
        m_robotArm->setExtension(0);
        m_robotArm->setClamp(0);
        m_penetrationMechanism->moveToPosition(PenetrationMechanismStateMachine::POSITION_A);
        m_clampMechanism->setClampState(ClampMechanismStateMachine::OPEN);
        m_connectionMechanism->setConnectionState(false);
        
        // 重置计数器
        m_pipeCount = 0;
        emit pipeCountChanged(m_pipeCount);
        
        // 完成重置后转到默认位置状态
        changeState(STATE_DEFAULT_POSITION);
    }
}

/**
 * @brief 创建主状态机状态
 */
void AutoDrillingStateMachine::createMainStates()
{
    // 添加主状态
    addState(STATE_SYSTEM_STARTUP);
    addState(STATE_DEFAULT_POSITION);
    addState(STATE_READY);
    addState(STATE_FIRST_TOOL_INSTALLATION);
    addState(STATE_FIRST_DRILLING);
    addState(STATE_PIPE_INSTALLATION_LOOP);
    addState(STATE_PIPE_REMOVAL_LOOP);
    addState(STATE_FIRST_TOOL_RECOVERY);
    addState(STATE_SYSTEM_RESET);
    addState(STATE_OPERATION_COMPLETE);
    
    // 定义允许的状态转换
    addTransition(STATE_SYSTEM_STARTUP, STATE_DEFAULT_POSITION);
    addTransition(STATE_DEFAULT_POSITION, STATE_READY);
    addTransition(STATE_READY, STATE_FIRST_TOOL_INSTALLATION);
    addTransition(STATE_FIRST_TOOL_INSTALLATION, STATE_FIRST_DRILLING);
    addTransition(STATE_FIRST_DRILLING, STATE_PIPE_INSTALLATION_LOOP);
    addTransition(STATE_PIPE_INSTALLATION_LOOP, STATE_PIPE_INSTALLATION_LOOP); // 循环
    addTransition(STATE_PIPE_INSTALLATION_LOOP, STATE_PIPE_REMOVAL_LOOP);
    addTransition(STATE_PIPE_REMOVAL_LOOP, STATE_PIPE_REMOVAL_LOOP); // 循环
    addTransition(STATE_PIPE_REMOVAL_LOOP, STATE_FIRST_TOOL_RECOVERY);
    addTransition(STATE_FIRST_TOOL_RECOVERY, STATE_SYSTEM_RESET);
    addTransition(STATE_SYSTEM_RESET, STATE_DEFAULT_POSITION);
    addTransition(STATE_DEFAULT_POSITION, STATE_OPERATION_COMPLETE);
    addTransition(STATE_OPERATION_COMPLETE, STATE_READY);
    
    // 允许从任何状态进入系统重置状态
    for (const QString& state : getStates()) {
        if (state != STATE_SYSTEM_RESET) {
            addTransition(state, STATE_SYSTEM_RESET);
        }
    }
}

/**
 * @brief 执行首根钻具安装流程
 */
void AutoDrillingStateMachine::executeFirstToolInstallation()
{
    DrillingParameters* params = DrillingParameters::getInstance();
    
    // 1. 机械手移动到存储区
    m_robotArm->setRotationPosition(1); // 90度 (存储区位置)
    
    // 进给机构移动到工作位置
    double workPosition = params->getParameter("Penetration", "WORK_POSITION").toDouble();
    m_penetrationMechanism->moveToPosition(workPosition, m_v3); // 空行程速度
    
    // 2. 机械手夹持钻具
    m_robotArm->setExtension(1); // 伸出
    m_robotArm->setClamp(1); // 夹紧
    m_robotArm->setExtension(0); // 缩回
    
    // 3. 机械手移动到钻台
    m_robotArm->setRotationPosition(0); // 0度 (钻台位置)
    m_robotArm->setExtension(1); // 伸出
    
    // 4. 进给机构移动到钻具安装起始位置
    double toolInstallStart = params->getParameter("Penetration", "TOOL_INSTALL_START").toDouble();
    m_penetrationMechanism->moveToPosition(toolInstallStart, m_v3); // 空行程速度
    
    // 5. 执行连接操作
    m_drillingMechanism->setRotationSpeed(m_omega_s); // 低速对接旋转
    
    double toolInstallEnd = params->getParameter("Penetration", "TOOL_INSTALL_END").toDouble();
    m_penetrationMechanism->moveToPosition(toolInstallEnd, m_v2); // 对接速度
    
    // 停止旋转
    m_drillingMechanism->setRotationSpeed(0);
    
    // 6. 对接机构伸出
    double connExtension = params->getParameter("ClampConnection", "CONNECTION_EXTENSION").toDouble();
    m_connectionMechanism->setConnectionState(true); // 伸出对接机构
    
    // 7. 机械手松开
    m_robotArm->setClamp(0); // 松开
    m_robotArm->setExtension(0); // 缩回
    
    // 安装完成, 转到首次钻进状态
    changeState(STATE_FIRST_DRILLING);
}

/**
 * @brief 执行首次钻进流程
 */
void AutoDrillingStateMachine::executeFirstDrilling()
{
    DrillingParameters* params = DrillingParameters::getInstance();
    
    // 1. 钻进电机、冲击电机和进给机构同时启动
    m_drillingMechanism->setRotationSpeed(m_omega); // 工作转速
    m_drillingMechanism->setPercussionFrequency(params->getParameter("冲击", "工作频率").toDouble()); // 工作频率
    
    double toolInstallEnd = params->getParameter("Penetration", "TOOL_INSTALL_END").toDouble();
    m_penetrationMechanism->moveToPosition(0, m_v1); // 钻进速度
    
    // 2. 监测钻进参数 (实际中需要添加钻进参数监测代码)
    
    // 3. 达到预定深度后停止
    m_drillingMechanism->setRotationSpeed(0);
    m_drillingMechanism->setPercussionFrequency(0);
    
    // 4. 下夹紧机构夹紧
    m_clampMechanism->setClampState(ClampMechanismStateMachine::TIGHT);
    
    // 5. 对接机构回收
    m_connectionMechanism->setConnectionState(false);
    
    // 6. 执行断开操作
    m_drillingMechanism->setRotationSpeed(-m_omega_s); // 反向低速旋转
    
    double disconnectPosition = params->getParameter("Penetration", "DISCONNECT_POSITION").toDouble();
    m_penetrationMechanism->moveToPosition(disconnectPosition, m_v2); // 对接速度
    
    // 停止旋转
    m_drillingMechanism->setRotationSpeed(0);
    
    // 7. 进给机构上升
    double standbyPosition = params->getParameter("Penetration", "STANDBY_POSITION").toDouble();
    m_penetrationMechanism->moveToPosition(standbyPosition, m_v3); // 空行程速度
    
    // 首次钻进完成, 增加计数并转到钻管安装循环状态
    incrementPipeCount();
    changeState(STATE_PIPE_INSTALLATION_LOOP);
}

/**
 * @brief 执行钻管安装循环流程
 */
void AutoDrillingStateMachine::executePipeInstallationLoop()
{
    DrillingParameters* params = DrillingParameters::getInstance();
    
    // 1. 机械手取钻管
    m_robotArm->setRotationPosition(1); // 90度 (存储区位置)
    m_robotArm->setExtension(1); // 伸出
    m_robotArm->setClamp(1); // 夹紧
    m_robotArm->setExtension(0); // 缩回
    m_robotArm->setRotationPosition(0); // 0度 (钻台位置)
    m_robotArm->setExtension(1); // 伸出
    
    // 2. 进给机构下降
    double pipeInstallStart = params->getParameter("Penetration", "PIPE_INSTALL_START").toDouble();
    m_penetrationMechanism->moveToPosition(pipeInstallStart, m_v3); // 空行程速度
    
    // 3. 执行连接操作
    m_drillingMechanism->setRotationSpeed(m_omega_s); // 低速对接旋转
    
    double pipeInstallMid = params->getParameter("Penetration", "PIPE_INSTALL_MID").toDouble();
    m_penetrationMechanism->moveToPosition(pipeInstallMid, m_v2); // 对接速度
    
    // 停止旋转
    m_drillingMechanism->setRotationSpeed(0);
    
    // 4. 对接机构推出
    m_connectionMechanism->setConnectionState(true);
    
    // 5. 机械手收回
    m_robotArm->setClamp(0); // 松开
    m_robotArm->setExtension(0); // 缩回
    
    // 6. 钻管之间的对接
    m_drillingMechanism->setRotationSpeed(m_omega_s); // 低速对接旋转
    
    double pipeInstallEnd = params->getParameter("Penetration", "PIPE_INSTALL_END").toDouble();
    m_penetrationMechanism->moveToPosition(pipeInstallEnd, m_v2); // 对接速度
    
    // 停止旋转
    m_drillingMechanism->setRotationSpeed(0);
    
    // 7. 下夹紧机构松开
    m_clampMechanism->setClampState(ClampMechanismStateMachine::OPEN);
    
    // 8. 钻机钻进
    m_drillingMechanism->setRotationSpeed(m_omega); // 工作转速
    m_drillingMechanism->setPercussionFrequency(params->getParameter("冲击", "工作频率").toDouble()); // 工作频率
    
    m_penetrationMechanism->moveToPosition(0, m_v1); // 钻进速度
    
    // 9. 下夹紧机构夹紧
    m_clampMechanism->setClampState(ClampMechanismStateMachine::TIGHT);
    
    // 停止钻进和冲击
    m_drillingMechanism->setRotationSpeed(0);
    m_drillingMechanism->setPercussionFrequency(0);
    
    // 10. 执行断开操作
    m_connectionMechanism->setConnectionState(false);
    m_drillingMechanism->setRotationSpeed(-m_omega_s); // 反向低速旋转
    
    double disconnectPosition = params->getParameter("Penetration", "DISCONNECT_POSITION").toDouble();
    m_penetrationMechanism->moveToPosition(disconnectPosition, m_v2); // 对接速度
    
    // 停止旋转
    m_drillingMechanism->setRotationSpeed(0);
    
    // 11. 进给机构上升
    double standbyPosition = params->getParameter("Penetration", "STANDBY_POSITION").toDouble();
    m_penetrationMechanism->moveToPosition(standbyPosition, m_v3); // 空行程速度
    
    // 钻管安装完成, 增加计数
    incrementPipeCount();
    
    // 根据钻管数量决定下一个状态
    if (m_pipeCount >= 5) { // 假设最多安装5根钻管
        changeState(STATE_PIPE_REMOVAL_LOOP);
    } else {
        // 继续安装下一根钻管
        changeState(STATE_PIPE_INSTALLATION_LOOP);
    }
}

/**
 * @brief 执行钻管拆卸循环流程
 */
void AutoDrillingStateMachine::executePipeRemovalLoop()
{
    DrillingParameters* params = DrillingParameters::getInstance();
    
    // 1. 进给机构下降
    double disconnectPosition = params->getParameter("Penetration", "DISCONNECT_POSITION").toDouble();
    m_penetrationMechanism->moveToPosition(disconnectPosition, m_v3); // 空行程速度
    
    // 执行连接操作
    m_drillingMechanism->setRotationSpeed(m_omega_s); // 低速对接旋转
    m_penetrationMechanism->moveToPosition(0, m_v2); // 对接速度
    
    // 停止旋转
    m_drillingMechanism->setRotationSpeed(0);
    
    // 对接机构推出
    m_connectionMechanism->setConnectionState(true);
    
    // 2. 下夹紧机构松开
    m_clampMechanism->setClampState(ClampMechanismStateMachine::OPEN);
    
    // 3. 进给机构上升
    double pipeRemovalStart = params->getParameter("Penetration", "PIPE_REMOVAL_START").toDouble();
    m_penetrationMechanism->moveToPosition(pipeRemovalStart, m_v3); // 空行程速度
    
    // 4. 下夹紧机构夹紧
    m_clampMechanism->setClampState(ClampMechanismStateMachine::TIGHT);
    
    // 5. 断开钻管间连接
    m_drillingMechanism->setRotationSpeed(-m_omega_s); // 反向低速旋转
    
    double pipeRemovalMid = params->getParameter("Penetration", "PIPE_REMOVAL_MID").toDouble();
    m_penetrationMechanism->moveToPosition(pipeRemovalMid, m_v2); // 对接速度
    
    // 停止旋转
    m_drillingMechanism->setRotationSpeed(0);
    
    // 6. 机械手抓取钻管
    m_robotArm->setExtension(1); // 伸出
    m_robotArm->setClamp(1); // 夹紧
    
    // 7. 对接机构回收
    m_connectionMechanism->setConnectionState(false);
    
    // 8. 断开钻管与钻进机构连接
    m_drillingMechanism->setRotationSpeed(-m_omega_s); // 反向低速旋转
    
    double pipeRemovalEnd = params->getParameter("Penetration", "PIPE_REMOVAL_END").toDouble();
    m_penetrationMechanism->moveToPosition(pipeRemovalEnd, m_v2); // 对接速度
    
    // 停止旋转
    m_drillingMechanism->setRotationSpeed(0);
    
    // 9. 存储机构旋转到空位
    int storagePosition = m_pipeCount - 1; // 存储位置
    m_storageUnit->rotateToPosition(storagePosition);
    
    // 10. 机械手回收
    m_robotArm->setExtension(0); // 缩回
    m_robotArm->setRotationPosition(1); // 90度 (存储区位置)
    m_robotArm->setExtension(1); // 伸出
    m_robotArm->setClamp(0); // 松开
    m_robotArm->setExtension(0); // 缩回
    m_robotArm->setRotationPosition(0); // 0度 (钻台位置)
    
    // 11. 进给机构上升到待机位置
    double standbyPosition = params->getParameter("Penetration", "STANDBY_POSITION").toDouble();
    m_penetrationMechanism->moveToPosition(standbyPosition, m_v3); // 空行程速度
    
    // 钻管拆卸完成, 减少计数
    decrementPipeCount();
    
    // 根据钻管数量决定下一个状态
    if (m_pipeCount > 1) {
        // 继续拆卸下一根钻管
        changeState(STATE_PIPE_REMOVAL_LOOP);
    } else {
        // 转到首根钻具回收状态
        changeState(STATE_FIRST_TOOL_RECOVERY);
    }
}

/**
 * @brief 执行首根钻具回收流程
 */
void AutoDrillingStateMachine::executeFirstToolRecovery()
{
    DrillingParameters* params = DrillingParameters::getInstance();
    
    // 1. 进给机构下降
    double disconnectPosition = params->getParameter("Penetration", "DISCONNECT_POSITION").toDouble();
    m_penetrationMechanism->moveToPosition(disconnectPosition, m_v3); // 空行程速度
    
    // 执行连接操作
    m_drillingMechanism->setRotationSpeed(m_omega_s); // 低速对接旋转
    m_penetrationMechanism->moveToPosition(0, m_v2); // 对接速度
    
    // 停止旋转
    m_drillingMechanism->setRotationSpeed(0);
    
    // 对接机构推出
    m_connectionMechanism->setConnectionState(true);
    
    // 2. 下夹紧松开
    m_clampMechanism->setClampState(ClampMechanismStateMachine::OPEN);
    
    // 3. 进给机构上升
    double toolRecoveryEnd = params->getParameter("Penetration", "TOOL_RECOVERY_END").toDouble();
    m_penetrationMechanism->moveToPosition(toolRecoveryEnd, m_v3); // 空行程速度
    
    // 4. 机械手抓取钻具
    m_robotArm->setExtension(1); // 伸出
    m_robotArm->setClamp(1); // 夹紧
    
    // 5. 对接机构回收
    m_connectionMechanism->setConnectionState(false);
    
    // 6. 断开钻具与钻进机构连接
    m_drillingMechanism->setRotationSpeed(-m_omega_s); // 反向低速旋转
    m_penetrationMechanism->moveToPosition(toolRecoveryEnd + 30, m_v2); // 对接速度
    
    // 停止旋转
    m_drillingMechanism->setRotationSpeed(0);
    
    // 7. 存储机构旋转到空位
    int storagePosition = 0; // 第一个存储位置
    m_storageUnit->rotateToPosition(storagePosition);
    
    // 8. 机械手回收
    m_robotArm->setExtension(0); // 缩回
    m_robotArm->setRotationPosition(1); // 90度 (存储区位置)
    m_robotArm->setExtension(1); // 伸出
    m_robotArm->setClamp(0); // 松开
    m_robotArm->setExtension(0); // 缩回
    m_robotArm->setRotationPosition(0); // 0度 (钻台位置)
    
    // 9. 进给机构下降到待机位置
    m_penetrationMechanism->moveToPosition(0, m_v3); // 空行程速度
    
    // 首根钻具回收完成, 减少计数
    decrementPipeCount();
    
    // 转到系统重置状态
    changeState(STATE_SYSTEM_RESET);
}

// -----------------------------------------------------------------------------
// 各组件状态机实现
// -----------------------------------------------------------------------------

/**
 * @brief 存储单元状态机构造函数
 */
StorageUnitStateMachine::StorageUnitStateMachine(QObject *parent)
    : ComponentStateMachine("存储单元", parent)
    , m_currentPosition(0)
{
}

/**
 * @brief 存储单元状态机析构函数
 */
StorageUnitStateMachine::~StorageUnitStateMachine()
{
}

/**
 * @brief 初始化存储单元
 */
bool StorageUnitStateMachine::initialize()
{
    // 添加状态
    for (int i = 0; i < MAX_POSITIONS; i++) {
        addState(QString::number(i));
    }
    
    // 添加所有可能的转换
    for (int i = 0; i < MAX_POSITIONS; i++) {
        for (int j = 0; j < MAX_POSITIONS; j++) {
            if (i != j) {
                addTransition(QString::number(i), QString::number(j));
            }
        }
    }
    
    // 设置初始状态
    setCurrentState("0");
    m_currentPosition = 0;
    
    return true;
}

/**
 * @brief 重置存储单元
 */
void StorageUnitStateMachine::reset()
{
    if (m_motionController) {
        m_motionController->moveMotor(STORAGE_MOTOR_ID, MotorMode::POSITION, 0);
    }
    
    setCurrentState("0");
    m_currentPosition = 0;
}

/**
 * @brief 旋转到指定位置
 */
bool StorageUnitStateMachine::rotateToPosition(int position)
{
    if (position < 0 || position >= MAX_POSITIONS) {
        qDebug() << "存储单元位置无效:" << position;
        return false;
    }
    
    if (m_motionController) {
        m_motionController->moveMotor(STORAGE_MOTOR_ID, MotorMode::POSITION, position * (360 / MAX_POSITIONS));
    }
    
    if (changeState(QString::number(position))) {
        m_currentPosition = position;
        return true;
    }
    
    return false;
}

/**
 * @brief 获取当前位置
 */
int StorageUnitStateMachine::getCurrentPosition() const
{
    return m_currentPosition;
}

/**
 * @brief 机械手状态机构造函数
 */
RobotArmStateMachine::RobotArmStateMachine(QObject *parent)
    : ComponentStateMachine("机械手", parent)
    , m_rotationPosition(0)
    , m_extension(0)
    , m_clamp(0)
    , m_extensionPosition(0)
{
}

/**
 * @brief 机械手状态机析构函数
 */
RobotArmStateMachine::~RobotArmStateMachine()
{
}

/**
 * @brief 初始化机械手
 */
bool RobotArmStateMachine::initialize()
{
    // 旋转位置状态 (0=对准钻进机构, 1=对准存储单元)
    addState("rotation_0");
    addState("rotation_1");
    addTransition("rotation_0", "rotation_1");
    addTransition("rotation_1", "rotation_0");
    
    // 伸缩状态 (0=缩回, 1=伸出)
    addState("extension_0");
    addState("extension_1");
    addTransition("extension_0", "extension_1");
    addTransition("extension_1", "extension_0");
    
    // 夹持状态 (0=未夹持, 1=夹紧)
    addState("clamp_0");
    addState("clamp_1");
    addTransition("clamp_0", "clamp_1");
    addTransition("clamp_1", "clamp_0");
    
    // 设置初始状态
    setCurrentState("rotation_0");  // 对准钻进机构
    setCurrentState("extension_0"); // 缩回
    setCurrentState("clamp_0");     // 未夹持
    
    m_rotationPosition = 0;
    m_extension = 0;
    m_clamp = 0;
    m_extensionPosition = 0;
    
    return true;
}

/**
 * @brief 重置机械手
 */
void RobotArmStateMachine::reset()
{
    // 机械手旋转回0位置
    if (m_motionController) {
        m_motionController->moveMotor(ROTATION_MOTOR_ID, MotorMode::POSITION, 0);
        m_motionController->moveMotor(EXTENSION_MOTOR_ID, MotorMode::POSITION, 0);
        m_motionController->moveMotor(CLAMP_MOTOR_ID, MotorMode::POSITION, 0);
    }
    
    setCurrentState("rotation_0");
    setCurrentState("extension_0");
    setCurrentState("clamp_0");
    
    m_rotationPosition = 0;
    m_extension = 0;
    m_clamp = 0;
    m_extensionPosition = 0;
}

/**
 * @brief 设置旋转位置
 */
bool RobotArmStateMachine::setRotationPosition(int position)
{
    DrillingParameters* params = DrillingParameters::getInstance();
    
    if (position < 0 || position > 1) {
        qDebug() << "机械手旋转位置无效:" << position;
        return false;
    }
    
    if (m_motionController) {
        double angle = 0;
        if (position == 0) {
            angle = params->getParameter("RobotPosition", "ROTATION_DRILL").toDouble();
        } else {
            angle = params->getParameter("RobotPosition", "ROTATION_STORAGE").toDouble();
        }
        
        m_motionController->moveMotor(ROTATION_MOTOR_ID, MotorMode::POSITION, angle);
    }
    
    if (changeState(QString("rotation_%1").arg(position))) {
        m_rotationPosition = position;
        return true;
    }
    
    return false;
}

/**
 * @brief 设置伸缩状态
 */
bool RobotArmStateMachine::setExtension(int extension)
{
    DrillingParameters* params = DrillingParameters::getInstance();
    
    if (extension < 0 || extension > 1) {
        qDebug() << "机械手伸缩状态无效:" << extension;
        return false;
    }
    
    if (m_motionController) {
        double position = 0;
        if (extension == 0) {
            position = params->getParameter("RobotPosition", "EXTENSION_RETRACTED").toDouble();
        } else {
            // 根据当前旋转位置选择合适的伸出距离
            if (m_rotationPosition == 0) { // 钻台位置
                position = params->getParameter("RobotPosition", "EXTENSION_DRILL").toDouble();
            } else { // 存储区位置
                position = params->getParameter("RobotPosition", "EXTENSION_STORAGE").toDouble();
            }
        }
        
        m_motionController->moveMotor(EXTENSION_MOTOR_ID, MotorMode::POSITION, position);
        m_extensionPosition = position;
    }
    
    if (changeState(QString("extension_%1").arg(extension))) {
        m_extension = extension;
        return true;
    }
    
    return false;
}

/**
 * @brief 设置夹持状态
 */
bool RobotArmStateMachine::setClamp(int clamp)
{
    DrillingParameters* params = DrillingParameters::getInstance();
    
    if (clamp < 0 || clamp > 1) {
        qDebug() << "机械手夹持状态无效:" << clamp;
        return false;
    }
    
    if (m_motionController) {
        double position = 0;
        if (clamp == 0) {
            position = params->getParameter("RobotPosition", "CLAMP_RELEASED").toDouble();
        } else {
            position = params->getParameter("RobotPosition", "CLAMP_ENGAGED").toDouble();
        }
        
        m_motionController->moveMotor(CLAMP_MOTOR_ID, MotorMode::POSITION, position);
    }
    
    if (changeState(QString("clamp_%1").arg(clamp))) {
        m_clamp = clamp;
        return true;
    }
    
    return false;
}

/**
 * @brief 获取旋转位置
 */
int RobotArmStateMachine::getRotationPosition() const
{
    return m_rotationPosition;
}

/**
 * @brief 获取伸缩状态
 */
int RobotArmStateMachine::getExtension() const
{
    return m_extension;
}

/**
 * @brief 获取夹持状态
 */
int RobotArmStateMachine::getClamp() const
{
    return m_clamp;
}

/**
 * @brief 钻进机构状态机构造函数
 */
DrillingMechanismStateMachine::DrillingMechanismStateMachine(QObject *parent)
    : ComponentStateMachine("钻进机构", parent)
    , m_rotationSpeed(0)
    , m_percussionFrequency(0)
    , m_connected(false)
    , m_drillMode(MotorMode::VELOCITY)
    , m_drillValue(0)
{
}

/**
 * @brief 钻进机构状态机析构函数
 */
DrillingMechanismStateMachine::~DrillingMechanismStateMachine()
{
}

/**
 * @brief 初始化钻进机构
 */
bool DrillingMechanismStateMachine::initialize()
{
    // 旋转速度状态
    addState("rotation_off");
    addState("rotation_on");
    addTransition("rotation_off", "rotation_on");
    addTransition("rotation_on", "rotation_off");
    
    // 冲击频率状态
    addState("percussion_off");
    addState("percussion_on");
    addTransition("percussion_off", "percussion_on");
    addTransition("percussion_on", "percussion_off");
    
    // 连接状态
    addState("connected_false");
    addState("connected_true");
    addTransition("connected_false", "connected_true");
    addTransition("connected_true", "connected_false");
    
    // 设置初始状态
    setCurrentState("rotation_off");
    setCurrentState("percussion_off");
    setCurrentState("connected_false");
    
    m_rotationSpeed = 0;
    m_percussionFrequency = 0;
    m_connected = false;
    
    return true;
}

/**
 * @brief 重置钻进机构
 */
void DrillingMechanismStateMachine::reset()
{
    // 停止所有运动
    if (m_motionController) {
        m_motionController->moveMotor(DRILL_MOTOR_ID, MotorMode::VELOCITY, 0);
        m_motionController->moveMotor(PERCUSSION_MOTOR_ID, MotorMode::VELOCITY, 0);
    }
    
    setCurrentState("rotation_off");
    setCurrentState("percussion_off");
    setCurrentState("connected_false");
    
    m_rotationSpeed = 0;
    m_percussionFrequency = 0;
    m_connected = false;
}

/**
 * @brief 设置旋转速度
 */
bool DrillingMechanismStateMachine::setRotationSpeed(double speed)
{
    if (m_motionController) {
        m_motionController->moveMotor(DRILL_MOTOR_ID, MotorMode::VELOCITY, speed);
    }
    
    if (speed == 0) {
        changeState("rotation_off");
    } else {
        changeState("rotation_on");
    }
    
    m_rotationSpeed = speed;
    return true;
}

/**
 * @brief 设置冲击频率
 */
bool DrillingMechanismStateMachine::setPercussionFrequency(double frequency)
{
    if (m_motionController) {
        m_motionController->moveMotor(PERCUSSION_MOTOR_ID, MotorMode::VELOCITY, frequency);
    }
    
    if (frequency == 0) {
        changeState("percussion_off");
    } else {
        changeState("percussion_on");
    }
    
    m_percussionFrequency = frequency;
    return true;
}

/**
 * @brief 设置连接状态
 */
bool DrillingMechanismStateMachine::setConnectionStatus(bool connected)
{
    if (connected) {
        changeState("connected_true");
    } else {
        changeState("connected_false");
    }
    
    m_connected = connected;
    return true;
}

/**
 * @brief 获取旋转速度
 */
double DrillingMechanismStateMachine::getRotationSpeed() const
{
    return m_rotationSpeed;
}

/**
 * @brief 获取冲击频率
 */
double DrillingMechanismStateMachine::getPercussionFrequency() const
{
    return m_percussionFrequency;
}

/**
 * @brief 获取连接状态
 */
bool DrillingMechanismStateMachine::getConnectionStatus() const
{
    return m_connected;
}

/**
 * @brief 设置钻进模式
 */
bool DrillingMechanismStateMachine::setDrillMode(int mode, double value)
{
    if (mode != MotorMode::VELOCITY && mode != MotorMode::TORQUE) {
        qDebug() << "无效的钻进模式:" << mode;
        return false;
    }
    
    m_drillMode = mode;
    m_drillValue = value;
    
    if (m_motionController && m_rotationSpeed != 0) {
        m_motionController->moveMotor(DRILL_MOTOR_ID, m_drillMode, value);
    }
    
    return true;
}

/**
 * @brief 进给机构状态机构造函数
 */
PenetrationMechanismStateMachine::PenetrationMechanismStateMachine(QObject *parent)
    : ComponentStateMachine("进给机构", parent)
    , m_currentPosition(POSITION_A)
    , m_currentPositionValue(0)
{
    // 初始化位置值
    m_positionValues[POSITION_A] = 0;       // 最底部位置
    m_positionValues[POSITION_A1] = 30;     // 断开/连接位置
    m_positionValues[POSITION_B1] = 580;    // 钻具安装位置
    m_positionValues[POSITION_B2] = 520;    // 钻管安装位置
    m_positionValues[POSITION_C1] = 610;    // 钻具中间位置
    m_positionValues[POSITION_C2] = 650;    // 钻管中间位置
    m_positionValues[POSITION_D] = 1315;    // 最顶部位置
}

/**
 * @brief 进给机构状态机析构函数
 */
PenetrationMechanismStateMachine::~PenetrationMechanismStateMachine()
{
}

/**
 * @brief 初始化进给机构
 */
bool PenetrationMechanismStateMachine::initialize()
{
    // 添加位置状态
    addState("position_a");
    addState("position_a1");
    addState("position_b1");
    addState("position_b2");
    addState("position_c1");
    addState("position_c2");
    addState("position_d");
    
    // 添加所有可能的转换
    addTransition("position_a", "position_a1");
    addTransition("position_a", "position_b1");
    addTransition("position_a", "position_b2");
    addTransition("position_a", "position_c1");
    addTransition("position_a", "position_c2");
    addTransition("position_a", "position_d");
    
    addTransition("position_a1", "position_a");
    addTransition("position_a1", "position_b1");
    addTransition("position_a1", "position_b2");
    addTransition("position_a1", "position_c1");
    addTransition("position_a1", "position_c2");
    addTransition("position_a1", "position_d");
    
    addTransition("position_b1", "position_a");
    addTransition("position_b1", "position_a1");
    addTransition("position_b1", "position_b2");
    addTransition("position_b1", "position_c1");
    addTransition("position_b1", "position_c2");
    addTransition("position_b1", "position_d");
    
    addTransition("position_b2", "position_a");
    addTransition("position_b2", "position_a1");
    addTransition("position_b2", "position_b1");
    addTransition("position_b2", "position_c1");
    addTransition("position_b2", "position_c2");
    addTransition("position_b2", "position_d");
    
    addTransition("position_c1", "position_a");
    addTransition("position_c1", "position_a1");
    addTransition("position_c1", "position_b1");
    addTransition("position_c1", "position_b2");
    addTransition("position_c1", "position_c2");
    addTransition("position_c1", "position_d");
    
    addTransition("position_c2", "position_a");
    addTransition("position_c2", "position_a1");
    addTransition("position_c2", "position_b1");
    addTransition("position_c2", "position_b2");
    addTransition("position_c2", "position_c1");
    addTransition("position_c2", "position_d");
    
    addTransition("position_d", "position_a");
    addTransition("position_d", "position_a1");
    addTransition("position_d", "position_b1");
    addTransition("position_d", "position_b2");
    addTransition("position_d", "position_c1");
    addTransition("position_d", "position_c2");
    
    // 设置初始状态
    setCurrentState("position_a");
    m_currentPosition = POSITION_A;
    m_currentPositionValue = m_positionValues[POSITION_A];
    
    return true;
}

/**
 * @brief 重置进给机构
 */
void PenetrationMechanismStateMachine::reset()
{
    // 移动到零位
    if (m_motionController) {
        m_motionController->moveMotor(PENETRATION_MOTOR_ID, MotorMode::POSITION, 0);
    }
    
    setCurrentState("position_a");
    m_currentPosition = POSITION_A;
    m_currentPositionValue = m_positionValues[POSITION_A];
}

/**
 * @brief 移动到指定位置
 */
bool PenetrationMechanismStateMachine::moveToPosition(Position position)
{
    // 默认使用速度模式
    return moveToPosition(m_positionValues[position], 0);
}

/**
 * @brief 移动到指定位置，带速度参数
 */
bool PenetrationMechanismStateMachine::moveToPosition(int position, double speed)
{
    // 参数position是直接的位置值，而不是枚举
    if (m_motionController) {
        if (speed == 0) {
            // 使用位置模式
            m_motionController->moveMotor(PENETRATION_MOTOR_ID, MotorMode::POSITION, position);
        } else {
            // 使用速度模式
            // 计算方向和距离
            double direction = (position > m_currentPositionValue) ? 1 : -1;
            double distance = fabs(position - m_currentPositionValue);
            
            // 设置速度
            m_motionController->moveMotor(PENETRATION_MOTOR_ID, MotorMode::VELOCITY, direction * speed);
            
            // 在实际应用中，这里需要添加逻辑来监控位置并在到达目标位置时停止
            // 这通常通过定时器或回调机制实现
            // 简化起见，我们假设控制器会处理这一点
        }
    }
    
    // 更新当前位置值
    m_currentPositionValue = position;
    
    // 确定最接近的预定义位置
    Position closestPosition = POSITION_A;
    double closestDistance = fabs(m_positionValues[POSITION_A] - position);
    
    for (int i = POSITION_A1; i <= POSITION_D; i++) {
        double distance = fabs(m_positionValues[i] - position);
        if (distance < closestDistance) {
            closestDistance = distance;
            closestPosition = static_cast<Position>(i);
        }
    }
    
    // 只有当接近预定义位置时才更新状态
    if (closestDistance < 5.0) { // 假设5mm的容差
        QString stateName = QString("position_%1").arg(QChar('a' + closestPosition));
        if (changeState(stateName)) {
            m_currentPosition = closestPosition;
            return true;
        }
    }
    
    return true;
}

/**
 * @brief 按增量移动
 */
bool PenetrationMechanismStateMachine::moveByIncrement(double increment)
{
    double newPosition = m_currentPositionValue + increment;
    return moveToPosition(newPosition, 0);
}

/**
 * @brief 获取当前位置
 */
PenetrationMechanismStateMachine::Position PenetrationMechanismStateMachine::getCurrentPosition() const
{
    return m_currentPosition;
}

/**
 * @brief 获取当前位置值
 */
double PenetrationMechanismStateMachine::getCurrentPositionValue() const
{
    return m_currentPositionValue;
}

/**
 * @brief 下夹紧机构状态机构造函数
 */
ClampMechanismStateMachine::ClampMechanismStateMachine(QObject *parent)
    : ComponentStateMachine("下夹紧机构", parent)
    , m_clampState(OPEN)
{
}

/**
 * @brief 下夹紧机构状态机析构函数
 */
ClampMechanismStateMachine::~ClampMechanismStateMachine()
{
}

/**
 * @brief 初始化下夹紧机构
 */
bool ClampMechanismStateMachine::initialize()
{
    // 添加状态
    addState("open");
    addState("loose");
    addState("tight");
    
    // 添加转换
    addTransition("open", "loose");
    addTransition("open", "tight");
    addTransition("loose", "open");
    addTransition("loose", "tight");
    addTransition("tight", "open");
    addTransition("tight", "loose");
    
    // 设置初始状态
    setCurrentState("open");
    m_clampState = OPEN;
    
    return true;
}

/**
 * @brief 重置下夹紧机构
 */
void ClampMechanismStateMachine::reset()
{
    // 松开夹持
    if (m_motionController) {
        m_motionController->moveMotor(CLAMP_MOTOR_ID, MotorMode::POSITION, 0);
    }
    
    setCurrentState("open");
    m_clampState = OPEN;
}

/**
 * @brief 设置夹紧状态
 */
bool ClampMechanismStateMachine::setClampState(ClampState state)
{
    QString stateName;
    double position = 0;
    
    switch (state) {
        case OPEN:
            stateName = "open";
            position = 0;
            break;
        case LOOSE:
            stateName = "loose";
            position = 50;
            break;
        case TIGHT:
            stateName = "tight";
            position = 100;
            break;
        default:
            return false;
    }
    
    if (m_motionController) {
        m_motionController->moveMotor(CLAMP_MOTOR_ID, MotorMode::POSITION, position);
    }
    
    if (changeState(stateName)) {
        m_clampState = state;
        return true;
    }
    
    return false;
}

/**
 * @brief 获取当前夹紧状态
 */
ClampMechanismStateMachine::ClampState ClampMechanismStateMachine::getClampState() const
{
    return m_clampState;
}

/**
 * @brief 对接机构状态机构造函数
 */
ConnectionMechanismStateMachine::ConnectionMechanismStateMachine(QObject *parent)
    : ComponentStateMachine("对接机构", parent)
    , m_isExtended(false)
{
}

/**
 * @brief 对接机构状态机析构函数
 */
ConnectionMechanismStateMachine::~ConnectionMechanismStateMachine()
{
}

/**
 * @brief 初始化对接机构
 */
bool ConnectionMechanismStateMachine::initialize()
{
    // 添加状态
    addState("retracted");
    addState("extended");
    
    // 添加转换
    addTransition("retracted", "extended");
    addTransition("extended", "retracted");
    
    // 设置初始状态
    setCurrentState("retracted");
    m_isExtended = false;
    
    return true;
}

/**
 * @brief 重置对接机构
 */
void ConnectionMechanismStateMachine::reset()
{
    // 收回对接机构
    if (m_motionController) {
        m_motionController->moveMotor(CONNECTION_MOTOR_ID, MotorMode::POSITION, 0);
    }
    
    setCurrentState("retracted");
    m_isExtended = false;
}

/**
 * @brief 设置对接状态
 */
bool ConnectionMechanismStateMachine::setConnectionState(bool extended)
{
    DrillingParameters* params = DrillingParameters::getInstance();
    
    if (m_motionController) {
        double position = 0;
        if (extended) {
            position = params->getParameter("ClampConnection", "CONNECTION_EXTENSION").toDouble();
        }
        
        m_motionController->moveMotor(CONNECTION_MOTOR_ID, MotorMode::POSITION, position);
    }
    
    if (extended) {
        if (changeState("extended")) {
            m_isExtended = true;
            return true;
        }
    } else {
        if (changeState("retracted")) {
            m_isExtended = false;
            return true;
        }
    }
    
    return false;
}

/**
 * @brief 获取当前对接状态
 */
bool ConnectionMechanismStateMachine::isExtended() const
{
    return m_isExtended;
}