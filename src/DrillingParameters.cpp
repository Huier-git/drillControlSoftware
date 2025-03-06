#include "inc/DrillingParameters.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

// 静态成员初始化
DrillingParameters* DrillingParameters::m_instance = nullptr;

/**
 * @brief 获取单例实例
 * @return 参数管理器实例
 */
DrillingParameters* DrillingParameters::getInstance()
{
    if (!m_instance) {
        m_instance = new DrillingParameters();
    }
    return m_instance;
}

/**
 * @brief 构造函数
 * @param parent 父对象
 */
DrillingParameters::DrillingParameters(QObject* parent)
    : QObject(parent)
{
    // 初始化默认参数
    QMap<QString, QVariant> robotParams;
    robotParams["DRILL_POSITION"] = RobotPosition::DRILL_POSITION;
    robotParams["STORAGE_POSITION"] = RobotPosition::STORAGE_POSITION;
    robotParams["EXTENDED"] = RobotPosition::EXTENDED;
    robotParams["RETRACTED"] = RobotPosition::RETRACTED;
    robotParams["CLAMPED"] = RobotPosition::CLAMPED;
    robotParams["RELEASED"] = RobotPosition::RELEASED;
    m_parameters["Robot"] = robotParams;
    
    QMap<QString, QVariant> drillParams;
    drillParams["DEFAULT_SPEED"] = DrillParameters::DEFAULT_SPEED;
    drillParams["MAX_SPEED"] = DrillParameters::MAX_SPEED;
    drillParams["PERCUSSION_FREQ"] = DrillParameters::PERCUSSION_FREQ;
    drillParams["MAX_PERCUSSION"] = DrillParameters::MAX_PERCUSSION;
    drillParams["WORK_SPEED"] = 120.0;         // 工作转速 (rpm)
    drillParams["CONNECTION_SPEED"] = 60.0;    // 对接转速 (rpm)
    m_parameters["Drill"] = drillParams;
    
    QMap<QString, QVariant> penetrationParams;
    penetrationParams["HOME_POSITION"] = PenetrationParameters::HOME_POSITION;
    penetrationParams["WORK_POSITION"] = PenetrationParameters::WORK_POSITION;
    penetrationParams["RETRACT_POSITION"] = PenetrationParameters::RETRACT_POSITION;
    penetrationParams["DEFAULT_SPEED"] = PenetrationParameters::DEFAULT_SPEED;
    // 新增关键位置参数
    penetrationParams["INITIAL_POSITION"] = 0.0;      // 初始位置 (mm)
    penetrationParams["WORK_POSITION"] = 1315.0;      // 工作位置 (mm)
    penetrationParams["TOOL_INSTALL_START"] = 610.0;  // 钻具安装起始位置 (mm)
    penetrationParams["TOOL_INSTALL_END"] = 580.0;    // 钻具安装结束位置 (mm)
    penetrationParams["DISCONNECT_POSITION"] = 30.0;  // 断开位置 (mm)
    penetrationParams["STANDBY_POSITION"] = 650.0;    // 待机位置 (mm)
    penetrationParams["PIPE_INSTALL_START"] = 580.0;  // 钻管安装起始位置 (mm)
    penetrationParams["PIPE_INSTALL_MID"] = 550.0;    // 钻管安装中间位置 (mm)
    penetrationParams["PIPE_INSTALL_END"] = 520.0;    // 钻管安装结束位置 (mm)
    penetrationParams["PIPE_REMOVAL_START"] = 550.0;  // 钻管拆卸起始位置 (mm)
    penetrationParams["PIPE_REMOVAL_MID"] = 580.0;    // 钻管拆卸中间位置 (mm)
    penetrationParams["PIPE_REMOVAL_END"] = 610.0;    // 钻管拆卸结束位置 (mm)
    penetrationParams["TOOL_RECOVERY_END"] = 640.0;   // 钻具回收结束位置 (mm)
    m_parameters["Penetration"] = penetrationParams;
    
    QMap<QString, QVariant> clampConnParams;
    clampConnParams["CLAMP_RELEASED"] = ClampConnectionParameters::CLAMP_RELEASED;
    clampConnParams["CLAMP_ENGAGED"] = ClampConnectionParameters::CLAMP_ENGAGED;
    clampConnParams["CONNECTION_DISENGAGED"] = ClampConnectionParameters::CONNECTION_DISENGAGED;
    clampConnParams["CONNECTION_READY"] = ClampConnectionParameters::CONNECTION_READY;
    clampConnParams["CONNECTION_ENGAGED"] = ClampConnectionParameters::CONNECTION_ENGAGED;
    clampConnParams["CONNECTION_EXTENSION"] = 15.0;   // 对接机构伸出距离 (mm)
    m_parameters["ClampConnection"] = clampConnParams;
    
    // 电机ID参数
    QMap<QString, QVariant> motorIdParams;
    motorIdParams["STORAGE"] = MotorID::STORAGE;
    motorIdParams["ROBOT_ROTATION"] = MotorID::ROBOT_ROTATION;
    motorIdParams["ROBOT_EXTENSION"] = MotorID::ROBOT_EXTENSION;
    motorIdParams["ROBOT_CLAMP"] = MotorID::ROBOT_CLAMP;
    motorIdParams["DRILL"] = MotorID::DRILL;
    motorIdParams["PERCUSSION"] = MotorID::PERCUSSION;
    motorIdParams["PENETRATION"] = MotorID::PENETRATION;
    motorIdParams["CLAMP"] = MotorID::CLAMP;
    motorIdParams["CONNECTION"] = MotorID::CONNECTION;
    m_parameters["MotorID"] = motorIdParams;
    
    // 电机模式参数
    QMap<QString, QVariant> motorModeParams;
    motorModeParams["POSITION"] = MotorMode::POSITION;
    motorModeParams["VELOCITY"] = MotorMode::VELOCITY;
    motorModeParams["TORQUE"] = MotorMode::TORQUE;
    m_parameters["MotorMode"] = motorModeParams;
    
    // 机械手位置参数
    QMap<QString, QVariant> robotPosParams;
    robotPosParams["ROTATION_DRILL"] = 0.0;       // 对准钻台位置 (度)
    robotPosParams["ROTATION_STORAGE"] = 90.0;    // 对准存储位置 (度)
    robotPosParams["EXTENSION_RETRACTED"] = 0.0;  // 完全缩回位置 (mm)
    robotPosParams["EXTENSION_STORAGE"] = 200.0;  // 存储区伸出位置 (mm)
    robotPosParams["EXTENSION_DRILL"] = 250.0;    // 钻台伸出位置 (mm)
    robotPosParams["CLAMP_RELEASED"] = 0.0;       // 夹持器松开位置
    robotPosParams["CLAMP_ENGAGED"] = 100.0;      // 夹持器夹紧位置
    m_parameters["RobotPosition"] = robotPosParams;
    
    // 速度参数
    QMap<QString, QVariant> speedParams;
    speedParams["V1"] = 0.01;                     // 钻进速度 (m/s)
    speedParams["V2"] = 0.05;                     // 对接速度 (m/s)
    speedParams["V3"] = 0.1;                      // 空行程速度 (m/s)
    m_parameters["Speed"] = speedParams;
}

