#include "drillingstate.h"
#include "autodrilling.h"
#include <QDebug>

// DrillState 基类实现
DrillState::DrillState(AutoDrillingStateMachine* machine)
    : State(static_cast<StateMachine*>(machine)), m_machine(machine)
{
}

void DrillState::enter()
{
    qDebug() << QString("进入状态");
}

void DrillState::exit()
{
    qDebug() << QString("退出状态");
}

void DrillState::update()
{
    // 基类默认实现为空
}

// SystemStartupState 实现
SystemStartupState::SystemStartupState(AutoDrillingStateMachine* machine)
    : DrillState(machine)
{
}

void SystemStartupState::enter()
{
    DrillState::enter();
    qDebug() << "系统启动状态：初始化所有组件";
}

void SystemStartupState::update()
{
    // TODO: 实现系统启动逻辑
}

// DefaultPositionState 实现
DefaultPositionState::DefaultPositionState(AutoDrillingStateMachine* machine)
    : DrillState(machine)
{
}

void DefaultPositionState::enter()
{
    DrillState::enter();
    qDebug() << "默认位置状态：移动所有组件到默认位置";
}

void DefaultPositionState::update()
{
    // TODO: 实现默认位置逻辑
}

// ReadyState 实现
ReadyState::ReadyState(AutoDrillingStateMachine* machine)
    : DrillState(machine)
{
}

void ReadyState::enter()
{
    DrillState::enter();
    qDebug() << "就绪状态：等待操作指令";
}

void ReadyState::update()
{
    // TODO: 实现就绪状态逻辑
}

// FirstToolInstallationState 实现
FirstToolInstallationState::FirstToolInstallationState(AutoDrillingStateMachine* machine)
    : DrillState(machine)
{
}

void FirstToolInstallationState::enter()
{
    DrillState::enter();
    qDebug() << "首根钻具安装状态：准备安装首根钻具";
    
    // 发送当前步骤变化信号
    m_machine->emit currentStepChanged("首根钻具安装开始");
}

void FirstToolInstallationState::update()
{
    // 实现首根钻具安装流程
    static int step = 0;
    
    // 获取组件状态机
    auto robotArm = dynamic_cast<RobotArmStateMachine*>(m_machine->getRobotArm().get());
    auto penetration = dynamic_cast<PenetrationMechanismStateMachine*>(m_machine->getPenetrationMechanism().get());
    auto drilling = dynamic_cast<DrillingMechanismStateMachine*>(m_machine->getDrillingMechanism().get());
    auto connection = dynamic_cast<ConnectionMechanismStateMachine*>(m_machine->getConnectionMechanism().get());
    
    if (!robotArm || !penetration || !drilling || !connection) {
        qDebug() << "错误：无法获取组件状态机";
        return;
    }
    
    switch (step) {
        case 0: // 1. 机械手移动到存储区
            m_machine->emit currentStepChanged("机械手移动到存储区");
            robotArm->setRotationPosition(90); // 旋转到存储区位置
            penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_B1); // 进给机构移动到工作位置
            step++;
            break;
            
        case 1: // 2. 机械手夹持钻具
            if (robotArm->getRotationPosition() == 90) {
                robotArm->setExtension(200); // 伸出
                step++;
            }
            break;
            
        case 2:
            if (robotArm->getExtension() == 200) {
                robotArm->setClamp(100); // 完全夹紧
                step++;
            }
            break;
            
        case 3:
            if (robotArm->getClamp() == 100) {
                robotArm->setExtension(0); // 回收
                step++;
            }
            break;
            
        case 4: // 3. 机械手移动到钻台
            if (robotArm->getExtension() == 0) {
                m_machine->emit currentStepChanged("机械手移动到钻台");
                robotArm->setRotationPosition(0); // 旋转到钻台位置
                step++;
            }
            break;
            
        case 5:
            if (robotArm->getRotationPosition() == 0) {
                robotArm->setExtension(250); // 对准钻台
                step++;
            }
            break;
            
        case 6: // 4. 进给机构移动
            if (robotArm->getExtension() == 250) {
                m_machine->emit currentStepChanged("进给机构移动到钻具安装起始位置");
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_A1); // 钻具安装起始位置
                step++;
            }
            break;
            
        case 7: // 5. 执行连接操作
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_A1) {
                m_machine->emit currentStepChanged("执行连接操作");
                drilling->setRotationSpeed(60); // 低速对接旋转
                step++;
            }
            break;
            
        case 8:
            if (drilling->getRotationSpeed() == 60) {
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_B1); // 对接速度缓慢下移
                step++;
            }
            break;
            
        case 9:
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_B1) {
                drilling->setRotationSpeed(0); // 停止旋转
                step++;
            }
            break;
            
        case 10: // 6. 对接机构
            if (drilling->getRotationSpeed() == 0) {
                m_machine->emit currentStepChanged("对接机构伸出");
                connection->setConnectionState(true); // 对接机构伸出
                step++;
            }
            break;
            
        case 11: // 7. 机械手松开
            if (connection->isExtended()) {
                m_machine->emit currentStepChanged("机械手松开");
                robotArm->setClamp(0); // 完全松开
                step++;
            }
            break;
            
        case 12:
            if (robotArm->getClamp() == 0) {
                robotArm->setExtension(0); // 完全收回
                step++;
            }
            break;
            
        case 13: // 完成首根钻具安装
            if (robotArm->getExtension() == 0) {
                m_machine->emit currentStepChanged("首根钻具安装完成");
                // 重置步骤计数器，为下一次使用做准备
                step = 0;
                // 切换到下一个状态
                m_machine->changeState(AutoDrillingStateMachine::STATE_FIRST_DRILLING);
            }
            break;
    }
}

