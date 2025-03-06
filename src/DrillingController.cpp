#include "inc/DrillingController.h"
#include "inc/DrillingParameters.h"
#include "inc/StateMachineWorker.h"
#include <QDebug>

/**
 * @brief 构造函数
 * @param parent 父对象
 */
DrillingController::DrillingController(QObject *parent)
    : QObject(parent)
    , m_stateMachine(nullptr)
    , m_motionController(nullptr)
    , m_workerThread(nullptr)
    , m_worker(nullptr)
    , m_running(false)
    , m_paused(false)
    , m_stopRequested(false)
{
    m_motionController = new MotionController(this);
    m_stateMachine = new AutoDrillingStateMachine(this);
    
    // 设置运动控制器
    m_stateMachine->setMotionController(m_motionController);
    
    // 连接信号
    connect(m_stateMachine, &StateMachine::stateChanged,
            this, &DrillingController::onStateMachineStateChanged);
    connect(m_stateMachine, &AutoDrillingStateMachine::drillingModeChanged,
            this, &DrillingController::onDrillingModeChanged);
    connect(m_stateMachine, &AutoDrillingStateMachine::percussionStatusChanged,
            this, &DrillingController::onPercussionStatusChanged);
    connect(m_stateMachine, &AutoDrillingStateMachine::pipeCountChanged,
            this, &DrillingController::onPipeCountChanged);
    
    connect(m_motionController, &MotionController::commandResponse,
            this, &DrillingController::onCommandResponse);
    connect(m_motionController, &MotionController::motorStatusChanged,
            this, &DrillingController::onMotorStatusUpdated);
    
    // 创建工作线程和工作对象
    m_workerThread = new QThread();
    m_worker = new StateMachineWorker(m_stateMachine);
    m_worker->moveToThread(m_workerThread);
    
    // 连接线程信号
    connect(m_workerThread, &QThread::started, m_worker, &StateMachineWorker::run);
    connect(&m_updateTimer, &QTimer::timeout, m_worker, &StateMachineWorker::run);
    
    // 连接工作对象的信号
    connect(this, &DrillingController::startWorker, m_worker, &StateMachineWorker::startWork);
    connect(this, &DrillingController::stopWorker, m_worker, &StateMachineWorker::stopWork);
    connect(this, &DrillingController::pauseWorker, m_worker, &StateMachineWorker::pauseWork);
    connect(this, &DrillingController::resumeWorker, m_worker, &StateMachineWorker::resumeWork);
    
    connect(m_worker, &StateMachineWorker::started, this, &DrillingController::onWorkerStarted);
    connect(m_worker, &StateMachineWorker::stopped, this, &DrillingController::onWorkerStopped);
    connect(m_worker, &StateMachineWorker::paused, this, &DrillingController::onWorkerPaused);
    connect(m_worker, &StateMachineWorker::resumed, this, &DrillingController::onWorkerResumed);
    
    // 启动线程
    m_workerThread->start();
}

/**
 * @brief 析构函数
 */
DrillingController::~DrillingController()
{
    // 释放资源
    release();
    
    // 删除状态机和控制器
    if (m_stateMachine) {
        delete m_stateMachine;
        m_stateMachine = nullptr;
    }
    
    if (m_motionController) {
        delete m_motionController;
        m_motionController = nullptr;
    }
    
    // 删除线程和工作对象
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_workerThread;
        m_workerThread = nullptr;
    }
    
    if (m_worker) {
        delete m_worker;
        m_worker = nullptr;
    }
}

/**
 * @brief 初始化控制器
 * @param ipAddress 控制器IP地址
 * @param debugMode 是否为调试模式
 * @return 是否初始化成功
 */
