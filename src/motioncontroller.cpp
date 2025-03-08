#include "inc/motioncontroller.h"
#include <QTimer>
#include <QThread>
#include <QDebug>
#include <QFutureWatcher>
#include <QFuture>
#include <QEventLoop>
#include <QtConcurrent>
#include <cstring>
#include <QElapsedTimer>
#include <QMutexLocker>

MotionController::MotionController(QObject *parent)
    : QObject(parent)
    , m_handle(NULL)
    , m_connected(false)
    , m_debugMode(false)
{
    // 初始化电机名称
    initializeMotorNames();
    
    // 连接定时器信号
    connect(&m_updateTimer, &QTimer::timeout, this, &MotionController::onUpdateTimerTimeout);
}

MotionController::~MotionController()
{
    // 释放资源
    release();
}

/**
 * @brief 初始化控制器
 * @param ipAddress 控制器IP地址
 * @param debugMode 是否为调试模式
 * @return 是否初始化成功
 */
bool MotionController::initialize(const QString& ipAddress, bool debugMode)
{
    QMutexLocker locker(&m_mutex);
    
    // 设置调试模式标志
    m_debugMode = debugMode;
    
    // 如果是调试模式，则不连接实际控制器
    if (m_debugMode) {
        m_connected = true;
        emit connectionChanged(m_connected);
        emit commandResponse("调试模式: 已模拟连接控制器");
        
        // 在调试模式下启动定时器更新模拟数据
        m_updateTimer.start(1000); // 每秒更新一次
        
        return true;
    }
    
    // 如果已连接，先释放
    if (m_connected) {
        release();
    }
    
    // 连接控制器
    QByteArray ipBytes = ipAddress.toUtf8();
    int result = ZAux_OpenEth(ipBytes.data(), &m_handle);
    
    if (result != 0) {
        logError("初始化控制器", result);
        return false;
    }
    
    // 设置连接标志
    m_connected = true;
    
    // 发送连接状态变化信号
    emit connectionChanged(m_connected);
    emit commandResponse("已连接到控制器 " + ipAddress);
    
    // 启动定时器，定期更新电机状态
    m_updateTimer.start(1000); // 每秒更新一次
    
    return true;
}

/**
 * @brief 释放控制器资源
 */
void MotionController::release()
{
    QMutexLocker locker(&m_mutex);
    
    // 停止定时器
    m_updateTimer.stop();
    
    // 如果是调试模式，则只需重置标志
    if (m_debugMode) {
        m_connected = false;
        emit connectionChanged(m_connected);
        emit commandResponse("调试模式: 已断开模拟连接");
        return;
    }
    
    // 如果已连接，释放控制器
    if (m_connected && m_handle != NULL) {
        ZAux_Close(m_handle);
        m_handle = NULL;
    }
    
    // 重置连接标志
    m_connected = false;
    
    // 发送连接状态变化信号
    emit connectionChanged(m_connected);
    emit commandResponse("已断开与控制器的连接");
}

/**
 * @brief 检查是否已连接
 * @return 是否已连接
 */
bool MotionController::isConnected() const
{
    return m_connected;
}

/**
 * @brief 检查是否为调试模式
 * @return 是否为调试模式
 */
bool MotionController::isDebugMode() const
{
    return m_debugMode;
}

/**
 * @brief 生成调试模式下的模拟电机参数
 * @param motorID 电机ID
 * @return 模拟参数
 */
QMap<QString, float> MotionController::generateDebugMotorParameters(int motorID)
{
    QMap<QString, float> params;
    
    // 根据电机ID生成不同的模拟数据
    params["type"] = 1.0;  // 伺服电机
    params["enabled"] = 1.0;  // 已启用
    params["dpos"] = 0.0 + (motorID * 10.0);  // 目标位置
    params["mpos"] = 0.0 + (motorID * 10.0);  // 实际位置
    params["speed"] = 0.0;  // 目标速度
    params["mspeed"] = 0.0;  // 实际速度
    params["units"] = 1.0;  // 单位换算
    params["accel"] = 10.0;  // 加速度
    params["decel"] = 10.0;  // 减速度
    params["dac"] = 0.0;  // DAC值
    
    // 添加一些随机变化
    static int counter = 0;
    counter++;
    
    // 每次调用略微改变位置，模拟轻微运动
    if (counter % 10 == 0) {
        params["mpos"] += (rand() % 10) / 10.0;
        params["mspeed"] = (rand() % 100) / 10.0;
    }
    
    return params;
}

