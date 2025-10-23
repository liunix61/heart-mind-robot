#include "mainwindow.h"
#include <QMenu>
#include "./ui_mainwindow.h"
#include "Log_util.h"
#include "LAppLive2DManager.hpp"
#include "resource_loader.hpp"
#include "event_handler.hpp"
#include <QMessageBox>
#include <QMouseEvent>
#include <QtGui/QGuiApplication>
#include "qaction.h"
#include "qactiongroup.h"
#include <QMenuBar>
#include "MouseEvent.h"
#include "platform_config.h" // 这个文件头不能放到mainwindow.h中，否则会报错

namespace {
    int pos_x;
    int pos_y;
}

MainWindow::MainWindow(QWidget *parent, QApplication *mapp)
        : QMainWindow(parent), ui(new Ui::MainWindow), m_systemTray(new QSystemTrayIcon(this)), app(mapp),
          dialog_window_(new ChatDialog(this)) {
    assert(app != nullptr);
    ui->setupUi(this);
    int cxScreen, cyScreen; // 获取屏幕的分辨率
    cxScreen = QApplication::primaryScreen()->availableGeometry().width();
    cyScreen = QApplication::primaryScreen()->availableGeometry().height();
    resource_loader::get_instance().screen_width = cxScreen;
    resource_loader::get_instance().screen_height = cyScreen;
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setWindowFlag(Qt::FramelessWindowHint);
    this->setWindowFlag(Qt::NoDropShadowWindowHint); //去掉窗口阴影，这个bug查了好久好久！！！！
    this->move(resource_loader::get_instance().current_model_x, resource_loader::get_instance().current_model_y);
    auto model = resource_loader::get_instance().get_current_model();
    this->resize(model->model_width, model->model_height);
    if (resource_loader::get_instance().is_top()) {
        this->setWindowFlag(Qt::WindowStaysOnTopHint);
    }
//------------------------------------------------------------------
    m_menu = new QMenu(this);
    m_move = new QMenu(this);
    m_dialog = new QMenu(this);
    auto menuBar = new QMenuBar(this);
    this->setMenuBar(menuBar);
    menuBar->addMenu(m_menu);

    m_menu->setTitle(QStringLiteral("菜单"));
    m_move->setTitle(QStringLiteral("位置和大小调节"));
    m_dialog->setTitle(QStringLiteral("对话"));

    // 添加菜单项
    auto action_exit = new QAction(QStringLiteral("退出"), this);
    auto action_set_top = new QAction(QStringLiteral("置顶"), this);
    action_set_top->setCheckable(true);
    action_set_top->setChecked(resource_loader::get_instance().is_top());
    auto action_voice = new QAction(QStringLiteral("语音"), this);
    action_voice->setCheckable(true);
    action_voice->setChecked(resource_loader::get_instance().is_voice());

    m_menu->addAction(action_exit);
    m_menu->addAction(action_set_top);
    m_menu->addAction(action_voice);
    m_menu->addMenu(m_move);
    m_menu->addMenu(m_dialog);

    // 位置调节菜单
    auto action_move = new QAction(QStringLiteral("移动"), this);
    auto action_change = new QAction(QStringLiteral("切换模型"), this);
    m_move->addAction(action_move);
    m_move->addAction(action_change);

    // 对话菜单
    auto action_dialog = new QAction(QStringLiteral("对话"), this);
    m_dialog->addAction(action_dialog);

    // 连接信号槽
    connect(action_exit, &QAction::triggered, this, &MainWindow::action_exit);
    connect(action_set_top, &QAction::triggered, this, &MainWindow::action_set_top);
    connect(action_voice, &QAction::triggered, this, &MainWindow::action_voice);
    connect(action_move, &QAction::triggered, this, &MainWindow::action_move);
    connect(action_change, &QAction::triggered, this, &MainWindow::action_change);
    connect(action_dialog, &QAction::triggered, this, &MainWindow::action_dialog);

    // 系统托盘
    m_systemTray->setIcon(QIcon(":/SystemIcon.png"));
    m_systemTray->setToolTip(QStringLiteral("Chat Friend"));
    m_systemTray->show();
    connect(m_systemTray, &QSystemTrayIcon::activated, this, &MainWindow::activeTray);

    // 鼠标事件处理
    mouseEventHandle = new MouseEventHandle(this);
    connect(mouseEventHandle, &MouseEventHandle::destroyed, this, &MainWindow::deleteLater);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::activeTray(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick) {
        this->showNormal();
    }
}

void MainWindow::action_exit() {
    CF_LOG_INFO("main_window exit");
    this->close();
}

void MainWindow::action_set_top() {
    bool isTop = this->windowFlags() & Qt::WindowStaysOnTopHint;
    if (isTop) {
        this->setWindowFlag(Qt::WindowStaysOnTopHint, false);
        resource_loader::get_instance().set_top(false);
    } else {
        this->setWindowFlag(Qt::WindowStaysOnTopHint, true);
        resource_loader::get_instance().set_top(true);
    }
    this->show();
}

void MainWindow::action_voice() {
    bool isVoice = resource_loader::get_instance().is_voice();
    resource_loader::get_instance().set_voice(!isVoice);
}

void MainWindow::action_move(QAction *action) {
    if (action->text() == QStringLiteral("移动")) {
        this->setCursor(Qt::SizeAllCursor);
        action->setText(QStringLiteral("完成"));
    } else {
        this->setCursor(Qt::ArrowCursor);
        action->setText(QStringLiteral("移动"));
    }
}

void MainWindow::action_change(QAction *action) {
    // 切换模型逻辑
    this->hide();
    // 重新加载模型
    this->show();
}

void MainWindow::action_dialog(QAction *action) {
    if (dialog_window_->isVisible()) {
        dialog_window_->hide();
    } else {
        dialog_window_->show();
    }
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        pos_x = event->x();
        pos_y = event->y();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        this->move(event->globalX() - pos_x, event->globalY() - pos_y);
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        resource_loader::get_instance().current_model_x = this->x();
        resource_loader::get_instance().current_model_y = this->y();
        resource_loader::get_instance().save_config();
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    CF_LOG_INFO("app exit");
    if (mouseEventHandle) {
        mouseEventHandle->stopMonitoring();
    }
    event->accept();
}