// FirstDrillingState 实现
FirstDrillingState::FirstDrillingState(AutoDrillingStateMachine* machine)
    : DrillState(machine)
{
}

void FirstDrillingState::enter()
{
    DrillState::enter();
    qDebug() << "首次钻进状态：开始首次钻进";
    
    // 发送当前步骤变化信号
    m_machine->emit currentStepChanged("首次钻进开始");
}

void FirstDrillingState::update()
{
    // 实现首次钻进流程
    static int step = 0;
    
    // 获取组件状态机
    auto drilling = dynamic_cast<DrillingMechanismStateMachine*>(m_machine->getDrillingMechanism().get());
    auto penetration = dynamic_cast<PenetrationMechanismStateMachine*>(m_machine->getPenetrationMechanism().get());
    auto clamp = dynamic_cast<ClampMechanismStateMachine*>(m_machine->getClampMechanism().get());
    auto connection = dynamic_cast<ConnectionMechanismStateMachine*>(m_machine->getConnectionMechanism().get());
    
    if (!drilling || !penetration || !clamp || !connection) {
        qDebug() << "错误：无法获取组件状态机";
        return;
    }
    
    switch (step) {
        case 0: // 1. 钻进电机、冲击电机和进给机构同时启动
            m_machine->emit currentStepChanged("启动钻进和冲击");
            drilling->setRotationSpeed(120); // 工作转速
            drilling->setPercussionFrequency(10); // 工作频率
            step++;
            break;
            
        case 1:
            if (drilling->getRotationSpeed() == 120 && drilling->getPercussionFrequency() == 10) {
                m_machine->emit currentStepChanged("进给机构下降");
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_A); // 钻进速度下降到工作位置
                step++;
            }
            break;
            
        case 2: // 2. 监测钻进参数 (这里简化为等待到达目标位置)
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_A) {
                m_machine->emit currentStepChanged("达到预定深度");
                step++;
            }
            break;
            
        case 3: // 3. 达到预定深度后停止
            drilling->setRotationSpeed(0); // 停止旋转
            drilling->setPercussionFrequency(0); // 停止冲击
            step++;
            break;
            
        case 4: // 4. 下夹紧机构夹紧
            if (drilling->getRotationSpeed() == 0 && drilling->getPercussionFrequency() == 0) {
                m_machine->emit currentStepChanged("下夹紧机构夹紧");
                clamp->setClampState(ClampMechanismStateMachine::TIGHT); // 完全夹紧
                step++;
            }
            break;
            
        case 5: // 5. 对接机构回收
            if (clamp->getClampState() == ClampMechanismStateMachine::TIGHT) {
                m_machine->emit currentStepChanged("对接机构回收");
                connection->setConnectionState(false); // 解锁钻具
                step++;
            }
            break;
            
        case 6: // 6. 执行断开操作
            if (!connection->isExtended()) {
                m_machine->emit currentStepChanged("执行断开操作");
                drilling->setRotationSpeed(-60); // 低速反向旋转
                step++;
            }
            break;
            
        case 7:
            if (drilling->getRotationSpeed() == -60) {
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_A1); // 对接速度缓慢上移
                step++;
            }
            break;
            
        case 8:
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_A1) {
                drilling->setRotationSpeed(0); // 停止旋转
                step++;
            }
            break;
            
        case 9: // 7. 进给机构上升
            if (drilling->getRotationSpeed() == 0) {
                m_machine->emit currentStepChanged("进给机构上升");
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_D); // 空行程速度上升
                step++;
            }
            break;
            
        case 10: // 完成首次钻进
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_D) {
                m_machine->emit currentStepChanged("首次钻进完成");
                // 重置步骤计数器，为下一次使用做准备
                step = 0;
                // 切换到下一个状态
                m_machine->changeState(AutoDrillingStateMachine::STATE_PIPE_INSTALLATION_LOOP);
            }
            break;
    }
}