/**
 * @brief 获取电机参数
 * @param motorID 电机ID
 * @param params 输出参数
 * @return 是否成功
 */
bool MotionController::getMotorParameters(int motorID, QMap<QString, float> &params)
{
    QMutexLocker locker(&m_mutex);
    
    // 检查是否已连接
    if (!m_connected) {
        emit errorOccurred("未连接到控制器");
        return false;
    }
    
    // 如果是调试模式，返回模拟数据
    if (m_debugMode) {
        params = generateDebugMotorParameters(motorID);
        return true;
    }
    
    int iEN, iAType;
    float fMPos, fDPos, fMVel, fDVel, fDAC, fUnit, fAcc, fDec;
    
    int ret = 0;
    ret += ZAux_Direct_GetAtype(m_handle, motorID, &iAType);
    ret += ZAux_Direct_GetAxisEnable(m_handle, motorID, &iEN);
    ret += ZAux_Direct_GetDpos(m_handle, motorID, &fDPos);
    ret += ZAux_Direct_GetMpos(m_handle, motorID, &fMPos);
    ret += ZAux_Direct_GetSpeed(m_handle, motorID, &fDVel);
    ret += ZAux_Direct_GetMspeed(m_handle, motorID, &fMVel);
    ret += ZAux_Direct_GetUnits(m_handle, motorID, &fUnit);
    ret += ZAux_Direct_GetAccel(m_handle, motorID, &fAcc);
    ret += ZAux_Direct_GetDecel(m_handle, motorID, &fDec);
    ret += ZAux_Direct_GetDAC(m_handle, motorID, &fDAC);
    
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

/**
 * @brief 设置电机参数
 * @param motorID 电机ID
 * @param paramName 参数名称
 * @param value 参数值
 * @return 是否成功
 */
bool MotionController::setMotorParameter(int motorID, const QString &paramName, float value)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected) {
        emit commandResponse(tr("控制器未连接"));
        return false;
    }
    
    // 获取电机名称
    QString motorName = getMotorName(motorID);
    if (motorName.isEmpty()) {
        emit commandResponse(tr("无效的电机ID: %1").arg(motorID));
        return false;
    }
    
    // 构建命令并执行
    char command[100];
    char response[256] = {0};
    
    // 根据不同参数调用不同的API
    int result = 0;
    
    // 判断参数类型
    if (paramName == "Pos" || paramName == "DPos") {
        result = ZAux_Direct_SetDpos(m_handle, motorID, value);
    } else if (paramName == "MPos") {
        result = ZAux_Direct_SetMpos(m_handle, motorID, value);
    } else if (paramName == "Speed" || paramName == "Vel") {
        result = ZAux_Direct_SetSpeed(m_handle, motorID, value);
    } else if (paramName == "Acc") {
        result = ZAux_Direct_SetAccel(m_handle, motorID, value);
    } else if (paramName == "Dec") {
        result = ZAux_Direct_SetDecel(m_handle, motorID, value);
    } else if (paramName == "Unit") {
        result = ZAux_Direct_SetUnits(m_handle, motorID, value);
    } else if (paramName == "EN") {
        result = ZAux_Direct_SetAxisEnable(m_handle, motorID, (int)value);
    } else if (paramName == "Atype") {
        // 记录旧模式和新模式名称
        QString oldModeName = "未知模式";
        QString newModeName = "未知模式";
        
        // 获取当前模式
        int oldMode = 0;
        ZAux_Direct_GetAtype(m_handle, motorID, &oldMode);
        
        // 转换模式到名称
        if (oldMode == 65) oldModeName = "位置模式";
        else if (oldMode == 66) oldModeName = "速度模式";
        else if (oldMode == 67) oldModeName = "力矩模式";
        
        if (value == 65) newModeName = "位置模式";
        else if (value == 66) newModeName = "速度模式";
        else if (value == 67) newModeName = "力矩模式";
        
        // 检查是否对钻进电机或冲击电机（ID 0 和 1）以外的电机设置速度模式
        bool forcedCorrection = false;
        if (value == 66 && motorID != 0 && motorID != 1) {
            qDebug() << "警告: 尝试为非钻进/冲击电机设置速度模式，强制使用位置模式。";
            value = 65; // 强制使用位置模式
            newModeName = "位置模式";
            forcedCorrection = true;
        }
        
        result = ZAux_Direct_SetAtype(m_handle, motorID, (int)value);
        
        if (result == 0) {
            QString msg;
            if (forcedCorrection) {
                msg = tr("电机 %1 (ID: %2) 模式由 %3 被强制修改为 %4")
                      .arg(motorName).arg(motorID).arg(oldModeName).arg(newModeName);
            } else {
                msg = tr("电机 %1 (ID: %2) 模式从 %3 修改为 %4")
                      .arg(motorName).arg(motorID).arg(oldModeName).arg(newModeName);
            }
            emit commandResponse(msg);
        }
    } else {
        // 使用通用命令
        snprintf(command, sizeof(command), "%s(%d)=%f", paramName.toUtf8().data(), motorID, value);
        result = executeCommand(command, response, sizeof(response));
    }
    
    if (result != 0) {
        logError(tr("设置电机参数失败 (电机: %1, 参数: %2, 值: %3)").arg(motorName).arg(paramName).arg(value), result);
        return false;
    }
    
    if (paramName != "Atype") { // Atype已经发送了特殊消息
        emit commandResponse(tr("电机 %1 (ID: %2) 参数 %3 设置为 %4").arg(motorName).arg(motorID).arg(paramName).arg(value));
    }
    
    return true;
}

