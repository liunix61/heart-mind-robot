@echo off
setlocal enabledelayedexpansion

rem 跨平台构建脚本 (Windows版本)
rem 使用方法: build.bat [platform] [arch] [cross-compile]
rem platform: windows, macos, linux (默认: windows)
rem arch: x86_64, arm64, x86, arm (默认: x86_64)
rem cross-compile: true, false (默认: false)

rem 显示帮助信息
if "%1"=="-h" goto :show_help
if "%1"=="--help" goto :show_help

set PLATFORM=%1
if "%PLATFORM%"=="" set PLATFORM=windows

set ARCH=%2
if "%ARCH%"=="" set ARCH=x86_64

set CROSS_COMPILE=%3
if "%CROSS_COMPILE%"=="" set CROSS_COMPILE=false

set BUILD_DIR=build

echo ==========================================
echo 跨平台构建脚本 (Windows版本)
echo ==========================================
echo 目标平台: %PLATFORM%
echo 目标架构: %ARCH%
echo 交叉编译: %CROSS_COMPILE%
echo 构建目录: %BUILD_DIR%
echo ==========================================

rem 清理旧的构建目录
if exist "%BUILD_DIR%" (
    echo 清理旧的构建目录...
    rmdir /s /q "%BUILD_DIR%"
)

rem 创建构建目录
mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

rem 配置CMake
if "%PLATFORM%"=="windows" (
    echo 配置 Windows 构建...
    if "%CROSS_COMPILE%"=="true" (
        echo 错误: Windows不支持交叉编译
        echo 请使用原生Windows环境构建
        exit /b 1
    )
    cmake -G "Visual Studio 17 2022" -A x64 ..
) else if "%PLATFORM%"=="macos" (
    echo 配置 macOS 构建...
    if "%CROSS_COMPILE%"=="true" (
        echo 错误: 在Windows上无法交叉编译macOS
        echo 请使用macOS环境构建
        exit /b 1
    )
    cmake ..
) else if "%PLATFORM%"=="linux" (
    echo 配置 Linux 构建...
    if "%CROSS_COMPILE%"=="true" (
        echo 设置Linux交叉编译环境...
        set CC=x86_64-linux-gnu-gcc
        set CXX=x86_64-linux-gnu-g++
        set AR=x86_64-linux-gnu-ar
        set STRIP=x86_64-linux-gnu-strip
        
        rem 检查工具链
        where %CC% >nul 2>&1
        if errorlevel 1 (
            echo 错误: 未找到交叉编译工具链
            echo 请安装相应的交叉编译工具链
            exit /b 1
        )
        
        cmake -DCMAKE_SYSTEM_NAME=Linux ^
              -DCMAKE_C_COMPILER=%CC% ^
              -DCMAKE_CXX_COMPILER=%CXX% ^
              -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER ^
              -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY ^
              -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY ^
              ..
    ) else (
        cmake ..
    )
) else (
    echo 不支持的平台: %PLATFORM%
    echo 支持的平台: windows, macos, linux
    exit /b 1
)

rem 编译项目
echo 开始编译...
cmake --build . --config Release

if %errorlevel% equ 0 (
    echo ==========================================
    echo 构建成功!
    echo 可执行文件位置: %BUILD_DIR%\bin\Release\ChatFriend.exe
    echo ==========================================
) else (
    echo 构建失败!
    exit /b 1
)

goto :end

:show_help
echo 跨平台构建脚本 (Windows版本)
echo.
echo 使用方法:
echo   build.bat [platform] [arch] [cross-compile]
echo.
echo 参数:
echo   platform     目标平台 (windows, macos, linux) [默认: windows]
echo   arch         目标架构 (x86_64, arm64, x86, arm) [默认: x86_64]
echo   cross-compile 是否交叉编译 (true, false) [默认: false]
echo.
echo 示例:
echo   build.bat windows                    # 构建Windows版本
echo   build.bat windows x86_64            # 构建Windows x86_64版本
echo   build.bat linux x86_64 true         # 交叉编译Linux x86_64版本
echo.
echo 交叉编译要求:
echo   Linux: 需要安装交叉编译工具链
echo.

:end