// PipeInstallationLoopState 实现
PipeInstallationLoopState::PipeInstallationLoopState(AutoDrillingStateMachine* machine)
    : DrillState(machine)
{
}

void PipeInstallationLoopState::enter()
{
    DrillState::enter();
    qDebug() << "钻管安装循环状态：准备安装新钻管";
    
    // 发送当前步骤变化信号
    m_machine->emit currentStepChanged("钻管安装循环开始");
}

void PipeInstallationLoopState::update()
{
    // 实现钻管安装循环流程
    static int step = 0;
    
    // 获取组件状态机
    auto robotArm = dynamic_cast<RobotArmStateMachine*>(m_machine->getRobotArm().get());
    auto storageUnit = dynamic_cast<StorageUnitStateMachine*>(m_machine->getStorageUnit().get());
    auto penetration = dynamic_cast<PenetrationMechanismStateMachine*>(m_machine->getPenetrationMechanism().get());
    auto drilling = dynamic_cast<DrillingMechanismStateMachine*>(m_machine->getDrillingMechanism().get());
    auto clamp = dynamic_cast<ClampMechanismStateMachine*>(m_machine->getClampMechanism().get());
    auto connection = dynamic_cast<ConnectionMechanismStateMachine*>(m_machine->getConnectionMechanism().get());
    
    if (!robotArm || !storageUnit || !penetration || !drilling || !clamp || !connection) {
        qDebug() << "错误：无法获取组件状态机";
        return;
    }
    
    // 获取当前钻管计数
    int pipeCount = m_machine->getCurrentPipeCount();
    
    switch (step) {
        case 0: // 1. 机械手取钻管 - 旋转到存储区
            m_machine->emit currentStepChanged("机械手移动到存储区");
            robotArm->setRotationPosition(90); // 旋转到存储区位置
            step++;
            break;
            
        case 1: // 机械手伸出
            if (robotArm->getRotationPosition() == 90) {
                robotArm->setExtension(200); // 伸出
                step++;
            }
            break;
            
        case 2: // 机械手夹紧
            if (robotArm->getExtension() == 200) {
                robotArm->setClamp(100); // 完全夹紧
                step++;
            }
            break;
            
        case 3: // 机械手缩回
            if (robotArm->getClamp() == 100) {
                robotArm->setExtension(0); // 缩回
                step++;
            }
            break;
            
        case 4: // 机械手旋转到钻台
            if (robotArm->getExtension() == 0) {
                robotArm->setRotationPosition(0); // 旋转到钻台位置
                step++;
            }
            break;
            
        case 5: // 机械手伸出
            if (robotArm->getRotationPosition() == 0) {
                robotArm->setExtension(200); // 伸出
                step++;
            }
            break;
            
        case 6: // 2. 进给机构下降
            if (robotArm->getExtension() == 200) {
                m_machine->emit currentStepChanged("进给机构下降到钻管安装高度");
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_C2); // 钻管安装高度
                step++;
            }
            break;
            
        case 7: // 3. 执行连接操作
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_C2) {
                m_machine->emit currentStepChanged("执行连接操作");
                drilling->setRotationSpeed(60); // 低速对接旋转
                step++;
            }
            break;
            
        case 8:
            if (drilling->getRotationSpeed() == 60) {
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_B2); // 对接速度缓慢下移
                step++;
            }
            break;
            
        case 9:
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_B2) {
                drilling->setRotationSpeed(0); // 停止旋转
                step++;
            }
            break;
            
        case 10: // 4. 对接机构推出
            if (drilling->getRotationSpeed() == 0) {
                m_machine->emit currentStepChanged("对接机构推出");
                connection->setConnectionState(true); // 锁住钻管
                step++;
            }
            break;
            
        case 11: // 5. 机械手收回
            if (connection->isExtended()) {
                m_machine->emit currentStepChanged("机械手松开并收回");
                robotArm->setClamp(0); // 松开
                step++;
            }
            break;
            
        case 12:
            if (robotArm->getClamp() == 0) {
                robotArm->setExtension(0); // 收回
                step++;
            }
            break;
            
        case 13: // 6. 钻管之间的对接
            if (robotArm->getExtension() == 0) {
                m_machine->emit currentStepChanged("钻管之间的对接");
                drilling->setRotationSpeed(60); // 低速对接旋转
                step++;
            }
            break;
            
        case 14:
            if (drilling->getRotationSpeed() == 60) {
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_A1); // 对接速度缓慢下降
                step++;
            }
            break;
            
        case 15:
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_A1) {
                drilling->setRotationSpeed(0); // 停止旋转
                step++;
            }
            break;
            
        case 16: // 7. 下夹紧机构松开
            if (drilling->getRotationSpeed() == 0) {
                m_machine->emit currentStepChanged("下夹紧机构松开");
                clamp->setClampState(ClampMechanismStateMachine::OPEN); // 松开
                step++;
            }
            break;
            
        case 17: // 8. 钻机钻进
            if (clamp->getClampState() == ClampMechanismStateMachine::OPEN) {
                m_machine->emit currentStepChanged("开始钻进");
                drilling->setRotationSpeed(120); // 工作转速
                drilling->setPercussionFrequency(10); // 工作频率
                step++;
            }
            break;
            
        case 18:
            if (drilling->getRotationSpeed() == 120 && drilling->getPercussionFrequency() == 10) {
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_A); // 钻进速度下降到工作位置
                step++;
            }
            break;
            
        case 19:
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_A) {
                drilling->setRotationSpeed(0); // 停止旋转
                drilling->setPercussionFrequency(0); // 停止冲击
                step++;
            }
            break;
            
        case 20: // 9. 下夹紧机构夹紧
            if (drilling->getRotationSpeed() == 0) {
                m_machine->emit currentStepChanged("下夹紧机构夹紧");
                clamp->setClampState(ClampMechanismStateMachine::TIGHT); // 夹紧
                step++;
            }
            break;
            
        case 21: // 10. 执行断开操作 - 对接机构回收
            if (clamp->getClampState() == ClampMechanismStateMachine::TIGHT) {
                m_machine->emit currentStepChanged("对接机构回收");
                connection->setConnectionState(false); // 回收
                step++;
            }
            break;
            
        case 22: // 钻进旋转反向
            if (!connection->isExtended()) {
                drilling->setRotationSpeed(-60); // 低速反向旋转
                step++;
            }
            break;
            
        case 23: // 进给机构缓慢上移
            if (drilling->getRotationSpeed() == -60) {
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_A1); // 对接速度缓慢上移
                step++;
            }
            break;
            
        case 24:
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_A1) {
                drilling->setRotationSpeed(0); // 停止旋转
                step++;
            }
            break;
            
        case 25: // 11. 进给机构上升
            if (drilling->getRotationSpeed() == 0) {
                m_machine->emit currentStepChanged("进给机构上升");
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_D); // 空行程速度上升
                step++;
            }
            break;
            
        case 26: // 完成钻管安装循环
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_D) {
                m_machine->emit currentStepChanged("钻管安装循环完成");
                
                // 检查是否需要继续安装钻管或切换到拆卸模式
                if (pipeCount < 5) { // 假设最多安装5根钻管
                    // 重置步骤计数器，继续安装下一根钻管
                    step = 0;
                } else {
                    // 重置步骤计数器，切换到钻管拆卸循环
                    step = 0;
                    m_machine->changeState(AutoDrillingStateMachine::STATE_PIPE_REMOVAL_LOOP);
                }
            }
            break;
    }
}

