#ifndef DEBUGTESTMOTION_H
#define DEBUGTESTMOTION_H

// 使用条件编译确保Debug模式下启用
//#ifdef _DEBUG
//#define ENABLE_DEBUG_TEST_MOTION
//#endif

#ifdef ENABLE_DEBUG_TEST_MOTION

#include "inc/autodrilling.h"
#include "inc/motioncontroller.h"
#include <QObject>
#include <QMap>
#include <memory>

// 前向声明
class DrillingController;

// 电机ID常量定义
namespace MotorID {
    constexpr int DRILL = 0;             // 旋转切割电机
    constexpr int PERCUSSION = 1;        // 冲击电机
    constexpr int PENETRATION = 2;       // 进给电机
    constexpr int CLAMP = 3;             // 下夹紧电机
    constexpr int ROBOT_CLAMP = 4;       // 机械手夹紧电机
    constexpr int ROBOT_ROTATION = 5;    // 机械手旋转电机
    constexpr int ROBOT_EXTENSION = 6;   // 机械手移动电机
    constexpr int STORAGE = 7;           // 存储电机
    constexpr int CONNECTION = 8;        // 对接电机（特殊，暂不修改）
}

/**
 * @brief 调试测试类，用于Debug模式下测试状态机的行为
 */
class DebugTestMotion : public QObject {
    Q_OBJECT

public:
    explicit DebugTestMotion(QObject *parent = nullptr);
    ~DebugTestMotion() override;

    // 初始化测试环境
    bool initialize();

    // 启动/停止/暂停/恢复测试
    void startTest();
    void stopTest();
    void pauseTest();
    void resumeTest();

    // 逐步执行
    void stepForward();

    // 打印当前状态
    void printCurrentState();

    // 测试指定状态序列
    void testStateSequence(const QStringList& stateSequence);

    // 获取钻进控制器
    DrillingController* getController() const { return m_controller; }

    // 打印机构状态
    void printMechanismStatus(int motorId) const;
    
    // 打印当前状态对应的运动流程
    void printMotionSequence(const QString& currentState) const;

private:
    // 辅助方法
    void initTestSequence();
    bool transitionToState(const QString& targetState);
    QString getStateDescription(const QString& stateName) const;
    void printStateSteps(const QString& stateName) const;
    void waitForOperation(int msec);

    // 钻进控制器
    DrillingController* m_controller;
    
    // 测试序列
    QStringList m_testSequence;
    
    // 状态描述映射
    QMap<QString, QStringList> m_stateDescriptions;
    
    // 测试状态
    int m_currentSequenceIndex;
    bool m_isStepMode;
    bool m_isRunning;
    QMap<int, QMap<QString, float>> m_motorStatus; // 存储电机状态

private slots:
    // 状态机事件响应
    void onStateChanged(const QString& oldState, const QString& newState);
    void onMachineStarted();
    void onMachineStopped();
    void onMachinePaused();
    void onMachineResumed();
    void onPipeCountChanged(int count);
    
    // 电机状态更新
    void onMotorStatusUpdated(int motorID, const QMap<QString, float>& params);
    
    // 命令响应
    void onCommandResponse(const QString& response);
};

// 全局调试测试实例
extern DebugTestMotion* g_debugTest;

// 辅助宏，简化调试代码
#define DEBUG_PRINT_STATE() if(g_debugTest) g_debugTest->printCurrentState()
#define DEBUG_STEP() if(g_debugTest) g_debugTest->stepForward()

#else // !ENABLE_DEBUG_TEST_MOTION

// 空宏，在非调试模式下不执行任何操作
#define DEBUG_PRINT_STATE()
#define DEBUG_STEP()

#endif // ENABLE_DEBUG_TEST_MOTION

#endif // DEBUGTESTMOTION_H