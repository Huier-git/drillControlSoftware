#include "inc/mdbprocess.h"

#define SINGLE_TIMER 0

mdbprocess::mdbprocess(QObject *parent) : QObject(parent)
{
    qDebug() << "mdbThread:" << QThread::currentThreadId();

    for (int i = 0; i < 4; i++){
    modbusDevices[i] = new QModbusTcpClient(this);
    }

#if SINGLE_TIMER
    tractTimer = new QTimer();
    connect(tractTimer, &QTimer::timeout, this, [=]()
            { Readtraction(Forcemdbport); });
    connect(tractTimer, &QTimer::timeout, this, [=]()
            { Readtorque(Torquemdbport); });
    connect(tractTimer, &QTimer::timeout, this, [=]()
            { Readposition(Poitionmdbport); });
#else
    tractTimer = new QTimer();
    connect(tractTimer, &QTimer::timeout, this, [=](){
        Readtraction(Forcemdbport);
    });

    torqueTimer = new QTimer();
    connect(torqueTimer, &QTimer::timeout, this, [=](){
        Readtorque(Torquemdbport);
    });

    positionTimer = new QTimer();
    connect(positionTimer, &QTimer::timeout, this, [=](){
        Readposition(Poitionmdbport);
    });
#endif


}

mdbprocess::~mdbprocess()
{

}

/**
 * @brief 将2个短字节的拼成一个长字节，并且转化标准补码成10进制
 * @param lowerShort
 * @param upperShort
 * @return
 */
int64_t concatenateShortsToLong(int16_t lowerShort, int16_t upperShort) {
    // Concatenate the two short integers into a long integer
    int64_t result = ((static_cast<int64_t>(upperShort) << 16) & 0xFFFF0000) | (lowerShort & 0x0000FFFF);
    // Check if the result is negative
    if (result & 0x80000000) {
        // If so, perform two's complement to get the negative value
        result = -((~result + 1) & 0xFFFFFFFF);
    }
    return result;
}
/**
 * @brief 把两个短字节拼成一个长字节
 * @param short1
 * @param short2
 * @return
 */
long ShortsToLong(int16_t short1, int16_t short2) {
    long long_byte = (short2 << 16) | short1;
    return long_byte;
}

void mdbprocess::ReceiveData(int mode, int reg, int num)
{
    QModbusReply* reply = qobject_cast<QModbusReply*>(sender());
    if (!reply) {
       qDebug() << "Invalid reply object";
       return;
    }

    if (reply->error() == QModbusDevice::NoError) {
       QModbusDataUnit unit = reply->result();
       if(mode == 1)            //需要拼接且需要从补码转化（拉力传感器）
       {
            for (uint i = 0; i < unit.valueCount(); i += 2)
            {
                int16_t lower = QString::number(unit.value(i)).toUInt();
                int16_t upper = QString::number(unit.value(i+1)).toUInt();
                long combine = concatenateShortsToLong(upper, lower);
                //qDebug() << "Value[t" << reg+i << "]: " << combine;
                emit tractionLCDshow(combine, reg);
            }
       }
       else if(mode == 2)       //需要拼接，但不转化（LONG)（扭矩传感器）
       {
           if(num % 2 == 0)
           {
               for (uint i = 0; i < unit.valueCount(); i += 2)
               {
                   short int short1 = QString::number(unit.value(i)).toUInt();
                   short int short2 = QString::number(unit.value(i+1)).toUInt();
                   long combine = ShortsToLong(short2, short1);
                   //qDebug() << "short" << short1 << short2;
                   //qDebug() << "Value[l" << reg+i << "]: " << combine;
                   emit torqueLCDshow(combine, reg);
               }
           }

       }
       else if(mode == 3)       //需要拼接，但不转化（位置传感器）
       {
           if(num % 2 == 0)
           {
               for (uint i = 0; i < unit.valueCount(); i += 2)
               {
                   short int short1 = QString::number(unit.value(i)).toUInt();
                   short int short2 = QString::number(unit.value(i+1)).toUInt();
                   long combine = ShortsToLong(short2, short1);
                   //qDebug() << "Value[l" << reg+i << "]: " << combine;
                   emit positionLCDshow(combine, reg);
               }
           }
       }
       if (mode == 4) {
           QVector<quint16> receivedData;
           for (uint i = 0; i < unit.valueCount(); i++) {
               quint16 value = static_cast<quint16>(unit.value(i));
               receivedData.append(value);
           }

           // 发射信号，传递整个数据向量和起始寄存器地址
           emit dataReceived(receivedData, reg);

        }
    } else {
       qDebug() << "Read error: " << reply->errorString();
    }

    reply->deleteLater();
}


void mdbprocess::ReadValue(int mdbport, int mdbID, int reg, int num, int mode)
{
    if(modbusDevices[mdbport]->state() == QModbusDevice::ConnectedState)
    {
        // 03 读寄存器 82开始 长字节读2个
        // 类型/地址/个数
        QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, reg, num);
        // 这里的1代表设备ID
        if (auto *reply = modbusDevices[mdbport]->sendReadRequest(readUnit, mdbID))
        {
           if (!reply->isFinished())
           {
                connect(reply, &QModbusReply::finished, this, [=](){
                    ReceiveData(mode, reg, num);
                });
                return;
           }
          reply->deleteLater();
        }
        else
        {
            qDebug()<< "Read value Error.";
        }
    }
}

