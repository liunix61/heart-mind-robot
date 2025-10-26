#!/bin/bash

# ChatFriend macOS DMG 打包脚本
# 用于创建可分发的 DMG 安装包

set -e  # 遇到错误立即退出

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

echo_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

echo_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 项目配置
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_NAME="HeartMindRobot"
APP_DISPLAY_NAME="Heart Mind Robot"
APP_VERSION="1.0.0"
BUILD_DIR="$PROJECT_ROOT/build"
APP_BUNDLE="$BUILD_DIR/bin/${APP_NAME}.app"
DIST_DIR="$PROJECT_ROOT/dist"
DMG_NAME="${APP_DISPLAY_NAME// /-}-${APP_VERSION}.dmg"
VOLUME_NAME="${APP_DISPLAY_NAME}"

echo_info "========================================="
echo_info "  ${APP_DISPLAY_NAME} DMG 打包工具"
echo_info "========================================="
echo_info "项目路径: $PROJECT_ROOT"
echo_info "应用名称: $APP_NAME"
echo_info "版本号: $APP_VERSION"
echo_info ""

# 检查 App Bundle 是否存在
if [ ! -d "$APP_BUNDLE" ]; then
    echo_error "找不到应用程序: $APP_BUNDLE"
    echo_error "请先运行 build.sh 编译项目"
    exit 1
fi

# 检查 macdeployqt 是否存在
if ! command -v macdeployqt &> /dev/null; then
    echo_error "找不到 macdeployqt 工具"
    echo_error "请确保 Qt 已正确安装"
    exit 1
fi

# 创建 dist 目录
echo_info "创建 dist 目录..."
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

# 复制 App Bundle 到 dist 目录
echo_info "复制应用程序到 dist 目录..."
cp -R "$APP_BUNDLE" "$DIST_DIR/"

# 复制资源文件
echo_info "复制资源文件..."
DIST_RESOURCES="$DIST_DIR/${APP_NAME}.app/Contents/Resources"

# 复制 models 目录（Live2D模型和AI模型）
if [ -d "$PROJECT_ROOT/models" ]; then
    echo_info "  -> 复制 models 目录"
    mkdir -p "$DIST_DIR/${APP_NAME}.app/Contents/models"
    cp -R "$PROJECT_ROOT/models/"* "$DIST_DIR/${APP_NAME}.app/Contents/models/" 2>/dev/null || true
fi

# 复制 config 目录（配置文件）
if [ -d "$PROJECT_ROOT/config" ]; then
    echo_info "  -> 复制 config 目录"
    mkdir -p "$DIST_DIR/${APP_NAME}.app/Contents/config"
    cp -R "$PROJECT_ROOT/config/"* "$DIST_DIR/${APP_NAME}.app/Contents/config/" 2>/dev/null || true
fi


# 复制第三方库
echo_info "复制第三方动态库..."
DIST_FRAMEWORKS="$DIST_DIR/${APP_NAME}.app/Contents/Frameworks"
mkdir -p "$DIST_FRAMEWORKS"

# 复制 Live2D 库
if [ -f "$PROJECT_ROOT/third/live2d/dll/macos/libLive2DCubismCore.dylib" ]; then
    echo_info "  -> 复制 Live2D 库"
    cp "$PROJECT_ROOT/third/live2d/dll/macos/libLive2DCubismCore.dylib" "$DIST_FRAMEWORKS/"
fi

# 复制 libopus
if [ -f "/usr/local/lib/libopus.dylib" ]; then
    echo_info "  -> 复制 libopus"
    cp "/usr/local/lib/libopus.0.dylib" "$DIST_FRAMEWORKS/" 2>/dev/null || \
    cp "/usr/local/lib/libopus.dylib" "$DIST_FRAMEWORKS/"
    chmod +w "$DIST_FRAMEWORKS"/libopus*.dylib 2>/dev/null || true
fi

