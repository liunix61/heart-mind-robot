@echo off
echo ==========================================
echo Visual C++ Redistributable 安装脚本
echo ==========================================
echo.

echo 检查是否已安装 Visual C++ 2015-2022 Redistributable...
reg query "HKLM\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" /v Version >nul 2>&1
if %errorlevel% equ 0 (
    echo Visual C++ Redistributable 已安装。
    goto :check_ucrt
) else (
    echo Visual C++ Redistributable 未安装，正在下载...
)

echo.
echo 正在下载 Visual C++ 2015-2022 Redistributable...
powershell -Command "& {Invoke-WebRequest -Uri 'https://aka.ms/vs/17/release/vc_redist.x64.exe' -OutFile 'vc_redist.x64.exe'}"
if %errorlevel% neq 0 (
    echo 下载失败！请手动下载并安装：
    echo https://aka.ms/vs/17/release/vc_redist.x64.exe
    pause
    exit /b 1
)

echo.
echo 正在安装 Visual C++ Redistributable...
vc_redist.x64.exe /quiet /norestart
if %errorlevel% neq 0 (
    echo 安装失败！请以管理员身份运行此脚本。
    pause
    exit /b 1
)

echo Visual C++ Redistributable 安装完成！

:check_ucrt
echo.
echo 检查 Universal C Runtime...
reg query "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{36F68A90-239C-34DF-B58C-64B30153CE35}" >nul 2>&1
if %errorlevel% equ 0 (
    echo Universal C Runtime 已安装。
) else (
    echo Universal C Runtime 未安装，正在下载...
    powershell -Command "& {Invoke-WebRequest -Uri 'https://download.microsoft.com/download/6/A/A/6AA4EDFF-645B-48C5-81CC-ED5963AEAD48/vc_redist.x64.exe' -OutFile 'ucrt_redist.x64.exe'}"
    if %errorlevel% equ 0 (
        ucrt_redist.x64.exe /quiet /norestart
        echo Universal C Runtime 安装完成！
    ) else (
        echo Universal C Runtime 下载失败，请手动安装。
    )
)

echo.
echo 清理临时文件...
if exist vc_redist.x64.exe del vc_redist.x64.exe
if exist ucrt_redist.x64.exe del ucrt_redist.x64.exe

echo.
echo ==========================================
echo 所有依赖项检查完成！
echo 现在可以运行 HeartMindRobot.exe
echo ==========================================
pause
