/***************************************************************************************************
 * Framework: Desktop Qt 5.15.2 MinGW 64-bit
 * Author: YeMinhui    Version: 1.0    Date: 2024-3-6
 * Description: Qt版本调用DLL进行触发N点采样示例程序 (VK701N-SD)
 * Others: Debug通过Qt 64位版本。DLL必须使用64位stdcall版本。
 ***************************************************************************************************/

#ifndef VK701NSD_H
#define VK701NSD_H

#include <QObject>
#include <QList>
#include <QThread>
#include <QMutex>
#include <QVector>
#include <QAtomicInt>
#include <memory>

// 定义采集卡工作状态枚举
enum class DAQState {
    Disconnected,   // 未连接
    Initializing,   // 初始化中
    Running,        // 正在运行采样
    Paused,         // 暂停采样
    Stopping,       // 正在停止
    Error           // 错误状态
};

class vk701nsd : public QObject
{
    Q_OBJECT
public:
    explicit vk701nsd(QObject *parent = nullptr);
    ~vk701nsd();

    //////////////////////////////////初始化参数/////////////////////////////////////
    int port = 8234;                        // 数据采集卡端口
    int cardId = 0;                         // 数据采集卡序列号 [0-7]
    double refVol = 1;                      // 选择数据采集卡模式 VK701N: [4 或 1]
    int bitMode = 2;                        // 采样分辨率 [0-8/1-16/2,3-24]
    int samplingFrequency = 10000;          // 采样频率 [1-100K], default changed to 10000 for consistency with tests
    int volRange = 0;                       // 电压输入范围 [见手册]
    //////////////////////////////////////////////////////////////////////////////////////////////
    bool initStatus = false;                // 初始化成功标志, explicitly initialized
    bool fDAQSampleClr;                     // 0-开始采样 1-停止采样
    int reconnectCounter = 2000;            // 初始化过程中的连接尝试次数
    int loopTimes = 0;                      // 循环次数计数

    // 设置缓冲区大小
    void setBufferSize(int size);
    
    // 获取当前状态
    DAQState getState() const;
    
    // 请求停止工作线程
    void requestStop();
    
    // 判断工作线程是否应该继续运行
    bool shouldContinue() const;
    
signals:
    void resultValue(QVector<double> *list);   // 传输数据采集卡的值
    void resultMsg(QString msg);               // 消息结果
    void resultClr(QString msg);               // 清除结果
    void resultConn(bool msg);                 // 连接状态
    void stateChanged(DAQState newState);      // 状态变更信号

public slots:
    void doWork();
    
private:
    // 初始化采集卡
    bool initializeDAQ();
    
    // 开始采样过程
    bool startSampling();
    
    // 处理数据采集
    void processSampling();
    
    // 处理暂停状态
    void handlePausedState();

private:
    // 使用QVector替代QList提高性能，使用互斥锁保护数据
    QVector<double> *dataBuffer;
    QMutex mutex;
    int bufferSize;
    
    // 使用原子变量控制工作线程的运行和状态
    QAtomicInt shouldStop;
    DAQState currentState;

    Q_DISABLE_COPY_MOVE(vk701nsd)
};

#endif // VK701NSD_H