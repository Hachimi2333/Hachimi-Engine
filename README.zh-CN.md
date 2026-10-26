# Hachimi-Engine

> [English](README.md) | **简体中文**

基于 OpenGL 4.6 Core、参考 Hazel 架构思路、使用 C++20 编写的 3D 游戏引擎与编辑器。

## 功能特性

### 引擎核心

- 仅支持 Windows x86_64，Debug / Release 两种配置
- 由 `CMakePresets.json` 驱动的 CMake 构建，提供 Visual Studio 2026 与 Ninja 预设
- Application / Entry Point / Layer / LayerStack / Event 系统
- 自有数学库 `HachimiEngine::Math`（内部封装 GLM，业务代码不直接依赖 GLM）
- 基于 EnTT 的 Scene / ECS，附组件注册表：组件在一处描述符里声明、持久化并在 Inspector 中编辑，而不是散落在五段 if 链里
- 带显式阶段的场景系统（PreUpdate / FixedUpdate / Update / LateUpdate），物理与脚本独立挂载
- 派生式实体层级（只存父节点），重父级带环检测，删除父节点销毁整棵子树
- yaml-cpp 场景（`.hscene`，带格式版本）与项目（`.hproj`）序列化，枚举按名字存储
- 资产管线：`AssetDatabase` 扫描 `Assets/`，每个资产在 `<文件>.meta` 侧车里持有 UUID 身份，
  组件通过 UUID 而非路径引用资产，因此改名或移动文件不会打断引用
- 材质资产（`Assets/Materials/*.hmaterial`）：引擎着色器、基础色与 albedo 贴图引用，编辑器可直接创建与编辑
- 每个资产的纹理导入设置（色彩空间、mipmap、wrap、filter）保存在侧车里
- 控制台日志（引擎与客户端双 logger，暂不输出日志文件）
- Lua 5.4 脚本系统，底层采用语言无关的后端抽象，为后续支持更多脚本语言预留

### 渲染

- OpenGL 4.6 Core 渲染后端，OpenGL 风格渲染抽象：VertexArray、VertexBuffer、IndexBuffer、Shader、Texture2D、TextureCube、Framebuffer
- CPU 几何（`MeshData`）与 GPU 几何（`Mesh`）分离，场景、序列化与测试套件都不需要 GL 上下文
- `RendererContext` 持有全部 GPU 资源，`SceneRenderer` 实例只持有每帧状态，编辑器视口 / Game 面板 / Player 互不干扰
- 有序 `RenderPass` 管线（方向光阴影、天空盒、网格地面、不透明）共用 `SceneRenderTarget`，每视图常量走一个 std140 uniform buffer
- 开启背面剔除，正面为逆时针；内置图元绕序已对齐
- 内置网格：Cube、Sphere、Plane（双面，作为地面图元）、Grid
- HDR 渲染管线：ACES Tone Mapping + Gamma 后处理
- Cook-Torrance PBR 直接光照、方向光阴影映射（3×3 PCF）
- 程序化天空盒与基于环境贴图的 IBL（irradiance + prefiltered specular）

### 物理

- Box3D 物理系统：Static / Kinematic / Dynamic 刚体、Box / Sphere / Capsule / Plane 碰撞体
- 固定步长模拟、渲染插值与暂停单步
- RayCast / RayCastAll、球/盒 Overlap、球形 ShapeCast、力/冲量和 Teleport
- 碰撞与触发器事件通过场景 dispatcher 在脚本 Update 前派发
- Box3D 子任务接入 JobSystem，默认 1 个 worker，0 自动选择可用线程池数量
- 编辑器碰撞体调试叠加与可撤销的场景物理设置

### 编辑器