# 复制 libportaudio
if [ -f "/usr/local/lib/libportaudio.dylib" ]; then
    echo_info "  -> 复制 libportaudio"
    cp "/usr/local/lib/libportaudio.2.dylib" "$DIST_FRAMEWORKS/" 2>/dev/null || \
    cp "/usr/local/lib/libportaudio.dylib" "$DIST_FRAMEWORKS/"
    chmod +w "$DIST_FRAMEWORKS"/libportaudio*.dylib 2>/dev/null || true
fi

# 复制 WebRTC Audio Processing 库
echo_info "检测系统架构并复制 WebRTC 库..."
ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ]; then
    WEBRTC_LIB="$PROJECT_ROOT/third/webrtc_apm/macos/arm64/libwebrtc_apm.dylib"
else
    WEBRTC_LIB="$PROJECT_ROOT/third/webrtc_apm/macos/x64/libwebrtc_apm.dylib"
fi

if [ -f "$WEBRTC_LIB" ]; then
    echo_info "  -> 复制 libwebrtc_apm.dylib (架构: $ARCH)"
    cp "$WEBRTC_LIB" "$DIST_FRAMEWORKS/libwebrtc_apm.dylib"
    chmod +w "$DIST_FRAMEWORKS/libwebrtc_apm.dylib" 2>/dev/null || true
else
    echo_warn "  -> 未找到 WebRTC 库: $WEBRTC_LIB"
fi

# 使用 macdeployqt 处理 Qt 依赖
echo_info "使用 macdeployqt 处理 Qt 依赖..."
macdeployqt "$DIST_DIR/${APP_NAME}.app" -verbose=1

# 手动复制可能缺失的 Qt 框架
echo_info "检查并添加缺失的 Qt 框架..."
MISSING_FRAMEWORKS=(
    "QtDBus"
    "QtPrintSupport"
)

for framework in "${MISSING_FRAMEWORKS[@]}"; do
    if [ -d "/usr/local/lib/${framework}.framework" ]; then
        if [ ! -d "$DIST_FRAMEWORKS/${framework}.framework" ] || [ -L "$DIST_FRAMEWORKS/${framework}.framework" ]; then
            echo_info "  -> 添加 ${framework}.framework"
            # 删除可能存在的符号链接
            rm -rf "$DIST_FRAMEWORKS/${framework}.framework"
            # 使用 ditto 复制实际文件（会解引用符号链接）
            ditto "/usr/local/lib/${framework}.framework" "$DIST_FRAMEWORKS/${framework}.framework"
            # 修复权限
            chmod -R +w "$DIST_FRAMEWORKS/${framework}.framework" 2>/dev/null || true
            
            # 修复框架的 ID
            if [ -f "$DIST_FRAMEWORKS/${framework}.framework/Versions/A/${framework}" ]; then
                echo_info "  -> 修复 ${framework} ID"
                install_name_tool -id "@executable_path/../Frameworks/${framework}.framework/Versions/A/${framework}" \
                    "$DIST_FRAMEWORKS/${framework}.framework/Versions/A/${framework}" 2>/dev/null || true
            fi
            
            # 修复框架内部的相互依赖路径
            echo_info "  -> 修复 ${framework} 内部依赖"
            for fw in QtDBus QtPrintSupport QtCore QtGui QtWidgets; do
                if [ -f "$DIST_FRAMEWORKS/${framework}.framework/Versions/A/${framework}" ]; then
                    install_name_tool -change "/usr/local/opt/qtbase/lib/${fw}.framework/Versions/A/${fw}" \
                        "@executable_path/../Frameworks/${fw}.framework/Versions/A/${fw}" \
                        "$DIST_FRAMEWORKS/${framework}.framework/Versions/A/${framework}" 2>/dev/null || true
                fi
            done
        fi
    fi
done