/**
 * @brief 移动电机到绝对位置
 * @param motorID 电机ID
 * @param position 目标位置
 * @return 是否成功
 */
bool MotionController::moveMotorAbsolute(int motorID, float position)
{
    QMutexLocker locker(&m_mutex);
    
    // 检查是否已连接
    if (!m_connected) {
        emit errorOccurred("未连接到控制器");
        return false;
    }
    
    // 调试模式下模拟操作
    if (m_debugMode) {
        QString motorName = getMotorName(motorID);
        QString cmdStr = QString("MOVEABS(%1,%2)").arg(motorID).arg(position);
        emit commandResponse(QString("调试模式: 电机%1(%2)移动到绝对位置%3").arg(motorID).arg(motorName).arg(position));
        
        // 生成并发送一个电机状态更新
        QMap<QString, float> params = generateDebugMotorParameters(motorID);
        params["dpos"] = position;  // 设置目标位置
        params["mpos"] = position;  // 在调试模式下，假设立即达到目标位置
        
        // 更新成功，调用回调函数
        if (m_callbacks.contains(motorID)) {
            m_callbacks[motorID](motorID, params);
        }
        
        emit motorStatusChanged(motorID, params);
        return true;
    }
    
    // 实际控制代码
    QString cmdStr = QString("MOVEABS(%1,%2)").arg(motorID).arg(position);
    
    // 执行命令
    bool success = executeCommand(cmdStr);
    if (!success) {
        emit errorOccurred(QString("移动电机%1到绝对位置%2失败").arg(motorID).arg(position));
        return false;
    }
    
    emit commandResponse(QString("电机%1(%2)开始移动到绝对位置%3").arg(motorID).arg(getMotorName(motorID)).arg(position));
    
    // 等待运动完成
    if (!waitForMotionComplete(motorID)) {
        emit errorOccurred(QString("电机%1(%2)移动到位置%3超时").arg(motorID).arg(getMotorName(motorID)).arg(position));
        return false;
    }
    
    // 检查是否到达目标位置
    if (!isAtPosition(motorID, position)) {
        emit errorOccurred(QString("电机%1(%2)未能精确到达目标位置%3").arg(motorID).arg(getMotorName(motorID)).arg(position));
        return false;
    }
    
    emit commandResponse(QString("电机%1(%2)已到达目标位置%3").arg(motorID).arg(getMotorName(motorID)).arg(position));
    return true;
}

