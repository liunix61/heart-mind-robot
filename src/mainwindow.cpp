#include "mainwindow.h"
#include "WebSocketChatDialog.h"
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
#include <QThread>

namespace {
    int pos_x;
    int pos_y;
}

MainWindow::MainWindow(QWidget *parent, QApplication *mapp)
        : QMainWindow(parent), ui(new Ui::MainWindow), m_systemTray(new QSystemTrayIcon(this)), app(mapp),
          dialog_window_(new ChatDialog(this)), websocket_dialog_window_(new WebSocketChatDialog(this)) {
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
    m_change = new QMenu(this);
    auto menuBar = new QMenuBar(this);
    this->setMenuBar(menuBar);
    menuBar->addMenu(m_menu);
    // 初始化时隐藏菜单栏
    menuBar->hide();

    m_menu->setTitle(QStringLiteral("菜单"));
    m_move->setTitle(QStringLiteral("位置和大小调节"));
    m_dialog->setTitle(QStringLiteral("对话框"));
    m_change->setTitle(QStringLiteral("切换"));

    // 创建 ActionGroup
    g_move = new QActionGroup(m_move);
    g_change = new QActionGroup(m_change);
    g_dialog = new QActionGroup(m_dialog);

    // 添加菜单项
    a_exit = new QAction(QStringLiteral("退出"), this);
    set_top = new QAction(QStringLiteral("置顶"), this);
    set_top->setCheckable(true);
    set_top->setChecked(resource_loader::get_instance().is_top());
    // auto a_voice = new QAction(QStringLiteral("语音"), this);  // 已移除语音设置项
    // a_voice->setCheckable(true);
    // a_voice->setChecked(resource_loader::get_instance().is_voice());

    m_menu->addAction(set_top);
    // m_menu->addAction(a_voice);  // 已移除语音设置项
    m_menu->addMenu(m_change);
    m_menu->addMenu(m_move);
    m_menu->addMenu(m_dialog);
    m_menu->addSeparator();
    m_menu->addAction(a_exit);

    // 位置调节菜单
    move_on = new QAction(QStringLiteral("开"), g_move);
    move_off = new QAction(QStringLiteral("关"), g_move);
    move_on->setCheckable(true);
    move_off->setCheckable(true);
    move_off->setChecked(true);  // 默认关闭移动
    g_move->setExclusive(true);
    m_move->addAction(move_on);
    m_move->addAction(move_off);

    // 对话菜单
    open_dialog = new QAction(QStringLiteral("打开"), g_dialog);
    close_dialog = new QAction(QStringLiteral("关闭"), g_dialog);
    open_dialog->setCheckable(true);
    close_dialog->setCheckable(true);
    open_dialog->setChecked(true);  // 默认打开对话框
    g_dialog->setExclusive(true);
    m_dialog->addAction(open_dialog);
    m_dialog->addAction(close_dialog);

    // 模型切换菜单 - 动态生成模型列表
    auto ms = resource_loader::get_instance().get_model_list();
    auto cm = resource_loader::get_instance().get_current_model();
    int current_index = 0;
    for (int i = 0; i < ms.size(); i++) {
        auto *tmp_model = new QAction(ms[i].name, g_change);
        tmp_model->setCheckable(true);
        model_list.push_back(tmp_model);
        if (ms[i].name == cm->name) {
            current_index = i;
        }
    }
    g_change->setExclusive(true);
    m_change->addActions(model_list);
    
    // 设置当前模型为选中状态
    if (current_index < model_list.size()) {
        model_list[current_index]->setChecked(true);
    }

    // 连接信号槽
    connect(a_exit, &QAction::triggered, this, &MainWindow::action_exit);
    connect(set_top, &QAction::triggered, this, &MainWindow::action_set_top);
    // connect(a_voice, &QAction::triggered, this, &MainWindow::action_voice);  // 已移除语音设置项
    connect(g_move, &QActionGroup::triggered, this, &MainWindow::action_move);
    connect(g_change, &QActionGroup::triggered, this, &MainWindow::action_change);
    connect(g_dialog, &QActionGroup::triggered, this, &MainWindow::action_dialog);

    // 系统托盘
    m_systemTray->setIcon(QIcon(resource_loader::get_instance().get_system_tray_icon_path()));
    m_systemTray->setToolTip("Heart Mind Robot");
    m_systemTray->setContextMenu(m_menu);
    m_systemTray->show();
    connect(m_systemTray, &QSystemTrayIcon::activated, this, &MainWindow::activeTray);

    // 鼠标事件处理
    mouse_event_ = new MouseEventHandle();
    event_handler::register_main_window(this);
    MouseEventHandle::EnableMousePassThrough(this->winId(), true);
    std::thread t(&MouseEventHandle::startMonitoring, mouse_event_);
    t.detach();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::activeTray(QSystemTrayIcon::ActivationReason r) {
    switch (r) {
        case QSystemTrayIcon::Context:
            m_menu->showNormal();
            break;
        default:
            break;
    }
}

void MainWindow::action_exit() {
    CF_LOG_INFO("main_window exit");
    resource_loader::get_instance().release();
    mouse_event_->stopMonitoring();
    QApplication::exit(0);
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

void MainWindow::action_move(QAction *a) {
    if (a == this->move_on) {
        CF_LOG_DEBUG("move on");
        MouseEventHandle::EnableMousePassThrough(this->winId(), false);
        this->m_change->setEnabled(false);
        this->a_exit->setEnabled(false);
        this->m_dialog->setEnabled(false);
        // 隐藏主窗口菜单栏
        this->menuBar()->hide();
        // 移动开启：对话框显示边框，可以拖动和缩放
        if (websocket_dialog_window_) {
            bool wasVisible = websocket_dialog_window_->isVisible();
            QPoint oldPos = websocket_dialog_window_->pos();
            QSize oldSize = websocket_dialog_window_->size();
            
            // 设置新的窗口标志（有边框，但无菜单）
            Qt::WindowFlags flags = Qt::Dialog;
            // 禁用系统菜单和标题栏按钮
            flags &= ~Qt::WindowSystemMenuHint;
            flags &= ~Qt::WindowCloseButtonHint;
            flags &= ~Qt::WindowMinimizeButtonHint;
            flags &= ~Qt::WindowMaximizeButtonHint;
            if (set_top->isChecked()) {
                flags |= Qt::WindowStaysOnTopHint;
            }
            websocket_dialog_window_->setWindowFlags(flags);
            websocket_dialog_window_->setAttribute(Qt::WA_TranslucentBackground, false);
            
            // 恢复位置和大小
            websocket_dialog_window_->move(oldPos);
            websocket_dialog_window_->resize(oldSize);
            
            if (wasVisible) {
                websocket_dialog_window_->show();
            }
            qDebug() << "WebSocketDialog: move mode ON - borders enabled, flags:" << websocket_dialog_window_->windowFlags();
        }
    } else if (a == this->move_off) {
        CF_LOG_DEBUG("move off");
        this->setWindowFlag(Qt::FramelessWindowHint, true);
        MouseEventHandle::EnableMousePassThrough(this->winId(), true);
        this->m_change->setEnabled(true);
        this->a_exit->setEnabled(true);
        this->m_dialog->setEnabled(true);
        // 显示主窗口菜单栏
        // this->menuBar()->show();
        auto &model = resource_loader::get_instance();
        // 移动关闭：对话框隐藏边框，变成无边框窗口
        if (websocket_dialog_window_) {
            bool wasVisible = websocket_dialog_window_->isVisible();
            QPoint oldPos = websocket_dialog_window_->pos();
            QSize oldSize = websocket_dialog_window_->size();
            
            // 设置新的窗口标志（无边框）
            Qt::WindowFlags flags = Qt::Dialog | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint;
            if (set_top->isChecked()) {
                flags |= Qt::WindowStaysOnTopHint;
            }
            websocket_dialog_window_->setWindowFlags(flags);
            websocket_dialog_window_->setAttribute(Qt::WA_TranslucentBackground, true);
            
            // 恢复位置和大小
            websocket_dialog_window_->move(oldPos);
            websocket_dialog_window_->resize(oldSize);
            
            if (wasVisible) {
                websocket_dialog_window_->show();
            }
            
            model.update_dialog_position(websocket_dialog_window_->x(), websocket_dialog_window_->y());
            // 注释掉自动保存对话框尺寸，允许用户在config.json中手动配置
            // model.update_dialog_size(websocket_dialog_window_->width(), websocket_dialog_window_->height());
            qDebug() << "WebSocketDialog: move mode OFF - borderless enabled, flags:" << websocket_dialog_window_->windowFlags();
        }
        model.update_current_model_position(this->x(), this->y());
        model.update_current_model_size(this->width(), this->height());
    }
    this->show();
}

void MainWindow::action_change(bool checked) {
    qDebug() << "action_change called with checked:" << checked;
    int counter = 0;
    for (auto &i: model_list) {
        if (i->isChecked()) {
            qDebug() << "Found checked model at index:" << counter << "name:" << i->text();
            break;
        }
        counter++;
    }
    qDebug() << "Selected model index:" << counter;

    // 检查是否选择了相同的模型，如果是则直接返回
    auto currentModel = resource_loader::get_instance().get_current_model();
    if (currentModel && counter < model_list.size() && model_list[counter]->text() == currentModel->name) {
        qDebug() << "Same model selected, skipping change";
        return;
    }

    if (resource_loader::get_instance().update_current_model(counter)) {
        bool load_fail = true;
        auto *m = resource_loader::get_instance().get_current_model();
        qDebug() << "Attempting to change scene to model:" << m->name;
        
        // 在切换模型前，先隐藏窗口以避免显示残留内容
        this->hide();
        
        // 强制刷新窗口以确保清理残留内容
        this->update();
        QApplication::processEvents();
        
        /// 将QString转化为char*
        if (LAppLive2DManager::GetInstance()->ChangeScene(m->name)) {
            this->resize(m->model_width, m->model_height);
            load_fail = false;
            qDebug() << "Model change successful, resizing to:" << m->model_width << "x" << m->model_height;
        } else {
            int _counter = 0;
            for (auto &item: resource_loader::get_instance().get_model_list()) {
                if (_counter != counter) {
                    if (LAppLive2DManager::GetInstance()->ChangeScene(item.name)) {
                        this->resize(item.model_width, item.model_height);
                        load_fail = false;
                        auto msgIcon = QSystemTrayIcon::MessageIcon(2);
                        this->m_systemTray->showMessage(QStringLiteral("waring"),
                                                        tr("load model fail,try load default model"), msgIcon, 5000);
                        this->model_list[_counter]->setChecked(true);
                        resource_loader::get_instance().update_current_model(_counter);
                        break;
                    }
                }
                _counter++;
            }
        }
        if (load_fail) {
            this->hide();
            this->resize(640, 480);
            int cxScreen, cyScreen;
            cxScreen = QApplication::primaryScreen()->availableGeometry().width();
            cyScreen = QApplication::primaryScreen()->availableGeometry().height();
            this->move(cxScreen / 2 - 320, cyScreen / 2 - 240);
            this->show();
            QMessageBox::critical(this, tr("CF"), QStringLiteral("资源文件错误,程序终止"));
            action_exit();
        }
        
        // 模型切换完成后重新显示窗口
        if (!load_fail) {
            // 添加短暂延迟以确保渲染状态完全清理
            QThread::msleep(50);
            this->show();
            qDebug() << "Window shown after model change";
        }
    }
    this->show();
}

void MainWindow::action_dialog(bool checked) {
    if (websocket_dialog_window_->isVisible()) {
        websocket_dialog_window_->hide();
    } else {
        websocket_dialog_window_->show();
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

void MainWindow::setDeskPetIntegration(DeskPetIntegration *integration) {
    if (websocket_dialog_window_) {
        websocket_dialog_window_->setDeskPetIntegration(integration);
    }
}

void MainWindow::showWebSocketChatDialog() {
    if (websocket_dialog_window_) {
        websocket_dialog_window_->show();
    }
}

void MainWindow::customEvent(QEvent *event)
{
    // 简单的实现
    Q_UNUSED(event)
}

