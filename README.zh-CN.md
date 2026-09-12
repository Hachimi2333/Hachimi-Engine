# Hachimi-Engine

> [English](README.md) | **简体中文**

基于 OpenGL 4.6 Core、参考 Hazel 架构思路、使用 C++20 编写的 3D 游戏引擎与编辑器。

## 功能特性

### 引擎核心

- 仅支持 Windows x86_64，Debug / Release 两种配置
- 由 `CMakePresets.json` 驱动的 CMake 构建，提供 Visual Studio 2026 与 Ninja 预设
- Application / Entry Point / Layer / LayerStack / Event 系统
- 自有数学库 `HachimiEngine::Math`（内部封装 GLM，业务代码不直接依赖 GLM）
- 基于 EnTT 的 Scene / ECS
- yaml-cpp 场景（`.hscene`）与项目（`.hproj`）序列化
- 控制台日志（引擎与客户端双 logger，暂不输出日志文件）
- Lua 5.4 脚本系统，底层采用语言无关的后端抽象，为后续支持更多脚本语言预留

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
- 游戏导出管线：Build Settings 弹窗、Windows_x64 导出、`Data.hpak` 资源打包，以及独立的 `Hachimi-Player` 运行时
- 大图标 Content Browser 网格，支持纹理缩略图，可将文件拖拽到 Inspector 资产字段
- Native File Dialog Extended 系统原生文件对话框
- Inter 字体与显示器 DPI 自适应 UI 缩放
- 选中相机 / 灯光实体时，在视口中绘制视锥、光照范围与方向等调试指示线
- ImGuizmo 变换工具：Translate / Rotate / Scale
- 编辑器相机：右键旋转、中键平移、滚轮缩放、WASD 移动

## 脚本系统

脚本为 `Assets/Scripts` 下的 Lua 5.4 源文件，通过 Inspector 的 `Script` 组件挂到实体上。路径相对 `Assets/Scripts` 存储，因此也支持 `Player/Controller.lua` 这样的子目录路径。

脚本文件返回一个模块表，并提供可选的生命周期回调：

```lua
local MyScript = {}

function MyScript:OnCreate() end
function MyScript:OnUpdate(deltaTime) end
function MyScript:OnDestroy() end

return MyScript
```

回调中 `self.entity` 为挂载该脚本的实体，全局 `HE` 表提供沙箱化引擎 API：

- `HE.Log.Info / Warn / Error`
- `HE.Time.DeltaTime / ElapsedTime`
- `HE.Input.IsKeyDown / IsMouseButtonDown / GetMousePosition`
- `HE.Key.*`、`HE.Mouse.*`
- `HE.Math.Vec3`，以及 `Length / Normalize / Dot / Cross / Clamp / Lerp / Radians / Degrees`
- `HE.Scene.FindEntityByName`
- 实体方法：`GetName / SetName / GetPosition / SetPosition / GetRotation / SetRotation / GetScale / SetScale / Translate / Rotate / GetWorldPosition`

脚本只在 Play 模式运行。每次 Play 会话使用独立 Lua VM，脚本报错只会输出到 Console，不会导致编辑器崩溃。

## 环境要求

- Windows
- Visual Studio 2026（Community 或更高版本）
- 已安装 OpenGL 4.6 Core 驱动

仓库已包含全部第三方库源码，无需额外配置。Visual Studio 2026 自带构建所需的 CMake 与 Ninja，
无需单独安装。

## 快速开始

### 配置与构建

构建完全由 `CMakePresets.json` 驱动。构建直接使用 Ninja 调用 MSVC 工具链，因此需要在
**VS 2026 Developer Command Prompt**（开始菜单中的 "Developer Command Prompt for VS 2026"）
中、于仓库根目录执行：

```
cmake --preset x64-debug
cmake --build --preset x64-debug
```

Release：

```
cmake --preset x64-release
cmake --build --preset x64-release
```

整个过程不会生成 Visual Studio 解决方案，由 CMake 与 Ninja 自行完成构建，唯一的构建命令就是
`cmake --build`。也可以直接用 Visual Studio 2026 或 VS Code 打开仓库文件夹，它们读取的是同一份
`CMakePresets.json`。

如果 `cmake` 不在 `PATH` 中，Visual Studio 2026 自带的路径为：

```
C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
```

输出目录：

- 最终产物：`Build/<preset>/bin/`
- CMake 构建文件与中间产物：`Build/<preset>/`，由 CMake 自行管理

例如：

```
Build/x64-debug/bin/Hachimi-Editor.exe
Build/x64-debug/bin/Hachimi-Player.exe
Build/x64-release/bin/Hachimi-Editor.exe
Build/x64-release/bin/Hachimi-Player.exe
```

着色器与 UI 字体会在每次构建后自动拷贝到各可执行文件所在目录。

### 验证

```
Build/x64-debug/bin/Hachimi-Tests.exe
```

`Hachimi-Tests` 执行无界面的包往返与虚拟文件系统校验，全部通过时退出码为 0。

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
    ├── Scripts/
    │   └── Rotator.lua
    └── Scenes/
        └── Default.hscene
```

默认 `Default.hscene` 是一个渲染与物理特性展示场景：PBR 金属/粗糙材质球与立方体、地面、
方向光阴影（含平台上的物体间投影）、两盏点光源、父级层级物体、天空盒与 IBL；
进入 Play 模式后，场景中的球体、立方体与簇状子物体会在 Box3D 物理模拟中下落、碰撞并停稳，
同时 `Scripted Spinner` 实体会由内置的 `Rotator.lua` 脚本持续旋转。
Game 面板使用主相机视角。

### 导出游戏

在编辑器菜单栏打开 `Build → Build Settings...`。弹窗按目标平台保存配置，当前阶段仅实现 Windows。Windows 配置包括：

- Product Name（导出的可执行文件名与窗口标题）
- Start Scene（`Assets/Scenes` 下的 `.hscene` 文件）
- 窗口宽高与 VSync

点击 `Export` 生成构建：

```
<ProjectName>/
└── Build/
    └── Windows_x64/
        ├── <ProductName>.exe    # 独立 Hachimi-Player 运行时
        └── Data.hpak            # 打包的项目文件、Assets、着色器与字体