/**
 * @brief 移动电机相对距离
 * @param motorID 电机ID
 * @param distance 相对距离
 * @return 是否成功
 */
bool MotionController::moveMotorRelative(int motorID, float distance)
{
    QMutexLocker locker(&m_mutex);
    
    // 检查是否已连接
    if (!m_connected) {
        emit errorOccurred("未连接到控制器");
        return false;
    }
    
    // 获取当前位置
    float currentPosition = getCurrentPosition(motorID);
    float targetPosition = currentPosition + distance;
    
    // 调试模式下模拟操作
    if (m_debugMode) {
        QString motorName = getMotorName(motorID);
        QString cmdStr = QString("MOVE(%1,%2)").arg(motorID).arg(distance);
        emit commandResponse(QString("调试模式: 电机%1(%2)相对移动%3").arg(motorID).arg(motorName).arg(distance));
        
        // 生成并发送一个电机状态更新
        QMap<QString, float> params = generateDebugMotorParameters(motorID);
        params["dpos"] = targetPosition;  // 设置目标位置
        params["mpos"] = targetPosition;  // 在调试模式下，假设立即达到目标位置
        
        // 更新成功，调用回调函数
        if (m_callbacks.contains(motorID)) {
            m_callbacks[motorID](motorID, params);
        }
        
        emit motorStatusChanged(motorID, params);
        return true;
    }
    
    // 实际控制代码
    QString cmdStr = QString("MOVE(%1,%2)").arg(motorID).arg(distance);
    
    // 执行命令
    bool success = executeCommand(cmdStr);
    if (!success) {
        emit errorOccurred(QString("移动电机%1相对距离%2失败").arg(motorID).arg(distance));
        return false;
    }
    
    emit commandResponse(QString("电机%1(%2)开始相对移动%3").arg(motorID).arg(getMotorName(motorID)).arg(distance));
    
    // 等待运动完成
    if (!waitForMotionComplete(motorID)) {
        emit errorOccurred(QString("电机%1(%2)相对移动%3超时").arg(motorID).arg(getMotorName(motorID)).arg(distance));
        return false;
    }
    
    // 检查是否到达目标位置
    if (!isAtPosition(motorID, targetPosition)) {
        emit errorOccurred(QString("电机%1(%2)未能精确到达目标位置%3").arg(motorID).arg(getMotorName(motorID)).arg(targetPosition));
        return false;
    }
    
    emit commandResponse(QString("电机%1(%2)已完成相对移动%3").arg(motorID).arg(getMotorName(motorID)).arg(distance));
    return true;
}

/**
 * @brief 停止电机
 * @param motorID 电机ID
 * @param stopMode 停止模式（0=减速停止，1=紧急停止）
 * @return 是否成功
 */
