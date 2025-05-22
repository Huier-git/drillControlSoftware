#include "inc/vk701nsd.h"
#include "./inc/VK70xNMC_DAQ2.h"

#include <QDebug>
#include <memory>

vk701nsd::vk701nsd(QObject *parent) : QObject(parent), 
    bufferSize(4 * samplingFrequency), // 初始缓冲区大小基于采样频率
    shouldStop(0),
    currentState(DAQState::Disconnected)
{
    // 初始化数据缓冲区
    dataBuffer = new QVector<double>();
    dataBuffer->reserve(bufferSize); // 预分配内存，减少重新分配
}

vk701nsd::~vk701nsd()
{
    // 请求停止工作线程
    requestStop();
    
    // 确保停止采样
    VK70xNMC_StopSampling(cardId);
    
    // 清理缓冲区内存
    if (dataBuffer) {
        delete dataBuffer;
        dataBuffer = nullptr;
    }
}

void vk701nsd::setBufferSize(int size)
{
    QMutexLocker locker(&mutex); // 自动锁定和解锁
    bufferSize = size;
    if (dataBuffer) {
        dataBuffer->reserve(bufferSize);
    }
}

DAQState vk701nsd::getState() const
{
    return currentState;
}

void vk701nsd::requestStop()
{
    shouldStop.storeRelaxed(1);
}

bool vk701nsd::shouldContinue() const
{
    return shouldStop.loadRelaxed() == 0;
}

// 初始化采集卡
bool vk701nsd::initializeDAQ()
{
    int curDeviceNum;
    int result;
    
    // 更新状态
    currentState = DAQState::Initializing;
    emit stateChanged(currentState);
    
    // 1.创建TCP连接
    qDebug() << "正在打开端口" << port << "...";
    do {
        // 检查是否应该停止
        if (!shouldContinue()) {
            return false;
        }
        
        result = Server_TCPOpen(port);
        QThread::msleep(20);
        if (result < 0) {
            qDebug() << "等待连接中... Error code:" << result;
        } else {
            qDebug() << "端口" << port << "已打开!";
        }
    } while (result < 0);
    QThread::msleep(100);

    // 2.获取已连接设备数量
    qDebug() << "获取已连接设备数量...";
    do {
        // 检查是否应该停止
        if (!shouldContinue()) {
            return false;
        }
        
        result = Server_Get_ConnectedClientNumbers(&curDeviceNum);
        QThread::msleep(20);
        if (result < 0) {
            qDebug() << "获取已连接设备数量失败. Error code:" << result;
        }
    } while (result < 0);
    qDebug() << "数据采集设备数量: " << curDeviceNum;
    QThread::msleep(100);

    // 3.初始化
    qDebug() << "初始化设备...";
    int retries = 0;
    long long sleepTime = 100; // Initial sleep time in ms
    const long long maxSleepTime = 1000; // Maximum sleep time in ms
    const int maxRetries = 5; // Total retry limit

    do {
        // 检查是否应该停止
        if (!shouldContinue()) {
            return false;
        }
        
        result = VK70xNMC_Initialize(cardId, refVol, bitMode, samplingFrequency, 
                                    volRange, volRange, volRange, volRange);
        if (result < 0) {
            qDebug() << "VK70xNMC_Initialize 失败. Error code:" << result << ". 尝试次数:" << retries + 1;
            if (retries < maxRetries) {
                QThread::msleep(sleepTime);
                sleepTime = qMin(sleepTime * 2, maxSleepTime);
            }
        }
        retries++;
    } while (result < 0 && retries < maxRetries);
    
    // 判断初始化结果
    if (result < 0) {
        qCritical() << "数据采集设备初始化失败. Error code:" << result;
        currentState = DAQState::Error;
        emit stateChanged(currentState);
        emit resultMsg(QString("数据采集设备初始化失败. Error code: %1").arg(result));
        return false;
    }
    
    QThread::msleep(100);
    return true;
}

