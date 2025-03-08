// autodrilling.h
#ifndef AUTODRILLING_H
#define AUTODRILLING_H

#include "statemachine.h"
#include "drillingstate.h"   
#include "motioncontroller.h"
#include <QObject>
#include <memory>
#include <QMap>

// 前向声明
class MotionController;

// 钻管数量宏定义
#define MAX_PIPE_COUNT 6       // 最大钻管数量
#define ACTIVE_PIPE_COUNT 1    // 当前激活的钻管数量

/**
 * @brief 组件状态机基类
 * 所有机构的状态机都继承自此类
 */
class ComponentStateMachine : public StateMachine {
    Q_OBJECT
    
public:
    explicit ComponentStateMachine(const QString& name, QObject *parent = nullptr);
    virtual ~ComponentStateMachine();
    
    // 获取组件名称
    QString getComponentName() const;
    
    // 抽象方法，由派生类实现
    virtual bool initialize() = 0;
    virtual void reset() = 0;  // 修改返回类型为void，与StateMachine::reset()一致
    
    // 设置运动控制器
    void setMotionController(MotionController* controller);
    
    // 添加调试模式支持
    void setDebugMode(bool debug);
    bool isDebugMode() const;
    
protected:
    QString m_componentName;
    MotionController* m_motionController;
    bool m_isDebugMode;  // 添加调试模式标志
};

/**
 * @brief 自动钻进状态机类，继承自StateMachine基类
 * 实现多杆钻系统的自动钻进控制
 */
class AutoDrillingStateMachine : public StateMachine {
    Q_OBJECT

public:
    // 主状态机状态枚举
    enum MainState {
        SYSTEM_STARTUP,       // 系统启动
        DEFAULT_POSITION,     // 默认位置
        READY,                // 准备就绪
        FIRST_TOOL_INSTALLATION, // 首根钻具安装
        FIRST_DRILLING,       // 首次钻进
        PIPE_INSTALLATION_LOOP, // 钻管安装循环
        PIPE_REMOVAL_LOOP,    // 钻管拆卸循环
        FIRST_TOOL_RECOVERY,  // 首根钻具回收
        SYSTEM_RESET,         // 系统重置
        OPERATION_COMPLETE    // 操作完成
    };

    // 钻进模式枚举
    enum DrillMode {
        CONSTANT_SPEED = 66,  // 恒速度模式
        CONSTANT_TORQUE = 67  // 恒力矩模式
    };

    // 构造函数和析构函数
    explicit AutoDrillingStateMachine(QObject *parent = nullptr);
    ~AutoDrillingStateMachine() override;

    // 初始化状态机
    void initialize();
    
    // 设置运动控制器
    void setMotionController(MotionController* controller);
    
    // 设置钻进模式（恒速度或恒力矩）
    void setDrillMode(DrillMode mode, double value);
    
    // 冲击功能控制
    void enablePercussion(bool enable);
    void setPercussionFrequency(double frequency);
    
    // 操作方法
    void startOperation();
    void stopOperation();
    void pauseOperation();
    void resumeOperation();
    
    // 获取当前钻管计数
    int getCurrentPipeCount() const;
    
    // 获取增量参数
    double getDeltaThread() const { return m_deltaThread; }
    double getDeltaTool() const { return m_deltaTool; }
    double getDeltaPipe() const { return m_deltaPipe; }
    double getDeltaDrill() const { return m_deltaDrill; }
    
    // 获取速度参数
    double getV1() const { return m_v1; } // 钻进速度
    double getV2() const { return m_v2; } // 对接速度
    double getV3() const { return m_v3; } // 空行程速度
    
    // 获取组件状态机
    std::shared_ptr<ComponentStateMachine> getStorageUnit() const { return m_storageUnit; }
    std::shared_ptr<ComponentStateMachine> getRobotArm() const { return m_robotArm; }
    std::shared_ptr<ComponentStateMachine> getDrillingMechanism() const { return m_drillingMechanism; }
    std::shared_ptr<ComponentStateMachine> getPenetrationMechanism() const { return m_penetrationMechanism; }
    std::shared_ptr<ComponentStateMachine> getClampMechanism() const { return m_clampMechanism; }
    std::shared_ptr<ComponentStateMachine> getConnectionMechanism() const { return m_connectionMechanism; }