bool MotionController::stopMotor(int motorID, int stopMode)
{
    QMutexLocker locker(&m_mutex);
    
    // 检查是否已连接
    if (!m_connected) {
        emit errorOccurred("未连接到控制器");
        return false;
    }
    
    // 调试模式下模拟操作
    if (m_debugMode) {
        QString motorName = getMotorName(motorID);
        QString cmdStr = stopMode == 0 ? QString("CANCEL(%1)").arg(motorID) : QString("RAPIDSTOP(%1)").arg(motorID);
        emit commandResponse(QString("调试模式: 电机%1(%2)%3").arg(motorID).arg(motorName)
                            .arg(stopMode == 0 ? "减速停止" : "紧急停止"));
        
        // 生成并发送一个电机状态更新
        QMap<QString, float> params = generateDebugMotorParameters(motorID);
        params["speed"] = 0.0;  // 设置目标速度为0
        params["mspeed"] = 0.0; // 在调试模式下，假设立即停止
        
        // 更新成功，调用回调函数
        if (m_callbacks.contains(motorID)) {
            m_callbacks[motorID](motorID, params);
        }
        
        emit motorStatusChanged(motorID, params);
        return true;
    }
    
    // 实际控制代码
    QString cmdStr;
    if (stopMode == 0) {
        cmdStr = QString("CANCEL(%1)").arg(motorID);
    } else {
        cmdStr = QString("RAPIDSTOP(%1)").arg(motorID);
    }
    
    // 执行命令
    bool success = executeCommand(cmdStr);
    if (success) {
        emit commandResponse(QString("电机%1(%2)%3").arg(motorID).arg(getMotorName(motorID))
                            .arg(stopMode == 0 ? "减速停止" : "紧急停止"));
    } else {
        emit errorOccurred(QString("停止电机%1失败").arg(motorID));
    }
    
    return success;
}

/**
 * @brief 使能电机
 * @param motorID 电机ID
 * @param enable 是否使能
 * @return 是否成功
 */
bool MotionController::enableMotor(int motorID, bool enable)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected) {
        emit commandResponse(tr("控制器未连接"));
        return false;
    }
    
    // 获取电机名称
    QString motorName = getMotorName(motorID);
    if (motorName.isEmpty()) {
        emit commandResponse(tr("无效的电机ID: %1").arg(motorID));
        return false;
    }
    
    // 构建命令并执行
    char command[100];
    char response[256] = {0};
    snprintf(command, sizeof(command), "AXIS_ENABLE(%d,%d)", motorID, enable ? 1 : 0);
    
    int result = executeCommand(command, response, sizeof(response));
    if (result != 0) {
        logError(tr("%1电机失败 (电机: %2)").arg(enable ? "使能" : "禁用").arg(motorName), result);
        return false;
    }
    
    emit commandResponse(tr("电机 %1 (ID: %2) 已%3").arg(motorName).arg(motorID).arg(enable ? "使能" : "禁用"));
    return true;
}

/**
 * @brief 清除电机报警
 * @param motorID 电机ID
 * @return 是否成功
 */
bool MotionController::clearAlarm(int motorID)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected) {
        emit commandResponse(tr("控制器未连接"));
        return false;
    }
    
    // 获取电机名称
    QString motorName = getMotorName(motorID);
    if (motorName.isEmpty()) {
        emit commandResponse(tr("无效的电机ID: %1").arg(motorID));
        return false;
    }
    
    // 构建命令并执行
    char command[100];
    char response[256] = {0};
    snprintf(command, sizeof(command), "ALARM_A(%d)=0", motorID);
    
    int result = executeCommand(command, response, sizeof(response));
    if (result != 0) {
        logError(tr("清除报警失败 (电机: %1)").arg(motorName), result);
        return false;
    }
    
    emit commandResponse(tr("电机 %1 (ID: %2) 报警已清除").arg(motorName).arg(motorID));
    return true;
}

/**
 * @brief 设置电机零位
 * @param motorID 电机ID
 * @return 是否成功
 */
bool MotionController::setZeroPosition(int motorID)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected) {
        emit commandResponse(tr("控制器未连接"));
        return false;
    }
    
    // 获取电机名称
    QString motorName = getMotorName(motorID);
    if (motorName.isEmpty()) {
        emit commandResponse(tr("无效的电机ID: %1").arg(motorID));
        return false;
    }
    
    // 构建命令并执行
    char command[100];
    char response[256] = {0};
    snprintf(command, sizeof(command), "DEFPOS(%d,0)", motorID);
    
    int result = executeCommand(command, response, sizeof(response));
    if (result != 0) {
        logError(tr("设置零位失败 (电机: %1)").arg(motorName), result);
        return false;
    }
    
    emit commandResponse(tr("电机 %1 (ID: %2) 零位已设置").arg(motorName).arg(motorID));
    return true;
}

/**
 * @brief 暂停所有电机
 * @return 是否成功
 */