// PipeRemovalLoopState 实现
PipeRemovalLoopState::PipeRemovalLoopState(AutoDrillingStateMachine* machine)
    : DrillState(machine)
{
}

void PipeRemovalLoopState::enter()
{
    DrillState::enter();
    qDebug() << "钻管拆卸循环状态：准备拆卸钻管";
    
    // 发送当前步骤变化信号
    m_machine->emit currentStepChanged("钻管拆卸循环开始");
}

void PipeRemovalLoopState::update()
{
    // 实现钻管拆卸循环流程
    static int step = 0;
    
    // 获取组件状态机
    auto robotArm = dynamic_cast<RobotArmStateMachine*>(m_machine->getRobotArm().get());
    auto storageUnit = dynamic_cast<StorageUnitStateMachine*>(m_machine->getStorageUnit().get());
    auto penetration = dynamic_cast<PenetrationMechanismStateMachine*>(m_machine->getPenetrationMechanism().get());
    auto drilling = dynamic_cast<DrillingMechanismStateMachine*>(m_machine->getDrillingMechanism().get());
    auto clamp = dynamic_cast<ClampMechanismStateMachine*>(m_machine->getClampMechanism().get());
    auto connection = dynamic_cast<ConnectionMechanismStateMachine*>(m_machine->getConnectionMechanism().get());
    
    if (!robotArm || !storageUnit || !penetration || !drilling || !clamp || !connection) {
        qDebug() << "错误：无法获取组件状态机";
        return;
    }
    
    // 获取当前钻管计数
    int pipeCount = m_machine->getCurrentPipeCount();
    
    switch (step) {
        case 0: // 1. 进给机构下降
            m_machine->emit currentStepChanged("进给机构下降");
            penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_A1); // 空行程速度下降
            step++;
            break;
            
        case 1: // 执行连接操作
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_A1) {
                m_machine->emit currentStepChanged("执行连接操作");
                drilling->setRotationSpeed(60); // 低速对接旋转
                step++;
            }
            break;
            
        case 2:
            if (drilling->getRotationSpeed() == 60) {
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_A); // 对接速度缓慢下移
                step++;
            }
            break;
            
        case 3:
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_A) {
                drilling->setRotationSpeed(0); // 停止旋转
                step++;
            }
            break;
            
        case 4: // 对接机构推出
            if (drilling->getRotationSpeed() == 0) {
                m_machine->emit currentStepChanged("对接机构推出");
                connection->setConnectionState(true); // 对接机构伸出
                step++;
            }
            break;
            
        case 5: // 2. 下夹紧松开
            if (connection->isExtended()) {
                m_machine->emit currentStepChanged("下夹紧机构松开");
                clamp->setClampState(ClampMechanismStateMachine::OPEN); // 松开
                step++;
            }
            break;
            
        case 6: // 3. 进给机构上升
            if (clamp->getClampState() == ClampMechanismStateMachine::OPEN) {
                m_machine->emit currentStepChanged("进给机构上升");
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_B2); // 空行程速度上升
                step++;
            }
            break;
            
        case 7: // 4. 下夹紧机构夹紧
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_B2) {
                m_machine->emit currentStepChanged("下夹紧机构夹紧");
                clamp->setClampState(ClampMechanismStateMachine::TIGHT); // 夹紧
                step++;
            }
            break;
            
        case 8: // 5. 断开钻管间连接
            if (clamp->getClampState() == ClampMechanismStateMachine::TIGHT) {
                m_machine->emit currentStepChanged("断开钻管间连接");
                drilling->setRotationSpeed(-60); // 低速反向旋转
                step++;
            }
            break;
            
        case 9:
            if (drilling->getRotationSpeed() == -60) {
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_C2); // 对接速度缓慢上移
                step++;
            }
            break;
            
        case 10:
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_C2) {
                drilling->setRotationSpeed(0); // 停止旋转
                step++;
            }
            break;
            
        case 11: // 6. 机械手抓取钻管
            if (drilling->getRotationSpeed() == 0) {
                m_machine->emit currentStepChanged("机械手抓取钻管");
                robotArm->setExtension(250); // 伸出
                step++;
            }
            break;
            
        case 12:
            if (robotArm->getExtension() == 250) {
                robotArm->setClamp(100); // 完全夹紧
                step++;
            }
            break;
            
        case 13: // 7. 对接机构回收
            if (robotArm->getClamp() == 100) {
                m_machine->emit currentStepChanged("对接机构回收");
                connection->setConnectionState(false); // 解锁钻进机构的钻管
                step++;
            }
            break;
            
        case 14: // 8. 断开钻管与钻进机构连接
            if (!connection->isExtended()) {
                m_machine->emit currentStepChanged("断开钻管与钻进机构连接");
                drilling->setRotationSpeed(-60); // 低速反向旋转
                step++;
            }
            break;
            
        case 15:
            if (drilling->getRotationSpeed() == -60) {
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_D); // 对接速度缓慢上移
                step++;
            }
            break;
            
        case 16:
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_D) {
                drilling->setRotationSpeed(0); // 停止旋转
                step++;
            }
            break;
            
        case 17: // 9. 存储机构旋转到空位
            if (drilling->getRotationSpeed() == 0) {
                m_machine->emit currentStepChanged("存储机构旋转到空位");
                // 计算存储位置：假设总共有14个位置，从0开始，当前钻管数为pipeCount
                int storagePosition = StorageUnitStateMachine::MAX_POSITIONS - pipeCount;
                storageUnit->rotateToPosition(storagePosition);
                step++;
            }
            break;
            
        case 18: // 10. 机械手回收
            if (storageUnit->getCurrentPosition() == StorageUnitStateMachine::MAX_POSITIONS - pipeCount) {
                m_machine->emit currentStepChanged("机械手回收钻管");
                robotArm->setExtension(0); // 缩回
                step++;
            }
            break;
            
        case 19:
            if (robotArm->getExtension() == 0) {
                robotArm->setRotationPosition(90); // 旋转到存储区
                step++;
            }
            break;
            
        case 20:
            if (robotArm->getRotationPosition() == 90) {
                robotArm->setExtension(250); // 伸出
                step++;
            }
            break;
            
        case 21:
            if (robotArm->getExtension() == 250) {
                robotArm->setClamp(0); // 完全松开
                step++;
            }
            break;
            
        case 22:
            if (robotArm->getClamp() == 0) {
                robotArm->setExtension(0); // 缩回
                step++;
            }
            break;
            
        case 23:
            if (robotArm->getExtension() == 0) {
                robotArm->setRotationPosition(0); // 旋转回钻台
                step++;
            }
            break;
            
        case 24: // 11. 进给机构上升
            if (robotArm->getRotationPosition() == 0) {
                m_machine->emit currentStepChanged("进给机构上升到待机位置");
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_D); // 待机位置
                step++;
            }
            break;
            
        case 25: // 完成钻管拆卸循环
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_D) {
                m_machine->emit currentStepChanged("钻管拆卸循环完成");
                
                // 检查是否还有钻管需要拆卸
                if (pipeCount > 1) { // 如果还有钻管（不包括首根钻具）
                    // 重置步骤计数器，继续拆卸下一根钻管
                    step = 0;
                } else {
                    // 重置步骤计数器，切换到首根钻具回收
                    step = 0;
                    m_machine->changeState(AutoDrillingStateMachine::STATE_FIRST_TOOL_RECOVERY);
                }
            }
            break;
    }
}

