/***************************************************************************************************
 Framework: Desktop Qt 5.15.2 MinGW 64-bit
 Author: YeMinhui	Version: 1.0	Date:2024-3-6
 Description: Qt version calling DLL for trigger N-point sampling example program (VK701N-SD)
 Others: Debug passed Qt 64bit vesion. DLL must use a 64-bit stdcall version.
 History:
***************************************************************************************************/

#ifndef VK701NSD_H
#define VK701NSD_H

#include <QObject>
#include <QList>
#include <QThread>

class vk701nsd : public QObject
{
    Q_OBJECT
public:
    explicit vk701nsd(QObject *parent = nullptr);
    ~vk701nsd();

    //////////////////////////////////Initialaztion parameter/////////////////////////////////////
    int port = 8234;                        // DAQ Port
    int cardId = 0;                         // DAQ serial number [0-7]
    double refVol = 1;                      // Select DAQ Mode mode VK701N: [4 or 1]
    int bitMode = 2;                        // Sampling resolution [0-8/1-16/2,3-24]
    int samplingFrequency = 5000;          // Sampling frequency [1-100K]
    int volRange = 0;                       // Voltage input range [See manual]
    //////////////////////////////////////////////////////////////////////////////////////////////
    bool initStatus;                        // Successful initialization the process
    bool fDAQSampleClr;                     // 0-StartSample 1-StopSample
    int reconnectCounter = 2000;            // Number of connection attempts during initialization
        int loopTimes = 0;

signals:
    void resultValue(QList<double> *list);      // transmit the DAQ value
    void resultMsg(QString msg);
    void resultClr(QString msg);
    void resultConn(bool msg);

public slots:
    void doWork();

private:
    QList<double> *list;

};

#endif // VK701NSD_H