bool MotionController::pauseAllMotors()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected) {
        emit commandResponse(tr("控制器未连接"));
        return false;
    }
    
    // 构建命令并执行
    char command[100];
    char response[256] = {0};
    snprintf(command, sizeof(command), "SUSPEND");
    
    int result = executeCommand(command, response, sizeof(response));
    if (result != 0) {
        logError(tr("暂停所有电机失败"), result);
        return false;
    }
    
    emit commandResponse(tr("所有电机已暂停"));
    return true;
}

/**
 * @brief 恢复所有电机
 * @return 是否成功
 */
bool MotionController::resumeAllMotors()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected) {
        emit commandResponse(tr("控制器未连接"));
        return false;
    }
    
    // 构建命令并执行
    char command[100];
    char response[256] = {0};
    snprintf(command, sizeof(command), "RESUME");
    
    int result = executeCommand(command, response, sizeof(response));
    if (result != 0) {
        logError(tr("恢复所有电机失败"), result);
        return false;
    }
    
    emit commandResponse(tr("所有电机已恢复"));
    return true;
}

/**
 * @brief 停止所有电机
 * @param stopMode 停止模式 (0=减速停止, 1=立即停止)
 * @return 是否成功
 */
bool MotionController::stopAllMotors(int stopMode)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected) {
        emit commandResponse(tr("控制器未连接"));
        return false;
    }
    
    // 构建命令并执行
    char command[100];
    char response[256] = {0};
    
    if (stopMode == 0) {
        // 减速停止
        snprintf(command, sizeof(command), "STOP");
    } else {
        // 立即停止
        snprintf(command, sizeof(command), "RAPIDSTOP");
    }
    
    int result = executeCommand(command, response, sizeof(response));
    if (result != 0) {
        logError(tr("停止所有电机失败 (模式: %1)").arg(stopMode), result);
        return false;
    }
    
    emit commandResponse(tr("所有电机已停止 (模式: %1)").arg(stopMode));
    return true;
}

/**
 * @brief 注册电机状态回调
 * @param motorID 电机ID
 * @param callback 回调函数
 */
void MotionController::registerMotionCallback(int motorID, MotionCallback callback)
{
    QMutexLocker locker(&m_mutex);
    m_callbacks[motorID] = callback;
}

/**
 * @brief 取消注册电机状态回调
 * @param motorID 电机ID
 */
void MotionController::unregisterMotionCallback(int motorID)
{
    QMutexLocker locker(&m_mutex);
    m_callbacks.remove(motorID);
}

/**
 * @brief 更新电机状态
 * @param motorID 电机ID
 */
void MotionController::updateMotorStatus(int motorID)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected || !m_callbacks.contains(motorID)) {
        return;
    }
    
    QMap<QString, float> params;
    if (getMotorParameters(motorID, params)) {
        // 发出信号通知状态变化
        emit motorStatusChanged(motorID, params);
        
        // 调用注册的回调函数
        m_callbacks[motorID](motorID, params);
    }
}

/**
 * @brief 更新所有电机状态
 */
void MotionController::updateAllMotorStatus()
{
    if (!m_connected) {
        return;
    }
    
    // 更新所有注册了回调的电机状态
    QList<int> motorIDs = m_callbacks.keys();
    foreach (int motorID, motorIDs) {
        updateMotorStatus(motorID);
    }
}

/**
 * @brief 定时器超时处理函数
 */
void MotionController::onUpdateTimerTimeout()
{
    // 调试模式下，为每个电机生成模拟数据
    if (m_debugMode) {
        // 模拟更新所有电机状态
        for (int motorID = 0; motorID <= 9; motorID++) {
            QMap<QString, float> params = generateDebugMotorParameters(motorID);
            
            // 更新成功，调用回调函数
            if (m_callbacks.contains(motorID)) {
                m_callbacks[motorID](motorID, params);
            }
            
            emit motorStatusChanged(motorID, params);
        }
        return;
    }
    
    // 真实模式下，获取实际电机状态
    updateAllMotorStatus();
}

