<p align="center">
  <a href="https://opensource.org/licenses/MIT">
    <img src="https://img.shields.io/badge/License-MIT-green.svg?style=flat-square" alt="License: MIT"/>
  </a>
  <img src="https://img.shields.io/badge/version-1.0.0-blue?style=flat-square" alt="Version"/>
  <img src="https://img.shields.io/badge/platform-macOS%20%7C%20Windows-lightgrey?style=flat-square" alt="Platform"/>
  <img src="https://img.shields.io/badge/language-C%2B%2B-orange?style=flat-square" alt="Language"/>
  <img src="https://img.shields.io/badge/framework-Qt6-brightgreen?style=flat-square" alt="Framework"/>
</p>

## 项目简介

Heart Mind Robot 是一个基于 Live2D 的智能桌面宠物应用，支持 AI 语音交互和实时通信。通过集成 小智AI 后端，实现自然流畅的语音对话体验。项目是骇客大赛参赛项目，仅限研究学习使用。

## 功能特点

### 🎭 Live2D 模型渲染

- **多角色支持**：Haru、Mao、Hiyori、Mark、Natori、Rice、Wanko、Miku、Shizuku 等多个 Live2D 模型
- **流畅动画**：Idle 动画、触摸交互、表情切换、物理引擎模拟
- **高清渲染**：支持 1024/2048 分辨率纹理，清晰细腻的画面表现
- **自定义模型**：支持导入和切换自定义 Live2D 模型
- **交互响应**：点击头部/身体触发不同动画反馈

### 🎤 音频处理与通信

- **高质量音频**：Opus 编解码技术，低延迟高效传输
- **回声消除**：集成 WebRTC 音频处理模块，提供清晰语音质量
- **实时录音**：PortAudio 跨平台音频输入管理
- **系统音频录制**：支持系统音频环回处理
- **语音活动检测**：智能检测语音活动，优化网络传输
- **音频权限管理**：macOS/Windows 平台音频设备权限控制

### 🤖 AI 对话功能

- **WebSocket 通信**：实时双向通信，低延迟数据传输
- **语音识别**：集成 AI 语音识别，将语音转换为文本
- **智能对话**：基于 AI 的上下文理解和自然回复
- **表情联动**：根据对话内容自动触发相应表情和动作
- **对话历史**：保存聊天记录，支持历史回顾
- **设备激活**：安全的设备激活机制，确保服务可用性

### 🖥️ 用户界面

- **窗口置顶**：可设置为保持窗口最前，不被其他窗口遮挡
- **透明窗口**：支持窗口透明度调节
- **系统托盘**：最小化到系统托盘，后台运行不占任务栏
- **全局快捷键**：支持自定义全局快捷键操作
- **聊天窗口**：独立的对话界面，显示 AI 回复和用户消息
- **右键菜单**：模型切换、置顶设置、窗口管理等功能
- **状态指示**：显示连接状态、模型加载状态等信息

### 🔧 系统集成

- **跨平台支持**：macOS 和 Windows 双平台原生支持
- **设备激活系统**：基于设备指纹的激活机制
- **配置管理**：JSON 格式配置文件，灵活自定义
- **日志系统**：完整的调试日志记录
- **错误恢复**：自动重连机制，网络中断自动恢复
- **资源管理**：智能资源加载和释放

### 🌐 网络功能

- **WebSocket 服务器连接**：与后端服务建立稳定连接
- **断线重连**：网络中断自动重连机制
- **消息队列**：异步消息队列处理，确保消息可靠传输
- **状态同步**：实时同步 AI 状态和桌宠状态
- **音频流传输**：实时音频流传输，低延迟对话体验

## 系统要求

### 基础要求

- **操作系统**：macOS 10.15+ 或 Windows 10+
- **内存**：至少 2GB RAM（推荐 4GB+）
- **显卡**：支持 OpenGL 3.3+ 的显卡
- **音频设备**：麦克风和扬声器设备
- **网络连接**：稳定的互联网连接（用于 AI 服务）

### 推荐配置

