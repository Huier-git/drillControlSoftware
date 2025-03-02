#ifndef MDBPROCESS_H
#define MDBPROCESS_H

#include <QObject>
#include <QDebug>
#include <QThread>
#include <QTimer>
#include <QModbusDataUnit>
#include <QModbusTcpClient>
#include <QModbusReply>


class mdbprocess : public QObject
{
    Q_OBJECT
public:
    explicit mdbprocess(QObject *parent = nullptr);
    ~mdbprocess();
    QModbusClient *modbusDevices[4] = {nullptr};            // Modbus对象
    QTimer *tractTimer;                                     // 拉力数据的定时器
    QTimer *torqueTimer;                                    // 扭矩的数据定时器
    QTimer *positionTimer;                                  // 位置传感器定时器
public slots:
    void TCPConnect(int port, QString addr);                // 与服务器建立TCP连接
    void TCPDisconnect();                                   // 断开服务器的连接
    // 读取寄存器的信息 服务器采集口0-3，modbus设备地址，寄存器地址，寄存器数量，是否需要翻译
    // mode: 1-Long补码翻译 2-Long 3-short
    void ReadValue(int mdbport, int mdbID, int reg, int num, int mode);
    // 翻译读取的寄存器的值，内容判断也在这里
    void ReceiveData(int mode, int reg, int num);
    void Readtraction(int mdbport);                         // 封装的便于读取拉力传感器的函数
    void Readtorque(int mdbport);
    void Readposition(int mdbport);

    void setReadtractionTimer(bool start, int interval);    // 定时读取拉力传感器的定时器（定时和传输速率有关）
    void setReadtorqueTimer(bool start, int interval);
    void setReadpositionTimer(bool start, int interval);

    void WriteValue(int mdbport, int mdbID, int reg, const QVector<quint16>& values);   //写入操作
    void ReadWriteValue(int mdbport, int mdbID, int readReg, int readNum, int writeReg, const QVector<quint16>& writeValues);
signals:
    void tractionLCDshow(long data, int reg);               // 发射读取到的拉力传感器的信号
    void torqueLCDshow(long data, int reg);
    void positionLCDshow(long data, int reg);
    void dataReceived(const QVector<quint16>& data, int startReg);
public:
    bool connectStatus;
    int  Forcemdbport;
    int  Torquemdbport;
    int  Poitionmdbport;
private:
    void ReceiveWriteResponse(int reg); //写入
    void ReceiveReadWriteResponse(int readReg, int writeReg);
};

#endif // MDBPROCESS_H
