//
// 全局热键管理器实现 - Windows
//

#include "GlobalHotkey.h"
#include <QDebug>
#include <QApplication>
#include <QAbstractNativeEventFilter>

// Windows热键ID
static const int kHotkeyID = 1;

// 全局实例指针，用于静态回调函数访问
static GlobalHotkey* g_globalHotkeyInstance = nullptr;

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
    , m_hotkeyId(kHotkeyID)
    , m_registered(false)
{
    qDebug() << "GlobalHotkey constructor called";
    
    // 设置全局实例指针
    g_globalHotkeyInstance = this;
    
    // 安装原生事件过滤器
    QApplication::instance()->installNativeEventFilter(this);
    qDebug() << "Native event filter installed";
}

GlobalHotkey::~GlobalHotkey()
{
    unregisterHotkey();
    QApplication::instance()->removeNativeEventFilter(this);
    g_globalHotkeyInstance = nullptr;
}

bool GlobalHotkey::registerHotkey()
{
    if (m_registered) {
        qDebug() << "Hotkey already registered";
        return true;
    }

    qDebug() << "Attempting to register global hotkey: Ctrl+Shift+V";

    // 注册全局热键：Ctrl+Shift+V
    // VK_V = 0x56, MOD_CONTROL = 0x0002, MOD_SHIFT = 0x0004
    bool success = RegisterHotKey(
        nullptr,                    // 窗口句柄（nullptr表示全局热键）
        m_hotkeyId,                 // 热键ID
        MOD_CONTROL | MOD_SHIFT,    // Ctrl+Shift修饰键
        0x56                        // V键 (VK_V)
    );

    if (!success) {
        DWORD error = GetLastError();
        qWarning() << "Failed to register hotkey. Error code:" << error;
        
        // 提供更详细的错误信息
        switch (error) {
            case ERROR_HOTKEY_ALREADY_REGISTERED:
                qWarning() << "Hotkey already registered by another application";
                break;
            case ERROR_ACCESS_DENIED:
                qWarning() << "Access denied - may need administrator privileges";
                break;
            default:
                qWarning() << "Unknown error occurred during hotkey registration";
                break;
        }
        return false;
    }

    m_registered = true;
    qDebug() << "Global hotkey registered successfully: Ctrl+Shift+V";
    qDebug() << "Hotkey ID:" << m_hotkeyId;
    return true;
}

void GlobalHotkey::unregisterHotkey()
{
    if (!m_registered) {
        return;
    }

    UnregisterHotKey(nullptr, m_hotkeyId);
    m_registered = false;
    qDebug() << "Global hotkey unregistered";
}

bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType)
    Q_UNUSED(result)
    
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        
        // 调试：记录所有消息类型
        if (msg->message == WM_HOTKEY) {
            qDebug() << "WM_HOTKEY message received. wParam:" << msg->wParam << "lParam:" << msg->lParam;
            
            if (msg->wParam == kHotkeyID) {
                qDebug() << "Global hotkey pressed: Ctrl+Shift+V";
                qDebug() << "Emitting hotkeyPressed signal...";
                
                // 发送信号
                emit hotkeyPressed();
                qDebug() << "hotkeyPressed signal emitted";
                return true; // 表示事件已处理
            } else {
                qDebug() << "Hotkey message received but wParam doesn't match our ID. Expected:" << kHotkeyID << "Got:" << msg->wParam;
            }
        }
    }
    
    return false; // 让其他事件继续处理
}
