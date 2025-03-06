#ifndef DRILLINGPARAMETERS_H
#define DRILLINGPARAMETERS_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QVariant>

/**
 * @brief 钻进系统参数配置类
 * 
 * 该类用于集中管理钻进系统的各种参数，包括电机ID、位置、速度、模式等
 * 所有参数可通过配置文件加载或在运行时修改
 */
class DrillingParameters : public QObject
{
    Q_OBJECT

public:
    // 单例模式
    static DrillingParameters* getInstance();

    // 电机ID定义
    struct MotorID {
        static const int STORAGE = 1;         // 存储装置电机
        static const int ROBOT_ROTATION = 2;  // 机械手旋转电机
        static const int ROBOT_EXTENSION = 3; // 机械手伸缩电机
        static const int ROBOT_CLAMP = 4;     // 机械手夹持电机
        static const int DRILL = 5;           // 钻进电机
        static const int PERCUSSION = 6;      // 冲击电机
        static const int PENETRATION = 7;     // 贯入机构电机
        static const int CLAMP = 8;           // 夹紧机构电机
        static const int CONNECTION = 9;      // 对接机构电机
    };

    // 电机模式定义
    struct MotorMode {
        static const int POSITION = 65;       // 位置模式
        static const int VELOCITY = 66;       // 速度模式
        static const int TORQUE = 67;         // 力矩模式
    };

    // 电机使能状态
    struct MotorEnable {
        static const int DISABLED = 0;        // 禁用
        static const int ENABLED = 1;         // 使能
    };

    // 机械手位置参数
    struct RobotPosition {
        static inline const float DRILL_POSITION = 0.0f;      // 钻台位置为0度
        static inline const float STORAGE_POSITION = 90.0f;   // 存储区位置为90度
        static inline const float EXTENDED = 250.0f;          // 机械手伸出位置
        static inline const float RETRACTED = 0.0f;           // 机械手收回位置
        static inline const float CLAMPED = 100.0f;           // 机械手夹紧位置
        static inline const float RELEASED = 0.0f;            // 机械手释放位置
    };

    // 钻进参数
    struct DrillParameters {
        static inline const float DEFAULT_SPEED = 60.0f;    // 默认钻进速度
        static inline const float MAX_SPEED = 120.0f;       // 最大钻进速度
        static inline const float PERCUSSION_FREQ = 10.0f;  // 默认冲击频率
        static inline const float MAX_PERCUSSION = 30.0f;   // 最大冲击频率
    };

    // 贯入参数
    struct PenetrationParameters {
        static inline const float HOME_POSITION = 0.0f;       // 原点位置
        static inline const float WORK_POSITION = 500.0f;     // 工作位置
        static inline const float RETRACT_POSITION = -50.0f;  // 撤回位置
        static inline const float DEFAULT_SPEED = 50.0f;      // 默认移动速度
    };

    // 夹紧与对接参数
    struct ClampConnectionParameters {
        static inline const float CLAMP_RELEASED = 0.0f;      // 夹紧装置释放位置
        static inline const float CLAMP_ENGAGED = 100.0f;     // 夹紧装置咬合位置
        static inline const float CONNECTION_DISENGAGED = 0.0f;   // 对接装置分离位置
        static inline const float CONNECTION_READY = 50.0f;       // 对接装置准备位置
        static inline const float CONNECTION_ENGAGED = 100.0f;    // 对接装置咬合位置
    };

    // 获取参数
    QVariant getParameter(const QString& category, const QString& name) const;
    
    // 设置参数
    bool setParameter(const QString& category, const QString& name, const QVariant& value);
    
    // 加载参数文件
    bool loadParameters(const QString& filename);
    
    // 保存参数文件
    bool saveParameters(const QString& filename);

signals:
    // 参数变更信号
    void parameterChanged(const QString& category, const QString& name, const QVariant& value);

private:
    // 私有构造函数(单例模式)
    explicit DrillingParameters(QObject* parent = nullptr);
    ~DrillingParameters();
    
    // 禁止拷贝
    DrillingParameters(const DrillingParameters&) = delete;
    DrillingParameters& operator=(const DrillingParameters&) = delete;
    
    // 参数存储
    QMap<QString, QMap<QString, QVariant>> m_parameters;
    
    // 单例实例
    static DrillingParameters* m_instance;
};

#endif // DRILLINGPARAMETERS_H 