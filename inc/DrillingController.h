#ifndef DRILLINGCONTROLLER_H
#define DRILLINGCONTROLLER_H

#include "autodrilling.h"
#include "motioncontroller.h"
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QTimer>

// 前置声明
class StateMachineWorker;

/**
 * @brief 钻进控制器类
 * 
 * 该类负责管理钻进状态机和运动控制器，将其运行在独立线程中
 */
class DrillingController : public QObject
{
    Q_OBJECT

public:
    explicit DrillingController(QObject *parent = nullptr);
    ~DrillingController();

    // 初始化控制器
    bool initialize(const QString& ipAddress = "192.168.0.11", bool debugMode = false);
    
    // 释放控制器资源
    void release();
    
    // 获取状态机指针
    AutoDrillingStateMachine* getStateMachine() const;
    
    // 获取运动控制器指针
    MotionController* getMotionController() const;
    
    // 启动状态机
    bool startStateMachine();
    
    // 停止状态机
    bool stopStateMachine();
    
    // 暂停状态机
    bool pauseStateMachine();
    
    // 恢复状态机
    bool resumeStateMachine();
    
    // 获取当前状态
    AutoDrillingStateMachine::MainState getCurrentState() const;
    
    // 是否正在运行
    bool isRunning() const;

signals:
    // 当前状态变更信号
    void currentStepChanged(const QString& oldState, const QString& newState);
    
    // 命令响应信号
    void commandResponseReceived(const QString& response);
    
    // 状态机启动信号
    void stateMachineStarted();
    
    // 状态机停止信号
    void stateMachineStopped();
    
    // 状态机暂停信号
    void stateMachinePaused();
    
    // 状态机恢复信号
    void stateMachineResumed();
    
    // 电机状态更新信号
    void motorStatusUpdated(int motorID, const QMap<QString, float>& params);
    
    // 工作线程控制信号
    void startWorker();
    void stopWorker();
    void pauseWorker();
    void resumeWorker();
    
    // 内部控制信号(不对外公开)
private:
    void internalStartStateMachine();
    void internalStopStateMachine();
    void internalPauseStateMachine();
    void internalResumeStateMachine();

private slots:
    // 状态机状态变更处理
    void onStateMachineStateChanged(const QString& oldState, const QString& newState);
    
    // 命令响应处理
    void onCommandResponse(const QString& response);
    
    // 电机状态更新处理
    void onMotorStatusUpdated(int motorID, const QMap<QString, float>& params);
    
    // 钻进模式变更处理
    void onDrillingModeChanged(AutoDrillingStateMachine::DrillMode mode, double value);
    
    // 冲击状态变更处理
    void onPercussionStatusChanged(bool enabled, double frequency);
    
    // 钻管计数变更处理
    void onPipeCountChanged(int count);
    
    // 工作对象状态处理
    void onWorkerStarted();
    void onWorkerStopped();
    void onWorkerPaused();
    void onWorkerResumed();

private:
    // 状态机
    AutoDrillingStateMachine* m_stateMachine;
    
    // 运动控制器
    MotionController* m_motionController;
    
    // 工作线程
    QThread* m_workerThread;
    
    // 工作对象
    StateMachineWorker* m_worker;
    
    // 互斥锁
    QMutex m_mutex;
    
    // 条件变量，用于线程同步
    QWaitCondition m_condition;
    
    // 控制标志
    bool m_running;
    bool m_paused;
    bool m_stopRequested;
    
    // 定时器，用于更新状态
    QTimer m_updateTimer;
};

#endif // DRILLINGCONTROLLER_H 