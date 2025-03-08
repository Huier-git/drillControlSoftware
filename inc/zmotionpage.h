#ifndef ZMOTIONPAGE_H
#define ZMOTIONPAGE_H

#include <QWidget>
#include <QDebug>
#include <QTimer>
#include <QDateTime>
#include <QList>
#include <QTableWidget>
#include <QRegularExpression>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QMessageBox>
#include <QButtonGroup>
#include "inc/zmcaux.h"
#include "inc/zmotion.h"
#include "inc/mdbprocess.h"
#include "inc/Global.h"
#include "inc/motioncontroller.h"

#define MAX_AXIS        20  //最大轴数
#define TOP_COUNT       1300000     //13000000 为最高点脉冲数
#define ROTATE_COUNT    850000      //850000对应120rpm


namespace Ui {
class zmotionpage;
}

class AutoModeThread;  // 前向声明

class zmotionpage : public QWidget
{
    Q_OBJECT

public:
    explicit zmotionpage(QWidget *parent = nullptr);
    ~zmotionpage();

    bool waitForConfirmation();
    void ShowMotorMap();

private:
    // 运动控制参数常量
    // 旋转运动参数
    static constexpr float ROBOTARM_ROTATION_SPEED = 50.0f;    // 旋转速度
    static constexpr float ROBOTARM_ROTATION_ACCEL = 10.0f;    // 旋转加速度
    static constexpr float ROBOTARM_ROTATION_DECEL = 10.0f;    // 旋转减速度
    
    // 伸缩运动参数
    static constexpr float ROBOTARM_EXTENT_SPEED = 30.0f;      // 伸缩速度
    static constexpr float ROBOTARM_EXTENT_ACCEL = 5.0f;       // 伸缩加速度
    static constexpr float ROBOTARM_EXTENT_DECEL = 5.0f;       // 伸缩减速度
    
    // 夹爪控制参数
    static constexpr int ROBOTARM_CLAMP_TORQUE_MODE = 67;      // 夹爪力矩控制模式
    static constexpr float ROBOTARM_CLAMP_DEFAULT_DAC = 1000.0f; // 默认夹爪力矩值
    static constexpr int DOWNCLAMP_MOTOR_ID = 4;           // 夹爪电机ID
    static constexpr float DOWNCLAMP_DEFAULT_DAC = 1000.0f;  // 默认夹爪力矩值
    static constexpr float DOWNCLAMP_POSITION_THRESHOLD = 0.1f; // 位置变化阈值
    static constexpr int DOWNCLAMP_TIMEOUT = 3000;         // 超时时间(ms)
    static constexpr int DOWNCLAMP_CHECK_INTERVAL = 100;   // 位置检查间隔(ms)

    // 位置常量
    static constexpr float ROBOTARM_DRILL_POSITION_ANGLE = 0.0f;    // 钻机位置角度
    static constexpr float ROBOTARM_STORAGE_POSITION_ANGLE = 90.0f; // 存储位置角度
    static constexpr float ROBOTARM_EXTENT_POSITION = 200.0f;       // 伸出位置
    static constexpr float ROBOTARM_RETRACT_POSITION = 0.0f;        // 回收位置
    
    // 进给机构深度控制参数
    static constexpr float PENETRATION_DEFAULT_SPEED = 20.0f;     // 默认进给速度
    static constexpr float PENETRATION_DEFAULT_ACCEL = 5.0f;      // 默认进给加速度
    static constexpr float PENETRATION_DEFAULT_DECEL = 5.0f;      // 默认进给减速度
    static constexpr double PENETRATION_MAX_HEIGHT = 1315.0;      // 最大高度(mm)
    static constexpr double PENETRATION_MAX_PULSE = 13100000.0;   // 最大高度对应的脉冲数
    static constexpr double PENETRATION_PULSE_PER_MM = PENETRATION_MAX_PULSE / PENETRATION_MAX_HEIGHT; // 每毫米对应的脉冲数

private slots:
    void on_btn_IP_Scan_clicked();

    void on_btn_IP_Connect_clicked();

    void on_btn_BusInit_clicked();

    void basicInfoRefresh();                                // 基础信息的刷新函数

    void advanceInfoRefreash();                             // 高级信息的刷新函数

    void initMotorTable();                                  // 初始化电机参数表格

    void modifyMotorTable(QTableWidgetItem *item);          // 找出修改电机表格的值

    void unmodifyMotorTable(int row, int column);

    void setMotorParm(int row, int col, QString value);     // 设置电机的值

    void PauseAllAxis();

