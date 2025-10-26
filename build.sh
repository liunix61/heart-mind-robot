#!/bin/bash

# 跨平台构建脚本
# 使用方法: ./build.sh [platform] [arch] [cross-compile]
# platform: macos, windows, linux (默认: macos)
# arch: x86_64, arm64, x86, arm (默认: 自动检测)
# cross-compile: true, false (默认: false)

set -e

# 显示帮助信息
show_help() {
    echo "跨平台构建脚本"
    echo ""
    echo "使用方法:"
    echo "  ./build.sh [platform] [arch] [cross-compile]"
    echo ""
    echo "参数:"
    echo "  platform     目标平台 (macos, windows, linux) [默认: macos]"
    echo "  arch         目标架构 (x86_64, arm64, x86, arm) [默认: 自动检测]"
    echo "  cross-compile 是否交叉编译 (true, false) [默认: false]"
    echo ""
    echo "示例:"
    echo "  ./build.sh macos                    # 构建macOS版本"
    echo "  ./build.sh windows x86_64           # 构建Windows x86_64版本"
    echo "  ./build.sh linux arm64 true         # 交叉编译Linux ARM64版本"
    echo "  ./build.sh windows x86 true         # 交叉编译Windows x86版本"
    echo ""
    echo "交叉编译要求:"
    echo "  Windows: 需要安装 mingw-w64 (brew install mingw-w64)"
    echo "  Linux:   需要安装交叉编译工具链"
}

# 检查参数
if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    show_help
    exit 0
fi

PLATFORM=${1:-macos}
ARCH=${2:-auto}
CROSS_COMPILE=${3:-false}
BUILD_DIR="build"

echo "=========================================="
echo "跨平台构建脚本"
echo "=========================================="
echo "目标平台: $PLATFORM"
echo "目标架构: $ARCH"
echo "交叉编译: $CROSS_COMPILE"
echo "构建目录: $BUILD_DIR"
echo "=========================================="

# 自动检测架构
if [ "$ARCH" = "auto" ]; then
    case $PLATFORM in
        "macos")
            ARCH=$(uname -m)
            ;;
        "windows")
            ARCH="x86_64"
            ;;
        "linux")
            ARCH=$(uname -m)
            ;;
    esac
    echo "自动检测架构: $ARCH"
fi

# 确保必需的目录和文件存在
echo "检查必需的目录和配置文件..."
if [ ! -d "config" ]; then
    echo "创建 config 目录..."
    mkdir -p config
fi

# 复制 config 目录下的所有配置文件
echo "检查并复制配置文件..."
config_copied=0

# 从 dist 目录复制配置文件
if [ -d "dist/HeartMindRobot.app/Contents/Resources" ]; then
    for ext in json png icns; do
        for file in dist/HeartMindRobot.app/Contents/Resources/*.$ext; do
            if [ -f "$file" ]; then
                filename=$(basename "$file")
                if [ ! -f "config/$filename" ]; then
                    echo "  从 dist 复制: $filename"
                    cp "$file" "config/$filename"
                    ((config_copied++))
                fi
            fi
        done
    done
fi

# 从 Resources 目录复制资源文件到 config 目录
if [ -d "Resources" ]; then
    for ext in json png icns; do
        for file in Resources/*.$ext; do
            if [ -f "$file" ]; then
                filename=$(basename "$file")
                if [ ! -f "config/$filename" ]; then
                    echo "  从 Resources 复制: $filename"
                    cp "$file" "config/$filename"
                    ((config_copied++))
                fi
            fi
        done
    done
fi

# 如果没有找到 config.json，创建一个默认的
if [ ! -f "config/config.json" ]; then
    echo "  警告: 未找到默认配置文件，创建空配置文件"
    echo "{}" > config/config.json
fi

if [ $config_copied -eq 0 ]; then
    echo "  所有配置文件已存在"
else
    echo "配置文件检查完成，共复制 $config_copied 个文件"
fi

# 确保 models/live2d 目录存在并包含所有模型
if [ ! -d "models/live2d" ]; then
    echo "创建 models/live2d 目录..."
    mkdir -p models/live2d
fi

# 从 Resources 和 dist 目录复制缺失的模型到 models/live2d/
echo "检查并复制模型文件..."
copied_count=0

# 首先从 Resources 目录复制
if [ -d "Resources" ]; then
    for model_dir in Resources/*/; do
        model_name=$(basename "$model_dir")
        
        # 跳过非模型目录（没有 .model3.json 文件的目录）
        if [ ! -f "${model_dir}${model_name}.model3.json" ]; then
            continue
        fi
        
        # 检查 models/live2d/ 下是否已存在该模型
        if [ ! -d "models/live2d/${model_name}" ]; then
            echo "  从 Resources 复制模型: ${model_name}"
            cp -r "$model_dir" "models/live2d/${model_name}"
            ((copied_count++))
        fi
    done
fi

