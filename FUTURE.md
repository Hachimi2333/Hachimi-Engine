# 后续画面效果与系统规划

本文件记录当前阶段未实现、计划在后续阶段完成的画面效果与引擎系统。

## 本次未纳入（用户选择延后）

- **Bloom 泛光**
  - 基于现有 HDR 后处理管线实现亮部提取、降采样模糊与叠加。
- **法线贴图 / 自发光 / AO 贴图**
  - 需要扩展顶点切线数据、Material 贴图通道、纹理导入与序列化。
- **MSAA / FXAA**
  - 为 HDR 场景 Framebuffer 增加多重采样，或实现后处理 FXAA。

## 相关延伸

- **点光源阴影映射**
  - 当前只实现了方向光 Shadow Mapping；点光源阴影需要 cube depth map 与 6 面阴影 pass。
- **天空盒自定义贴图导入**
  - 当前使用内置程序化天空；后续可支持 6 面图 / equirectangular 贴图导入并接入场景环境设置。

## 物理系统后续规划

- **Joints 关节**
  - Box3D 支持 revolute、prismatic、distance、spherical、weld、wheel、motor 等关节。
  - 需要补充 ECS Joint 组件、Inspector 编辑、`.hscene` 序列化，以及创建/销毁时的 Box3D 同步。
- **Character Mover 角色移动**
  - 接入 Box3D 的 `b3World_CastMover` / `b3World_CollideMover` 与平面求解，提供带爬坡、台阶与防穿墙的角色控制器。
- **Mesh / HeightField / Compound 碰撞体**
  - Box3D 已提供三角形网格、高度场和复合碰撞体 API；后续结合未来模型导入与地形系统暴露为 ColliderComponent 类型。
- **每实体多碰撞体 / 动态父子刚体**
  - 当前每个实体只支持一个 Collider；后续支持 shape 列表。
  - 当前动态父子实体会作为独立 body 模拟；后续实现父子合并为同一刚体或 compound body。
- **Box3D 多线程任务系统**
  - 当前 `workerCount` 保持单线程；后续接入引擎任务系统并实现 `b3EnqueueTaskCallback` / `b3FinishTaskCallback`。
- **物理 Debug Draw**
  - 通过 `b3World_Draw` 与 `b3DebugDraw` 在编辑器视口叠加显示碰撞体、质心、接触点与关节。
- **碰撞 / 传感器事件**
  - 当前只同步 Transform；后续将 `b3ContactEvents` 与 `b3SensorEvents` 转换为 ECS 事件或脚本回调，支持触发器和游戏逻辑。
- **物理查询接口**
  - 后续封装 RayCast、ShapeCast、Overlap 查询，供鼠标拾取、角色移动、技能判定等使用。
- **物理场景设置 UI**
  - Gravity、FixedTimeStep、SubStepCount 已序列化，但尚未提供 Inspector 面板；后续在编辑器增加场景级 Physics 设置界面。
- **物理材质与碰撞层级管理**
  - 当前摩擦/恢复等参数内联在 ColliderComponent；后续可抽象为 Material 资源与更友好的碰撞层配置。