void mdbprocess::TCPConnect(int port, QString addr)
{
    QStringList ipParts = addr.split(".");
    // 遍历4个modbusDevice并连接
    for (int i = 0; i < 4; i++) {
        if(modbusDevices[i]->state() != QModbusDevice::ConnectedState)
        {
            // 设置连接参数
            modbusDevices[i]->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
            modbusDevices[i]->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                                   QString("%1.%2.%3.%4").arg(ipParts[0]).arg(ipParts[1])
                                                   .arg(ipParts[2]).arg(ipParts[3].toInt() + i));
            qDebug() << QString("%1.%2.%3.%4").arg(ipParts[0]).arg(ipParts[1]).arg(ipParts[2]).arg(ipParts[3].toInt() + i);
            modbusDevices[i]->connectDevice();
            connectStatus = true;
        }
    }
}

void mdbprocess::TCPDisconnect()
{
    for (int i = 0; i < 4; i++) {
        if(modbusDevices[i]->state() == QModbusTcpClient::ConnectedState)
        {
            modbusDevices[i]->disconnectDevice();
            qDebug() << "[B] Disconnected device" << i + 1;
        }
    }
    connectStatus = false;
}


void mdbprocess::Readtraction(int mdbport)
{
    ReadValue(mdbport-1, 1, 450, 2, 1);
    ReadValue(mdbport-1, 1, 452, 2, 1);
}

void mdbprocess::Readtorque(int mdbport)
{
    ReadValue(mdbport-1, 1, 0x00, 2, 2);        // 扭矩
    //ReadValue(mdbport-1, 1, 0x02, 2, 2);        // 转速
    //ReadValue(mdbport-1, 1, 0x04, 2, 2);        // 功率
}

void mdbprocess::Readposition(int mdbport)
{
    ReadValue(mdbport-1, 1, 0x00, 2, 3);
}


void mdbprocess::setReadtractionTimer(bool start, int interval)
{
    if(start == true)
    {
        tractTimer->start(interval);
    }
    else
    {
        tractTimer->stop();
    }
}

void mdbprocess::setReadtorqueTimer(bool start, int interval)
{
    if(start == true)
    {
        torqueTimer->start(interval);
    }
    else
    {
        torqueTimer->stop();
    }
}

void mdbprocess::setReadpositionTimer(bool start, int interval)
{
    if(start == true)
    {
        positionTimer->start(interval);
    }
    else
    {
        positionTimer->stop();
    }
}

//写入寄存器
void mdbprocess::WriteValue(int mdbport, int mdbID, int reg, const QVector<quint16>& values)
{
    if(modbusDevices[mdbport]->state() == QModbusDevice::ConnectedState)
    {
        QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, reg, values.size());
        for (int i = 0; i < values.size(); ++i) {
            writeUnit.setValue(i, values[i]);
        }

        if (auto *reply = modbusDevices[mdbport]->sendWriteRequest(writeUnit, mdbID))
        {
            if (!reply->isFinished())
            {
                connect(reply, &QModbusReply::finished, this, [this, reg](){
                    this->ReceiveWriteResponse(reg);
                });
            }
            else
            {
                reply->deleteLater();
            }
        }
        else
        {
            qDebug() << "Write error: " << modbusDevices[mdbport]->errorString();
        }
    }
}

// mo
void mdbprocess::ReadWriteValue(int mdbport, int mdbID, int readReg, int readNum, int writeReg, const QVector<quint16>& writeValues)
{
    if(modbusDevices[mdbport]->state() != QModbusDevice::ConnectedState) {
        qDebug() << "Device not connected";
        return;
    }

    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, readReg, readNum);
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, writeReg, writeValues.size());
    
    // Copy write values
    for(int i = 0; i < writeValues.size(); ++i) {
        writeUnit.setValue(i, writeValues[i]);
    }

    auto *reply = modbusDevices[mdbport]->sendReadWriteRequest(readUnit, writeUnit, mdbID);
    if(!reply) {
        qDebug() << "ReadWrite error: " << modbusDevices[mdbport]->errorString();
        return;
    }

    if(!reply->isFinished()) {
        connect(reply, &QModbusReply::finished, this, [this, readReg, writeReg](){
            this->ReceiveReadWriteResponse(readReg, writeReg);
        });
    } else {
        reply->deleteLater();
    }
}

void mdbprocess::ReceiveWriteResponse(int reg)
{
    QModbusReply *reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;

    if (reply->error() == QModbusDevice::NoError) {
        qDebug() << "Write successful to register" << reg;
    } else {
        qDebug() << "Write error: " << reply->errorString();
    }

    reply->deleteLater();
}

void mdbprocess::ReceiveReadWriteResponse(int readReg, int writeReg)
{
    QModbusReply *reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;

    if (reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();
        for (uint i = 0; i < unit.valueCount(); i++) {
            qDebug() << "Read value at" << readReg + i << ":" << unit.value(i);
        }
        qDebug() << "Write successful to register" << writeReg;
    } else {
        qDebug() << "ReadWrite error: " << reply->errorString();
    }

    reply->deleteLater();
}
