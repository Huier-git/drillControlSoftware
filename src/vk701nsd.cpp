#include "inc/vk701nsd.h"
#include "./inc/VK70xNMC_DAQ2.h"

#include <QDebug>


vk701nsd::vk701nsd(QObject *parent) : QObject(parent)
{

}

vk701nsd::~vk701nsd()
{
    VK70xNMC_StopSampling(cardId);
}


void vk701nsd::doWork()
{

    int curDeviceNum;
    int result;
    //double* pucRecBuf = new double[4 * samplingFrequency];      //max samplingFrequency points
    //使用智能指针或在函数结束时删除:
    std::unique_ptr<double[]> pucRecBuf(new double[4 * samplingFrequency]);

    // 1.Create TCP connect
    do{
        result = Server_TCPOpen(8234);
        QThread::msleep(20);
        if(result < 0)
        {
            qDebug() << "waiting...\n";
        }else
        {
            qDebug() << "Port " << port << " opened!";
        }
    }while(result < 0);
    QThread::msleep(500);

    // 2.Get the number of connected devices
    do{
        Server_Get_ConnectedClientNumbers(&curDeviceNum);
        QThread::msleep(20);
    }while(result < 0);
    qDebug() << "DAQ num: " << curDeviceNum;
    QThread::msleep(500);

    // 3.Initialization
    int temp = 0 ;
    do{
        result = VK70xNMC_Initialize(cardId, refVol, bitMode, samplingFrequency, volRange, volRange, volRange, volRange);
        QThread::msleep(20);
        if (temp < reconnectCounter)
        {
            if(result == -11){
                qDebug() << "Server not open.";
            }
            else if(result == -12 || result == -13){
                qDebug() << "DAQ not connected or does not exist. " << "try " <<temp;
            }
            else{
                qDebug() << "DAQ not connected or does not exist. " << "try " <<temp;
            }
        }
        temp++;
    }while(result < 0);
    QThread::msleep(500);

    //4.Start sampling
    result = VK70xNMC_StartSampling(cardId);
    if (result < 0)
    {
        //goto err;
        qDebug()<< "DAQ ERR";
        initStatus = false;
        goto Sloop;
    }
    else
    {
        qDebug() << "DAQ successfully connected!";
        initStatus = true;
        QThread::msleep(500);
        goto Rloop;
    }



Rloop:
    //Read Data Loop
    result = VK70xNMC_StartSampling(cardId);

    while(result >= 0)
    {
        //qDebug()<<"Fre:"<<samplingFrequency;
        // for Pause
        if(fDAQSampleClr)       //if fDAQSampleClr = 1 stop sampling
        {
            goto Sloop;
        }
        int recv;
        recv = VK70xNMC_GetFourChannel(cardId, pucRecBuf.get(), samplingFrequency);
        if (recv > 0)
        {
            list = new QList<double>();
            for (int i = 0; i < 4 * recv; i++)
            {
                list->append(pucRecBuf[i]);
            }    
            emit resultValue(list);
        }
        else
        {
            //goto err;
            if(recv < 0)
            {
                qDebug() << "Abnormal exit";
            }

            //qDebug() << "No data";

        }
        QThread::msleep(50);
    }


Sloop:
    while (1)
    {
        if(!fDAQSampleClr)
        {
            goto Rloop;
        }
        else
        {
            result = VK70xNMC_StopSampling(cardId);
        }
        QThread::msleep(1000);
    }

// err:
//     if (result == -1)
//     {
//         emit resultMsg("DAQ connection failed");
//         qDebug()<< "Err:DAQ connection failed";
//     }
//     else
//     {
//         emit resultMsg(QString("Exception exit, code: %1！").arg(result));
//         qDebug() << QString("Exception exit, code: %1！").arg(result);
//     }

}
