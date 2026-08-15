# Hachimi-Engine

Hachimi-Engine 是一个借鉴 Hazel 架构思路、使用 C++20 编写的 3D 游戏引擎与编辑器项目。

## 当前功能

- 仅支持 Windows x86_64，Debug / Release 两种配置
- Premake5 生成 Visual Studio 2026 解决方案
- OpenGL 4.6 Core 渲染后端
- Application / Entry Point / Layer / LayerStack / Event 系统
- OpenGL 风格渲染抽象：VertexArray、VertexBuffer、IndexBuffer、Shader、Texture2D、Framebuffer
- 内置网格：Cube、Sphere、Plane、Grid
- 编辑器相机：右键旋转、中键平移、滚轮缩放、WASD 移动
- 基于 EnTT 的 Scene / ECS
- yaml-cpp 场景（`.hscene`）与项目（`.hproj`）序列化
- ImGui Docking 编辑器：Project Hub、Viewport、Scene Hierarchy、Inspector、Content Browser、Console
- ImGuizmo 变换工具：Translate / Rotate / Scale
- ImGuiFileDialog 文件对话框
- 控制台日志（引擎与客户端双 logger，暂不输出日志文件）

## 环境要求

- Windows
- Visual Studio 2026（Community 或更高版本）
- 已安装 OpenGL 4.6 Core 驱动
- 仓库已包含 Premake5 与全部第三方库源码

## 生成解决方案

双击仓库根目录的：

```
GenerateSolution.bat
```

或手动执行：

```
Vendor\Premake\Bin\premake5.exe vs2026 --file=premake5.lua
```

生成 `Hachimi-Engine.slnx` 后，用 Visual Studio 2026 打开并构建。

## 构建输出

- 最终产物：`Bin/<configuration>-<system>-<architecture>/`
- 中间产物：`Bin/Obj/<configuration>-<system>-<architecture>/<ProjectName>/`

例如：

```
Bin/Debug-windows-x86_64/Hachimi-Editor.exe
Bin/Release-windows-x86_64/Hachimi-Editor.exe
```

## 编辑器使用

启动 `Hachimi-Editor.exe` 后会进入 Project Hub：

- 新建项目：项目默认创建在 `%USERPROFILE%\Documents\HachimiProjects`
- 打开最近项目
- 打开任意 `.hproj` 项目文件

新建项目会自动生成：

```
<ProjectName>/
├── <ProjectName>.hproj
└── Assets/
    ├── Meshes/
    ├── Textures/
    ├── Materials/
    └── Scenes/
        └── Default.hscene
```

### 视口操作

| 操作 | 按键 |
| --- | --- |
| 旋转视角 | 鼠标右键拖动 |
| 平移视角 | 鼠标中键拖动 |
| 缩放 | 鼠标滚轮 |
| 移动 | W / A / S / D / Q / E |
| 加速移动 | 按住 Left Shift |
| 平移 Gizmo | Translate 按钮 |
| 旋转 Gizmo | Rotate 按钮 |
| 缩放 Gizmo | Scale 按钮 |

## 仓库结构

```text
Hachimi-Engine/          # 引擎核心（静态库）
  Source/                # 引擎源码
  Vendor/                # 引擎使用的第三方库
Hachimi-Editor/          # 编辑器客户端（可执行文件）
  Source/                # 编辑器源码
  Vendor/                # 编辑器使用的第三方库
Vendor/Premake/          # Premake5 工具链
Bin/                     # 构建产物（gitignore）
TMP/                     # 第三方库下载暂存区（gitignore）
```

## 当前范围说明

以下内容已预留架构位置，但暂未实现：

- 音频系统
- 脚本系统
- 除 OpenGL 4.6 Core 外的渲染后端
- 外部 3D 模型导入（当前使用内置网格）
- 日志文件输出（当前仅控制台）

## 开发规范

仓库根目录的 `AGENTS.md` 包含完整约束与开发规范，请在开发前阅读。