/**
 * @brief 析构函数
 */
DrillingParameters::~DrillingParameters()
{
    // 清理参数
    m_parameters.clear();
}

/**
 * @brief 获取参数值
 * @param category 参数类别
 * @param name 参数名称
 * @return 参数值
 */
QVariant DrillingParameters::getParameter(const QString& category, const QString& name) const
{
    if (m_parameters.contains(category) && m_parameters[category].contains(name)) {
        return m_parameters[category][name];
    }
    return QVariant(); // 返回无效值
}

/**
 * @brief 设置参数值
 * @param category 参数类别
 * @param name 参数名称
 * @param value 参数值
 * @return 是否设置成功
 */
bool DrillingParameters::setParameter(const QString& category, const QString& name, const QVariant& value)
{
    if (!m_parameters.contains(category)) {
        m_parameters[category] = QMap<QString, QVariant>();
    }
    
    m_parameters[category][name] = value;
    emit parameterChanged(category, name, value);
    return true;
}

/**
 * @brief 从JSON文件加载参数
 * @param filename 文件名
 * @return 是否加载成功
 */
bool DrillingParameters::loadParameters(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开参数文件:" << filename;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "参数文件格式错误:" << filename;
        return false;
    }
    
    QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        QString category = it.key();
        if (it.value().isObject()) {
            QJsonObject categoryObj = it.value().toObject();
            for (auto paramIt = categoryObj.begin(); paramIt != categoryObj.end(); ++paramIt) {
                QString paramName = paramIt.key();
                QVariant paramValue;
                
                if (paramIt.value().isDouble()) {
                    paramValue = paramIt.value().toDouble();
                } else if (paramIt.value().isString()) {
                    paramValue = paramIt.value().toString();
                } else if (paramIt.value().isBool()) {
                    paramValue = paramIt.value().toBool();
                } else if (paramIt.value().isArray()) {
                    paramValue = paramIt.value().toArray();
                }
                
                setParameter(category, paramName, paramValue);
            }
        }
    }
    
    return true;
}

/**
 * @brief 保存参数到JSON文件
 * @param filename 文件名
 * @return 是否保存成功
 */
bool DrillingParameters::saveParameters(const QString& filename)
{
    QJsonObject root;
    
    for (auto categoryIt = m_parameters.begin(); categoryIt != m_parameters.end(); ++categoryIt) {
        QJsonObject categoryObj;
        for (auto paramIt = categoryIt.value().begin(); paramIt != categoryIt.value().end(); ++paramIt) {
            QVariant value = paramIt.value();
            
            if (value.type() == QVariant::Double || value.type() == QVariant::Int) {
                categoryObj[paramIt.key()] = value.toDouble();
            } else if (value.type() == QVariant::String) {
                categoryObj[paramIt.key()] = value.toString();
            } else if (value.type() == QVariant::Bool) {
                categoryObj[paramIt.key()] = value.toBool();
            } else if (value.type() == QVariant::List) {
                categoryObj[paramIt.key()] = QJsonArray::fromVariantList(value.toList());
            }
        }
        
        root[categoryIt.key()] = categoryObj;
    }
    
    QJsonDocument doc(root);
    QFile file(filename);
    
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "无法写入参数文件:" << filename;
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    return true;
} 