// 开始采样
bool vk701nsd::startSampling()
{
    int result = VK70xNMC_StartSampling(cardId);
    if (result < 0) {
        qCritical() << "启动数据采集设备采样失败. Error code:" << result;
        currentState = DAQState::Error;
        emit stateChanged(currentState);
        emit resultMsg(QString("启动数据采集设备采样失败. Error code: %1").arg(result));
        initStatus = false;
        return false;
    } else {
        qDebug() << "数据采集设备连接成功!";
        currentState = DAQState::Running;
        emit stateChanged(currentState);
        initStatus = true;
        return true;
    }
}

// 处理数据采集
void vk701nsd::processSampling()
{
    std::unique_ptr<double[]> pucRecBuf(new double[bufferSize]);
    
    // 注释掉此处的重复启动采样，因为startSampling已在doWork中调用
    // int result = VK70xNMC_StartSampling(cardId);
    // if (result < 0) {
    //     currentState = DAQState::Error;
    //     emit stateChanged(currentState);
    //     qCritical() << "处理数据采集时启动采样失败. Error code:" << result;
    //     emit resultMsg(QString("处理数据采集时启动采样失败. Error code: %1").arg(result));
    //     return;
    // }
    
    // 读取数据
    int recv = VK70xNMC_GetFourChannel(cardId, pucRecBuf.get(), samplingFrequency);
    if (recv > 0) {
        // 创建新的数据容器并锁定互斥锁，保证数据安全
        QMutexLocker locker(&mutex);
        
        // 重用数据缓冲区，避免频繁分配内存
        dataBuffer->clear();
        dataBuffer->reserve(4 * recv);
        
        // 复制数据
        for (int i = 0; i < 4 * recv; ++i) { // Use ++i as a common convention
            dataBuffer->append(pucRecBuf[i]);
        }
        
        // 发送数据信号
        emit resultValue(dataBuffer);
    } else if (recv < 0) {
        qDebug() << "异常退出，错误码: " << recv;
        currentState = DAQState::Error;
        emit stateChanged(currentState);
    }
}

// 处理暂停状态
void vk701nsd::handlePausedState()
{
    // 确保采集卡已停止
    VK70xNMC_StopSampling(cardId);
    currentState = DAQState::Paused;
    emit stateChanged(currentState);
}

void vk701nsd::doWork()
{
    // 初始化采集卡
    if (!initializeDAQ()) {
        if (shouldContinue()) {
            // 只有在不是由外部请求停止时才发送错误信息
            emit resultMsg("初始化采集卡失败");
        }
        return;
    }
    
    // 开始采样
    if (!startSampling()) { // This call is necessary for the initial start.
        // emit resultMsg("启动采样失败"); // This is already handled inside startSampling()
        return;
    }
    
    // 主工作循环
    while (shouldContinue()) {
        // 根据暂停标志判断是否处于暂停状态
        if (fDAQSampleClr) {
            // 处理暂停状态
            handlePausedState();
            
            // 暂停状态下的循环
            while (fDAQSampleClr && shouldContinue()) {
                QThread::msleep(500); // 暂停状态下降低CPU使用
            }
            
            // 如果退出暂停状态且应继续运行，则重新开始采样
            if (shouldContinue() && !fDAQSampleClr) {
                if (!startSampling()) {
                    qCritical() << "从暂停状态恢复采样失败";
                    emit resultMsg("从暂停状态恢复采样失败");
                    break; // 重新开始采样失败，退出循环
                }
            }
        } else {
            // 处理运行状态 - 采集数据
            processSampling();
            
            // 适当休眠以降低CPU使用率
            QThread::msleep(50);
        }
    }
    
    // 线程退出前的清理工作
    VK70xNMC_StopSampling(cardId);
    currentState = DAQState::Stopping;
    emit stateChanged(currentState);
    qDebug() << "数据采集线程已退出";
}