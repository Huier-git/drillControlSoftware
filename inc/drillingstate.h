#ifndef DRILLINGSTATE_H
#define DRILLINGSTATE_H

#include "statemachine.h"
#include "autodrilling.h"
#include "motioncontroller.h"

// 前向声明
class AutoDrillingStateMachine;

/**
 * @brief 钻进状态基类
 */
class DrillState : public State {
public:
    explicit DrillState(AutoDrillingStateMachine* machine);
    ~DrillState() override = default;

    // 实现State的纯虚函数
    void enter() override;
    void exit() override;
    void update() override;

protected:
    AutoDrillingStateMachine* m_machine;
};

/**
 * @brief 系统启动状态
 */
class SystemStartupState : public DrillState {
public:
    explicit SystemStartupState(AutoDrillingStateMachine* machine);
    void enter() override;
    void update() override;
};

/**
 * @brief 默认位置状态
 */
class DefaultPositionState : public DrillState {
public:
    explicit DefaultPositionState(AutoDrillingStateMachine* machine);
    void enter() override;
    void update() override;
};

/**
 * @brief 就绪状态
 */
class ReadyState : public DrillState {
public:
    explicit ReadyState(AutoDrillingStateMachine* machine);
    void enter() override;
    void update() override;
};

/**
 * @brief 首根钻具安装状态
 */
class FirstToolInstallationState : public DrillState {
public:
    explicit FirstToolInstallationState(AutoDrillingStateMachine* machine);
    void enter() override;
    void update() override;
};

/**
 * @brief 首次钻进状态
 */
class FirstDrillingState : public DrillState {
public:
    explicit FirstDrillingState(AutoDrillingStateMachine* machine);
    void enter() override;
    void update() override;
};

/**
 * @brief 钻管安装循环状态
 */
class PipeInstallationLoopState : public DrillState {
public:
    explicit PipeInstallationLoopState(AutoDrillingStateMachine* machine);
    void enter() override;
    void update() override;
};

/**
 * @brief 钻管拆卸循环状态
 */
class PipeRemovalLoopState : public DrillState {
public:
    explicit PipeRemovalLoopState(AutoDrillingStateMachine* machine);
    void enter() override;
    void update() override;
};

/**
 * @brief 首根钻具回收状态
 */
class FirstToolRecoveryState : public DrillState {
public:
    explicit FirstToolRecoveryState(AutoDrillingStateMachine* machine);
    void enter() override;
    void update() override;
};

/**
 * @brief 系统重置状态
 */
class SystemResetState : public DrillState {
public:
    explicit SystemResetState(AutoDrillingStateMachine* machine);
    void enter() override;
    void update() override;
};

/**
 * @brief 操作完成状态
 */
class OperationCompleteState : public DrillState {
public:
    explicit OperationCompleteState(AutoDrillingStateMachine* machine);
    void enter() override;
    void update() override;
};

#endif // DRILLINGSTATE_H 