/**
 * @brief 执行命令
 * @param cmdStr 命令字符串
 * @return 是否成功
 */
bool MotionController::executeCommand(const QString& cmdStr)
{
    // 调试模式下模拟执行
    if (m_debugMode) {
        // 记录命令
        logCommand(cmdStr);
        
        // 模拟命令执行成功
        return true;
    }
    
    // 实际控制代码
    QByteArray cmdBytes = cmdStr.toUtf8();
    char response[256] = {0};
    
    int result = ZAux_Execute(m_handle, cmdBytes.data(), response, sizeof(response));
    
    if (result != 0) {
        logError(QString("执行命令失败: %1").arg(cmdStr), result);
        return false;
    }
    
    if (response[0] != '\0') {
        logResponse(QString::fromUtf8(response));
    }
    
    return true;
}

/**
 * @brief 记录命令
 * @param command 命令字符串
 */
void MotionController::logCommand(const QString& command)
{
    qDebug() << "发送命令:" << command;
    
    if (m_debugMode) {
        emit commandResponse(QString("调试模式: 发送命令: %1").arg(command));
    }
}

/**
 * @brief 记录响应
 * @param response 响应字符串
 */
void MotionController::logResponse(const QString& response)
{
    qDebug() << "命令响应:" << response;
    emit commandResponse(response);
}

/**
 * @brief 执行ZMC命令
 * @param command 命令字符串
 * @param response 响应缓冲区
 * @param responseLength 响应缓冲区长度
 * @return 错误码，0表示成功
 */
int MotionController::executeCommand(const char* command, char* response, uint32_t responseLength)
{
    if (!m_connected || m_handle == 0) {
        return -1;
    }
    
    // 调试模式下模拟执行
    if (m_debugMode) {
        logCommand(QString::fromUtf8(command));
        
        // 模拟成功响应
        if (response && responseLength > 0) {
            strcpy(response, "OK");
        }
        return 0;
    }
    
    int result = ZAux_Execute(m_handle, command, response, responseLength);
    
    if (response[0] != '\0') {
        logResponse(QString::fromUtf8(response));
    }
    
    return result;
}

/**
 * @brief 记录错误日志
 * @param operation 操作描述
 * @param errorCode 错误码
 */
void MotionController::logError(const QString &operation, int errorCode)
{
    QString errorMsg = tr("%1 失败，错误码: %2").arg(operation).arg(errorCode);
    qDebug() << "错误: " << errorMsg;
    emit commandResponse(errorMsg);
}

/**
 * @brief 初始化电机名称映射
 */
void MotionController::initializeMotorNames()
{
    // 根据项目需求设置电机名称
    m_motorNames[0] = tr("钻进电机");
    m_motorNames[1] = tr("冲击电机");
    m_motorNames[2] = tr("摇臂旋转");
    m_motorNames[3] = tr("摇臂伸缩");
    m_motorNames[4] = tr("钻杆夹紧");
    m_motorNames[5] = tr("钻杆旋转");
    m_motorNames[6] = tr("钻杆夹紧2");
    m_motorNames[7] = tr("上料电机");
    m_motorNames[8] = tr("钻杆升降");
    m_motorNames[9] = tr("预留电机");
}

/**
 * @brief 获取电机名称
 * @param motorID 电机ID
 * @return 电机名称
 */
QString MotionController::getMotorName(int motorID) const
{
    return m_motorNames.value(motorID, QString());
}

/**
 * @brief 等待电机运动完成
 * @param motorID 电机ID
 * @param timeout 超时时间（毫秒）
 * @return 是否成功完成运动
 */