    void ResumeAllAxis();

    void on_Btn_Enable_clicked();

    void on_Btn_ClearAlm_clicked();

    void on_Btn_setCurrZero_clicked();

    void RefreshTableContent();

    void RefreshTableRealTimeContent();

    void on_Btn_StopMove_clicked();

    void handleModbusCommand(const QString &cmd);

    void handleZmotion(const QString& cmd);

    void handleReceivedData(const QVector<quint16>& data, int startReg);


    void on_btn_sendCmd_clicked();      //终端发送
    void on_btn_rapidStop_clicked();    //快速停止
    void runningMode();                 //切换运行模式
    void onAutoModeCompleted();
    void handleConfirmation();

    void on_btn_pipeConnect_clicked();  // 张开的连接、

    // 模式切换相关函数
    void initModeSelection();
    void onModeConfirm();
    void runManualMode();
    void runAutomaticMode();
    void runSemiAutomaticMode();
    void enableManualControls(bool enable);
    void stopSemiAutoMode();

    // 机械手控制相关函数
    // 旋转控制
    void on_btn_rotation_reset_clicked();       // 重置旋转起点
    void on_btn_drill_position_clicked();       // 移动到钻机位置（0度）
    void on_btn_storage_position_clicked();     // 移动到存储位置（90度）
    void updateRotationAngle();                 // 更新旋转角度显示
    void on_le_set_rotation_angle_editingFinished(); // 设置自定义旋转角度
    
    // 伸出控制
    void on_btn_robotarm_reset_clicked();       // 重置伸出起点
    void on_btn_robotarm_extent_clicked();      // 控制机械手伸出
    void on_btn_robotarm_retract_clicked();     // 控制机械手回收
    void updateExtentLength();                  // 更新伸出长度显示
    void on_le_set_extent_length_editingFinished(); // 设置自定义伸出长度
    
    // 夹爪控制
    void on_btn_robotarm_clamp_open_clicked();  // 夹爪张开
    void on_btn_robotarm_clamp_close_clicked(); // 夹爪闭合
    void updateClampStatus();                   // 更新夹爪状态显示

    // 进给机构深度控制
    void on_btn_penetration_start_clicked();    // 开始进给到目标深度
    void on_btn_penetration_cancel_clicked();   // 取消进给运动
    void on_le_penetration_target_editingFinished(); // 设置目标深度
    void on_le_penetration_speed_editingFinished();  // 设置进给速度
    void updatePenetrationDepth();              // 更新当前深度显示

    // 钻管存储机构控制函数
    void on_btn_storage_backward_clicked();  // 存储机构向后转位
    void on_btn_storage_forward_clicked();   // 存储机构向前转位
    void updateStorageStatus();              // 更新存储机构状态显示

    // 旋转和冲击控制函数
    void on_btn_rotation_clicked();           // 开始旋转
    void on_btn_rotation_stop_clicked();      // 停止旋转
    void on_btn_percussion_clicked();         // 开始冲击
    void on_btn_percussion_stop_clicked();    // 停止冲击
    void on_le_rotation_editingFinished();    // 设置旋转速度
    void on_le_percussion_editingFinished();  // 设置冲击频率
    void updateRotationStatus();              // 更新旋转状态
    void updatePercussionStatus();            // 更新冲击状态

    // 夹爪控制函数
    void on_btn_downclamp_open_clicked();     // 打开夹爪
    void on_btn_downclamp_close_clicked();    // 关闭夹爪
    void on_le_downclamp_DAC_editingFinished(); // 设置夹爪力矩
    void updateDownclampStatus();             // 更新夹爪状态显示

signals:
    void confirmationReceived(bool isConfirmed);

private:
    bool initflag;                                          // 正常初始化的标志 0-未初始化，1-已初始化
    /////基础信息显示/////
    float fBusType;                                         // 总线类型
    float fInitStatus;                                      // 总线初始化状态
    int   iNodeNum;                                         // 总线上的节点数量
    /////对比信息//////
    QString oldCellValue;                                   // 表格中旧的值
    int     oldRow;
    int     oldCol;
    int     basicAxisCB;
    /////极限限制//////
    int     limitDACRotate;
    int     limitDACDownClamp;
    int     limitPosTop;
    int     limitPosDown;
    /////钻管撑开//////
    mdbprocess *mdbProcessor;

private:
    Ui::zmotionpage *ui;
    QTimer *basicInfoTimer;                                 // 用于定时刷新基础的信息的定时器
    QTimer *advanceInfoTimer;                               // 用于定时刷新高级的信息的定时器
    QTimer *realtimeParmTimer;
    QList<QTableWidgetItem*> tableItems;                    // 用于存储对象
    MotionController* m_motionController;                   // 添加运动控制器成员变量

private:
    AutoModeThread *m_autoModeThread;
    bool m_isAutoModeRunning;
    void startAutoMode();
    void stopAutoMode();
    // QMutex m_confirmationMutex;
    // QWaitCondition m_confirmationWaitCondition;
    // bool m_confirmationReceived;
    void setUIEnabled(bool enabled);