    // 状态机状态名称常量
    static const QString STATE_SYSTEM_STARTUP;
    static const QString STATE_DEFAULT_POSITION;
    static const QString STATE_READY;
    static const QString STATE_FIRST_TOOL_INSTALLATION;
    static const QString STATE_FIRST_DRILLING;
    static const QString STATE_PIPE_INSTALLATION_LOOP;
    static const QString STATE_PIPE_REMOVAL_LOOP;
    static const QString STATE_FIRST_TOOL_RECOVERY;
    static const QString STATE_SYSTEM_RESET;
    static const QString STATE_OPERATION_COMPLETE;

protected:
    // 重写StateMachine的钩子方法
    bool beforeStateChange(const QString& oldState, const QString& newState) override;
    void afterStateChange(const QString& oldState, const QString& newState) override;

    // 创建主状态机状态
    void createMainStates();

private:
    // 钻管计数增减
    void incrementPipeCount();
    void decrementPipeCount();
    
    // 运动控制器
    MotionController* m_motionController;
    
    // 组件状态机
    std::shared_ptr<ComponentStateMachine> m_storageUnit;      // 存储单元
    std::shared_ptr<ComponentStateMachine> m_robotArm;         // 机械手
    std::shared_ptr<ComponentStateMachine> m_drillingMechanism; // 钻进机构
    std::shared_ptr<ComponentStateMachine> m_penetrationMechanism; // 进给机构
    std::shared_ptr<ComponentStateMachine> m_clampMechanism;   // 下夹紧机构
    std::shared_ptr<ComponentStateMachine> m_connectionMechanism; // 对接机构
    
    // 操作参数
    int m_pipeCount;            // 钻管计数
    bool m_percussionEnabled;   // 冲击功能启用状态
    double m_percussionFrequency; // 冲击频率
    DrillMode m_drillMode;      // 钻进模式
    double m_drillParameter;    // 钻进参数值
    
    // 增量参数
    double m_deltaThread;       // Δ_thread: 螺旋接口标准高度增量
    double m_deltaTool;         // Δ_tool: 钻具螺旋接口增量
    double m_deltaPipe;         // Δ_pipe: 钻管螺旋接口增量
    double m_deltaDrill;        // Δ_drill: 钻进行程距离
    
    // 速度参数
    double m_v1;                // 钻进速度
    double m_v2;                // 钻管接卸速度
    double m_v3;                // 空行程速度
    
    // 旋转速度参数
    double m_omega;             // 正常钻进旋转速度
    double m_omega_s;           // 低速对接/断开旋转速度

signals:
    // GUI更新信号
    void pipeCountChanged(int count);
    void currentStepChanged(const QString& step);
    void drillingModeChanged(DrillMode mode, double value);
    void percussionStatusChanged(bool enabled, double frequency);
    
    // 错误信号
    void operationError(const QString& errorMessage);
};

/**
 * @brief 存储单元状态机
 * 管理存储单元的旋转位置
 */
class StorageUnitStateMachine : public ComponentStateMachine {
    Q_OBJECT
    
public:
    explicit StorageUnitStateMachine(QObject *parent = nullptr);
    ~StorageUnitStateMachine() override;
    
    // 初始化和重置
    bool initialize() override;
    void reset() override;
    
    // 旋转到指定位置 (0-6)
    bool rotateToPosition(int position);
    
    // 获取当前位置
    int getCurrentPosition() const;
    
    // 电机ID常量
    static const int STORAGE_MOTOR_ID;
    static const int MAX_POSITIONS = 7; // 存储单元最大位置数，0号位置存放钻具，1-6存放钻管
    
private:
    int m_currentPosition;
};

/**
 * @brief 机械手状态机
 * 管理机械手的旋转、伸缩和夹持状态
 */
class RobotArmStateMachine : public ComponentStateMachine {
    Q_OBJECT
    
public:
    explicit RobotArmStateMachine(QObject *parent = nullptr);
    ~RobotArmStateMachine() override;
    
    // 初始化和重置
    bool initialize() override;
    void reset() override;
    
    // 设置旋转位置 (0=对准钻进机构, 1=对准存储单元)
    bool setRotationPosition(int position);
    
    // 设置伸缩状态 (0=缩回, 1=伸出)
    bool setExtension(int extension);
    
    // 设置夹持状态 (0=未夹持, 1=夹紧)
    bool setClamp(int clamp);
    
    // 获取当前状态
    int getRotationPosition() const;
    int getExtension() const;
    int getClamp() const;
    
    // 新增：获取伸缩位置值
    int getExtensionPosition() const { return m_extensionPosition; }
    
