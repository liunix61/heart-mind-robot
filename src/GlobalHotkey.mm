//
// 全局热键管理器实现 - macOS
//

#include "GlobalHotkey.hpp"
#include <QDebug>

// 热键ID
static const UInt32 kHotkeyID = 1;

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
    , m_hotkeyRef(nullptr)
    , m_handlerRef(nullptr)
    , m_registered(false)
{
}

GlobalHotkey::~GlobalHotkey()
{
    unregisterHotkey();
}

bool GlobalHotkey::registerHotkey()
{
    if (m_registered) {
        qDebug() << "Hotkey already registered";
        return true;
    }

    // 设置热键：Cmd+Shift+V
    EventHotKeyID hotkeyID;
    hotkeyID.signature = 'htk1';
    hotkeyID.id = kHotkeyID;

    // 注册热键事件类型
    EventTypeSpec eventTypes[2];
    eventTypes[0].eventClass = kEventClassKeyboard;
    eventTypes[0].eventKind = kEventHotKeyPressed;
    eventTypes[1].eventClass = kEventClassKeyboard;
    eventTypes[1].eventKind = kEventHotKeyReleased;

    // 安装事件处理器
    OSStatus status = InstallEventHandler(
        GetApplicationEventTarget(),
        &GlobalHotkey::hotkeyHandler,
        2,
        eventTypes,
        this,
        &m_handlerRef
    );

    if (status != noErr) {
        qWarning() << "Failed to install event handler:" << status;
        return false;
    }

    // 注册热键：Cmd(cmdKey) + Shift(shiftKey) + V
    status = RegisterEventHotKey(
        kVK_ANSI_V,                          // V键
        cmdKey | shiftKey,                   // Cmd+Shift修饰键
        hotkeyID,
        GetApplicationEventTarget(),
        0,
        &m_hotkeyRef
    );

    if (status != noErr) {
        qWarning() << "Failed to register hotkey:" << status;
        RemoveEventHandler(m_handlerRef);
        m_handlerRef = nullptr;
        return false;
    }

    m_registered = true;
    qDebug() << "Global hotkey registered: Cmd+Shift+V";
    return true;
}

void GlobalHotkey::unregisterHotkey()
{
    if (!m_registered) {
        return;
    }

    if (m_hotkeyRef) {
        UnregisterEventHotKey(m_hotkeyRef);
        m_hotkeyRef = nullptr;
    }

    if (m_handlerRef) {
        RemoveEventHandler(m_handlerRef);
        m_handlerRef = nullptr;
    }

    m_registered = false;
    qDebug() << "Global hotkey unregistered";
}

OSStatus GlobalHotkey::hotkeyHandler(EventHandlerCallRef nextHandler,
                                      EventRef event,
                                      void *userData)
{
    GlobalHotkey *hotkey = static_cast<GlobalHotkey*>(userData);
    
    EventHotKeyID hotkeyID;
    GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID,
                      nullptr, sizeof(hotkeyID), nullptr, &hotkeyID);

    UInt32 eventKind = GetEventKind(event);
    
    if (hotkeyID.id == kHotkeyID) {
        if (eventKind == kEventHotKeyPressed) {
            qDebug() << "Global hotkey pressed: Cmd+Shift+V";
            emit hotkey->hotkeyPressed();
        } else if (eventKind == kEventHotKeyReleased) {
            qDebug() << "Global hotkey released: Cmd+Shift+V";
            emit hotkey->hotkeyReleased();
        }
    }

    return noErr;
}

