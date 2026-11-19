# Physics 作者资源 v5

当前保留的 Blender 作者编解码与 C++ reader 格式是 `EFFPHYS` v5，purpose 为 `authoring`。旧作者 v1、v3、v4 不再读取，须重新导出。`EFFPHYS` v2 的 purpose 是原生源图，属于另一种文档类型；不能把它当作作者 v5 的旧版。

作者文档引用同包内的 EFFSKEL v2，保存组、节点、球体或胶囊碰撞体、基础参数、节点半径曲线以及有限的 `serializeData` 原生参数。拓扑、运行结果字段与无效骨骼引用在导出或解析时拒绝。胶囊的 `span` 表示两个端球心间距；原生长度是 `span + radius + endRadius`。

Blender 的 `eiem_physics_document.py` 负责作者资源的校验与编码，`src/eiem_physics_document.h` 提供 C++ 读取器，AnimeStudio 的 `EiemPackageValidator.cs` 检查资源包结构。Format 3 按需读取 INI 声明的 Physics 资源，在目标 Mesh 的消费者 Prefab 内装配原生 `BeyondBoneCloth`、骨骼和碰撞体。游戏负责模拟及组件生命周期。

角色动画控制器通过 `SetClothSimulateWeight` 更新模拟权重。正式 DLL 使用与 Prefab 装配共用的组件命名规则识别 EFF 新增的布料组。这些组使用 Physics 资源中的 `serializeData.clothSimulateWeight`，原生布料接收游戏传入的权重。过期的缓存记录按弱引用回收。

LVT 的五个自定义组已在实机日志中命中这项策略，资源配置值为 1。最终物理姿态还受 `blendWeight`、速度和距离衰减等影响。详细计算见 [Physics 权重和动画分析](physics-ui-rework-analysis.md#116-动画影响量与最终位置混合2026-10-11只读)。

后续游戏接入要求见[三端设计契约](physics-authoring-design.md)，当前范围见[文档索引](README.md)。
