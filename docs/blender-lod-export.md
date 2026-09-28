# Blender LOD 导出

Blender 导出器的 LOD 选项只作用于当前选中的 EFF Mesh。导出器从导入资源元数据读取 LOD，不根据 Blender 对象显示名称或手写文件名猜测游戏资源。

导出面板提供 `LOD0` 到 `LOD4` 复选框以及“全部已发现 LOD”。“全部”表示当前 `.blend` 中同一资源族已经导入的全部 LOD；未导入、未观察到的级别不会生成 Mesh、Render 或其他 INI 规则。

当选中一个 LOD Mesh 并勾选多个目标级别时，选中的 Mesh 是唯一的导出模板。导出器只写一份 Mesh 资源，并为每个已发现的目标级别生成独立、精确的 `asset=` Render 规则；这些规则共同引用同一个 Mesh section。导出过程不改写 `.blend` 中的 Mesh、材质、骨骼或切换状态。例如：

```ini
[RenderBody_lod0]
asset=S_actor_body_lod0
mesh=MeshBody_lod0

[RenderBody_lod1]
asset=S_actor_body_lod1
mesh=MeshBody_lod0
```

按键切换变量只声明一次。每个 LOD 的 Render 规则复用同一个变量和相同的 submesh 条件，因此世界、NPC、UI 实例以及不同 LOD 共享同一 Mod 状态。导出器不会创建 `asset=*lod*` 通配规则，也不会新增 DLL LOD Hook。

同一源资源的多个部件先按稳定顺序合并为一份 Mesh；所有目标 LOD 复用这份合并结果和同一套全局 submesh 槽位。模板的顶点权重、bindpose、骨骼路径及 EFFMESH v6“源 Mesh + 原始槽号候选”记录保持不变。目标 LOD 只负责命中游戏 Renderer，不会把 LOD0 权重索引重新解释成 LOD1/2/3 各自的原生 `bones[]` 顺序。

这一约束是蒙皮正确性的组成部分。游戏的各原生 LOD Renderer 可以拥有不同长度或不同顺序的局部骨骼 palette；复制同一几何后若把来源槽改成目标 LOD 的同序号槽，局部顶点会被错误骨骼拉伸。共享一份模板 Mesh 后，DLL 始终在当前世界、NPC 或 UI 实例内按模板记录的源槽取得 Transform。

如果显式勾选的目标级别在当前工程没有发现，导出会失败并提示没有可用目标，不会静默生成可能无法命中的规则。Mesh-only 导出使用相同的 LOD 选择逻辑，但仍跳过 Skeleton、Physics 和作者控制依赖。
