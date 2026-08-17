# Hachimi-Engine

> [English](README.md) | **简体中文**

基于 OpenGL 4.6 Core、参考 Hazel 架构思路、使用 C++20 编写的 3D 游戏引擎与编辑器。

## 功能特性

### 引擎核心

- 仅支持 Windows x86_64，Debug / Release 两种配置
- Premake5 生成 Visual Studio 2026 解决方案
- Application / Entry Point / Layer / LayerStack / Event 系统
- 自有数学库 `HachimiEngine::Math`（内部封装 GLM，业务代码不直接依赖 GLM）
- 基于 EnTT 的 Scene / ECS
- yaml-cpp 场景（`.hscene`）与项目（`.hproj`）序列化
- 控制台日志（引擎与客户端双 logger，暂不输出日志文件）

### 渲染

- OpenGL 4.6 Core 渲染后端，OpenGL 风格渲染抽象：VertexArray、VertexBuffer、IndexBuffer、Shader、Texture2D、TextureCube、Framebuffer
- 内置网格：Cube、Sphere、Plane、Grid
- HDR 渲染管线：ACES Tone Mapping + Gamma 后处理
- Cook-Torrance PBR 直接光照、方向光阴影映射（3×3 PCF）
- 程序化天空盒与基于环境贴图的 IBL（irradiance + prefiltered specular）

### 物理

- Box3D 物理系统：Static / Kinematic / Dynamic 刚体、Box / Sphere / Capsule / Plane 碰撞体
- 固定步长模拟与 Play 模式 Transform 同步

### 编辑器

- ImGui Docking 编辑器：Project Hub、Viewport、Scene Hierarchy、Inspector、Content Browser、Console
- Inter 字体与显示器 DPI 自适应 UI 缩放
- 选中相机 / 灯光实体时，在视口中绘制视锥、光照范围与方向等调试指示线
- ImGuizmo 变换工具：Translate / Rotate / Scale
- ImGuiFileDialog 文件对话框
- 编辑器相机：右键旋转、中键平移、滚轮缩放、WASD 移动

## 环境要求

- Windows
- Visual Studio 2026（Community 或更高版本）
- 已安装 OpenGL 4.6 Core 驱动

仓库已包含 Premake5 与全部第三方库源码，无需额外配置。

## 快速开始

### 生成解决方案

双击仓库根目录的 `GenerateSolution.bat`，或手动执行：

```
Vendor\Premake\Bin\premake5.exe vs2026 --file=premake5.lua
```

### 构建

用 Visual Studio 2026 打开生成的 `Hachimi-Engine.slnx` 并构建解决方案。

输出目录：

- 最终产物：`Bin/<configuration>-<system>-<architecture>/`
- 中间产物：`Bin/Obj/<configuration>-<system>-<architecture>/<ProjectName>/`

例如：

```
Bin/Debug-windows-x86_64/Hachimi-Editor.exe
Bin/Release-windows-x86_64/Hachimi-Editor.exe
```

### 运行

启动 `Hachimi-Editor.exe` 进入 Project Hub：

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

默认 `Default.hscene` 是一个渲染与物理特性展示场景：PBR 金属/粗糙材质球与立方体、地面、
方向光阴影（含平台上的物体间投影）、两盏点光源、父级层级物体、天空盒与 IBL；
进入 Play 模式后，场景中的球体、立方体与簇状子物体会在 Box3D 物理模拟中下落、碰撞并停稳。
Game 面板使用主相机视角。

### 视口操作

| 操作 | 按键 |
| --- | --- |
| 旋转视角 | 鼠标右键拖动（或 Alt + 左键拖动） |
| 平移视角 | 鼠标中键拖动 |
| 缩放 | 鼠标滚轮（或 Alt + 右键上下拖动） |
| 移动 | 按住鼠标右键 + W / A / S / D / Q / E |
| 加速移动 | 按住 Left Shift |
| 平移 Gizmo | Translate 按钮 |
| 旋转 Gizmo | Rotate 按钮 |
| 缩放 Gizmo | Scale 按钮 |

## 仓库结构

```text
Hachimi-Engine/          # 引擎核心（静态库）
  Resources/Shaders/     # 引擎内置 GLSL 着色器
  Source/                # 引擎源码
  Vendor/                # 引擎使用的第三方库
Hachimi-Editor/          # 编辑器客户端（可执行文件）
  Source/                # 编辑器源码
  Vendor/                # 编辑器使用的第三方库
Vendor/Premake/          # Premake5 工具链
Bin/                     # 构建产物（gitignore）
Vendor/Downloads/        # 第三方库下载暂存区（gitignore）
Utils/                   # 临时调试工具（FramebufferTest 帧缓冲测试、UIAutomation UI 自动化）
```

## 当前范围说明

以下内容已预留架构位置，但暂未实现：

- 音频系统
- 脚本系统
- 除 OpenGL 4.6 Core 外的渲染后端
- 外部 3D 模型导入（当前使用内置网格）
- 日志文件输出（当前仅控制台）
- 物理 Joints / Character Mover / Mesh / HeightField 碰撞体 / 物理 Debug Draw / Box3D 多线程（当前使用基础刚体与凸碰撞体）

后续画面效果与系统规划详见 `FUTURE.md`。

## 开发规范

仓库根目录的 `AGENTS.md` 包含完整约束与开发规范，请在开发前阅读。

## 许可证

Hachimi-Engine 采用 [MIT License](LICENSE)。Copyright (c) 2026 Hachimi2333。

## 致谢

Hachimi-Engine 建立在以下开源项目之上，感谢所有作者与贡献者：

- [Hazel](https://github.com/TheCherno/Hazel) — 引擎架构参考
- [Box3D](https://github.com/erincatto/box3d) — 物理引擎
- [EnTT](https://github.com/skypjack/entt) — ECS 框架
- [GLAD](https://github.com/Dav1dde/glad) — OpenGL 函数加载器
- [GLFW](https://github.com/glfw/glfw) — 窗口与输入处理
- [GLM](https://github.com/g-truc/glm) — 数学库（内部封装为 `HachimiEngine::Math`）
- [Dear ImGui](https://github.com/ocornut/imgui) — 编辑器用户界面
- [ImGuiFileDialog](https://github.com/aiekick/ImGuiFileDialog) — 文件对话框
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) — 变换 Gizmo
- [spdlog](https://github.com/gabime/spdlog) — 日志库
- [stb](https://github.com/nothings/stb) — 单头文件图像库
- [yaml-cpp](https://github.com/jbeder/yaml-cpp) — YAML 序列化
- [Inter](https://github.com/rsms/inter) — 编辑器字体，SIL Open Font License 1.1 许可
- [Premake5](https://github.com/premake/premake-core) — 构建系统生成工具

各第三方库的许可证见其对应 `Vendor` 目录下的 `LICENSE` 文件。