# 复制 QtDBus 依赖的 libdbus
echo_info "检查并添加 DBus 库..."
if [ -f "/usr/local/opt/dbus/lib/libdbus-1.3.dylib" ]; then
    echo_info "  -> 添加 libdbus-1.3.dylib"
    ditto "/usr/local/opt/dbus/lib/libdbus-1.3.dylib" "$DIST_FRAMEWORKS/libdbus-1.3.dylib"
    chmod +w "$DIST_FRAMEWORKS/libdbus-1.3.dylib" 2>/dev/null || true
    
    # 修复 libdbus 自己的 ID
    echo_info "  -> 修复 libdbus ID"
    install_name_tool -id "@executable_path/../Frameworks/libdbus-1.3.dylib" \
        "$DIST_FRAMEWORKS/libdbus-1.3.dylib" 2>/dev/null || true
    
    # 修复 QtDBus 中的依赖路径
    if [ -f "$DIST_FRAMEWORKS/QtDBus.framework/Versions/A/QtDBus" ]; then
        echo_info "  -> 修复 QtDBus 中的 libdbus 路径"
        install_name_tool -change "/usr/local/opt/dbus/lib/libdbus-1.3.dylib" \
            "@executable_path/../Frameworks/libdbus-1.3.dylib" \
            "$DIST_FRAMEWORKS/QtDBus.framework/Versions/A/QtDBus" 2>/dev/null || true
    fi
fi

# 修复动态库路径
echo_info "修复动态库路径..."
EXECUTABLE="$DIST_DIR/${APP_NAME}.app/Contents/MacOS/${APP_NAME}"

# 修复 libopus 路径
if [ -f "$DIST_FRAMEWORKS/libopus.dylib" ]; then
    echo_info "  -> 修复 libopus 路径"
    install_name_tool -change "/usr/local/lib/libopus.dylib" "@executable_path/../Frameworks/libopus.dylib" "$EXECUTABLE" 2>/dev/null || true
    install_name_tool -change "/usr/local/lib/libopus.0.dylib" "@executable_path/../Frameworks/libopus.0.dylib" "$EXECUTABLE" 2>/dev/null || true
    install_name_tool -id "@executable_path/../Frameworks/libopus.dylib" "$DIST_FRAMEWORKS/libopus.dylib" 2>/dev/null || true
fi

# 修复 libportaudio 路径
if [ -f "$DIST_FRAMEWORKS/libportaudio.dylib" ]; then
    echo_info "  -> 修复 libportaudio 路径"
    install_name_tool -change "/usr/local/lib/libportaudio.dylib" "@executable_path/../Frameworks/libportaudio.dylib" "$EXECUTABLE" 2>/dev/null || true
    install_name_tool -change "/usr/local/lib/libportaudio.2.dylib" "@executable_path/../Frameworks/libportaudio.2.dylib" "$EXECUTABLE" 2>/dev/null || true
    install_name_tool -id "@executable_path/../Frameworks/libportaudio.dylib" "$DIST_FRAMEWORKS/libportaudio.dylib" 2>/dev/null || true
fi

# 修复 Live2D 路径
if [ -f "$DIST_FRAMEWORKS/libLive2DCubismCore.dylib" ]; then
    echo_info "  -> 修复 Live2D 路径"
    install_name_tool -change "$PROJECT_ROOT/third/live2d/dll/macos/libLive2DCubismCore.dylib" "@executable_path/../Frameworks/libLive2DCubismCore.dylib" "$EXECUTABLE" 2>/dev/null || true
    install_name_tool -id "@executable_path/../Frameworks/libLive2DCubismCore.dylib" "$DIST_FRAMEWORKS/libLive2DCubismCore.dylib" 2>/dev/null || true
fi

# 修复 WebRTC 路径
if [ -f "$DIST_FRAMEWORKS/libwebrtc_apm.dylib" ]; then
    echo_info "  -> 修复 WebRTC 路径"
    install_name_tool -id "@executable_path/../Frameworks/libwebrtc_apm.dylib" "$DIST_FRAMEWORKS/libwebrtc_apm.dylib" 2>/dev/null || true
fi

