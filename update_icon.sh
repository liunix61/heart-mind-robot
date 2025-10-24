#!/bin/bash

# Heart Mind Robot 图标更新脚本
# 将 SystemIcon.png 转换为 macOS .icns 格式

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_ICON="$SCRIPT_DIR/Resources/SystemIcon.png"
OUTPUT_ICNS="$SCRIPT_DIR/AppIcon.icns"
OUTPUT_PNG="$SCRIPT_DIR/AppIcon.png"
ICONSET_DIR="$SCRIPT_DIR/AppIcon.iconset"

echo "=========================================="
echo "  Heart Mind Robot 图标更新工具"
echo "=========================================="
echo ""

# 检查源图标是否存在
if [ ! -f "$SOURCE_ICON" ]; then
    echo "❌ 错误: 找不到源图标文件: $SOURCE_ICON"
    exit 1
fi

echo "📁 源图标: $SOURCE_ICON"
echo ""

# 创建 iconset 目录
echo "🔄 创建临时图标集..."
rm -rf "$ICONSET_DIR"
mkdir -p "$ICONSET_DIR"

# 生成基础图（512x512）
echo "🔄 生成各种尺寸..."
sips -z 512 512 "$SOURCE_ICON" --out "$ICONSET_DIR/temp_512.png" >/dev/null 2>&1

# 生成所有需要的尺寸
sips -z 16 16     "$ICONSET_DIR/temp_512.png" --out "$ICONSET_DIR/icon_16x16.png" >/dev/null 2>&1
sips -z 32 32     "$ICONSET_DIR/temp_512.png" --out "$ICONSET_DIR/icon_16x16@2x.png" >/dev/null 2>&1
sips -z 32 32     "$ICONSET_DIR/temp_512.png" --out "$ICONSET_DIR/icon_32x32.png" >/dev/null 2>&1
sips -z 64 64     "$ICONSET_DIR/temp_512.png" --out "$ICONSET_DIR/icon_32x32@2x.png" >/dev/null 2>&1
sips -z 128 128   "$ICONSET_DIR/temp_512.png" --out "$ICONSET_DIR/icon_128x128.png" >/dev/null 2>&1
sips -z 256 256   "$ICONSET_DIR/temp_512.png" --out "$ICONSET_DIR/icon_128x128@2x.png" >/dev/null 2>&1
sips -z 256 256   "$ICONSET_DIR/temp_512.png" --out "$ICONSET_DIR/icon_256x256.png" >/dev/null 2>&1
sips -z 512 512   "$ICONSET_DIR/temp_512.png" --out "$ICONSET_DIR/icon_256x256@2x.png" >/dev/null 2>&1
sips -z 512 512   "$ICONSET_DIR/temp_512.png" --out "$ICONSET_DIR/icon_512x512.png" >/dev/null 2>&1
sips -z 1024 1024 "$ICONSET_DIR/temp_512.png" --out "$ICONSET_DIR/icon_512x512@2x.png" >/dev/null 2>&1

# 删除临时文件
rm "$ICONSET_DIR/temp_512.png"

# 创建 .icns 文件
echo "🔄 生成 .icns 文件..."
iconutil -c icns "$ICONSET_DIR" -o "$OUTPUT_ICNS"

# 复制 PNG 图标
echo "🔄 复制 PNG 图标..."
cp "$SOURCE_ICON" "$OUTPUT_PNG"

# 清理临时目录
rm -rf "$ICONSET_DIR"

echo ""
echo "✅ 图标更新完成！"
echo ""
echo "📦 输出文件："
echo "  • $OUTPUT_ICNS ($(du -h "$OUTPUT_ICNS" | cut -f1))"
echo "  • $OUTPUT_PNG ($(du -h "$OUTPUT_PNG" | cut -f1))"
echo ""
echo "🔧 下一步："
echo "  1. 运行 ./build.sh 重新编译应用"
echo "  2. 或者在已有 build 目录中运行: cd build && make"
echo ""

