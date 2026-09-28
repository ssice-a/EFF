# 共享骨架与 Mesh 蒙皮绑定

这份文档定义当前 EIEMESH v6 的运行时契约。世界角色、角色 UI 和 NPC 使用同一套绑定逻辑；每个角色实例分别建立自己的骨骼表，Transform 指针不会跨实例共享。

## 1. 唯一的运行时身份

一个 Skinned Mesh 的每个局部骨骼槽同时对应顶点权重、bind pose 和一个运行时 Transform。EIEM 不再用 Blender 骨骼名称、PFB 名称、Renderer 的 `bones[]` 局部序号或 LOD 号猜测这个 Transform。

Blender 在导出时为每个槽写入 `boneIndexPaths`：这是相对于导出 Rig 根节点的斜杠分隔子节点索引，例如 `0/1/0`。同一层级结构在不同 PFB 中改名不会改变这个身份。

DLL 在当前 Renderer 所属实例内建立一次索引表：

```text
Renderer.rootBone（优先）或 skinningRoot
  ""       -> 根 Transform
  "0"      -> 第一个子 Transform
  "0/1/0"  -> 继续按子节点索引遍历
```

表只在本次模型装配事务中缓存，按根节点隔离；世界、UI、NPC 和两个同时存在的角色不会借用彼此的 Transform。LOD1/2/3 复用同一实例表，不再从低 LOD 的局部槽位重新推断。

## 2. EIEMESH v6 数据

`bonePaths`、哈希以及 `boneSources`/`boneSourceCandidates` 是作者侧的来源信息，用于 Blender 往返和诊断。它们不参与 DLL 的骨骼选择。

对有蒙皮的 Mesh，`boneIndexPaths` 必须和 bind pose 一一对应、非空且不能重复。缺少它、路径格式非法、路径在当前实例不存在或 Transform 已失效时，DLL 拒绝本次替换并保留源 Mesh；不会回退到名称、LOD 槽号或另一个角色。

来源候选仍写入 v6 文件以保留作者信息。某个槽没有原生候选是合法的，这为将来的 Mod-owned Skeleton 骨骼保留接口；当前 DLL 仍要求该槽的结构索引路径能在原生或已装配的 Skeleton 层级中解析。

## 3. Skeleton 与未来新增骨骼

EIESKEL v2 的节点使用同样的层级索引顺序。现有源节点锚定到游戏骨架，Mod-owned 节点由 Skeleton 实例创建并加入同一张索引表。Mesh 可以引用这些节点，即使它们没有任何原生 Mesh 供体槽。

目前只提供节点创建、绑定和生命周期接口，不实现新的物理求解或游戏物理注册。后续物理模块应通过 Skeleton 实例取得节点，而不是自行寻找场景 Transform。

## 4. Blender 导出约束

- 旧项目必须用当前 AnimeStudio 重新导入源资源，再用当前 Blender 插件重新导出；旧 Mesh 不再走候选供体回退。
- 导出器保持权重、bind pose、`bonePaths`、`boneIndexPaths` 的槽顺序一致。
- 合并 Mesh 只有在各部件的结构索引和 bind pose 一致时才允许合并。
- 新建骨骼可以没有原生供体，但必须属于共享 Rig，并由 Skeleton 资源一并导出。

## 5. 回归边界

自动测试覆盖：不同 PFB 骨骼改名、两个实例隔离、rootBone 优先与 skinningRoot 回退、不同 LOD 共用完整表、缺失/重复路径拒绝，以及 Skeleton 新节点绑定。游戏内仍需分别验证冷启动、F10、世界、UI 和 NPC 场景。