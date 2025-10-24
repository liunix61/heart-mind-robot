# Heart Mind Robot - 分发指南

## 📦 已完成的修复

### 1. 代码层面修复
- ✅ 优化了资源路径加载逻辑，支持macOS App Bundle结构
- ✅ 添加了详细的错误日志输出
- ✅ 错误对话框现在显示具体的路径信息
- ✅ 修复了WebRTC库加载路径
- ✅ 添加了Resources目录内容诊断

### 2. 打包改进
- ✅ DMG自动复制WebRTC、Live2D等第三方库
- ✅ 自动检测系统架构(x86_64/arm64)
- ✅ 修复了所有动态库路径

### 3. 用户支持文档
- ✅ `用户安装指南.txt` - 给最终用户的简单安装说明
- ✅ `故障排除指南.md` - 完整的技术文档和解决方案
- ✅ `查看诊断日志.command` - 一键查看日志工具

---

## 🎯 用户报告"资源加载失败"的原因

经过分析，**最可能的原因是macOS的安全机制**：

### 根本原因
应用**未经过代码签名和公证**，macOS Gatekeeper会阻止应用运行。

### 症状
- 在你的电脑上正常运行 ✅
- 分发给用户后显示"资源加载失败" ❌

### 为什么会这样？
1. 你的Mac：开发环境，可能关闭了某些安全检查
2. 用户的Mac：从网络下载的应用被标记为"隔离"状态
3. macOS在隔离状态下限制应用访问资源

---

## 💡 解决方案

### 给用户的临时解决方法

**让用户执行以下命令移除隔离属性：**

```bash
sudo xattr -rd com.apple.quarantine /Applications/HeartMindRobot.app
```

### 根本解决方案（开发者需要做）

#### 选项A：使用Apple Developer证书签名（推荐）

**前提条件：**
- Apple Developer账号（$99/年）
- Developer ID Application证书

**步骤：**

1. **创建entitlements.plist**
```bash
cat > /Users/zhaoming/coding/source/heart-mind-robot/entitlements.plist << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>com.apple.security.cs.allow-unsigned-executable-memory</key>
    <true/>
    <key>com.apple.security.cs.disable-library-validation</key>
    <true/>
    <key>com.apple.security.device.audio-input</key>
    <true/>
</dict>
</plist>
EOF
```

2. **签名应用**
```bash
# 编译
./build.sh

# 签名
codesign --force --deep \
         --sign "Developer ID Application: Your Name (TEAM_ID)" \
         --entitlements entitlements.plist \
         --options runtime \
         --timestamp \
         build/bin/HeartMindRobot.app

# 验证签名
codesign -dvvv build/bin/HeartMindRobot.app
```

3. **打包DMG**
```bash
./create_dmg.sh
```

4. **公证DMG**
```bash
# 提交公证
xcrun notarytool submit Heart-Mind-Robot-1.0.0.dmg \
                --apple-id "your@email.com" \
                --team-id "TEAM_ID" \
                --password "app-specific-password" \
                --wait

# 装订票据
xcrun stapler staple Heart-Mind-Robot-1.0.0.dmg

# 验证
xcrun stapler validate Heart-Mind-Robot-1.0.0.dmg
spctl -a -vvv -t install Heart-Mind-Robot-1.0.0.dmg
```

#### 选项B：在DMG中包含安装辅助脚本

创建 `安装助手.command` 脚本放入DMG：

```bash
#!/bin/bash

echo "==========================================="
echo "  Heart Mind Robot 安装助手"
echo "==========================================="
echo ""
echo "此脚本将帮助您完成安装"
echo ""

# 检查应用是否在Applications
if [ -f "/Applications/HeartMindRobot.app/Contents/MacOS/HeartMindRobot" ]; then
    echo "✓ 检测到应用已安装在 /Applications"
    echo ""
    echo "正在移除隔离属性..."
    
    sudo xattr -rd com.apple.quarantine /Applications/HeartMindRobot.app
    
    if [ $? -eq 0 ]; then
        echo "✓ 隔离属性已成功移除"
        echo ""
        echo "现在可以正常运行应用了！"
        echo ""
        echo "是否现在打开应用？(y/n)"
        read answer
        if [ "$answer" = "y" ]; then
            open /Applications/HeartMindRobot.app
        fi
    else
        echo "✗ 移除失败，请检查是否输入了正确的密码"
    fi
else
    echo "✗ 未找到应用"
    echo ""
    echo "请先将 HeartMindRobot.app 拖到 Applications 文件夹"
fi

echo ""
echo "按回车键关闭..."
read
```

---

## 📝 当前DMG包含的文件

