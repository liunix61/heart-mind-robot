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
#include <QAbstractNativeEventFilter>
#endif

class GlobalHotkey : public QObject
#ifdef _WIN32
    , public QAbstractNativeEventFilter
#endif
{
    Q_OBJECT

public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey();

    // 注册全局热键 (Cmd+Shift+V)
    bool registerHotkey();
    
    // 注销全局热键
    void unregisterHotkey();

#ifdef _WIN32
    // Windows原生事件过滤器
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

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
#endif
    bool m_registered;
};

#endif // GLOBALHOTKEY_H

