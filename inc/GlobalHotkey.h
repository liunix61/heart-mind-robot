//
// 全局热键管理器 - macOS实现
//

#ifndef GLOBALHOTKEY_H
#define GLOBALHOTKEY_H

#include <QObject>
#include <Carbon/Carbon.h>

class GlobalHotkey : public QObject
{
    Q_OBJECT

public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey();

    // 注册全局热键 (Cmd+Shift+V)
    bool registerHotkey();
    
    // 注销全局热键
    void unregisterHotkey();

signals:
    // 热键被按下
    void hotkeyPressed();
    
    // 热键被释放
    void hotkeyReleased();

private:
    EventHotKeyRef m_hotkeyRef;
    EventHandlerRef m_handlerRef;
    bool m_registered;
    
    // macOS热键回调
    static OSStatus hotkeyHandler(EventHandlerCallRef nextHandler,
                                   EventRef event,
                                   void *userData);
};

#endif // GLOBALHOTKEY_H