- **内存**：4GB RAM 或更多
- **处理器**：现代多核 CPU
- **存储空间**：至少 500MB 可用空间（用于模型和资源文件）
- **显卡**：支持 OpenGL 4.0+，独立显卡更佳
- **网络**：稳定的宽带连接，延迟低于 100ms

### 依赖库

**macOS:**
```bash
brew install qt6 portaudio opus cmake
```

**Windows:**
- Qt6 (需要手动安装)
- PortAudio
- Opus
- Visual Studio 2019+ (C++ 编译器)

## 请先看这里

- 📖 仔细阅读本文档的快速开始部分
- 🔑 首次运行需要激活设备，请确保网络连接正常
- 💬 支持通过 WebSocket 与 AI 后端通信
- 🎨 支持切换多个 Live2D 模型，模型文件位于 `models/live2d/` 目录

## 快速开始

### 克隆项目

```bash
git clone https://github.com/your-repo/heart-mind-robot.git
cd heart-mind-robot
```

### macOS 构建

```bash
# 安装依赖
brew install qt6 portaudio opus cmake

# 使用构建脚本
./build.sh

# 或手动构建
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
```

### Windows 构建

```powershell
# 安装 Qt6 和 Visual Studio

# 使用批处理文件
.\build.bat

# 或使用 Visual Studio
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

### 配置文件

编辑 `config/config.json` 配置后端服务：

```json
{
  "azure_api": {
    "key": "your_api_key",
    "url": "your_api_url",
    "system_prompt": "your_system_prompt"
  },
  "module": [
    {
      "name": "Haru",
      "width": 281,
      "height": 442
    }
  ]
}
```

### 运行应用

```bash
# 正常运行
./bin/HeartMindRobot.app/Contents/MacOS/HeartMindRobot

# 跳过激活（调试模式）
./bin/HeartMindRobot.app/Contents/MacOS/HeartMindRobot --skip-activation
```

## 技术架构

### 核心架构设计

- **分层架构**：应用层、渲染层、音频层、网络层清晰分离
- **事件驱动**：基于 Qt 事件循环的异步处理机制
- **单例模式**：核心组件采用单例模式，确保资源统一管理
- **插件化设计**：支持模块化扩展和自定义功能

### 关键技术组件

- **图形渲染**：Qt6 OpenGL、Live2D Cubism SDK、GLEW、GLFW
- **音频处理**：Opus 编解码、WebRTC 回声消除、PortAudio
- **通信协议**：WebSocket 实时通信、HTTP API 调用
- **配置管理**：JSON 格式配置、QSettings 平台配置管理

### 性能优化

- **异步优先**：音频和网络操作采用异步处理，避免阻塞主线程
- **内存管理**：智能指针管理内存，避免内存泄漏
- **资源缓存**：Live2D 模型和纹理资源智能缓存
- **渲染优化**：60 FPS 流畅动画，有效利用 GPU 资源

### 安全机制

- **设备认证**：基于设备指纹的激活验证
- **加密通信**：支持 WSS 加密传输（需后端支持）
- **权限控制**：音频设备权限请求和管理
- **错误隔离**：异常隔离和优雅降级机制

## 开发指南

### 项目结构

```
heart-mind-robot/
├── CMakeLists.txt              # CMake 构建配置
├── build.sh / build.bat        # 构建脚本
├── config/                     # 配置文件
│   └── config.json            # 应用配置
├── models/                     # Live2D 模型
│   └── live2d/                # 模型文件目录
├── inc/                        # 头文件
│   ├── LAppDelegate.hpp       # Live2D 代理
│   ├── LAppLive2DManager.hpp  # Live2D 管理
│   ├── AudioInputManager.hpp  # 音频输入
│   ├── WebSocketManager.hpp   # WebSocket 管理
│   └── ...                    # 其他头文件
├── src/                        # 源文件
│   ├── main.cpp               # 程序入口
│   ├── mainwindow.cpp         # 主窗口
│   ├── LAppDelegate.cpp       # Live2D 代理实现
│   ├── AudioInputManager.cpp  # 音频输入实现
│   ├── WebSocketManager.cpp  # WebSocket 实现
│   ├── OpusEncoder.cpp        # Opus 编码
│   └── ...                    # 其他源文件
└── third/                     # 第三方库
    ├── live2d/               # Live2D SDK
    ├── glfw/                 # GLFW
    ├── glew/                 # GLEW
    ├── opus/                 # Opus
    ├── portaudio/            # PortAudio
    └── webrtc_apm/           # WebRTC
