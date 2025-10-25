cmake_minimum_required(VERSION 3.10)
project(HeartMindRobot LANGUAGES CXX C)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
cmake_policy(SET CMP0079 NEW)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_UTILS OFF CACHE BOOL "" FORCE)
find_package(Qt6 COMPONENTS Widgets OpenGL REQUIRED)

find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
find_package(Qt6 COMPONENTS WebSockets REQUIRED) # 添加WebSocket支持
find_package(Qt6 COMPONENTS Multimedia REQUIRED) # 添加qt多媒体模块
find_package(Qt6 COMPONENTS Network REQUIRED) # 添加qt网络模块

# 使用本地编译好的PortAudio库
if(WIN32)
    set(PORTAUDIO_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third/portAudio/x64")
    set(PORTAUDIO_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/third/portAudio/x64/portaudio.lib")
    message(STATUS "Using local PortAudio library: ${PORTAUDIO_LIBRARY}")
    include_directories(${PORTAUDIO_INCLUDE_DIR})
else()
    # macOS/Linux平台：查找系统安装的PortAudio
    find_path(PORTAUDIO_INCLUDE_DIR portaudio.h 
        PATHS 
        /usr/local/include
        /usr/include
    )
    
    find_library(PORTAUDIO_LIBRARY portaudio 
        PATHS 
        /usr/local/lib
        /usr/lib
    )
    
    if(NOT PORTAUDIO_INCLUDE_DIR OR NOT PORTAUDIO_LIBRARY)
        message(WARNING "PortAudio not found! Please install:")
        if(APPLE)
            message(WARNING "  macOS: brew install portaudio")
        else()
            message(WARNING "  Linux: sudo apt-get install libportaudio2-dev")
        endif()
        set(PORTAUDIO_INCLUDE_DIR "")
        set(PORTAUDIO_LIBRARY "")
    else()
        message(STATUS "PortAudio found: ${PORTAUDIO_INCLUDE_DIR}")
        include_directories(${PORTAUDIO_INCLUDE_DIR})
    endif()
endif()
#find_library(APPLICATION_SERVICES_FRAMEWORK ApplicationServices)
#
#if (NOT APPLICATION_SERVICES_FRAMEWORK)
#    message(FATAL_ERROR "ApplicationServices framework not found!")
#endif()

add_library(Live2DCubismCore SHARED IMPORTED)
if(WIN32)
    set_target_properties(Live2DCubismCore
            PROPERTIES
            IMPORTED_LOCATION
            ${CMAKE_CURRENT_SOURCE_DIR}/third/live2d/dll/windows/x86_64/Live2DCubismCore.dll
            IMPORTED_IMPLIB
            ${CMAKE_CURRENT_SOURCE_DIR}/third/live2d/dll/windows/x86_64/Live2DCubismCore.lib
            INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/third/live2d/inc
    )
elseif(APPLE)
    set_target_properties(Live2DCubismCore
            PROPERTIES
            IMPORTED_LOCATION
            ${CMAKE_CURRENT_SOURCE_DIR}/third/live2d/dll/macos/libLive2DCubismCore.dylib
            INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/third/live2d/inc
    )
endif()

set(GLEW_PATH ${CMAKE_CURRENT_SOURCE_DIR}/third/glew)
set(GLFW_PATH  ${CMAKE_CURRENT_SOURCE_DIR}/third/glfw)
add_subdirectory(${GLEW_PATH}/build/cmake)
add_subdirectory(${GLFW_PATH})
set(FRAMEWORK_SOURCE OpenGL)
add_subdirectory(Framework)
target_compile_definitions(Framework PUBLIC CSM_TARGET_MAC_GL)
target_include_directories(Framework PUBLIC ${GLEW_PATH}/include)
target_link_libraries(Framework Live2DCubismCore glew_s)
find_package(OpenGL REQUIRED)
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DCF_DEBUG") # debug模式下定义CF_DEBUG, 用来控制日志输出
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DGLEW_STATIC") # 定义GLEW_STATIC宏
include_directories(inc)
include_directories(src)
include_directories(${GLEW_PATH}/include)
include_directories(Framework/src)
include_directories(src/glwidget.h)
include_directories(third/stb)
if(WIN32)
    include_directories(third/libopus_v1.4_msvc17/include)
else()
    include_directories(/usr/local/include/opus)
endif()
# 基础源文件（所有平台通用）
set(COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LAppAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LAppDefine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LAppDelegate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LAppLive2DManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LAppModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LAppPal.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LAppSprite.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LAppTextureManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LAppView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TouchManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/glwidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/event_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/message_queue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resource_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Networkutil.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ChatDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SimpleActivationWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DeviceFingerprint.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SystemInitializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ConfigManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WebSocketManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DeskPetStateManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DeskPetController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DeskPetIntegration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DeskPetWebSocketExample.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/src/WebSocketChatDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OpusDecoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OpusEncoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/OpusEncoder.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OpusDecoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PortAudioEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LAppWavFileHandler_stub.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WebRTCAudioProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioInputManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/AudioInputManager.hpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/inc/GlobalHotkey.h
)

# macOS特定源文件（需要Objective-C++）
set(MACOS_SPECIFIC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioPermission.mm
    # ${CMAKE_CURRENT_SOURCE_DIR}/src/GlobalHotkey.mm
)

# 平台特定的源文件
if(WIN32)
    # Windows 平台使用 .cpp 文件
    set(PLATFORM_SOURCES ${COMMON_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioUtil.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MouseEvent.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/mainwindow.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioPermission.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app.rc
    )
elseif(APPLE)
    # macOS 平台使用原有的 .mm 文件
    set(PLATFORM_SOURCES ${COMMON_SOURCES}
        ${MACOS_SPECIFIC_SOURCES}
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/LAppWavFileHandler.mm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.mm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/mainwindow.mm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioUtil.mm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MouseEvent.mm
    )
endif()

if(APPLE)
    # 设置应用图标
    set(MACOSX_BUNDLE_ICON_FILE AppIcon.icns)
    set(APP_ICON_MACOS ${CMAKE_CURRENT_SOURCE_DIR}/AppIcon.icns)
    set_source_files_properties(${APP_ICON_MACOS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    
    add_executable(HeartMindRobot MACOSX_BUNDLE ${PLATFORM_SOURCES} ${APP_ICON_MACOS})
    set_target_properties(HeartMindRobot PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
        MACOSX_BUNDLE_ICON_FILE AppIcon.icns
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    # 复制Resources目录到应用包（用于开发测试）
    add_custom_command(TARGET HeartMindRobot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory 
            $<TARGET_FILE_DIR:HeartMindRobot>/../Resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/Resources
            $<TARGET_FILE_DIR:HeartMindRobot>/../Resources
        COMMENT "Copying Resources directory to app bundle for development"
    )
else()
    add_executable(HeartMindRobot ${PLATFORM_SOURCES})
    set_target_properties(HeartMindRobot PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    
    # Windows: 设置为窗口应用程序，隐藏控制台窗口
    if(WIN32)
        set_target_properties(HeartMindRobot PROPERTIES
            WIN32_EXECUTABLE TRUE
        )
        # 设置链接器选项以隐藏控制台窗口
        set_target_properties(HeartMindRobot PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:WINDOWS"
        )
        # 启用资源编译器
        enable_language(RC)
    endif()
    
    # Windows: 复制必要的 DLL 文件到输出目录
    if(WIN32)
        # 复制 Live2D DLL
        add_custom_command(TARGET HeartMindRobot POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/third/live2d/dll/windows/x86_64/Live2DCubismCore.dll
                $<TARGET_FILE_DIR:HeartMindRobot>
            COMMENT "Copying Live2DCubismCore.dll to output directory"
        )
        
        # 复制 Qt6 DLL 文件
        set(QT6_DLL_PATH "D:/Qt/6.9.3/msvc2022_64/bin")
        add_custom_command(TARGET HeartMindRobot POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QT6_DLL_PATH}/Qt6Core.dll
                $<TARGET_FILE_DIR:HeartMindRobot>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QT6_DLL_PATH}/Qt6Gui.dll
                $<TARGET_FILE_DIR:HeartMindRobot>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QT6_DLL_PATH}/Qt6Widgets.dll
                $<TARGET_FILE_DIR:HeartMindRobot>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QT6_DLL_PATH}/Qt6OpenGL.dll
                $<TARGET_FILE_DIR:HeartMindRobot>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QT6_DLL_PATH}/Qt6OpenGLWidgets.dll
                $<TARGET_FILE_DIR:HeartMindRobot>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QT6_DLL_PATH}/Qt6Network.dll
                $<TARGET_FILE_DIR:HeartMindRobot>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QT6_DLL_PATH}/Qt6WebSockets.dll
                $<TARGET_FILE_DIR:HeartMindRobot>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QT6_DLL_PATH}/Qt6Multimedia.dll
                $<TARGET_FILE_DIR:HeartMindRobot>
            COMMENT "Copying Qt6 DLL files to output directory"
        )
        
        # 复制 Qt6 平台插件
        add_custom_command(TARGET HeartMindRobot POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:HeartMindRobot>/plugins/platforms
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QT6_DLL_PATH}/../plugins/platforms/qwindows.dll
                $<TARGET_FILE_DIR:HeartMindRobot>/plugins/platforms/
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QT6_DLL_PATH}/../plugins/platforms/qminimal.dll
                $<TARGET_FILE_DIR:HeartMindRobot>/plugins/platforms/
            COMMENT "Copying Qt6 platform plugins to output directory"
        )
        
        # 复制 Qt6 TLS 插件和 OpenSSL 库
        add_custom_command(TARGET HeartMindRobot POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:HeartMindRobot>/plugins/tls
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QT6_DLL_PATH}/../plugins/tls/qopensslbackend.dll
                $<TARGET_FILE_DIR:HeartMindRobot>/plugins/tls/
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "C:/Program Files/Adobe/Acrobat DC/Acrobat/plug_ins/libssl-3-x64.dll"
                $<TARGET_FILE_DIR:HeartMindRobot>/
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "C:/Program Files/Adobe/Acrobat DC/Acrobat/plug_ins/libcrypto-3-x64.dll"
                $<TARGET_FILE_DIR:HeartMindRobot>/
            COMMENT "Copying Qt6 TLS plugins and OpenSSL libraries to output directory"
        )
    endif()
