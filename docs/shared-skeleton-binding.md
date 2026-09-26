# 共享骨架与 Mesh 局部骨骼槽

本文定义当前静态 Mesh 替换的蒙皮契约。现行格式为 EIEMESH v6；世界、角色 UI 和 NPC 使用同一套规则，但每个模型实例分别绑定自己的 Transform。

## 1. 基本模型

Unity 蒙皮不在运行时用 Blender 顶点组名称寻找骨骼。一个 Skinned Mesh 的局部槽 `i` 同时连接：

- 顶点权重中的 bone index `i`；
- Mesh 的 bind pose `i`；
- 当前 Renderer 的 `bones[i]` Transform。

因此，同一个 Mesh 资源可以被不同 PFB 实例使用。各实例的 `bones[]` 是实例数据，不能跨世界、UI、NPC 或两个同屏角色共享。

骨骼显示名和 Transform 路径不是既有槽位的权威身份。不同 PFB 可以把同一语义骨骼改名，只要游戏给原 Mesh 装配的槽位仍正确，EIEM 应复用游戏已经选好的 Transform。

## 2. EIEMESH v6 槽来源

v6 为每个局部骨骼槽保存以下数据：

| 数据 | 用途 |
|---|---|
| bind pose | 保持该 Mesh 的绑定空间 |
| 作者骨骼路径与哈希 | Blender 编辑、诊断和旧格式回退 |
| 层级索引路径 | v4 回退信息，不是 v6 的首选身份 |
| 源 Mesh 逻辑路径 | 定位当前模型实例内的原生 Renderer |
| 源 Mesh asset 名 | 路径不可用时的资源身份 |
| 源 Mesh 原始槽号 | 从该原生 Renderer 的 `bones[]` 取 Transform |
| 源 Mesh/槽候选列表 | 当前模型只存在某些 LOD 或 PFB 时，逐候选寻找仍存在的原生供体 |

权威映射是“源 Mesh 身份 + 原始槽号”的候选列表，不是角色名、PFB 名、Renderer 名、骨骼名或目标 LOD 的同序号槽。

例如作者 Mesh 的某个槽来自源 Mesh A 的槽 17，也可以同时记录源 Mesh B 的槽 4。NPC PFB 即使缺少 A，DLL 仍会在当前 NPC 实例内尝试 B；当前实例中存在的候选必须解析到同一个 Transform，否则绑定失败。世界实例和 UI 实例分别执行同样的解析，不会借用 NPC 的 Transform。

## 3. Blender 导入与导出

导入 v6 Mesh 时，插件把每槽候选保存在 `eiem_bone_source_candidates_json`。重新导出必须保持以下数组逐槽对齐：

```text
weights index
  <-> bind pose
  <-> bone path/hash
  <-> hierarchy index path
  <-> source Mesh/path/slot candidates
```

删除几何或零权重槽不能压缩原槽表。合并多个作者部件时，导出器建立一个联合局部表，并把每个部件的顶点权重重映射到联合槽；同一个联合槽若声明了互相冲突的源记录，导出必须报错，不能任选一条继续。

当前静态阶段只保证复用游戏原骨架中已有的槽。导出器扫描同一 Blender Armature 下所有原生 Mesh 供体；新增骨骼若没有任何供体槽会直接报错，不能伪造一个现有 Mesh 槽号。

旧 EIEMESH v2-v5 不再读取；历史项目必须用当前 AnimeStudio 重新导出源资源，再用当前 Blender 插件导出为 v6。

## 4. DLL 解析流程

每次对一个模型根应用规则时，DLL 在修改任何 Renderer 前完成一次模型内快照：

1. 枚举该模型根下的原生 SkinnedMeshRenderer，包括未激活 LOD。
2. 记录每个原生 Mesh 的逻辑路径、asset 名和原始 `bones[]`。
3. F10 或重复生命周期调用若已替换 Renderer，则从 override 记录读取 `originalBonesHandle`，不会把 replacement palette 当作源表。
4. 对 v6 replacement 的每个槽，按候选源 Mesh 身份找到当前模型实例内的供体，再读取记录的原始槽号；只有候选都缺失才失败。
5. 全部槽准备完成后，一次性提交 replacement Mesh 和新的 `bones[]`。

完整 v6 候选表可以在不读取任何骨骼名称和层级的情况下完成绑定。运行时没有旧版名称路径或层级索引回退。

以下情况明确失败并保留源 Mesh：

- 当前模型实例内不存在任何声明的源 Mesh 候选；
- 原始槽号越界；
- 同一源身份在模型内命中不同 Transform，结果有歧义；
- Transform 已失效；
- 各逐槽数组数量不一致；
- Mesh 或 `bones[]` 提交失败。

解析只使用当前模型根的快照，不做全场景骨骼搜索，不缓存实例 Transform 到资源对象，也不按角色或 PFB 建立特殊映射。

## 5. 换角色时的成立条件

该方案没有 Typhoea、裙骨或特定 Renderer 的功能性硬编码。换成其他角色时，只要满足以下条件，PFB 中的骨骼改名不会改变结果：

- Blender 从当前插件导入并重新导出 EIEMESH v6；
- 目标实例仍装配导出记录所引用的源 Mesh；
- 源 Mesh 的局部槽语义未被游戏资源版本改写；
- 同一模型根内的源 Mesh 身份足以唯一确定 Transform；
- replacement 的 bind pose 与其局部槽对应。

若游戏版本把某个源 Mesh 的槽表本身重排，v6 文件也必须重新导入、重新导出。来源记录解决的是跨 PFB 命名和层级差异，不是跨游戏版本猜测槽位变化。

## 6. 验证

合同测试必须覆盖：

- 全部骨骼名称均不匹配时，v6 候选来源表仍能解析；
- 两个模型实例使用同一 Mesh 资源时，各自得到自己的 Transform；
- 两个源 Mesh 都使用槽 0 时，合并 Mesh 仍按源 Mesh 身份得到不同骨骼；
- 歧义、缺失和越界均失败，不退回目标 Renderer 的同序号槽；
- 当前 resolver 不包含角色、部件或骨骼名称常量。

游戏内验收仍需分别覆盖世界、角色 UI、NPC、LOD 切换和 F10。构建通过或日志显示赋值成功不能替代画面验收。
