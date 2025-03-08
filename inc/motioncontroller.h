#ifndef MOTIONCONTROLLER_H
#define MOTIONCONTROLLER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QString>
#include <functional>
#include <QTimer>
#include <QRecursiveMutex>
#include "zmcaux.h"
#include "DrillingParameters.h"

// 预定义 ZMC_HANDLE 类型
typedef void* ZMC_HANDLE;

// 假设的 ZAux API 函数原型
extern "C" {
    int ZAux_Direct_GetAtype(ZMC_HANDLE handle, int axis, int* iValue);
    int ZAux_Direct_GetAxisEnable(ZMC_HANDLE handle, int axis, int* iValue);
    int ZAux_Direct_GetDpos(ZMC_HANDLE handle, int axis, float* fValue);
    int ZAux_Direct_GetMpos(ZMC_HANDLE handle, int axis, float* fValue);
    int ZAux_Direct_GetSpeed(ZMC_HANDLE handle, int axis, float* fValue);
    int ZAux_Direct_GetMspeed(ZMC_HANDLE handle, int axis, float* fValue);
    int ZAux_Direct_GetUnits(ZMC_HANDLE handle, int axis, float* fValue);
    int ZAux_Direct_GetAccel(ZMC_HANDLE handle, int axis, float* fValue);
    int ZAux_Direct_GetDecel(ZMC_HANDLE handle, int axis, float* fValue);
    int ZAux_Direct_GetDAC(ZMC_HANDLE handle, int axis, float* fValue);
}

/**
 * @brief 运动控制器类，封装与运动控制卡的通信
 */
class MotionController : public QObject
{
    Q_OBJECT

public:
    // 定义回调函数类型，用于通知电机状态变化
    typedef std::function<void(int motorID, const QMap<QString, float>&)> MotionCallback;

    // 定义位置到达判断的容差范围（默认为0.1单位）
    static constexpr float POSITION_TOLERANCE = 0.1f;
    
    // 定义运动完成检查的超时时间（默认为10秒）
    static constexpr int MOTION_TIMEOUT = 10000;

    explicit MotionController(QObject *parent = nullptr);
    virtual ~MotionController();

    // 连接和初始化
    bool initialize(const QString &ipAddress = "192.168.0.11", bool debugMode = false);
    void release();
    bool isConnected() const;
    bool isDebugMode() const;
    
    // 设置控制器句柄
    void setControllerHandle(ZMC_HANDLE handle);

    // 电机参数操作
    bool getMotorParameters(int motorID, QMap<QString, float> &params);
    bool setMotorParameter(int motorID, const QString &paramName, float value);

    // 电机运动控制
    bool moveMotorAbsolute(int motorID, float position);
    bool moveMotorRelative(int motorID, float distance);
    bool stopMotor(int motorID, int stopMode = 0);
    bool enableMotor(int motorID, bool enable);
    bool clearAlarm(int motorID);
    bool setZeroPosition(int motorID);

    // 全局操作
    bool pauseAllMotors();
    bool resumeAllMotors();
    bool stopAllMotors(int stopMode = 0);

    // 电机状态回调注册
    void registerMotionCallback(int motorID, MotionCallback callback);
    void unregisterMotionCallback(int motorID);
    
    // 更新电机状态
    void updateMotorStatus(int motorID);
    void updateAllMotorStatus();
    
    // 获取电机名称
    QString getMotorName(int motorID) const;

    // 运动状态反馈相关方法
    bool waitForMotionComplete(int motorID, int timeout = MOTION_TIMEOUT);
    bool isMotionComplete(int motorID) const;
    bool isAtPosition(int motorID, float targetPosition, float tolerance = POSITION_TOLERANCE) const;
    float getCurrentPosition(int motorID) const;
    float getTargetPosition(int motorID) const;

signals:
    void connectionChanged(bool connected);
    void commandResponse(const QString &response);
    void motorStatusChanged(int motorID, const QMap<QString, float> &params);
    void errorOccurred(const QString &errorMessage);

private slots:
    // 定时更新电机状态
    void onUpdateTimerTimeout();

protected:
    // 记录错误日志
    void logError(const QString &operation, int errorCode);
    
    // 初始化电机名称映射
    void initializeMotorNames();
    
    // 命令执行
    int executeCommand(const char* command, char* response, uint32_t responseLength);
    
    // 简化的命令执行，主要用于调试模式
    bool executeCommand(const QString& cmdStr);
    
    // 日志辅助函数
    void logCommand(const QString& command);
    void logResponse(const QString& response);

private:
    // ZMC控制器句柄
    ZMC_HANDLE m_handle;
    
    // 连接状态
    bool m_connected;
    
    // 互斥锁，用于保护控制器操作
    mutable QRecursiveMutex m_mutex;
    
    // 电机回调函数映射
    QMap<int, MotionCallback> m_callbacks;
    
    // 电机名称映射
    QMap<int, QString> m_motorNames;
    
    // 定时器，用于定期更新电机状态
    QTimer m_updateTimer;

    // 调试模式
    bool m_debugMode;

    // 为调试模式生成模拟的电机参数
    QMap<QString, float> generateDebugMotorParameters(int motorID);

    // 运动状态检查辅助函数
    bool checkEndMove(int motorID) const;
    bool checkPositionReached(int motorID, float targetPosition, float tolerance) const;
};

#endif // MOTIONCONTROLLER_H