```
Heart-Mind-Robot-1.0.0.dmg
├── HeartMindRobot.app        # 主应用
├── Applications (链接)        # 指向/Applications的快捷方式
└── 使用说明.txt              # 安装和使用说明
```

**建议添加：**
- ✅ `用户安装指南.txt` - 已创建
- ✅ `安装助手.command` - 可选，简化用户操作

---

## 🔍 如何收集用户的错误日志

### 方法1：查看错误对话框
新版本会在错误对话框中显示：
```
资源加载失败，程序无法启动

应用目录: /Applications/HeartMindRobot.app/Contents/MacOS
资源路径: /Applications/HeartMindRobot.app/Contents/Resources
config.json: /Applications/HeartMindRobot.app/Contents/Resources/config.json

请检查应用是否被移动或损坏。
如果问题持续，请尝试重新下载安装。
```

让用户截图发给你。

### 方法2：Console.app日志
让用户：
1. 打开 Console.app
2. 搜索 "HeartMindRobot"
3. 运行应用
4. 复制所有相关日志

关键日志信息：
- `Application directory:` - 实际的应用路径
- `macOS bundle resource path:` - 计算出的资源路径
- `Resources directory exists: YES/NO` - 目录是否存在
- `Resources directory contents:` - 目录中的文件列表
- `config.json does not exist` - 配置文件缺失
- `open config.json failed` - 配置文件无法打开

---

## ✅ 测试检查清单

分发前的测试：

### 在开发机上测试
- [ ] 从build目录运行 ✅
- [ ] 从DMG安装到/Applications运行 ✅
- [ ] 移除隔离属性后运行 ✅

### 在干净的Mac上测试
- [ ] 虚拟机或其他Mac
- [ ] 从DMG安装
- [ ] 不移除隔离（验证会被阻止）
- [ ] 移除隔离后能正常运行
- [ ] 检查所有功能正常

### 模拟用户下载场景
```bash
# 给DMG添加隔离属性（模拟网络下载）
xattr -w com.apple.quarantine "0081;$(date +%s);Safari;$(uuidgen)" Heart-Mind-Robot-1.0.0.dmg

# 然后尝试安装和运行
```

---

## 📊 统计和反馈

收集用户反馈时询问：

1. **系统信息**
   - macOS版本？（运行 `sw_vers`）
   - 芯片类型？（Intel/Apple Silicon）

2. **安装过程**
   - 从哪里下载的DMG？
   - 如何安装的？（拖到Applications？）
   - 首次打开时有什么提示？

3. **错误信息**
   - 完整的错误对话框截图
   - Console.app中的日志
   - 执行了哪些解决步骤？

4. **是否解决**
   - 移除隔离属性后是否能运行？
   - 如果不能，是什么新错误？

---

## 🚀 下一步计划

### 立即可做
1. ✅ 更新DMG中的使用说明
2. ✅ 添加详细的错误提示
3. ✅ 创建故障排除文档
4. ⏳ 收集真实用户的反馈日志

### 长期改进
1. 🎯 申请Apple Developer账号
2. 🎯 实现代码签名
3. 🎯 实现公证（Notarization）
4. 🎯 考虑通过Mac App Store分发

---

## 📞 技术支持模板

给用户的回复模板：

```
您好！感谢反馈这个问题。

这是由于macOS的安全机制导致的，请按以下步骤解决：

1. 确保应用已安装到 /Applications 文件夹

2. 打开"终端"应用（在Applications/实用工具中）

3. 复制粘贴以下命令并按回车：
   sudo xattr -rd com.apple.quarantine /Applications/HeartMindRobot.app

4. 输入您的Mac密码（输入时不显示）

5. 现在应该可以正常打开应用了

如果还有问题，请提供：
- 错误对话框的截图
- 您的macOS版本
- Console.app中的相关日志

我们正在准备经过Apple认证的版本，届时将不再需要这个额外步骤。

感谢您的理解和支持！
```

---

## 📁 相关文件

- `/Users/zhaoming/coding/source/heart-mind-robot/用户安装指南.txt` - 给用户看的
- `/Users/zhaoming/coding/source/heart-mind-robot/故障排除指南.md` - 技术文档
- `/Users/zhaoming/coding/source/heart-mind-robot/查看诊断日志.command` - 日志工具
- `/Users/zhaoming/coding/source/heart-mind-robot/create_dmg.sh` - 打包脚本
- `/Users/zhaoming/coding/source/heart-mind-robot/Heart-Mind-Robot-1.0.0.dmg` - 最新DMG

---

**版本：** 1.0.1 (带详细日志)  
**日期：** 2025-10-24  
**状态：** 未签名（需要用户手动移除隔离属性）

