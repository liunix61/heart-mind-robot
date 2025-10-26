# 项目目录结构重构总结

## 🎯 重构目标

将项目从 `Resources/` 目录结构重构为更语义化的 `models/` 和 `config/` 分离结构，提高项目的可维护性和可扩展性。

## 📁 重构前后对比

### 重构前
```
Resources/
├── config.json              # 配置文件
├── SystemIcon.png           # 系统图标
└── Haru/                    # Live2D 模型目录
    ├── Haru.moc3           # 模型文件
    ├── Haru.model3.json    # 模型配置
    ├── Haru.physics3.json  # 物理配置
    ├── Haru.pose3.json     # 姿态配置
    ├── Haru.userdata3.json # 用户数据
    ├── Haru.cdi3.json      # CDI 配置
    ├── Haru.2048/          # 纹理文件
    ├── expressions/        # 表情文件
    ├── motions/            # 动作文件
    └── sounds/             # 声音文件
```

### 重构后
```
models/
├── live2d/                  # Live2D 模型
│   └── Haru/               # Haru 角色模型
│       ├── Haru.moc3       # 模型文件
│       ├── Haru.model3.json # 模型配置
│       ├── Haru.physics3.json # 物理配置
│       ├── Haru.pose3.json # 姿态配置
│       ├── Haru.userdata3.json # 用户数据
│       ├── Haru.cdi3.json  # CDI 配置
│       ├── Haru.2048/      # 纹理文件
│       ├── expressions/     # 表情文件
│       ├── motions/        # 动作文件
│       └── sounds/         # 声音文件
└── ai/                      # AI 模型（预留）

config/
├── config.json              # 配置文件
└── SystemIcon.png           # 系统图标
```

## 🔧 修改的文件

### 1. 核心代码文件
- **`src/resource_loader.cpp`**
  - 修改配置文件路径：`/config/config.json`
  - 修改系统图标路径：`/config/SystemIcon.png`
  - 修改资源路径查找逻辑：从 `Resources/` 改为项目根目录
  - 更新开发模式路径检测：检查 `models/` 和 `config/` 目录存在性
  - **路径调整**：config 和 models 目录现在在 exe 同层目录

- **`src/LAppLive2DManager.cpp`**
  - 修改模型路径：`/models/live2d/` + 模型名称

### 2. 构建和打包文件
- **`CMakeLists.txt`**
  - 更新 macOS 应用包复制命令
  - 复制 `models/` 和 `config/` 目录
  - **Windows 平台**：添加复制命令，将 `models/` 和 `config/` 复制到 exe 同层目录

- **`create_dmg.sh`**
  - 更新 macOS 打包脚本
  - 分别复制 `models/` 和 `config/` 目录

### 3. 版本控制文件
- **`.gitignore`**
  - 更新忽略规则：`models/ai/` 和 `config/`
  - 保留 `models/live2d/` 在版本控制中

## 🎯 重构优势

### 1. **语义统一**
- `models/` - 所有模型文件（Live2D + AI 模型）
- `config/` - 配置文件
- `cache/` - 缓存文件

### 2. **结构清晰**
- 模型文件和配置文件分离
- 便于管理和维护
- 符合现代应用架构

### 3. **扩展性强**
- 易于添加新的 Live2D 模型
- 易于添加 AI 模型
- 便于版本管理

### 4. **维护性好**
- 目录结构直观明了
- 文件分类清晰
- 便于团队协作

## 📊 目录用途说明

| 目录 | 用途 | Git 状态 | 说明 |
|------|------|----------|------|
| `models/live2d/` | Live2D 模型文件 | 被跟踪 | 核心模型资源 |
| `models/ai/` | AI 模型文件 | 被忽略 | 运行时下载的 AI 模型 |
| `config/` | 配置文件 | 被忽略 | 用户配置和系统图标 |
| `cache/` | 缓存文件 | 被忽略 | 运行时缓存数据 |

## ✅ 验证结果

- ✅ CMake 配置成功
- ✅ 目录结构正确
- ✅ 路径引用更新完成
- ✅ 构建脚本更新完成
- ✅ 版本控制规则更新完成
- ✅ 资源路径查找逻辑修复完成

## 🚀 后续建议

1. **添加新模型**：将新的 Live2D 模型放在 `models/live2d/` 目录下
2. **AI 模型管理**：将 AI 相关模型放在 `models/ai/` 目录下
3. **配置管理**：用户配置和系统配置统一放在 `config/` 目录下
4. **文档更新**：更新相关文档以反映新的目录结构

## 🎯 **最终路径结构**

### 开发模式
```
项目根目录/
├── models/              # Live2D 和 AI 模型
├── config/              # 配置文件
└── build/bin/           # 构建输出
    ├── HeartMindRobot.exe
    ├── models/          # 复制到 exe 同层
    └── config/          # 复制到 exe 同层
```

### 打包后（Windows）
```
应用目录/
├── HeartMindRobot.exe
├── models/              # Live2D 和 AI 模型
└── config/              # 配置文件
```

### 打包后（macOS）
```
xxx.app/Contents/
├── MacOS/HeartMindRobot
├── models/              # Live2D 和 AI 模型
└── config/              # 配置文件
```

---

*重构完成时间：2025-01-26*  
*重构内容：Resources 目录分离为 models/ 和 config/ 目录，路径调整为 exe 同层目录*