bool MotionController::waitForMotionComplete(int motorID, int timeout)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected) {
        emit errorOccurred(tr("控制器未连接"));
        return false;
    }
    
    // 调试模式下直接返回成功
    if (m_debugMode) {
        emit commandResponse(tr("调试模式: 电机 %1 (%2) 运动完成").arg(motorID).arg(getMotorName(motorID)));
        return true;
    }
    
    // 记录开始时间
    QElapsedTimer timer;
    timer.start();
    
    // 循环检查运动是否完成
    while (!checkEndMove(motorID)) {
        // 检查是否超时
        if (timer.elapsed() > timeout) {
            emit errorOccurred(tr("等待电机 %1 (%2) 运动完成超时").arg(motorID).arg(getMotorName(motorID)));
            return false;
        }
        
        // 暂停一小段时间，避免过度占用CPU
        QThread::msleep(10);
    }
    
    emit commandResponse(tr("电机 %1 (%2) 运动完成").arg(motorID).arg(getMotorName(motorID)));
    return true;
}

/**
 * @brief 检查电机运动是否完成
 * @param motorID 电机ID
 * @return 是否完成运动
 */
bool MotionController::isMotionComplete(int motorID) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected) {
        return false;
    }
    
    // 调试模式下直接返回true
    if (m_debugMode) {
        return true;
    }
    
    return checkEndMove(motorID);
}

/**
 * @brief 检查电机是否到达指定位置
 * @param motorID 电机ID
 * @param targetPosition 目标位置
 * @param tolerance 容差范围
 * @return 是否到达目标位置
 */
bool MotionController::isAtPosition(int motorID, float targetPosition, float tolerance) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected) {
        return false;
    }
    
    // 调试模式下直接返回true
    if (m_debugMode) {
        return true;
    }
    
    return checkPositionReached(motorID, targetPosition, tolerance);
}

/**
 * @brief 获取电机当前位置
 * @param motorID 电机ID
 * @return 当前位置
 */
float MotionController::getCurrentPosition(int motorID) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected) {
        return 0.0f;
    }
    
    // 调试模式下返回模拟数据
    if (m_debugMode) {
        QMap<QString, float> params = const_cast<MotionController*>(this)->generateDebugMotorParameters(motorID);
        return params["mpos"];
    }
    
    float position = 0.0f;
    if (ZAux_Direct_GetMpos(m_handle, motorID, &position) != 0) {
        return 0.0f;
    }
    
    return position;
}

/**
 * @brief 获取电机目标位置
 * @param motorID 电机ID
 * @return 目标位置
 */
float MotionController::getTargetPosition(int motorID) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected) {
        return 0.0f;
    }
    
    // 调试模式下返回模拟数据
    if (m_debugMode) {
        QMap<QString, float> params = const_cast<MotionController*>(this)->generateDebugMotorParameters(motorID);
        return params["dpos"];
    }
    
    float position = 0.0f;
    if (ZAux_Direct_GetDpos(m_handle, motorID, &position) != 0) {
        return 0.0f;
    }
    
    return position;
}

/**
 * @brief 检查电机运动是否完成（内部辅助函数）
 * @param motorID 电机ID
 * @return 是否完成运动
 */
bool MotionController::checkEndMove(int motorID) const
{
    float endMove = 0.0f;
    if (ZAux_Direct_GetEndMove(m_handle, motorID, &endMove) != 0) {
        return false;
    }
    return endMove != 0.0f;
}

/**
 * @brief 检查电机是否到达指定位置（内部辅助函数）
 * @param motorID 电机ID
 * @param targetPosition 目标位置
 * @param tolerance 容差范围
 * @return 是否到达目标位置
 */
bool MotionController::checkPositionReached(int motorID, float targetPosition, float tolerance) const
{
    float currentPosition = 0.0f;
    if (ZAux_Direct_GetMpos(m_handle, motorID, &currentPosition) != 0) {
        return false;
    }
    
    return std::abs(currentPosition - targetPosition) <= tolerance;
}

void MotionController::setControllerHandle(ZMC_HANDLE handle)
{
    QMutexLocker locker(&m_mutex);
    m_handle = handle;
    m_connected = (handle != NULL);
    emit connectionChanged(m_connected);
    
    if (m_connected) {
        emit commandResponse("已连接到控制器");
        // 启动定时器，定期更新电机状态
        m_updateTimer.start(1000); // 每秒更新一次
    } else {
        m_updateTimer.stop();
        emit commandResponse("已断开与控制器的连接");
    }
} 