    // 电机ID常量
    static const int ROTATION_MOTOR_ID;
    static const int EXTENSION_MOTOR_ID;
    static const int CLAMP_MOTOR_ID;
    
private:
    int m_rotationPosition;    // 0=对准钻进机构, 1=对准存储单元
    int m_extension;           // 0=缩回, 1=伸出
    int m_clamp;              // 0=未夹持, 1=夹紧
    int m_extensionPosition;   // 伸缩位置值
};

/**
 * @brief 钻进机构状态机
 * 管理钻进机构的旋转、冲击和连接状态
 */
class DrillingMechanismStateMachine : public ComponentStateMachine {
    Q_OBJECT
    
public:
    explicit DrillingMechanismStateMachine(QObject *parent = nullptr);
    ~DrillingMechanismStateMachine() override;
    
    // 初始化和重置
    bool initialize() override;
    void reset() override;
    
    // 设置旋转速度
    bool setRotationSpeed(double speed);
    
    // 设置冲击频率
    bool setPercussionFrequency(double frequency);
    
    // 设置连接状态 (true=已对接, false=未对接)
    bool setConnectionStatus(bool connected);
    
    // 获取当前状态
    double getRotationSpeed() const;
    double getPercussionFrequency() const;
    bool getConnectionStatus() const;
    
    // 设置钻进模式 (66=恒速度, 67=恒力矩)
    bool setDrillMode(int mode, double value);
    
    // 电机ID常量
    static const int DRILL_MOTOR_ID;
    static const int PERCUSSION_MOTOR_ID;
    
private:
    double m_rotationSpeed;       // 旋转速度
    double m_percussionFrequency; // 冲击频率
    bool m_connected;             // 连接状态
    int m_drillMode;             // 钻进模式
    double m_drillValue;         // 钻进参数值
};

/**
 * @brief 进给机构状态机
 * 管理进给机构的垂直位置
 */
class PenetrationMechanismStateMachine : public ComponentStateMachine {
    Q_OBJECT
    
public:
    // 预定义位置枚举
    enum Position {
        POSITION_A,   // 最底部位置
        POSITION_A1,  // 断开/连接螺旋接口所需的位置
        POSITION_B1,  // 钻具安装完成位置
        POSITION_B2,  // 钻管安装完成位置
        POSITION_C1,  // 钻具中间安装位置
        POSITION_C2,  // 钻管中间安装位置
        POSITION_D    // 最顶部位置
    };
    
    explicit PenetrationMechanismStateMachine(QObject *parent = nullptr);
    ~PenetrationMechanismStateMachine() override;
    
    // 初始化和重置
    bool initialize() override;
    void reset() override;
    
    // 移动到指定位置
    bool moveToPosition(Position position);
    // 新增：带速度参数的移动方法
    bool moveToPosition(int position, double speed);
    
    // 按增量移动
    bool moveByIncrement(double increment);
    
    // 获取当前位置
    Position getCurrentPosition() const;
    double getCurrentPositionValue() const;
    
    // 电机ID常量
    static const int PENETRATION_MOTOR_ID;
    
private:
    Position m_currentPosition;
    double m_currentPositionValue;
    double m_positionValues[7];
};

/**
 * @brief 下夹紧机构状态机
 * 管理下夹紧机构的状态
 */
class ClampMechanismStateMachine : public ComponentStateMachine {
    Q_OBJECT
    
public:
    // 夹紧状态枚举
    enum ClampState {
        OPEN,  // 张开
        LOOSE, // 夹紧未成功
        TIGHT  // 夹紧成功
    };
    
    explicit ClampMechanismStateMachine(QObject *parent = nullptr);
    ~ClampMechanismStateMachine() override;
    
    // 初始化和重置
    bool initialize() override;
    void reset() override;
    
    // 设置夹紧状态
    bool setClampState(ClampState state);
    
    // 获取当前状态
    ClampState getClampState() const;
    
    // 电机ID常量
    static const int CLAMP_MOTOR_ID;
    
private:
    ClampState m_clampState;
};

/**
 * @brief 对接机构状态机
 * 管理对接机构的状态
 */
class ConnectionMechanismStateMachine : public ComponentStateMachine {
    Q_OBJECT
    
public:
    explicit ConnectionMechanismStateMachine(QObject *parent = nullptr);
    ~ConnectionMechanismStateMachine() override;
    
    // 初始化和重置
    bool initialize() override;
    void reset() override;
    
    // 设置对接状态
    bool setConnectionState(bool extended);
    
    // 获取当前状态
    bool isExtended() const;
    
    // 电机ID常量
    static const int CONNECTION_MOTOR_ID;
    
private:
    bool m_isExtended; // true=推出状态, false=收回状态
};

#endif // AUTODRILLING_H