- ImGui Docking 编辑器：Project Hub、Viewport、Scene Hierarchy、Inspector、Content Browser、Console
- 撤销 / 重做命令栈（Ctrl+Z / Ctrl+Y），gizmo 拖拽与文本输入等连续编辑合并为一条记录
- 关闭编辑器、切换场景、返回 Project Hub 前提示保存未保存的改动
- Content Browser 文件管理：新建文件夹、新建材质、重命名、复制、删除（删除前列出引用者）与拖拽移动，`.meta` 随资产移动
- 游戏导出管线：Project Settings 弹窗、Windows_x64 导出、`Data.hpak` 资源打包，以及独立的 `Hachimi-Player` 运行时
- 大图标 Content Browser 网格，支持纹理缩略图，可将文件拖拽到 Inspector 资产字段
- Native File Dialog Extended 系统原生文件对话框
- Inter 字体与显示器 DPI 自适应 UI 缩放
- 选中相机 / 灯光实体时，在视口中绘制视锥、光照范围与方向等调试指示线
- ImGuizmo 变换工具：Translate / Rotate / Scale
- 编辑器相机：右键旋转、中键平移、滚轮缩放、WASD 移动

## 脚本系统

脚本为 `Assets/Scripts` 下的 Lua 5.4 源文件，通过 Inspector 的 `Script` 组件挂到实体上。引用存的是脚本资产的 UUID，因此移动或重命名 `.lua` 文件不会断开挂载；面板会同时显示文件名。

脚本文件返回一个模块表，并提供可选的生命周期回调：

```lua
local MyScript = {}

function MyScript:OnCreate() end
function MyScript:FixedUpdate(fixedDeltaTime) end
function MyScript:OnUpdate(deltaTime) end
function MyScript:OnTriggerEnter(other) end
function MyScript:OnReload() end
function MyScript:OnDestroy() end

return MyScript
```

回调中 `self.entity` 为挂载该脚本的实体，全局 `HE` 表提供沙箱化引擎 API：

- `HE.Log.Info / Warn / Error`
- `HE.Time.DeltaTime / UnscaledDeltaTime / FixedDeltaTime / ElapsedTime / TimeScale / FrameCount`
- `HE.Input.GetActionPressed / GetActionHeld / GetActionReleased / GetAxis`，保留原始轮询接口以兼容已有脚本
- `HE.Key.*`、`HE.Mouse.*`
- `HE.Math.Vec3`，以及 `Length / Normalize / Dot / Cross / Clamp / Lerp / Radians / Degrees`
- `HE.Scene.FindEntityByName / CreateEntity / DestroyEntity`
- `HE.Physics.Raycast / OverlapSphere`
- 刚体接口：`GetLinearVelocity / SetLinearVelocity / GetAngularVelocity / SetAngularVelocity`、
  `ApplyForce / ApplyImpulse / ApplyTorque / ApplyAngularImpulse / Teleport`、`IsTriggered / SetTriggered`
- 延迟实体操作：`AddComponent / RemoveComponent / SetParent / ClearParent / Destroy`
- 实体方法：`GetName / SetName / GetPosition / SetPosition / GetRotation / SetRotation / GetScale / SetScale / Translate / Rotate / GetWorldPosition`

脚本在编辑器 Play 与导出的 Player 中运行，每个场景拥有独立 VM。在 Inspector 声明
Bool / Int / Float / String / Vec3 字段，通过 `self.fields.Name` 访问；序列化只保存默认值，
运行时值留在会话内。`self.scene` / `self.time` 与 `HE.Scene` / `HE.Time` 指向同一服务。

`require("Helper")` 通过 VFS 加载 `Assets/Scripts/Helper.lua`，缓存模块并拒绝越界路径。
磁盘脚本及 require 的模块每 0.5 秒检查一次；成功热重载保留当前字段并调用
OnDestroy → OnCreate → OnReload，加载失败保留旧模块。仅存在于资源包中的脚本不轮询磁盘。
回调错误输出文件与行号，后续调用继续重试。

## 统一运行时

编辑器 Play 与 Player 都通过 `SceneRuntime` 驱动独立时钟、输入、模拟和延迟初始化的渲染资源。
帧时间夹到 0.1 秒，每帧最多补 4 个固定步；物理事件先于脚本 Update，结构性场景命令在
Update 结束、LateUpdate 开始前统一执行。新实体此前为 Pending，组件读写返回空值并告警一次。

