#ifndef MOTIONCONTROLLER_H
#define MOTIONCONTROLLER_H

#include <QObject>
#include <QVector>
#include <QMap>
#include <atomic>
#include <functional>
#include "inc/zmcaux.h"
#include "inc/zmotion.h"

// 前向声明
class QTimer;

class MotionController : public QObject
{
    Q_OBJECT

public:
    explicit MotionController(QObject *parent = nullptr);
    ~MotionController();

    // 连接与初始化
    bool connectController(const QString &ipAddress);
    void disconnectController();
    bool initializeBus();
    bool isConnected() const { return m_handle != NULL; }
    bool isInitialized() const { return m_initialized; }

    // 基本参数获取
    float getAxisNum() const { return m_axisNum; }
    int getNodeNum() const { return m_nodeNum; }
    
    // 电机参数操作
    bool getMotorParameters(int motorID, QMap<QString, float> &params);
    bool setMotorParameter(int motorID, const QString &paramName, float value);
    bool moveMotorAbsolute(int motorID, float position);
    bool moveMotorRelative(int motorID, float distance);
    bool stopMotor(int motorID, int stopMode = 2);
    bool enableMotor(int motorID, bool enable);
    bool clearAlarm(int motorID);
    bool setZeroPosition(int motorID);
    
    // 全局操作
    bool pauseAllMotors();
    bool resumeAllMotors();
    bool stopAllMotors(int stopMode = 2);
    
    // 获取映射表
    const QVector<int>& getMotorMap() const { return m_motorMap; }
    void updateMotorMap(const QVector<int>& newMap);
    
    // 执行命令
    bool executeCommand(const QString &command, QString &response);

    // 添加回调注册接口
    using MotionCallback = std::function<void(const QMap<QString, float>&)>;
    void registerMotionCallback(int motorID, MotionCallback callback);
    void unregisterMotionCallback(int motorID);

signals:
    void connectionChanged(bool connected);
    void initializationChanged(bool initialized);
    void motorParametersUpdated(int motorID, const QMap<QString, float> &params);
    void allMotorParametersUpdated(const QVector<QMap<QString, float>> &allParams);
    void commandResponse(const QString &response);
    void errorOccurred(const QString &errorMessage);

public slots:
    void updateAllMotorParameters();

private:
    ZMC_HANDLE m_handle;
    bool m_initialized;
    float m_axisNum;
    int m_nodeNum;
    QVector<int> m_motorMap;
    QTimer *m_updateTimer;
    
    // 辅助方法
    bool checkConnection();
    void logError(const QString &operation, int errorCode);

    QMap<int, MotionCallback> m_motionCallbacks;
    void updateMotorStatus(int motorID);
};

#endif // MOTIONCONTROLLER_H 