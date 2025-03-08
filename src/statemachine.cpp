#include "statemachine.h"

// State 类实现
State::State(StateMachine* machine) : m_machine(machine) {}

StateMachine* State::getMachine() const {
    return m_machine;
}

// StateMachine 类实现
StateMachine::StateMachine(QObject *parent)
    : QObject(parent)
    , m_isRunning(false)
    , m_isPaused(false)
{
    logDebug("状态机创建");
}

StateMachine::~StateMachine() {
    stop();
    m_states.clear();
    logDebug("状态机销毁");
}

void StateMachine::addState(const QString& stateName, std::shared_ptr<State> state) {
    if (m_states.contains(stateName)) {
        logWarning(QString("状态 '%1' 已存在，将被覆盖").arg(stateName));
    }
    m_states[stateName] = state;
    logDebug(QString("添加状态: %1").arg(stateName));
}

void StateMachine::setInitialState(const QString& stateName) {
    if (!m_states.contains(stateName)) {
        logError(QString("设置初始状态失败：状态 '%1' 不存在").arg(stateName));
        return;
    }
    m_initialStateName = stateName;
    logInfo(QString("设置初始状态: %1").arg(stateName));
}

bool StateMachine::changeState(const QString& stateName) {
    if (!m_states.contains(stateName)) {
        logError(QString("状态切换失败：目标状态 '%1' 不存在").arg(stateName));
        return false;
    }

    if (!m_isRunning) {
        logWarning("状态机未运行，无法切换状态");
        return false;
    }

    if (m_isPaused) {
        logWarning("状态机已暂停，无法切换状态");
        return false;
    }

    QString oldState = m_currentStateName;
    
    // 调用状态切换前的钩子
    if (!beforeStateChange(oldState, stateName)) {
        logWarning(QString("状态切换被阻止: %1 -> %2").arg(oldState).arg(stateName));
        return false;
    }

    // 退出当前状态
    if (!oldState.isEmpty() && m_states.contains(oldState)) {
        m_states[oldState]->exit();
    }

    // 更新当前状态
    m_currentStateName = stateName;
    
    // 进入新状态
    if (m_states.contains(m_currentStateName)) {
        m_states[m_currentStateName]->enter();
    }

    // 调用状态切换后的钩子
    afterStateChange(oldState, stateName);

    // 发送状态改变信号
    emit stateChanged(oldState, stateName);
    logInfo(QString("状态切换完成: %1 -> %2").arg(oldState).arg(stateName));
    
    return true;
}

QString StateMachine::getCurrentStateName() const {
    return m_currentStateName;
}

std::shared_ptr<State> StateMachine::getCurrentState() const {
    return m_states.value(m_currentStateName);
}

bool StateMachine::start() {
    if (m_isRunning) {
        logWarning("状态机已在运行中");
        return false;
    }

    if (m_initialStateName.isEmpty()) {
        logError("未设置初始状态，无法启动状态机");
        return false;
    }

    m_isRunning = true;
    m_isPaused = false;
    
    // 进入初始状态
    if (!changeState(m_initialStateName)) {
        m_isRunning = false;
        logError("无法进入初始状态");
        return false;
    }

    // 确保当前状态被正确设置
    if (m_currentStateName.isEmpty()) {
        m_currentStateName = m_initialStateName;
        if (m_states.contains(m_currentStateName)) {
            m_states[m_currentStateName]->enter();
        }
    }

    emit machineStarted();
    logInfo(QString("状态机启动，当前状态: %1").arg(m_currentStateName));
    return true;
}

void StateMachine::stop() {
    if (!m_isRunning) {
        return;
    }

    // 退出当前状态
    if (!m_currentStateName.isEmpty() && m_states.contains(m_currentStateName)) {
        m_states[m_currentStateName]->exit();
    }

    m_isRunning = false;
    m_isPaused = false;
    m_currentStateName.clear();

    emit machineStopped();
    logInfo("状态机停止");
}

void StateMachine::pause() {
    if (!m_isRunning || m_isPaused) {
        return;
    }

    m_isPaused = true;
    emit machinePaused();
    logInfo("状态机暂停");
}

void StateMachine::resume() {
    if (!m_isRunning || !m_isPaused) {
        return;
    }

    m_isPaused = false;
    emit machineResumed();
    logInfo("状态机恢复");
}

void StateMachine::reset() {
    stop();
    
    // 确保有初始状态
    if (!m_initialStateName.isEmpty()) {
        // 重新启动状态机
        if (start()) {
            logInfo(QString("状态机重置成功，当前状态: %1").arg(m_currentStateName));
        } else {
            logError("状态机重置失败");
        }
    }
    
    emit machineReset();
}

bool StateMachine::isRunning() const {
    return m_isRunning;
}

bool StateMachine::isPaused() const {
    return m_isPaused;
}

bool StateMachine::beforeStateChange(const QString& oldState, const QString& newState) {
    Q_UNUSED(oldState);
    Q_UNUSED(newState);
    return true;
}

void StateMachine::afterStateChange(const QString& oldState, const QString& newState) {
    Q_UNUSED(oldState);
    Q_UNUSED(newState);
}

// 日志输出方法实现
void StateMachine::logError(const QString& message) {
    qDebug() << "[ERROR]" << message;
    emit error(message);
}

void StateMachine::logWarning(const QString& message) {
    qDebug() << "[WARNING]" << message;
    emit warning(message);
}

void StateMachine::logInfo(const QString& message) {
    qDebug() << "[INFO]" << message;
    emit info(message);
}

void StateMachine::logDebug(const QString& message) {
    qDebug() << "[DEBUG]" << message;
    emit debug(message);
} 