# 然后从 dist 目录复制（处理 Haru 等特殊模型）
if [ -d "dist/HeartMindRobot.app/Contents/Resources" ]; then
    for model_dir in dist/HeartMindRobot.app/Contents/Resources/*/; do
        model_name=$(basename "$model_dir")
        
        # 跳过非模型目录（没有 .model3.json 文件的目录）
        if [ ! -f "${model_dir}${model_name}.model3.json" ]; then
            continue
        fi
        
        # 检查 models/live2d/ 下是否已存在该模型
        if [ ! -d "models/live2d/${model_name}" ]; then
            echo "  从 dist 复制模型: ${model_name}"
            cp -r "$model_dir" "models/live2d/${model_name}"
            ((copied_count++))
        fi
    done
fi

if [ $copied_count -eq 0 ]; then
    echo "  所有模型已存在，无需复制"
else
    echo "模型文件检查完成，共复制 $copied_count 个模型"
fi

# 清理旧的构建目录
if [ -d "$BUILD_DIR" ]; then
    echo "清理旧的构建目录..."
    rm -rf "$BUILD_DIR"
fi

# 创建构建目录
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 设置交叉编译环境
setup_cross_compile() {
    case $PLATFORM in
        "windows")
            if [ "$CROSS_COMPILE" = "true" ]; then
                echo "设置Windows交叉编译环境..."
                export CC=x86_64-w64-mingw32-gcc
                export CXX=x86_64-w64-mingw32-g++
                export AR=x86_64-w64-mingw32-ar
                export STRIP=x86_64-w64-mingw32-strip
                export RC=x86_64-w64-mingw32-windres
                
                # 检查工具链
                if ! command -v $CC &> /dev/null; then
                    echo "错误: 未找到 mingw-w64 工具链"
                    echo "请安装: brew install mingw-w64"
                    exit 1
                fi
            fi
            ;;
        "linux")
            if [ "$CROSS_COMPILE" = "true" ]; then
                echo "设置Linux交叉编译环境..."
                case $ARCH in
                    "arm64")
                        export CC=aarch64-linux-gnu-gcc
                        export CXX=aarch64-linux-gnu-g++
                        export AR=aarch64-linux-gnu-ar
                        export STRIP=aarch64-linux-gnu-strip
                        ;;
                    "arm")
                        export CC=arm-linux-gnueabihf-gcc
                        export CXX=arm-linux-gnueabihf-g++
                        export AR=arm-linux-gnueabihf-ar
                        export STRIP=arm-linux-gnueabihf-strip
                        ;;
                esac
                
                # 检查工具链
                if ! command -v $CC &> /dev/null; then
                    echo "错误: 未找到交叉编译工具链"
                    echo "请安装相应的交叉编译工具链"
                    exit 1
                fi
            fi
            ;;
    esac
}

# 配置CMake
configure_cmake() {
    case $PLATFORM in
        "macos")
            echo "配置 macOS 构建..."
            CMAKE_ARGS="-DCMAKE_OSX_ARCHITECTURES=$ARCH"
            if [ "$ARCH" = "arm64" ]; then
                CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0"
            fi
            cmake $CMAKE_ARGS ..
            ;;
        "windows")
            echo "配置 Windows 构建..."
            if [ "$CROSS_COMPILE" = "true" ]; then
                echo "=========================================="
                echo "警告: Windows交叉编译限制"
                echo "=========================================="
                echo "由于Qt6库的兼容性问题，Windows交叉编译可能无法正常工作。"
                echo "建议使用以下方法："
                echo "1. 在Windows环境下进行原生构建"
                echo "2. 使用Docker容器进行Windows构建"
                echo "3. 使用GitHub Actions等CI/CD服务"
                echo ""
                echo "是否继续尝试交叉编译？(y/N)"
                read -r response
                if [[ ! "$response" =~ ^[Yy]$ ]]; then
                    echo "取消交叉编译"
                    exit 0
                fi
                
                setup_cross_compile
                
                # 尝试交叉编译，但可能失败
                cmake -DCMAKE_SYSTEM_NAME=Windows \
                      -DCMAKE_C_COMPILER=$CC \
                      -DCMAKE_CXX_COMPILER=$CXX \
                      -DCMAKE_RC_COMPILER=$RC \
                      -DCMAKE_FIND_ROOT_PATH="/usr/local/x86_64-w64-mingw32" \
                      -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
                      -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
                      -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
                      -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE \
                      ..
            else
                cmake ..
            fi
            ;;
        "linux")
            echo "配置 Linux 构建..."
            if [ "$CROSS_COMPILE" = "true" ]; then
                setup_cross_compile
                cmake -DCMAKE_SYSTEM_NAME=Linux \
                      -DCMAKE_C_COMPILER=$CC \
                      -DCMAKE_CXX_COMPILER=$CXX \
                      -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
                      -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
                      -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
                      ..
            else
                cmake ..
            fi
            ;;
        *)
            echo "不支持的平台: $PLATFORM"
            echo "支持的平台: macos, windows, linux"
            exit 1
            ;;
    esac
}

# 执行构建
configure_cmake

# 编译
echo "开始编译..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

if [ $? -eq 0 ]; then
    echo "=========================================="
    echo "构建成功!"
    echo "可执行文件位置: $BUILD_DIR/bin/HeartMindRobot.app"
    echo "=========================================="
else
    echo "构建失败!"
    exit 1
fi
