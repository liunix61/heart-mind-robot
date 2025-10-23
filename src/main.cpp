#include "mainwindow.h"
#include "event_handler.hpp"
#include "resource_loader.hpp"
#include "ActivationManager.h"
#include "ActivationWindow.h"
#include "DeskPetIntegration.h"
#include <QApplication>
#include <QMessageBox>
#include <QCommandLineParser>
#include "MouseEvent.h"
#include "platform_config.h"

/// TODO 官方框架自带的json解析器似乎有问题，时而崩溃？需要排查一下。还有动画播放卡顿（Idle结束的时候）

bool checkActivationStatus() {
    ActivationManager* activationManager = ActivationManager::getInstance();
    bool isActivated = activationManager->isActivated();
    qDebug() << "Checking activation status:" << isActivated;
    qDebug() << "Activation state:" << activationManager->getActivationState();
    return isActivated;
}

bool showActivationDialog(QApplication& app) {
    qDebug() << "Creating ActivationWindow...";
    ActivationWindow activationWindow;
    qDebug() << "ActivationWindow created, setting title...";
    activationWindow.setWindowTitle("设备激活 - Live2D桌宠");
    
    qDebug() << "Connecting signals...";
    // 连接激活完成信号
    QObject::connect(&activationWindow, &ActivationWindow::activationCompleted, 
                     [&app](bool success) {
                         if (success) {
                             qDebug() << "Activation completed successfully";
                         } else {
                             qDebug() << "Activation failed or cancelled";
                         }
                     });
    
    qDebug() << "Showing activation window...";
    // 显示激活窗口
    int result = activationWindow.exec();
    qDebug() << "Activation window closed with result:" << result;
    
    if (result == QDialog::Accepted) {
        return activationWindow.isActivated();
    }
    
    return false;
}

int main(int argc, char *argv[]) {
    qDebug() << "=== Main function started ===";
    event_handler::get_instance();
    qDebug() << "Event handler initialized";
    QApplication a(argc, argv);
    qDebug() << "QApplication created";
    
    // 设置应用程序信息
    a.setApplicationName("Live2D桌宠");
    a.setApplicationVersion("1.0.0");
    a.setOrganizationName("Live2D桌宠");
    
    // 解析命令行参数
    QCommandLineParser parser;
    parser.setApplicationDescription("Live2D桌面宠物应用");
    parser.addHelpOption();
    parser.addVersionOption();
    
    // 添加跳过激活选项
    QCommandLineOption skipActivationOption("skip-activation", "跳过激活流程（仅用于调试）");
    parser.addOption(skipActivationOption);
    
    // 添加激活模式选项
    QCommandLineOption activationModeOption("activation-mode", "激活模式 (gui/cli)", "mode", "gui");
    parser.addOption(activationModeOption);
    
    parser.process(a);
    
    // 检查是否跳过激活
    bool skipActivation = parser.isSet(skipActivationOption);
    
    // 强制显示激活对话框进行测试
    qDebug() << "Forcing activation dialog display for testing...";
    if (!showActivationDialog(a)) {
        qDebug() << "Activation failed or cancelled, exiting...";
        return 1;
    }
    
    // 初始化资源加载器
    if (!resource_loader::get_instance().initialize()) {
        QMessageBox::critical(nullptr, "错误", "资源加载失败，程序无法启动");
        return 1;
    }
    
    // 创建并显示主窗口
    MainWindow w(nullptr, &a);
    w.show();
    
    // 创建并初始化WebSocket桌宠集成
    qDebug() << "Initializing WebSocket DeskPet Integration...";
    DeskPetIntegration *deskPetIntegration = new DeskPetIntegration(&w);
    
    if (deskPetIntegration->initialize(&w)) {
        qDebug() << "DeskPetIntegration initialized successfully";
        
        // 将DeskPetIntegration传递给MainWindow
        w.setDeskPetIntegration(deskPetIntegration);
        
        // 显示WebSocket聊天框
        w.showWebSocketChatDialog();
        
        // 尝试连接到服务器
        qDebug() << "Attempting to connect to WebSocket server...";
        if (deskPetIntegration->connectToServer()) {
            qDebug() << "WebSocket connection request sent successfully";
        } else {
            qDebug() << "Failed to send WebSocket connection request";
        }
    } else {
        qDebug() << "Failed to initialize DeskPetIntegration";
    }
    
    return QApplication::exec();
}
