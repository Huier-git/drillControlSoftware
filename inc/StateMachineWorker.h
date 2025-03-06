#ifndef STATEMACHINEWORKER_H
#define STATEMACHINEWORKER_H

#include "autodrilling.h"
#include <QObject>

/**
 * @brief 线程工作类，用于在单独线程中管理状态机
 */
class StateMachineWorker : public QObject
{
    Q_OBJECT
public:
    explicit StateMachineWorker(AutoDrillingStateMachine* stateMachine, QObject* parent = nullptr);

public slots:
    void run();
    void startWork();
    void stopWork();
    void pauseWork();
    void resumeWork();

signals:
    void started();
    void stopped();
    void paused();
    void resumed();

private:
    AutoDrillingStateMachine* m_stateMachine;
    bool m_running;
    bool m_paused;
    bool m_stopRequested;
};

#endif // STATEMACHINEWORKER_H 