```

导出的游戏启动后立即运行 Start Scene（含物理与 Lua 脚本）。Player 就地把 `Data.hpak` 挂载为虚拟文件系统，
所有资源直接从包内读取：**不做任何解压**，不产生缓存目录，可从只读介质运行，且只解压真正被加载的资源。

Player 同时提供无窗口模式，让导出构建在没有 GPU 的情况下也能被验证：

```
<ProductName>.exe [<package>] [--content=<dir>] [--verify] [--list] [--stats]
```

| 参数 | 作用 |
| --- | --- |
| `<package>` | 要运行的资源包，默认为可执行文件同目录下的 `Data.hpak` |
| `--content=<dir>` | 以更高优先级挂载一个松散文件目录，改资源后无需重新导出 |
| `--verify` | 校验全部 entry 的内容哈希后退出，成功返回 0，失败返回 1 |
| `--list` | 打印资源包目录表（路径、原始大小、压缩后大小、方式、块数） |
| `--stats` | 打印包头信息 |

### 资源包格式

`Data.hpak` 是基于 [Zstandard](https://github.com/facebook/zstd) 的自定义容器，不再是 ZIP：

- 头部包含 magic、格式版本、包 ID 与字典信息；尾部镜像目录表位置，因此从文件两端都能定位目录。
- 目录表记录每个 entry 的路径哈希、偏移、大小、内容哈希与分块表。
- 每个 entry 被切分为可独立解压的块，这是支持区间读取与流式读取的基础。
- 可选共享 zstd 字典由包内容训练得到，对「大量小文本资源」的工程收益明显。
- 本身就压缩过的资源（PNG、TTF 等）直接 store，不做二次压缩。
- 资源包以只读方式内存映射，store 的 entry 可以零拷贝直接交付。
- 打包阶段通过引擎任务系统并行压缩，且相同输入会产生逐字节一致的资源包。

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
CMakeLists.txt           # 唯一构建入口（也是唯一的 project() 调用）
CMakePresets.json        # x64-debug / x64-release 的配置与构建预设
Hachimi-Engine/          # 引擎核心（静态库）
  CMakeLists.txt
  Resources/Shaders/     # 引擎内置 GLSL 着色器
  Resources/Fonts/       # 编辑器 UI 字体（Inter）及其许可证
  Source/                # 引擎源码
  Source/Packaging/      # 游戏导出配置、.hpak 格式与读写实现
  Source/Scripting/      # 语言无关脚本核心与 Lua 后端
Hachimi-Editor/          # 编辑器客户端（可执行文件）
  CMakeLists.txt
  Source/                # 编辑器源码
Hachimi-Player/          # 导出构建使用的独立游戏运行时
  CMakeLists.txt
  Source/                # Player 源码
Hachimi-Tests/           # 无窗口验证目标（资源包往返、虚拟文件系统）
  CMakeLists.txt
  Source/                # 测试源码
Vendor/                  # 全部第三方库，每个库一个目录
  Box3D/ EnTT/ GLAD/ GLFW/ ImGuizmo/ Lua/
  NativeFileDialogExtended/ glm/ imgui/ sol2/ spdlog/ stb/ yaml-cpp/ zstd/
Build/                   # 构建产物，每个预设一个目录（gitignore）
Utils/                   # 临时调试工具（FramebufferTest 帧缓冲测试、UIAutomation UI 自动化）
```

每个第三方库都通过 `add_subdirectory` 引入并导出自己的 CMake target，因此 include
路径随 target 传递，不再逐个工程手写。GLAD、Lua、stb 与 imgui 未提供可用的
`CMakeLists.txt`，这四个目录下各有一个本项目自己维护的薄文件；`Vendor/zstd/CMakeLists.txt`
则转发到 zstd 位于 `build/cmake/` 的自有工程。

## 当前范围说明

以下内容已预留架构位置，但暂未实现：

- 音频系统
- 除 Lua 外的更多脚本语言（后端抽象已预留）
- Windows 以外的导出平台（Build Settings 的平台布局已预留，当前实现 Windows_x64）
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
- [Lua](https://www.lua.org/) — Lua 5.4 脚本语言运行时
- [sol2](https://github.com/ThePhD/sol2) — 现代 C++ Lua 绑定库
- [zstd](https://github.com/facebook/zstd) — 资源包容器使用的 Zstandard 压缩，以及其内置 xxHash（路径与内容哈希）
- [Dear ImGui](https://github.com/ocornut/imgui) — 编辑器用户界面
- [Native File Dialog Extended](https://github.com/btzy/nativefiledialog-extended) — 系统原生文件对话框
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) — 变换 Gizmo
- [spdlog](https://github.com/gabime/spdlog) — 日志库
- [stb](https://github.com/nothings/stb) — 单头文件图像库
- [yaml-cpp](https://github.com/jbeder/yaml-cpp) — YAML 序列化
- [Inter](https://github.com/rsms/inter) — 编辑器字体，SIL Open Font License 1.1 许可
- [CMake](https://cmake.org/) — 构建系统

各第三方库的许可证见其对应 `Vendor` 目录下的 `LICENSE` 文件。
