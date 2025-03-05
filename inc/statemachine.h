#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QDebug>
#include <memory>

// 前向声明
class State;

/**
 * @brief 状态机基类
 * 提供状态机的基本框架，包括状态添加、切换、启动/停止等核心功能
 */
class StateMachine : public QObject
{
    Q_OBJECT

public:
    explicit StateMachine(QObject *parent = nullptr);
    virtual ~StateMachine();

    // 状态管理
    void addState(const QString& stateName, std::shared_ptr<State> state);
    void setInitialState(const QString& stateName);
    bool changeState(const QString& stateName);
    QString getCurrentStateName() const;
    std::shared_ptr<State> getCurrentState() const;

    // 状态机控制
    virtual bool start();
    virtual void stop();
    virtual void pause();
    virtual void resume();
    virtual void reset();

    // 状态机状态查询
    bool isRunning() const;
    bool isPaused() const;

signals:
    // 状态变化信号
    void stateChanged(const QString& oldState, const QString& newState);
    void machineStarted();
    void machineStopped();
    void machinePaused();
    void machineResumed();
    void machineReset();
    
    // 错误和日志信号
    void error(const QString& errorMessage);
    void warning(const QString& warningMessage);
    void info(const QString& infoMessage);
    void debug(const QString& debugMessage);

protected:
    // 状态转换相关
    virtual bool beforeStateChange(const QString& oldState, const QString& newState);
    virtual void afterStateChange(const QString& oldState, const QString& newState);

    // 日志输出方法
    void logError(const QString& message);
    void logWarning(const QString& message);
    void logInfo(const QString& message);
    void logDebug(const QString& message);

private:
    QMap<QString, std::shared_ptr<State>> m_states;        // 状态映射表
    QString m_currentStateName;                            // 当前状态名
    QString m_initialStateName;                            // 初始状态名
    bool m_isRunning;                                      // 运行状态标志
    bool m_isPaused;                                      // 暂停状态标志
};

/**
 * @brief 状态基类
 * 定义状态的基本接口
 */
class State
{
public:
    explicit State(StateMachine* machine);
    virtual ~State() = default;

    // 状态生命周期方法
    virtual void enter() = 0;                              // 进入状态时调用
    virtual void exit() = 0;                               // 退出状态时调用
    virtual void update() = 0;                             // 状态更新时调用

    // 状态机引用
    StateMachine* getMachine() const;

protected:
    StateMachine* m_machine;                               // 所属状态机指针
};

#endif // STATEMACHINE_H 