bool DrillingController::initialize(const QString& ipAddress, bool debugMode)
{
    QMutexLocker locker(&m_mutex);
    
    // 在调试模式下不初始化实际的运动控制器连接
    if (!debugMode) {
        // 初始化运动控制器
        if (!m_motionController->initialize(ipAddress)) {
            qDebug() << "初始化运动控制器失败";
            return false;
        }
    } else {
        qDebug() << "调试模式: 跳过运动控制器实际连接";
    }
    
    // 初始化状态机
    m_stateMachine->initialize(); // 不检查返回值，因为该方法返回void
    
    // 在非调试模式下注册电机回调
    if (!debugMode) {
        for (int i = 0; i <= 9; i++) {
            m_motionController->registerMotionCallback(i, 
                [this](int motorID, const QMap<QString, float>& params) {
                    onMotorStatusUpdated(motorID, params);
                });
        }
    }
    
    qDebug() << "钻进控制器初始化成功" << (debugMode ? "(调试模式)" : "");
    return true;
}

/**
 * @brief 释放控制器资源
 */
void DrillingController::release()
{
    QMutexLocker locker(&m_mutex);
    
    // 停止状态机
    stopStateMachine();
    
    // 释放运动控制器
    if (m_motionController) {
        m_motionController->release();
    }
    
    qDebug() << "钻进控制器已释放";
}

/**
 * @brief 获取状态机指针
 * @return 状态机指针
 */
AutoDrillingStateMachine* DrillingController::getStateMachine() const
{
    return m_stateMachine;
}

/**
 * @brief 获取运动控制器指针
 * @return 运动控制器指针
 */
MotionController* DrillingController::getMotionController() const
{
    return m_motionController;
}

/**
 * @brief 启动状态机
 * @return 是否成功
 */
bool DrillingController::startStateMachine()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_running) {
        qDebug() << "状态机已经在运行";
        return false;
    }
    
    // 重置标志
    m_running = true;
    m_paused = false;
    m_stopRequested = false;
    
    // 启动定时器以定期调用工作对象的run方法
    m_updateTimer.start(100);
    
    // 发送启动信号给工作对象
    emit startWorker();
    
    qDebug() << "状态机启动请求已发送";
    return true;
}

/**
 * @brief 停止状态机
 * @return 是否成功
 */
bool DrillingController::stopStateMachine()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_running) {
        qDebug() << "状态机未运行";
        return false;
    }
    
    // 设置停止标志
    m_stopRequested = true;
    m_running = false;
    
    // 停止定时器
    m_updateTimer.stop();
    
    // 发送停止信号给工作对象
    emit stopWorker();
    
    qDebug() << "状态机停止请求已发送";
    return true;
}

/**
 * @brief 暂停状态机
 * @return 是否成功
 */
bool DrillingController::pauseStateMachine()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_running || m_paused) {
        qDebug() << "状态机未运行或已暂停";
        return false;
    }
    
    // 设置暂停标志
    m_paused = true;
    
    // 发送暂停信号给工作对象
    emit pauseWorker();
    
    qDebug() << "状态机暂停请求已发送";
    return true;
}

/**
 * @brief 恢复状态机
 * @return 是否成功
 */
bool DrillingController::resumeStateMachine()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_running || !m_paused) {
        qDebug() << "状态机未运行或未暂停";
        return false;
    }
    
    // 清除暂停标志
    m_paused = false;
    
    // 发送恢复信号给工作对象
    emit resumeWorker();
    
    qDebug() << "状态机恢复请求已发送";
    return true;
}

/**
 * @brief 获取当前状态
 * @return 当前状态
 */
AutoDrillingStateMachine::MainState DrillingController::getCurrentState() const
{
    if (!m_stateMachine) {
        return AutoDrillingStateMachine::SYSTEM_STARTUP;
    }
    
    QString stateName = m_stateMachine->getCurrentStateName();
    
    // 将状态名称转换为枚举值
    if (stateName == "SYSTEM_STARTUP") return AutoDrillingStateMachine::SYSTEM_STARTUP;
    if (stateName == "DEFAULT_POSITION") return AutoDrillingStateMachine::DEFAULT_POSITION;
    if (stateName == "READY") return AutoDrillingStateMachine::READY;
    if (stateName == "FIRST_TOOL_INSTALLATION") return AutoDrillingStateMachine::FIRST_TOOL_INSTALLATION;
    if (stateName == "FIRST_DRILLING") return AutoDrillingStateMachine::FIRST_DRILLING;
    if (stateName == "PIPE_INSTALLATION_LOOP") return AutoDrillingStateMachine::PIPE_INSTALLATION_LOOP;
    if (stateName == "PIPE_REMOVAL_LOOP") return AutoDrillingStateMachine::PIPE_REMOVAL_LOOP;
    if (stateName == "FIRST_TOOL_RECOVERY") return AutoDrillingStateMachine::FIRST_TOOL_RECOVERY;
    if (stateName == "SYSTEM_RESET") return AutoDrillingStateMachine::SYSTEM_RESET;
    if (stateName == "OPERATION_COMPLETE") return AutoDrillingStateMachine::OPERATION_COMPLETE;
    
    return AutoDrillingStateMachine::SYSTEM_STARTUP;
}

