#ifndef MOUSEEVENT_H
#define MOUSEEVENT_H
#include "GL/glew.h"
#include <QtGui/QWindow>
#include "PlatformConfig.hpp"

class MouseEventHandle : public QObject {
public:
    explicit MouseEventHandle(QObject *parent = nullptr);
    static void EnableMousePassThrough(WId windowId, bool enable);
    void startMonitoring();
    bool stopMonitoring();

private:
    bool is_monitoring{false};
    
    // 平台特定的成员变量
    MACOS_SPECIFIC(
        CFMachPortRef eventTap;
        CFRunLoopRef ref;
    )
    
    WINDOWS_SPECIFIC(
        HHOOK mouseHook;
    )

    // 平台特定的回调函数
    MACOS_SPECIFIC(
        static CGEventRef mouseEventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon);
        CGEventRef handleMouseEvent(CGEventType type, CGEventRef event);
    )
    
    WINDOWS_SPECIFIC(
        static LRESULT CALLBACK mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
        LRESULT handleMouseEvent(WPARAM wParam, LPARAM lParam);
    )
};

#endif //MOUSEEVENT_H