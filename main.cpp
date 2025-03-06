#include "mainwindow.h"
#include <QApplication>
#include <QTextCodec>
#include "inc/autodrilling.h"
#include "inc/DebugTestMotion.h"
#include "inc/DrillingController.h"
#include <QCoreApplication>
#include <iostream>
#include <QThread>

int main(int argc, char *argv[])
{
    // 设置UTF-8编码，确保中文显示正常
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    
    // 创建应用程序实例
    QApplication a(argc, argv);
    
#ifdef ENABLE_DEBUG_TEST_MOTION
    // 创建调试测试对象
    DebugTestMotion debugTest;
    
    if (!debugTest.initialize()) {
        std::cerr << "调试测试初始化失败" << std::endl;
        return 1;
    }
    
    std::cout << "\n=== 自动钻进状态机测试程序 ===" << std::endl;
    std::cout << "请选择测试模式:" << std::endl;
    std::cout << "1. 单步执行模式 (按Enter执行下一步，输入'p'查看状态，输入'q'退出)" << std::endl;
    std::cout << "2. 自动测试模式 (每个状态会暂停等待确认)" << std::endl;
    std::cout << "3. 跳过测试，直接启动GUI" << std::endl;
    std::cout << "请输入选择 (1, 2 或 3): ";
    std::cout.flush();
    
    char choice = 0;
    std::string input;
    std::getline(std::cin, input);
    if (!input.empty()) {
        choice = input[0];
    }
    
    if (choice == '1') {
        // 单步执行模式
        std::cout << "\n=== 单步执行模式 ===" << std::endl;
        std::cout << "命令说明:" << std::endl;
        std::cout << "- 按Enter: 执行下一步" << std::endl;
        std::cout << "- 输入'p': 打印当前状态" << std::endl;
        std::cout << "- 输入'q': 退出测试" << std::endl;
        
        std::cout << "\n按Enter开始测试..." << std::endl;
        std::getline(std::cin, input);
        
        debugTest.stepForward(); // 执行第一步
        
        while (true) {
            std::getline(std::cin, input);
            
            if (input == "q" || input == "quit" || input == "exit") {
                break;
            } else if (input == "p" || input == "print") {
                debugTest.printCurrentState();
            } else {
                debugTest.stepForward();
            }
        }
    } else if (choice == '2') {
        // 自动测试模式
        std::cout << "\n=== 自动测试模式 ===" << std::endl;
        std::cout << "每个状态转换后会暂停等待确认" << std::endl;
        std::cout << "按Enter开始测试，按'q'退出测试" << std::endl;
        std::cout << "\n按Enter开始..." << std::endl;
        
        std::getline(std::cin, input);
        if (input != "q") {
            debugTest.startTest();
        }
    } else if (choice != '3') {
        std::cout << "无效选择，将直接启动GUI" << std::endl;
    }
#else
    std::cout << "调试测试模式未启用" << std::endl;
#endif

    // 启动GUI
    std::cout << "\n测试完成，正在启动GUI界面..." << std::endl;
    MainWindow w;
    w.show();
    return a.exec();
}