/**
 * @brief 是否正在运行
 * @return 是否正在运行
 */
bool DrillingController::isRunning() const
{
    return m_running;
}

/**
 * @brief 内部启动状态机
 */
void DrillingController::internalStartStateMachine()
{
    if (m_stateMachine) {
        m_stateMachine->startOperation();
        emit stateMachineStarted();
    }
}

/**
 * @brief 内部停止状态机
 */
void DrillingController::internalStopStateMachine()
{
    if (m_stateMachine) {
        m_stateMachine->stopOperation();
        emit stateMachineStopped();
    }
}

/**
 * @brief 内部暂停状态机
 */
void DrillingController::internalPauseStateMachine()
{
    if (m_stateMachine) {
        m_stateMachine->pauseOperation();
        emit stateMachinePaused();
    }
}

/**
 * @brief 内部恢复状态机
 */
void DrillingController::internalResumeStateMachine()
{
    if (m_stateMachine) {
        m_stateMachine->resumeOperation();
        emit stateMachineResumed();
    }
}

/**
 * @brief 状态机状态变更处理
 * @param oldState 旧状态
 * @param newState 新状态
 */
void DrillingController::onStateMachineStateChanged(const QString& oldState, const QString& newState)
{
    emit currentStepChanged(oldState, newState);
}

/**
 * @brief 命令响应处理
 * @param response 响应消息
 */
void DrillingController::onCommandResponse(const QString& response)
{
    emit commandResponseReceived(response);
}

/**
 * @brief 电机状态更新处理
 * @param motorID 电机ID
 * @param params 参数
 */
void DrillingController::onMotorStatusUpdated(int motorID, const QMap<QString, float>& params)
{
    emit motorStatusUpdated(motorID, params);
}

/**
 * @brief 钻进模式变更处理
 * @param mode 钻进模式
 * @param value 参数值
 */
void DrillingController::onDrillingModeChanged(AutoDrillingStateMachine::DrillMode mode, double value)
{
    qDebug() << "钻进模式变更: " << (mode == AutoDrillingStateMachine::CONSTANT_SPEED ? "恒速度" : "恒力矩") << ", 值: " << value;
}

/**
 * @brief 冲击状态变更处理
 * @param enabled 是否启用
 * @param frequency 频率
 */
void DrillingController::onPercussionStatusChanged(bool enabled, double frequency)
{
    qDebug() << "冲击状态变更: " << (enabled ? "启用" : "禁用") << ", 频率: " << frequency;
}

/**
 * @brief 钻管计数变更处理
 * @param count 计数
 */
void DrillingController::onPipeCountChanged(int count)
{
    qDebug() << "钻管计数变更: " << count;
}

/**
 * @brief 工作对象启动处理
 */
void DrillingController::onWorkerStarted()
{
    qDebug() << "状态机工作对象启动";
}

/**
 * @brief 工作对象停止处理
 */
void DrillingController::onWorkerStopped()
{
    qDebug() << "状态机工作对象停止";
}

/**
 * @brief 工作对象暂停处理
 */
void DrillingController::onWorkerPaused()
{
    qDebug() << "状态机工作对象暂停";
}

/**
 * @brief 工作对象恢复处理
 */
void DrillingController::onWorkerResumed()
{
    qDebug() << "状态机工作对象恢复";
} 