#include "inc/StateMachineWorker.h"
#include <QDebug>

/**
 * @brief 构造函数
 * @param stateMachine 状态机指针
 * @param parent 父对象
 */
StateMachineWorker::StateMachineWorker(AutoDrillingStateMachine* stateMachine, QObject* parent)
    : QObject(parent)
    , m_stateMachine(stateMachine)
    , m_running(false)
    , m_paused(false)
    , m_stopRequested(false)
{
}

/**
 * @brief 运行函数，定期处理状态机的逻辑
 * 
 * 该函数会被定时器周期性调用，用于驱动状态机
 */
void StateMachineWorker::run()
{
    if (!m_running || !m_stateMachine || m_stopRequested) {
        return;
    }
    
    if (m_paused) {
        // 暂停状态下不处理状态机逻辑
        return;
    }
    
    // 获取当前状态
    QString currentState = m_stateMachine->getCurrentStateName();
    
    // 根据当前状态执行相应的操作
    if (currentState == AutoDrillingStateMachine::STATE_SYSTEM_STARTUP) {
        // 系统启动状态，检查所有设备是否就绪
        // 如果就绪，则转到默认位置状态
        m_stateMachine->changeState(AutoDrillingStateMachine::STATE_DEFAULT_POSITION);
    }
    else if (currentState == AutoDrillingStateMachine::STATE_DEFAULT_POSITION) {
        // 默认位置状态，检查所有机构是否在默认位置
        // 如果在默认位置，则转到就绪状态
        m_stateMachine->changeState(AutoDrillingStateMachine::STATE_READY);
    }
    else if (currentState == AutoDrillingStateMachine::STATE_READY) {
        // 就绪状态，等待用户操作
        // 此状态不会自动转换到其他状态
    }
    else if (currentState == AutoDrillingStateMachine::STATE_FIRST_TOOL_INSTALLATION) {
        // 首根钻具安装流程
        // 该状态下的子步骤会在状态机内部自动执行
        // 状态机会在完成后自动转换到首次钻进状态
    }
    else if (currentState == AutoDrillingStateMachine::STATE_FIRST_DRILLING) {
        // 首次钻进流程
        // 该状态下的子步骤会在状态机内部自动执行
        // 状态机会在完成后自动转换到钻管安装循环状态
    }
    else if (currentState == AutoDrillingStateMachine::STATE_PIPE_INSTALLATION_LOOP) {
        // 钻管安装循环流程
        // 该状态下的子步骤会在状态机内部自动执行
        // 根据钻管数量可能会继续循环或转到钻管拆卸循环状态
    }
    else if (currentState == AutoDrillingStateMachine::STATE_PIPE_REMOVAL_LOOP) {
        // 钻管拆卸循环流程
        // 该状态下的子步骤会在状态机内部自动执行
        // 根据钻管数量可能会继续循环或转到首根钻具回收状态
    }
    else if (currentState == AutoDrillingStateMachine::STATE_FIRST_TOOL_RECOVERY) {
        // 首根钻具回收流程
        // 该状态下的子步骤会在状态机内部自动执行
        // 状态机会在完成后自动转换到系统重置状态
    }
    else if (currentState == AutoDrillingStateMachine::STATE_SYSTEM_RESET) {
        // 系统重置流程
        // 该状态下的子步骤会在状态机内部自动执行
        // 状态机会在完成后自动转换到默认位置状态
    }
    else if (currentState == AutoDrillingStateMachine::STATE_OPERATION_COMPLETE) {
        // 操作完成状态，等待用户操作
        // 此状态不会自动转换到其他状态
    }
    
    // 在实际应用中，可能需要检查各种传感器和状态反馈
    // 并根据结果来决定是否需要触发状态转换或错误处理
}

/**
 * @brief 开始工作
 */
void StateMachineWorker::startWork()
{
    if (!m_running) {
        m_running = true;
        m_paused = false;
        m_stopRequested = false;
        qDebug() << "状态机工作对象已启动";
        emit started();
    }
}

/**
 * @brief 停止工作
 */
void StateMachineWorker::stopWork()
{
    if (m_running) {
        m_stopRequested = true;
        m_running = false;
        m_paused = false;
        qDebug() << "状态机工作对象已停止";
        emit stopped();
    }
}

/**
 * @brief 暂停工作
 */
void StateMachineWorker::pauseWork()
{
    if (m_running && !m_paused) {
        m_paused = true;
        qDebug() << "状态机工作对象已暂停";
        emit paused();
    }
}

/**
 * @brief 恢复工作
 */
void StateMachineWorker::resumeWork()
{
    if (m_running && m_paused) {
        m_paused = false;
        qDebug() << "状态机工作对象已恢复";
        emit resumed();
    }
}