#include "inc/motioncontroller.h"
#include <QTimer>
#include <QThread>
#include <QDebug>
#include <QFutureWatcher>
#include <QFuture>
#include <QEventLoop>
#include <QtConcurrent>

MotionController::MotionController(QObject *parent)
    : QObject(parent)
    , m_handle(NULL)
    , m_initialized(false)
    , m_axisNum(0)
    , m_nodeNum(0)
    , m_updateTimer(new QTimer(this))
{
    // 初始化电机映射表
    m_motorMap = {9, 8, 0, 1, 2, 3, 4, 5, 6, 7};
    
    // 创建更新定时器
    connect(m_updateTimer, &QTimer::timeout, this, &MotionController::updateAllMotorParameters);
    m_updateTimer->setInterval(100); // 100ms更新间隔
}

MotionController::~MotionController()
{
    if (m_handle != NULL) {
        disconnectController();
    }
    delete m_updateTimer;
}

bool MotionController::connectController(const QString &ipAddress)
{
    // TODO: 实现实际的控制器连接逻辑
    m_handle = (ZMC_HANDLE)1; // 模拟连接成功
    emit connectionChanged(true);
    return true;
}

void MotionController::disconnectController()
{
    if (m_handle != NULL) {
        // TODO: 实现实际的断开连接逻辑
        m_handle = NULL;
        m_initialized = false;
        emit connectionChanged(false);
        emit initializationChanged(false);
    }
}

bool MotionController::initializeBus()
{
    if (!checkConnection()) return false;
    
    // TODO: 实现实际的总线初始化逻辑
    m_initialized = true;
    emit initializationChanged(true);
    return true;
}

bool MotionController::getMotorParameters(int motorID, QMap<QString, float> &params)
{
    if (!checkConnection() || !m_initialized) return false;
    
    int mappedID = m_motorMap[motorID];
    int iEN, iAType;
    float fMPos, fDPos, fMVel, fDVel, fDAC, fUnit, fAcc, fDec;
    
    int ret = 0;
    ret += ZAux_Direct_GetAtype(m_handle, mappedID, &iAType);
    ret += ZAux_Direct_GetAxisEnable(m_handle, mappedID, &iEN);
    ret += ZAux_Direct_GetDpos(m_handle, mappedID, &fDPos);
    ret += ZAux_Direct_GetMpos(m_handle, mappedID, &fMPos);
    ret += ZAux_Direct_GetSpeed(m_handle, mappedID, &fDVel);
    ret += ZAux_Direct_GetMspeed(m_handle, mappedID, &fMVel);
    ret += ZAux_Direct_GetUnits(m_handle, mappedID, &fUnit);
    ret += ZAux_Direct_GetAccel(m_handle, mappedID, &fAcc);
    ret += ZAux_Direct_GetDecel(m_handle, mappedID, &fDec);
    ret += ZAux_Direct_GetDAC(m_handle, mappedID, &fDAC);
    
    if (ret != 0) {
        logError(QString("获取电机 %1 参数").arg(motorID), ret);
        return false;
    }
    
    params["EN"] = iEN;
    params["MPos"] = fMPos;
    params["Pos"] = fDPos;
    params["MVel"] = fMVel;
    params["Vel"] = fDVel;
    params["DAC"] = fDAC;
    params["Atype"] = iAType;
    params["Unit"] = fUnit;
    params["Acc"] = fAcc;
    params["Dec"] = fDec;
    
    return true;
}

bool MotionController::setMotorParameter(int motorID, const QString &paramName, float value)
{
    if (!checkConnection()) return false;
    
    // TODO: 实现实际的参数设置逻辑
    qDebug() << "设置电机" << motorID << "参数" << paramName << "为" << value;
    return true;
}

bool MotionController::moveMotorAbsolute(int motorID, float position)
{
    if (!checkConnection()) return false;
    
    // TODO: 实现实际的绝对运动逻辑
    qDebug() << "电机" << motorID << "移动到绝对位置" << position;
    return true;
}

bool MotionController::moveMotorRelative(int motorID, float distance)
{
    if (!checkConnection()) return false;
    
    // TODO: 实现实际的相对运动逻辑
    qDebug() << "电机" << motorID << "相对移动" << distance;
    return true;
}

bool MotionController::stopMotor(int motorID, int stopMode)
{
    if (!checkConnection()) return false;
    
    // TODO: 实现实际的停止逻辑
    qDebug() << "停止电机" << motorID << "模式" << stopMode;
    return true;
}

bool MotionController::enableMotor(int motorID, bool enable)
{
    if (!checkConnection()) return false;
    
    // TODO: 实现实际的使能逻辑
    qDebug() << "电机" << motorID << (enable ? "使能" : "失能");
    return true;
}

bool MotionController::clearAlarm(int motorID)
{
    if (!checkConnection()) return false;
    
    // TODO: 实现实际的报警清除逻辑
    qDebug() << "清除电机" << motorID << "报警";
    return true;
}

bool MotionController::setZeroPosition(int motorID)
{
    if (!checkConnection()) return false;
    
    // TODO: 实现实际的零位设置逻辑
    qDebug() << "设置电机" << motorID << "零位";
    return true;
}

bool MotionController::pauseAllMotors()
{
    if (!checkConnection()) return false;
    
    // TODO: 实现实际的暂停逻辑
    qDebug() << "暂停所有电机";
    return true;
}

bool MotionController::resumeAllMotors()
{
    if (!checkConnection()) return false;
    
    // TODO: 实现实际的恢复逻辑
    qDebug() << "恢复所有电机";
    return true;
}

bool MotionController::stopAllMotors(int stopMode)
{
    if (!checkConnection()) return false;
    
    // TODO: 实现实际的停止逻辑
    qDebug() << "停止所有电机，模式" << stopMode;
    return true;
}

void MotionController::updateMotorMap(const QVector<int>& newMap)
{
    m_motorMap = newMap;
}

bool MotionController::executeCommand(const QString &command, QString &response)
{
    if (!checkConnection()) return false;
    
    // TODO: 实现实际的命令执行逻辑
    response = "命令执行成功";
    emit commandResponse(response);
    return true;
}

void MotionController::registerMotionCallback(int motorID, MotionCallback callback)
{
    m_motionCallbacks[motorID] = callback;
}

void MotionController::unregisterMotionCallback(int motorID)
{
    m_motionCallbacks.remove(motorID);
}

void MotionController::updateMotorStatus(int motorID)
{
    if (!m_motionCallbacks.contains(motorID)) return;
    
    QMap<QString, float> params;
    if (getMotorParameters(motorID, params)) {
        m_motionCallbacks[motorID](params);
    }
}

void MotionController::updateAllMotorParameters()
{
    if (!checkConnection() || !m_initialized) return;
    
    // 更新所有电机状态
    for (int i = 0; i < m_axisNum; i++) {
        updateMotorStatus(i);
    }
}

bool MotionController::checkConnection()
{
    if (m_handle == NULL) {
        emit errorOccurred("控制器未连接");
        return false;
    }
    return true;
}

void MotionController::logError(const QString &operation, int errorCode)
{
    emit errorOccurred(QString("%1 失败，错误码: %2").arg(operation).arg(errorCode));
} 