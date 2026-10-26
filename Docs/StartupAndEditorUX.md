# 启动分析与编辑器 UX 调整

测试日期：2026-10-26。机器：AMD Ryzen 9 9955HX、NVIDIA GeForce RTX 5060 Laptop GPU，Windows x64，MSVC 14.51.36231。

## 原因与处理

旧启动顺序是创建可见窗口，随后在主线程完成渲染资源初始化，最后才进入事件轮询和首帧渲染。
固定天空环境每次都要重新执行漫反射卷积和 GGX 预过滤：

- 漫反射：6 × 32 × 32 × 512 = 3,145,728 次采样。
- 非零粗糙度预过滤：6 × (64² + 32² + 16²) × 128 = 4,128,768 次采样。
- 合计 7,274,496 次，还不包括天空盒和镜面层计算。

Debug 的 `/Od /Ob0 /RTC1` 显著放大了这段包含向量运算、归一化和三角函数的 CPU 计算。
稳定测量中，项目中心环境光生成中位数为 4,177.86 ms，占首帧中位数的约 93%；字体和资产扫描都不是主要瓶颈。

现在由 CPU 工具 `BakeEnvironment` 在构建阶段生成 `Environment/Default.henv`，运行时只通过 VFS 读取并上传到 GPU。
生成算法、采样数、分辨率和 RGBA32F 精度均沿用原实现。资源由唯一的 `copy_runtime_resources` 发布，并随游戏进入 `Data.hpak`。
烘焙工具只链接生成所需的代码和依赖，普通引擎改动不会因重新链接整个引擎而触发烘焙。

Debug 直接打开项目的环境光读取与上传中位数为 11.86 ms；包内 Player 为 37.98 ms。
Player 关闭了 ImGui、字体和停靠初始化。窗口在 OpenGL 初始化完成后尽早提交背景帧。
Debug 编译选项和调试能力保持不变。

## 首帧实测

| 构建 | 启动路径 | 优化前首帧 | 优化后首帧 | 耗时下降 |
|---|---|---:|---:|---:|
| Debug | 编辑器项目中心 | 4497.59 ms | 313.02 ms | 93.0% |
| Debug | 编辑器直接打开项目 | 4602.49 ms | 414.36 ms | 91.0% |
| Debug | 导出游戏（Player） | 4569.36 ms | 438.37 ms | 90.4% |
| Release | 编辑器项目中心 | 628.25 ms | 303.04 ms | 51.8% |
| Release | 编辑器直接打开项目 | 672.48 ms | 338.52 ms | 49.7% |
| Release | 导出游戏（Player） | 666.23 ms | 338.16 ms | 49.2% |

每项为顺序启动三次的中位数，构建与测试任务结束后采样，未附加调试器。
计时从入口处日志初始化后的 `StartupTimer::Begin` 到首个实际画面帧提交完成，不包含进程加载和日志初始化。
早期背景清屏不计为实际首帧。系统和驱动缓存未清空，因此这些数据不是严格冷启动基准。

编辑器和 Player 使用同一份测试项目：现有 NewProject 的 Assets 副本，仅在 `Build/StartupBenchmark` 内重写了项目路径。
游戏通过实际 `ProjectPackager` 导出。测试进程的 APPDATA 重定向到该目录，用户原有最近项目和偏好设置未改动。

原始证据保留在以下构建产物中：

- [三次采样明细](../Build/StartupBenchmark/measurements.json)。
- `Build/StartupBenchmark/baseline-{debug,release}-{hub,project,player}-{1,2,3}.txt`。
- `Build/StartupBenchmark/optimized-{debug,release}-{hub,project,player}-{1,2,3}.txt`。
- 包内容与哈希验证：`Build/StartupBenchmark/optimized-{debug,release}-verify.txt`。

复核命令（仓库根目录，路径替换为待测项目）：

```powershell
Build/x64-debug/bin/Hachimi-Editor.exe --startup-profile
Build/x64-debug/bin/Hachimi-Editor.exe --project <project.hproj> --startup-profile
Build/x64-debug/bin/EmitPackage.exe Build/Probe.hpak --project <project.hproj>
Build/x64-debug/bin/Hachimi-Player.exe Build/Probe.hpak --startup-profile
Build/x64-debug/bin/Hachimi-Player.exe Build/Probe.hpak --verify --list --stats
```

`--startup-profile` 会创建图形窗口、提交一帧后退出，只用于简单启动探测；它不是无窗口测试模式。
既有导出目录需要重新导出，使新的 Player 与含烘焙资源的 Data.hpak 一起更新。
项目和场景文件格式保持不变。

## 设置与工作流

- `Settings → Scene Settings` 集中渲染和物理配置；步长、子步、线程数以及运行统计默认折叠。渲染配置接入撤销、重做和脏标记。
- `Settings → Project Settings...` 集中游戏信息、窗口和导出设置，保留 `Build → Export Game...` 快捷入口。设置在草稿中编辑，保存失败会回滚内存配置；场景保存失败会停止导出。
- 固定工具栏替代独立停靠面板，显示当前工具和播放状态。控制台、包检查器和设置面板按需打开，`View` 管理面板与重置布局。
- 项目中心优先展示打开与最近项目，新建改为独立弹窗；资源浏览器合并新建入口。项目设置在打开时扫描场景，不在每帧遍历资源目录。
- 纹理设置草稿跨帧保留，提供 Apply/Revert。未应用资产编辑在切换选择、关闭、播放或离开工作流前提示处理；导出前需处理未应用资产编辑。
- 修复了关闭事件先结束主循环、使保存提示失效的顺序问题。

布局与面板状态保存到 `%APPDATA%/HachimiEngine/EditorSettings.yaml`，版本为 1；ImGui 布局以内存序列化内容存为 YAML 多行字符串。
最近项目继续使用同目录的 `HubSettings.yaml`。自动保存使用延迟写入和原子替换，退出前补存，损坏配置恢复默认布局。
ImGui 的 `IniFilename` 被设为 `nullptr`，没有中间 ini 文件，也没有项目目录回退路径。
本次已删除仓库根目录、Debug/bin 和 Release/bin 中的三处 `imgui.ini`，忽略规则保留。

## 验证

- Debug、Release 均完成 CMake preset 配置及干净重建，最终增量构建也成功。
- 两套 `Tests.exe` 各通过 308 个测试、12,962 条断言，退出码 0；所有测试保持无窗口、无 OpenGL 上下文。
- 新测试覆盖环境数据生成、无损序列化、截断/错误头/非有限值、VFS 包读取、YAML 往返与损坏恢复、设置撤销、项目保存失败回滚和纹理草稿保留。
- 两套真实导出包均包含 `Environment/Default.henv`，Player 的包哈希验证全部通过。
- 增量构建仅复制运行资源，环境资源时间戳不变，没有重新烘焙。
- 缺失环境资源与损坏环境资源的启动探测均明确报错并退出 1；损坏个人 YAML 恢复默认，退出 0。
- 在个人 YAML 中启用控制台、包检查器、场景设置后启动成功，退出时可见性仍被保存。
- 清理后重复启动编辑器和 Player，仓库内仍无 `imgui.ini`。

未执行截图识别、像素点击或复杂 GUI 测试。最终面板布局、配置操作体验和渲染画面一致性仍需人工确认。
