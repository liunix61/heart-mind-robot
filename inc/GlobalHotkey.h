//
// 全局热键管理器 - 跨平台实现
//

#ifndef GLOBALHOTKEY_H
#define GLOBALHOTKEY_H

#include <QObject>

#ifdef __APPLE__
#include <Carbon/Carbon.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

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
#ifdef __APPLE__
    EventHotKeyRef m_hotkeyRef;
    EventHandlerRef m_handlerRef;
    // macOS热键回调
    static OSStatus hotkeyHandler(EventHandlerCallRef nextHandler,
                                   EventRef event,
                                   void *userData);
#elif defined(_WIN32)
    int m_hotkeyId;
    // Windows热键回调
    static LRESULT CALLBACK hotkeyHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
    bool m_registered;
};

#endif // GLOBALHOTKEY_H