    int parseHexOrDec(const QString &str);

    // 机械手控制相关变量
    int m_rotationMotorID;       // 旋转电机ID
    int m_extentMotorID;         // 伸出电机ID
    int m_clampMotorID;          // 夹爪电机ID
    
    float m_rotationOffset;      // 旋转角度偏移量
    float m_extentOffset;        // 伸出长度偏移量
    
    QTimer* m_robotArmStatusTimer;  // 机械手状态更新定时器

    // 进给机构深度控制相关变量
    int m_penetrationMotorID = 3;       // 进给电机ID（假设为3，根据实际情况调整）
    float m_penetrationSpeed = PENETRATION_DEFAULT_SPEED;  // 当前设定的进给速度
    double m_penetrationTargetDepth = 0.0;    // 目标钻进深度(mm)
    double m_penetrationOffset = 0.0;         // 进给深度偏移量(脉冲)

    // 钻管存储机构相关参数
    static constexpr int STORAGE_MOTOR_ID = 5;              // 存储机构电机ID
    static constexpr float STORAGE_SPEED = 10.0f;           // 存储机构旋转速度
    static constexpr float STORAGE_ACCEL = 50.0f;           // 存储机构加速度
    static constexpr float STORAGE_DECEL = 50.0f;           // 存储机构减速度
    static constexpr int STORAGE_POSITIONS = 7;             // 存储机构位置数量
    static constexpr float STORAGE_ANGLE_PER_POSITION = 360.0f / STORAGE_POSITIONS; // 每个位置的角度
    static constexpr float PULSES_PER_REVOLUTION = 10000.0f; // 电机每转脉冲数
    
    int m_storageCurrentPosition;                           // 当前存储机构位置（0-6）
    float m_storageOffset;                                  // 存储机构零点偏移量

    // 旋转和冲击相关参数
    static constexpr int ROTATION_MOTOR_ID = 6;     // 旋转电机ID
    static constexpr int PERCUSSION_MOTOR_ID = 7;   // 冲击电机ID
    static constexpr float ROTATION_ACCEL = 50.0f;  // 旋转加速度
    static constexpr float ROTATION_DECEL = 50.0f;  // 旋转减速度
    static constexpr float PERCUSSION_ACCEL = 100.0f; // 冲击加速度
    static constexpr float PERCUSSION_DECEL = 100.0f; // 冲击减速度
    static constexpr float DEFAULT_ROTATION_SPEED = 10.0f; // 默认旋转速度
    static constexpr float DEFAULT_PERCUSSION_FREQ = 5.0f; // 默认冲击频率
    
    float m_rotationSpeed;                    // 当前旋转速度
    float m_percussionFrequency;              // 当前冲击频率
    bool m_isRotating;                        // 是否正在旋转
    bool m_isPercussing;                      // 是否正在冲击

    float m_downclampDAC;        // 当前夹爪力矩值
    bool m_isDownclamping;       // 是否正在夹紧
    QTimer* m_downclampTimer;    // 夹爪状态监控定时器

    void initializeUI();  // 初始化UI组件
    void connectSignalsAndSlots();  // 连接信号和槽

};

class AutoModeThread : public QThread
{
    Q_OBJECT
public:
    explicit AutoModeThread(QObject *parent = nullptr);
    ~AutoModeThread();

    void stop();

signals:
    void requestConfirmation(); // 请求用户确认
    void operationCompleted();  // 自动模式完成
    void requestShowMotorMap(); // 请求显示电机映射（如果需要）
    void messageLogged(const QString& message); // 日志消息信号

public slots:
    void receiveConfirmation(bool confirmed); // 接收用户确认结果

protected:
    void run() override;

private:
    std::atomic<bool> m_stopFlag;
    QMutex m_mutex;
    QWaitCondition m_waitCondition;
    bool m_confirmed;

    // 禁用拷贝构造和赋值
    AutoModeThread(const AutoModeThread &) = delete;
    AutoModeThread &operator=(const AutoModeThread &) = delete;
};

#endif // ZMOTIONPAGE_H