输入绑定从 `Assets/Input.hinput` 加载，支持键盘、鼠标与 GLFW 手柄，也能直接读取 `Data.hpak`。
缺失文件时使用内置映射，新项目自动生成默认文件。玩法代码使用命名动作/轴；
`TimeScale = 0` 停止固定步推进，编辑器 Pause 暂停整个会话。
Play 继续、Pause 暂停、Step 在暂停时精确推进一个固定步、Stop 丢弃运行时副本。
Game 面板聚焦时捕获光标，Escape 释放，点击面板可重新捕获；Play 时禁用编辑器相机控制。
Play 态拾取使用物理碰撞体，编辑态仍使用网格三角形。

在动态刚体脚本上声明 Float 字段 `Speed`，可使用以下脚本验证 WASD 与手柄移动：

```lua
local Player = {}
function Player:FixedUpdate(dt)
    local x = HE.Input.GetAxis("MoveX") + HE.Input.GetAxis("GamepadMoveX")
    local z = HE.Input.GetAxis("MoveY") - HE.Input.GetAxis("GamepadMoveY")
    local velocity = self.entity:GetLinearVelocity()
    self.entity:SetLinearVelocity(HE.Math.Vec3(x * self.fields.Speed, velocity.y, -z * self.fields.Speed))
end
function Player:OnTriggerEnter(other)
    HE.Log.Info("Entered trigger: " .. other:GetName())
end
return Player
```

场景格式升级到版本 4，旧版本明确拒绝加载、不自动迁移。
画面、手柄和导出后行为由用户手动验证；模拟、脚本、序列化及编辑器/Player 状态一致性由无窗口测试覆盖。

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
Build/x64-debug/bin/Tests.exe
```

`Tests` 是基于 [doctest](https://github.com/doctest/doctest) 的无界面验证套件，覆盖资源包往返、
分块与流式读取、损坏处理、虚拟文件系统、项目/导出流水线、场景模型与其序列化、组件注册表、
渲染管线以及引擎的纯工具函数，全部通过时退出码为 0。
用例按子系统分类放在 `Tests/` 下，可用 doctest 自带参数选择运行范围：

```
Build/x64-debug/bin/Tests.exe -ts=Packaging        # 单个套件
Build/x64-debug/bin/Tests.exe -tc="*round trip*"   # 匹配的用例
Build/x64-debug/bin/Tests.exe --list-test-cases    # 列出全部用例
```

同一套用例也注册到了 CTest，可用 `ctest --test-dir Build/x64-debug --output-on-failure` 运行。
套件内不会创建窗口或 OpenGL 上下文；需要图形上下文的验证仍由人工在编辑器内完成。

`Build/x64-debug/bin/EmitPackage.exe <输出.hpak>` 会写出一个用于验证 Player 无窗口模式的合成资源包：

```
Build/x64-debug/bin/EmitPackage.exe Build/x64-debug/bin/SmokeTest.hpak
Build/x64-debug/bin/Hachimi-Player.exe Build/x64-debug/bin/SmokeTest.hpak --verify --list --stats
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
    ├── Input.hinput
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

### 设置与启动

`Settings → Scene Settings` 集中管理渲染和物理设置，高级模拟参数与运行统计默认折叠。
场景设置支持撤销、重做；播放期间的修改只影响运行副本。可通过 `View` 开启辅助面板或重置布局。

个人布局与面板可见性保存在 `%APPDATA%/HachimiEngine/EditorSettings.yaml`，最近项目继续使用同目录下的
`HubSettings.yaml`。程序不再读取或生成 `imgui.ini`。资产属性草稿在 Apply 或 Revert 前持续保留，离开时会提示处理。

CMake 在构建阶段将固定天空与环境光烘焙为 `Environment/Default.henv`，保持原有画质。
统一资源复制目标将它放到可执行文件旁，导出时纳入 `Data.hpak`；Player 不再初始化 ImGui。

编辑器和 Player 均接受 `--startup-profile`：输出启动分阶段计时，提交一个实际画面帧后退出。
该诊断会创建图形窗口；自动化 `Tests.exe` 始终无窗口。详见[启动分析与实测](Docs/StartupAndEditorUX.md)。

### 导出游戏

在编辑器菜单栏打开 `Settings → Project Settings...`，也可以通过 `Build → Export Game...` 进入。修改先保留在草稿中，保存后才写入项目；当前仅支持 Windows x86_64。配置包括：