// FirstToolRecoveryState 实现
FirstToolRecoveryState::FirstToolRecoveryState(AutoDrillingStateMachine* machine)
    : DrillState(machine)
{
}

void FirstToolRecoveryState::enter()
{
    DrillState::enter();
    qDebug() << "首根钻具回收状态：准备回收首根钻具";
    
    // 发送当前步骤变化信号
    m_machine->emit currentStepChanged("首根钻具回收开始");
}

void FirstToolRecoveryState::update()
{
    // 实现首根钻具回收流程
    static int step = 0;
    
    // 获取组件状态机
    auto robotArm = dynamic_cast<RobotArmStateMachine*>(m_machine->getRobotArm().get());
    auto storageUnit = dynamic_cast<StorageUnitStateMachine*>(m_machine->getStorageUnit().get());
    auto penetration = dynamic_cast<PenetrationMechanismStateMachine*>(m_machine->getPenetrationMechanism().get());
    auto drilling = dynamic_cast<DrillingMechanismStateMachine*>(m_machine->getDrillingMechanism().get());
    auto clamp = dynamic_cast<ClampMechanismStateMachine*>(m_machine->getClampMechanism().get());
    auto connection = dynamic_cast<ConnectionMechanismStateMachine*>(m_machine->getConnectionMechanism().get());
    
    if (!robotArm || !storageUnit || !penetration || !drilling || !clamp || !connection) {
        qDebug() << "错误：无法获取组件状态机";
        return;
    }
    
    switch (step) {
        case 0: // 1. 进给机构下降
            m_machine->emit currentStepChanged("进给机构下降");
            penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_A1); // 空行程速度下降
            step++;
            break;
            
        case 1: // 执行连接操作
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_A1) {
                m_machine->emit currentStepChanged("执行连接操作");
                drilling->setRotationSpeed(60); // 低速对接旋转
                step++;
            }
            break;
            
        case 2:
            if (drilling->getRotationSpeed() == 60) {
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_A); // 对接速度缓慢下移
                step++;
            }
            break;
            
        case 3:
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_A) {
                drilling->setRotationSpeed(0); // 停止旋转
                step++;
            }
            break;
            
        case 4: // 对接机构推出
            if (drilling->getRotationSpeed() == 0) {
                m_machine->emit currentStepChanged("对接机构推出");
                connection->setConnectionState(true); // 对接机构伸出
                step++;
            }
            break;
            
        case 5: // 2. 下夹紧松开
            if (connection->isExtended()) {
                m_machine->emit currentStepChanged("下夹紧机构松开");
                clamp->setClampState(ClampMechanismStateMachine::OPEN); // 松开
                step++;
            }
            break;
            
        case 6: // 3. 进给机构上升
            if (clamp->getClampState() == ClampMechanismStateMachine::OPEN) {
                m_machine->emit currentStepChanged("进给机构上升");
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_C2); // 空行程速度上升
                step++;
            }
            break;
            
        case 7: // 4. 机械手抓取钻管
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_C2) {
                m_machine->emit currentStepChanged("机械手抓取钻具");
                robotArm->setExtension(250); // 伸出
                step++;
            }
            break;
            
        case 8:
            if (robotArm->getExtension() == 250) {
                robotArm->setClamp(100); // 完全夹紧
                step++;
            }
            break;
            
        case 9: // 5. 对接机构回收
            if (robotArm->getClamp() == 100) {
                m_machine->emit currentStepChanged("对接机构回收");
                connection->setConnectionState(false); // 解锁钻进机构的钻管
                step++;
            }
            break;
            
        case 10: // 6. 断开钻管与钻进机构连接
            if (!connection->isExtended()) {
                m_machine->emit currentStepChanged("断开钻具与钻进机构连接");
                drilling->setRotationSpeed(-60); // 低速反向旋转
                step++;
            }
            break;
            
        case 11:
            if (drilling->getRotationSpeed() == -60) {
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_D); // 对接速度缓慢上移
                step++;
            }
            break;
            
        case 12:
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_D) {
                drilling->setRotationSpeed(0); // 停止旋转
                step++;
            }
            break;
            
        case 13: // 7. 存储机构旋转到空位
            if (drilling->getRotationSpeed() == 0) {
                m_machine->emit currentStepChanged("存储机构旋转到空位");
                // 计算存储位置：假设总共有14个位置，从0开始，当前钻管数为0（首根钻具）
                int storagePosition = StorageUnitStateMachine::MAX_POSITIONS;
                storageUnit->rotateToPosition(storagePosition);
                step++;
            }
            break;
            
        case 14: // 8. 机械手回收
            if (storageUnit->getCurrentPosition() == StorageUnitStateMachine::MAX_POSITIONS) {
                m_machine->emit currentStepChanged("机械手回收钻具");
                robotArm->setExtension(0); // 缩回
                step++;
            }
            break;
            
        case 15:
            if (robotArm->getExtension() == 0) {
                robotArm->setRotationPosition(90); // 旋转到存储区
                step++;
            }
            break;
            
        case 16:
            if (robotArm->getRotationPosition() == 90) {
                robotArm->setExtension(250); // 伸出
                step++;
            }
            break;
            
        case 17:
            if (robotArm->getExtension() == 250) {
                robotArm->setClamp(0); // 完全松开
                step++;
            }
            break;
            
        case 18:
            if (robotArm->getClamp() == 0) {
                robotArm->setExtension(0); // 缩回
                step++;
            }
            break;
            
        case 19:
            if (robotArm->getExtension() == 0) {
                robotArm->setRotationPosition(0); // 旋转回钻台
                step++;
            }
            break;
            
        case 20: // 9. 进给机构下降
            if (robotArm->getRotationPosition() == 0) {
                m_machine->emit currentStepChanged("进给机构下降到待机位置");
                penetration->moveToPosition(PenetrationMechanismStateMachine::POSITION_A); // 待机位置
                step++;
            }
            break;
            
        case 21: // 完成首根钻具回收
            if (penetration->getCurrentPosition() == PenetrationMechanismStateMachine::POSITION_A) {
                m_machine->emit currentStepChanged("首根钻具回收完成");
                // 重置步骤计数器，为下一次使用做准备
                step = 0;
                // 切换到操作完成状态
                m_machine->changeState(AutoDrillingStateMachine::STATE_OPERATION_COMPLETE);
            }
            break;
    }
}

// SystemResetState 实现
SystemResetState::SystemResetState(AutoDrillingStateMachine* machine)
    : DrillState(machine)
{
}

void SystemResetState::enter()
{
    DrillState::enter();
    qDebug() << "系统重置状态：重置所有组件";
}

void SystemResetState::update()
{
    // TODO: 实现系统重置逻辑
}

// OperationCompleteState 实现
OperationCompleteState::OperationCompleteState(AutoDrillingStateMachine* machine)
    : DrillState(machine)
{
}

void OperationCompleteState::enter()
{
    DrillState::enter();
    qDebug() << "操作完成状态：所有操作已完成";
}

void OperationCompleteState::update()
{
    // TODO: 实现操作完成逻辑
} 