```

### 开发环境设置

```bash
# macOS
brew install qt6 portaudio opus cmake
cd heart-mind-robot
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j4

# 调试运行
lldb ./bin/HeartMindRobot.app/Contents/MacOS/HeartMindRobot
```

### 核心开发模式

- **C++14 标准**：使用现代 C++ 特性
- **Qt 信号槽**：事件驱动的响应式编程
- **智能指针**：使用 `std::unique_ptr` 和 `std::shared_ptr` 管理内存
- **RAII**：资源获取即初始化，自动资源管理

### 添加新功能

**添加新的 Live2D 模型：**
1. 将模型文件放入 `models/live2d/` 目录
2. 在 `config/config.json` 中添加模型配置
3. 重新编译运行

**扩展音频功能：**
1. 修改 `src/AudioInputManager.cpp`
2. 添加新的音频处理逻辑
3. 更新 `inc/AudioInputManager.hpp` 接口

**增强 WebSocket 功能：**
1. 修改 `src/WebSocketManager.cpp`
2. 添加新的消息类型和处理逻辑
3. 更新消息协议定义

### 状态流转图

```
                      +------------+
                      |            |
                      v            |
+------+  用户交互  +----------+   |   +-----------+
| IDLE | ----------> | LISTENING |--+-> | SPEAKING |
+------+  (点击/唤醒) +----------+      +-----------+
   ^                          |
   |                          | AI 响应
   |                          v
   |                    +------------+
   +------------------- | AI_TALKING |
       完成播放         +------------+
```

## 贡献指南

欢迎提交问题报告和代码贡献。请确保遵循以下规范：

1. 代码风格遵循 Qt 编码规范
2. 使用 C++14 标准编写代码
3. 提交的 PR 包含适当的测试
4. 更新相关文档和注释

## 社区与支持

### 相关项目

- **[Live2D Cubism SDK](https://www.live2d.com/)** - Live2D 技术框架
- **[py-xiaozhi](https://github.com/huangjunsen0406/py-xiaozhi)** - python实现的 AI 语音客户端
- **[Qt](https://www.qt.io/)** - 跨平台开发框架
- **[PortAudio](http://www.portaudio.com/)** - 音频库
- **[Opus Codec](https://www.opus-codec.org/)** - 音频编解码
- **[Macos_live2d_deskpet](https://github.com/kun4399/Macos_live2d_deskpet/)** - live2d桌宠项目客户端
### 特别鸣谢

感谢所有为本项目提供支持的个人和组织：

- Live2D Inc. 提供的 Live2D 技术
- py-xiaozhi 团队提供的 AI 后端支持
- Qt 社区提供的跨平台框架支持

## 常见问题

### Q: 如何添加新的 Live2D 模型？

A: 将模型文件复制到 `models/live2d/` 目录，然后在 `config/config.json` 中添加配置即可。

### Q: 如何自定义 AI 对话？

A: 修改 `config/config.json` 中的 `system_prompt` 字段，重新启动应用即可生效。

### Q: 音频无法录制怎么办？

A: 检查系统麦克风权限设置（macOS 需要授权），确认 PortAudio 安装正确。

### Q: WebSocket 连接失败怎么办？

A: 检查网络连接，确认后端服务正常运行，查看日志文件获取详细错误信息。

### Q: 如何跳过激活流程？

A: 使用命令行参数 `--skip-activation` 启动应用（仅用于调试）。

## 许可证

[MIT License](LICENSE)

---

**Made with ❤️ by 2025-gd-hackers**
