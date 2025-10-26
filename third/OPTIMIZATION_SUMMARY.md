# Third Party Libraries Optimization Summary

## 优化概述

本次优化清理了 `third/` 目录中的第三方库，删除了重复和未使用的库，统一了命名规范。

## 优化前的问题

1. **重复的 Opus 库**：存在 `libopus/` 和 `libopus_v1.4_msvc17/` 两个版本
2. **命名不一致**：`portAudio` 使用驼峰命名，其他库使用小写
3. **未使用的库**：`libopus/` 目录中的库未被项目引用

## 优化后的结构

```
third/
├── glew/                    # OpenGL 扩展加载库
├── glfw/                    # 跨平台窗口和输入处理库
├── live2d/                  # Live2D Cubism SDK
│   ├── cubism-sdk/          # SDK 框架（已重构）
│   │   ├── Framework/       # Live2D 核心框架
│   │   └── README.md        # SDK 说明文档
│   ├── dll/                 # 平台特定的动态库
│   └── inc/                 # Live2D 核心头文件
├── opus/                     # Opus 音频编码库（Windows）
├── portaudio/               # 音频 I/O 库（已重命名）
├── stb/                     # 单头文件图像处理库
└── webrtc_apm/              # WebRTC 音频处理模块
```

## 具体优化内容

### 1. 删除重复的 Opus 库
- **删除**：`third/libopus/` （跨平台版本，未使用）
- **重命名**：`third/libopus_v1.4_msvc17/` → `third/opus/` （Windows 平台使用）

**原因**：
- Windows 平台使用 `opus/` 中的预编译库
- macOS/Linux 平台使用系统安装的 libopus
- `third/libopus/` 中的库未被项目引用
- 简化目录名称，去除版本号和编译器信息

### 2. 重命名 PortAudio 目录
- **重命名**：`third/portAudio/` → `third/portaudio/`
- **更新**：CMakeLists.txt 中的路径引用

**原因**：统一命名规范，与其他库保持一致

### 3. Framework 目录重构
- **移动**：`Framework/` → `third/live2d/cubism-sdk/Framework/`
- **更新**：所有相关的 CMakeLists.txt 路径引用
- **添加**：版本说明文档

**原因**：更规范的项目结构，便于版本管理

## 平台支持情况

| 库名 | Windows | macOS | Linux | 说明 |
|------|---------|-------|-------|------|
| `glew` | ✅ | ✅ | ✅ | OpenGL 扩展 |
| `glfw` | ✅ | ✅ | ✅ | 窗口管理 |
| `live2d` | ✅ | ✅ | ✅ | Live2D 渲染 |
| `opus` | ✅ | ❌ | ❌ | Windows 专用 |
| `portaudio` | ✅ | ✅ | ✅ | 音频 I/O |
| `stb` | ✅ | ✅ | ✅ | 图像处理 |
| `webrtc_apm` | ✅ | ✅ | ✅ | 音频处理 |

## CMakeLists.txt 更新

### PortAudio 路径更新
```cmake
# 更新前
set(PORTAUDIO_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third/portAudio/x64")
set(PORTAUDIO_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/third/portAudio/x64/portaudio.lib")

# 更新后
set(PORTAUDIO_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third/portaudio/x64")
set(PORTAUDIO_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/third/portaudio/x64/portaudio.lib")
```

### Framework 路径更新
```cmake
# 更新前
add_subdirectory(Framework)
include_directories(Framework/src)

# 更新后
add_subdirectory(third/live2d/cubism-sdk/Framework)
include_directories(third/live2d/cubism-sdk/Framework/src)
```

### Opus 库路径更新
```cmake
# 更新前
include_directories(third/libopus_v1.4_msvc17/include)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/third/libopus_v1.4_msvc17/lib/x64)
target_link_libraries(... ${CMAKE_CURRENT_SOURCE_DIR}/third/libopus_v1.4_msvc17/lib/x64/libopus.lib ...)

# 更新后
include_directories(third/opus/include)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/third/opus/lib/x64)
target_link_libraries(... ${CMAKE_CURRENT_SOURCE_DIR}/third/opus/lib/x64/libopus.lib ...)
```

## 验证结果

- ✅ CMake 配置成功
- ✅ 编译成功
- ✅ 所有功能正常
- ✅ 目录结构清晰
- ✅ 命名规范统一

## 优化效果

1. **减少冗余**：删除了未使用的 `libopus/` 目录
2. **统一命名**：所有库使用小写命名，去除版本号和编译器信息
3. **规范结构**：Framework 移动到更合适的位置
4. **便于维护**：清晰的目录结构，便于版本管理
5. **功能完整**：所有必要的库都保留，功能不受影响

## 注意事项

- macOS 平台使用系统安装的 libopus，需要确保已安装：`brew install opus`
- Linux 平台同样使用系统安装的 libopus：`sudo apt-get install libopus-dev`
- Windows 平台使用 `opus/` 中的预编译库

---
*优化完成时间：2025-01-26*
*优化内容：删除重复库、重命名目录、重构 Framework 结构、简化 Opus 库命名*
