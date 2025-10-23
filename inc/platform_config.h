#ifndef PLATFORM_CONFIG_H
#define PLATFORM_CONFIG_H

// 平台检测
#ifdef _WIN32
    #define PLATFORM_WINDOWS 1
    #define PLATFORM_MACOS 0
#elif defined(__APPLE__)
    #define PLATFORM_WINDOWS 0
    #define PLATFORM_MACOS 1
#else
    #define PLATFORM_WINDOWS 0
    #define PLATFORM_MACOS 0
#endif

// 条件编译宏
#if PLATFORM_WINDOWS
    #define PLATFORM_SPECIFIC(code) code
    #define MACOS_SPECIFIC(code)
    #define WINDOWS_SPECIFIC(code) code
#elif PLATFORM_MACOS
    #define PLATFORM_SPECIFIC(code) code
    #define MACOS_SPECIFIC(code) code
    #define WINDOWS_SPECIFIC(code)
#else
    #define PLATFORM_SPECIFIC(code)
    #define MACOS_SPECIFIC(code)
    #define WINDOWS_SPECIFIC(code)
#endif

// 平台特定的包含文件
#if PLATFORM_WINDOWS
    #include <windows.h>
    #include <dinput.h>
#elif PLATFORM_MACOS
    // 只在 .mm 文件中包含 Objective-C 头文件
    #ifdef __OBJC__
        #import <Cocoa/Cocoa.h>
        #include <ApplicationServices/ApplicationServices.h>
    #endif
#endif

#endif // PLATFORM_CONFIG_H
