#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QTimer>
#include "inc/vk701nsd.h"

const QColor color[4] = {Qt::darkRed, Qt::darkGreen, Qt::darkBlue, Qt::darkYellow};

bool AllRecordStart = false;// Plot color

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);


    // Instantiate motorpage
    this->ppagemotor = new motorpage;
    connect(ui->btn_motorparm, &QPushButton::clicked, [=](){
        ppagemotor->setVisible(!ppagemotor->isVisible());
    });

    // Instantiate vk701page
    this->ppagevk701 = new vk701page;
    connect(ui->btn_vibeparm, &QPushButton::clicked, [=](){
        ppagevk701->setVisible(!ppagevk701->isVisible());
    });

    // Instantiate zmotionpage
    this->ppagezmotion = new zmotionpage;
    connect(ui->btn_motorctrl, &QPushButton::clicked, [=](){
        ppagezmotion->setVisible(!ppagezmotion->isVisible());
    });

    // Instantiate zmotionpage
    this->mdbtcp = new MdbTCP;
    connect(ui->btn_mdbtcp, &QPushButton::clicked, [=](){
        mdbtcp->setVisible(!mdbtcp->isVisible());
    });




    // Start/stop recording
    connect(ui->btn_record, &QPushButton::clicked, this, [=]() {
        bool isRecording = ui->btn_record->text() == "Record";
        AllRecordStart = isRecording;
        
        if (isRecording) {
            UnistartTime = QDateTime::currentDateTime();
            ui->btn_record->setText("StopRecord");
        } else {
            UnistopTime = QDateTime::currentDateTime();
            ui->btn_record->setText("Record");
            qint64 intervalMs = UnistartTime.msecsTo(UnistopTime);
            ui->textEdit->append("Time: " + QString::number(intervalMs) + " ms");
        }
    });




    // for debug
    debugtimer = new QTimer(this);
    connect(debugtimer, SIGNAL(timeout()), this, SLOT(checkThreadStatus()));
    //debugtimer->start(1000);





}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::checkThreadStatus()    //for debug
{
    if(workerThread->isRunning())
    {
        qDebug()<< "[D]workThread is running!";
    }
}