- Product Name（导出的可执行文件名与窗口标题）
- Start Scene（`Assets/Scenes` 下的 `.hscene` 文件）
- 窗口宽高与 VSync

点击 `Save and Export` 保存项目与场景后生成构建：

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
  Source/Asset/          # 资产身份、侧车元数据、资产数据库、纹理缓存与材质资产
  Source/Editor/         # 可撤销编辑命令、命令历史与场景脏标记
  Source/Packaging/      # 游戏导出配置、.hpak 格式与读写实现
  Source/Renderer/       # 渲染上下文、管线、通道、视图与资源
  Source/Scene/          # 组件注册表、组件、实体、场景、系统
  Source/Scripting/      # 语言无关脚本核心与 Lua 后端
Hachimi-Editor/          # 编辑器客户端（可执行文件）
  CMakeLists.txt
  Source/                # 编辑器源码
  Source/Components/     # Inspector 控件、绘制注册表与各组件绘制器
  Source/UI/             # 资产网格、资产选择弹窗与共享的资产字段控件
Hachimi-Player/          # 导出构建使用的独立游戏运行时
  CMakeLists.txt
  Source/                # Player 源码
Tests/                   # 无窗口 doctest 用例，按子系统分类
  CMakeLists.txt
  Main.cpp               # doctest 入口与引擎生命周期
  Support/               # 共享夹具（样例目录树、测试资源包）
Vendor/                  # 全部第三方库，每个库一个目录
  Box3D/ EnTT/ GLAD/ GLFW/ ImGuizmo/ Lua/
  NativeFileDialogExtended/ doctest/ glm/ imgui/ sol2/ spdlog/ stb/ yaml-cpp/ zstd/
Build/                   # 构建产物，每个预设一个目录（gitignore）
Utils/                   # 临时调试工具（FramebufferTest 帧缓冲测试、UIAutomation UI 自动化）
  EmitPackage/           # Player 无窗口模式使用的资源包生成工具（由 CMake 构建）
```

每个第三方库都通过 `add_subdirectory` 引入并导出自己的 CMake target，因此 include
路径随 target 传递，不再逐个工程手写。GLAD、Lua、stb 与 imgui 未提供可用的
`CMakeLists.txt`，这四个目录下各有一个本项目自己维护的薄文件；`Vendor/zstd/CMakeLists.txt`
则转发到 zstd 位于 `build/cmake/` 的自有工程。doctest 保留上游 CMake 工程，由根
`CMakeLists.txt` 与其它库一样引入。

工程拥有的一切都在 `<项目>/Assets` 下，每个资产文件旁边有一个 `<文件>.meta` 侧车，
记录它的 UUID（纹理还记录导入设置）。侧车是资产引用稳定的原因：组件与材质存的是 UUID，
改名或移动文件只改变数据库解析路径。两种文件都会被打进 `Data.hpak`，因此导出后的游戏
解析到的引用与编辑器里完全一致。

## 当前范围说明

以下内容已预留架构位置，但暂未实现：

- 音频系统
- 除 Lua 外的更多脚本语言（后端抽象已预留）
- Windows 以外的导出平台（数据模型预留了其他平台，当前实现 Windows_x64）
- 除 OpenGL 4.6 Core 外的渲染后端
- 外部 3D 模型导入（当前使用内置网格）
- 日志文件输出（当前仅控制台）
- 物理关节 / 角色控制器 / Mesh、HeightField、Compound 碰撞体 / 每实体多碰撞体 / 物理材质资产
- 除 albedo 外的材质贴图通道（法线 / 自发光 / AO / 粗糙度 / 金属度）与 Alpha 模式；材质资产目前只有 albedo 通道
- 编辑器之外的资产改名追踪：手工删除 `.meta` 侧车后会以新身份重建，指向它的引用随之失效

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
- [doctest](https://github.com/doctest/doctest) — 无窗口 `Tests` 套件使用的测试框架
- [Inter](https://github.com/rsms/inter) — 编辑器字体，SIL Open Font License 1.1 许可
- [CMake](https://cmake.org/) — 构建系统

各第三方库的许可证见其对应 `Vendor` 目录下的 `LICENSE` 文件。