# 修复 Framework 库中的路径
echo_info "修复 Framework 库的依赖路径..."
for lib in "$DIST_DIR/${APP_NAME}.app/Contents/Frameworks/"*.a "$DIST_DIR/${APP_NAME}.app/Contents/Frameworks/"*.dylib; do
    if [ -f "$lib" ]; then
        # 修复这些库可能依赖的其他库路径
        install_name_tool -change "/usr/local/lib/libopus.dylib" "@executable_path/../Frameworks/libopus.dylib" "$lib" 2>/dev/null || true
        install_name_tool -change "/usr/local/lib/libportaudio.dylib" "@executable_path/../Frameworks/libportaudio.dylib" "$lib" 2>/dev/null || true
    fi
done

# 设置权限
echo_info "设置权限..."
chmod +x "$EXECUTABLE"

# 创建 DMG 临时目录
echo_info "创建 DMG 临时目录..."
DMG_TEMP="$DIST_DIR/dmg_temp"
rm -rf "$DMG_TEMP"
mkdir -p "$DMG_TEMP"

# 复制 App 到临时目录
cp -R "$DIST_DIR/${APP_NAME}.app" "$DMG_TEMP/"

# 创建 Applications 链接
echo_info "创建 Applications 快捷方式..."
ln -s /Applications "$DMG_TEMP/Applications"

# 创建 README
echo_info "创建 README..."
cat > "$DMG_TEMP/使用说明.txt" << 'EOF'
Heart Mind Robot - 桌面宠物聊天助手
================================

安装说明：
1. 将 HeartMindRobot.app 拖到 Applications 文件夹
2. 首次运行时，如果提示"无法打开"，请前往 系统偏好设置 -> 安全性与隐私 -> 通用 -> 点击"仍要打开"
3. 程序需要麦克风权限才能使用语音输入功能

使用说明：
- 右键点击桌宠打开菜单
- 选择"对话框 -> 打开"来开始对话
- 使用 Cmd+Shift+V 快捷键进行语音输入
- 长按对话框中的麦克风按钮也可以语音输入

注意事项：
- 首次运行需要授予麦克风权限
- 需要配置 WebSocket 服务器地址才能正常对话

版本: 1.0.0
EOF

# 删除旧的 DMG 文件
if [ -f "$PROJECT_ROOT/$DMG_NAME" ]; then
    echo_info "删除旧的 DMG 文件..."
    rm "$PROJECT_ROOT/$DMG_NAME"
fi

# 创建 DMG
echo_info "创建 DMG 文件..."

# 添加卷图标到临时目录
if [ -f "$PROJECT_ROOT/AppIcon.icns" ]; then
    echo_info "  -> 添加 DMG 卷图标"
    cp "$PROJECT_ROOT/AppIcon.icns" "$DMG_TEMP/.VolumeIcon.icns"
    # 使用 SetFile 设置图标属性（如果可用）
    if command -v SetFile &> /dev/null; then
        SetFile -c icnC "$DMG_TEMP/.VolumeIcon.icns" 2>/dev/null || true
    fi
fi

# 创建 DMG
hdiutil create -volname "$VOLUME_NAME" \
    -srcfolder "$DMG_TEMP" \
    -ov -format UDZO \
    "$PROJECT_ROOT/$DMG_NAME"

# 清理临时文件
echo_info "清理临时文件..."
rm -rf "$DMG_TEMP"

# 完成
echo_info ""
echo_info "========================================="
echo_info "  ✅ DMG 打包完成！"
echo_info "========================================="
echo_info "DMG 文件位置: $PROJECT_ROOT/$DMG_NAME"
echo_info "文件大小: $(du -h "$PROJECT_ROOT/$DMG_NAME" | cut -f1)"
echo_info ""
echo_info "测试建议："
echo_info "1. 双击打开 DMG 文件"
echo_info "2. 将 HeartMindRobot.app 拖到 Applications"
echo_info "3. 从 Applications 启动应用测试"
echo_info ""

