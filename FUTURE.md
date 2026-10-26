# 后续画面效果与系统规划

本文件记录当前阶段未实现、计划在后续阶段完成的画面效果与引擎系统。

## 本次未纳入（用户选择延后）

- **Bloom 泛光**
  - 基于现有 HDR 后处理管线实现亮部提取、降采样模糊与叠加。
- **法线贴图 / 自发光 / AO / 粗糙度 / 金属度贴图**
  - 材质资产（`.hmaterial`）与纹理导入设置（色彩空间 / wrap / filter）已就位，当前只有 albedo 通道；
    后续需要扩展顶点切线数据与材质通道，并接入 `TextureType::Normal` / `Data`。
- **Alpha 模式（Opaque / Cutout / Blend）与透明通道排序**
  - 需要在材质资产中增加 alpha 模式，并新增一个后向前的透明 pass。
- **MSAA / FXAA**
  - 为 HDR 场景 Framebuffer 增加多重采样，或实现后处理 FXAA。

## 资产管线后续规划

资产身份、`.meta` 侧车、材质资产与编辑器文件管理已实现。以下为后续延伸：

- **编辑器之外的改名追踪**
  - 当前 `.meta` 记录与文件同名，手工改名后会被当作新资产；后续可引入中央清单或文件 ID 表。
- **引用修复与查找引用面板**
  - `AssetDatabase::FindReferences` 已在删除时列出引用者；后续可做成可跳转的面板并在改名后自动改写引用。
- **导入设置扩展**
  - 纹理目前已支持色彩空间 / mipmap / wrap / filter；后续按各向异性、压缩格式与模型导入扩展。
- **资产依赖追踪与缓存淘汰**
  - 纹理缓存已有按资产版本的失效机制；后续可扩展为按依赖图淘汰与异步预加载。

## 相关延伸

- **点光源阴影映射**
  - 当前只实现了方向光 Shadow Mapping；点光源阴影需要 cube depth map 与 6 面阴影 pass。
- **天空盒自定义贴图导入**
  - 当前使用内置程序化天空；后续可支持 6 面图 / equirectangular 贴图导入并接入场景环境设置。

## 脚本系统后续规划

M3 已实现字段、热重载、安全 require、延迟场景命令、刚体运动接口和物理事件回调。

- **更多语言后端**：实现 ScriptEngine / ScriptRuntime 的同一生命周期约定并注册扩展名。
- **脚本调试支持**：断点、变量查看与 Console 内跳转源码；当前已输出错误文件与行号。

## 独立里程碑：关节与角色控制器

这两项从 M3 整体移出，待物理运行时稳定后独立设计与验证。

- **Joints 关节**：设计 JointComponent，配套 Inspector、撤销和 `.hscene` 序列化；
  经 ComponentRegistry 一次注册，并补齐实体/刚体/关节创建销毁同步与失效引用测试。
  类型覆盖 revolute、prismatic、distance、spherical、weld、wheel、motor。
- **Character Mover 角色控制器**：接入 `b3World_CastMover` / `b3World_CollideMover` /
  `b3SolvePlanes`，暴露爬坡角、台阶高度、接地与防穿墙参数，验证斜坡、墙角和移动平台。

## 物理系统后续规划

M3 已实现查询、碰撞/传感器事件、JobSystem 多线程、调试绘制与场景设置面板。

- **Mesh / HeightField / Compound 碰撞体**：等待模型导入和地形系统，再决定形状资产布局。
- **每实体多碰撞体**：当前每实体一个 Collider，后续扩展 shape 列表与独立局部变换。
- **动态父子刚体合并**：当前独立 body 模拟；合并需先确定 compound 的质量与层级语义。
- **物理材质资源与碰撞层编辑**：当前参数内联在 Collider，资源化需配套资产与 Inspector 设计。

## M4 渲染范围

点光源阴影、Bloom、MSAA/FXAA 继续延后，优先稳定 M3 的模拟、脚本和统一运行时。
旋转插值目前使用欧拉角线性混合，四元数 Slerp 留给后续数学与动画工作。
