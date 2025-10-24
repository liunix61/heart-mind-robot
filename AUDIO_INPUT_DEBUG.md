# 语音输入问题诊断指南

## 问题描述
语音输入功能没有响应，点击麦克风按钮后无法录音。

## 已修复的问题

### 1. 添加备用定时器机制
**问题**: QAudioSource 的 `readyRead` 信号在某些情况下可能不会触发
**解决方案**: 添加了一个每 100ms 触发的定时器，主动检查是否有音频数据可读

### 2. 优化缓冲区设置
**问题**: 默认缓冲区可能过大，导致延迟
**解决方案**: 设置缓冲区为 4 帧大小，提高响应速度

### 3. 增强调试日志
添加了详细的调试信息，帮助定位问题：
- 音频设备状态
- 可用字节数
- 数据读取情况
- 定时器触发情况

## 排查步骤

### 步骤 1: 检查麦克风权限
在 macOS 上，应用需要麦克风权限才能录音。

**检查方法**:
1. 打开 **系统设置** > **隐私与安全性** > **麦克风**
2. 确保你的应用（ChatFriend）在列表中并且已启用
3. 如果没有在列表中，尝试重启应用

**Info.plist 配置**:
```xml
<key>NSMicrophoneUsageDescription</key>
<string>此应用需要访问麦克风以进行语音输入和语音交互</string>
```

### 步骤 2: 查看控制台日志
运行应用时查看控制台输出，关注以下日志：

**正常情况应该看到**:
```
AudioInputManager: initializing audio input manager...
AudioInputManager initialized successfully
WebRTC processor initialized and enabled
Audio input setup completed
AudioInputManager: Using audio device: [设备名称]
AudioInputManager: Device supports format: true
AudioInputManager: Created QAudioSource, buffer size: 2560
Recording started
```

**录音时应该看到**:
```
AudioInputManager::onAudioDataReady() called, bytes available: [数字]
AudioInputManager: received [数字] bytes of audio data
AudioInputManager: encoding 320 samples
AudioInputManager: encoded to [数字] bytes, emitting signal
Sent audio data: [数字] bytes
```

**如果看到以下错误**:
- `Microphone permission not granted`: 权限未授予
- `No audio input device available`: 没有可用的音频输入设备
- `Failed to start audio input`: 音频输入启动失败
- `no bytes available to read`: 没有可读数据（可能是设备问题）

### 步骤 3: 检查音频输入设备
**macOS 检查**:
1. 打开 **系统设置** > **声音** > **输入**
2. 确认有可用的麦克风设备
3. 选择正确的输入设备
4. 对着麦克风说话，观察输入音量条是否有反应

### 步骤 4: 测试音频设备
**使用系统工具测试**:
```bash
# 列出所有音频输入设备
system_profiler SPAudioDataType

# 使用系统录音测试（如果可以录音，说明设备正常）
# 在终端运行，然后打开QuickTime Player录音测试
```

### 步骤 5: 检查 WebSocket 连接
语音输入需要 WebSocket 连接到服务器。

**检查连接状态**:
- 对话框标题应显示 "Send (已连接)"
- 如果显示 "Send (未连接)"，则无法使用语音功能

**日志中应该看到**:
```
WebSocket连接已建立，可以开始对话了！
```

### 步骤 6: 重新编译和运行
如果以上步骤都正常但仍有问题，尝试重新编译：

```bash
cd /Users/zhaoming/coding/source/heart-mind-robot
rm -rf build
mkdir build
cd build
cmake ..
make
```

然后运行应用：
```bash
./bin/ChatFriend.app/Contents/MacOS/ChatFriend
```

## 常见问题及解决方案

### Q1: 点击麦克风按钮没反应
**可能原因**:
- WebSocket 未连接
- 麦克风权限未授予

**解决方案**:
1. 确保 WebSocket 连接正常（按钮显示"已连接"）
2. 检查系统设置中的麦克风权限
3. 查看控制台日志中的错误信息

### Q2: 麦克风图标变红但没有数据发送
**可能原因**:
- 音频设备未正常工作
- `readyRead` 信号未触发

**解决方案**:
1. 新的备用定时器应该能解决这个问题
2. 查看日志中是否有 "Fallback timer triggered" 消息
3. 如果定时器也没有数据，检查音频设备

### Q3: 控制台显示权限被拒绝
**错误信息**: `Microphone permission denied`

**解决方案**:
1. 打开系统设置 > 隐私与安全性 > 麦克风
2. 如果应用在列表中但被禁用，启用它
3. 如果应用不在列表中：
   - 完全退出应用
   - 删除应用的权限缓存：
     ```bash
     tccutil reset Microphone com.tenclass.chatfriend
     ```
   - 重新启动应用，会再次请求权限

### Q4: 音频数据读取为空
**日志显示**: `readAll() returned empty data`

**可能原因**:
- 音频设备驱动问题
- 音频格式不支持

**解决方案**:
1. 检查日志中的 "Device supports format" 是否为 true
2. 尝试使用不同的音频输入设备
3. 重启音频服务（macOS）:
   ```bash
   sudo killall coreaudiod
   ```

## 代码修改说明

### 修改的文件
1. `src/AudioInputManager.cpp`
2. `inc/AudioInputManager.hpp`

### 主要改动
1. **添加备用定时器** (`m_fallbackTimer`):
   - 每 100ms 检查一次是否有音频数据
   - 防止 `readyRead` 信号未触发的情况

2. **设置合适的缓冲区大小**:
   ```cpp
   m_audioSource->setBufferSize(m_frameSizeBytes * 4);
   ```

3. **增强调试日志**:
   - 添加更详细的状态信息
   - 记录字节可用数和读取情况

## 下一步建议

如果问题依然存在，请：
1. 复制完整的控制台日志输出
2. 检查系统设置中的权限状态截图
3. 说明具体的错误现象（点击后有什么反应）
4. 提供系统版本和音频设备信息

## 测试清单
- [ ] 麦克风权限已授予
- [ ] 系统声音设置中有可用输入设备
- [ ] 输入音量条有反应
- [ ] WebSocket 已连接
- [ ] 点击麦克风按钮图标变为红色停止键
- [ ] 控制台有 "Recording started" 日志
- [ ] 控制台有 "onAudioDataReady" 或 "Fallback timer triggered" 日志
- [ ] 控制台有 "Sent audio data" 日志