endif()

add_subdirectory(src)

if(WIN32)
    # Windows平台：添加opus库路径
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/third/libopus_v1.4_msvc17/lib/x64)
    target_link_libraries(HeartMindRobot PRIVATE glew_s OpenGL::GL Qt6::Widgets Qt6::OpenGL Framework Qt6::OpenGLWidgets glfw Qt6::Network Qt6::WebSockets Qt6::Multimedia winmm dsound ${CMAKE_CURRENT_SOURCE_DIR}/third/libopus_v1.4_msvc17/lib/x64/libopus.lib ${PORTAUDIO_LIBRARY})
elseif(APPLE)
    target_link_libraries(HeartMindRobot PRIVATE Qt6::Widgets Qt6::OpenGL glew_s Framework Qt6::OpenGLWidgets "-framework Cocoa -framework AVFoundation -framework CoreAudio -framework AudioToolbox -framework Carbon" glfw Qt6::Network Qt6::WebSockets Qt6::Multimedia opus ${PORTAUDIO_LIBRARY})
endif()
#执行alias alias otool='/usr/bin/otool'
#alias install_name_tool='/usr/bin/install_name_tool'
#sudo macdeployqt /path/to/HeartMindRobot.app 千万不要修改bash_profile中的路径（不明原因无法执行与anaconda3有关）

