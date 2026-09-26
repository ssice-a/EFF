# 资源替换重构设计

状态：v1.0.0 是功能参照；四槽样本在大世界手动 50 次 F10 无躺地，旧 UI F10 闪退和 NPC 材质异常已记录，最新 NPC 反馈正常。本轮普通候选尚未再次三端实机验收。Skeleton、Physics 和碰撞体属于后续阶段。见[当前装配状态](unified-assembly-status-20260926.md)。


## 1. 目标

EIEM 需要支持：

- 替换或增加 Mesh、材质、贴图和材质参数；
- 同一 Mod 的世界、角色 UI、NPC 实例共享规则和按键状态；
- 由游戏管理实例、动画、蒙皮、LOD 和释放；
- 按键只改变目标部件显隐；
- F10 更新已存在实例使用的资源；
- 后续接入新增 Skeleton、Physics、碰撞体和物理参数；
- Blender 导出与 DLL 契约一致。

资源规则按原资源身份命中，不硬编码角色、PFB、场景或某个实例。不同 PFB 可以拥有不同 owner 和 Transform 层级，只要最终消费同一原 Mesh 身份，就使用同一 Mod 规则。

## 2. 已验证事实

### 2.1 三类实例有不同入口

| 场景 | 已验证边界 | DLL 职责 |
|---|---|---|
| 大世界 | `BaseModelViewPart`、`EntityRenderHelper`、`RendererInfo` | 在游戏装配边界识别并应用规则 |
| 角色 UI | `UIModelLoader`、`CharUIModelMono`、`RendererInfo` | 注册 UI owner，并复用相同资源规则 |
| NPC | `CreateSMSGO`、`CreateSMSInfoForPostModel`、`AssignSkin`、`StartNPC` | 原生 skin 完成后注册并应用相同资源规则，v1.0.0 已实机通过 |

三端需要轻量 owner 适配器，但不能各自实现一套 Mesh、材质或状态语义。

### 2.2 已观察到的静态替换链

当前 Typhoea 包实际启用三条 Render：

- `RenderS_actor_typhoea_body_01_lod0_0`
- `RenderS_actor_typhoea_cloth_01_lod0_2`
- `RenderS_actor_typhoea_cloth_02_lod0_3`

日志确认三条规则在同类实例中都经过：

```text
set_sharedMesh
  -> MOD-SKIN
  -> 材质提交
  -> RendererInfo._Init
  -> NPC AssignSkin（NPC 链）
  -> 规则再应用
  -> LOD 登记
```

大世界 50 次 F10 的三条目标规则均被创建和读取，`native Mesh +0x1C8` 写后均为 4；这不能证明三端装配稳定。另一轮 UI F10 闪退，NPC 显示异常，需按场景继续验收。

### 2.3 既有骨骼槽位按源 Mesh 身份继承

Unity 蒙皮使用 Mesh 内的骨骼索引、Renderer 的 `bones[]` 数组和 bindpose，不使用 Blender 顶点组名称在运行时查找 Transform。

Blender 导出器必须为 EIEMESH v6 的每个局部槽保存来源 Mesh 身份、原始槽号及可用供体候选。DLL 遵守以下规则：

1. 修改任何 Renderer 前，快照当前模型实例内所有原生 SkinnedMeshRenderer 的 Mesh 身份和原始 `bones[]`。
2. replacement 的每个槽按“源 Mesh 身份 + 原始槽号”读取当前实例 Transform，不按目标 Renderer 的同序号猜测。
3. 世界、UI、NPC 各自解析自己的实例 Transform，不跨模型实例共享数组。
4. 完整 v6 供体表不依赖 Transform 名称或层级；旧格式回退已删除。
5. 缺失、越界或歧义时拒绝替换，不为角色、PFB 或骨骼名称增加特殊映射。

这允许修改后的 Mesh 使用同一原生模型中其他源 Mesh 已装配的骨骼，也覆盖不同 PFB 对同一骨骼改名的情况。完整契约见[共享骨架绑定](shared-skeleton-binding.md)。

### 2.4 合并 Mesh 取代 Partner

一个源 Mesh 命中多个作者部件时，默认导出为一个合并 Mesh：

- 作者部件成为 submesh；
- 每个 submesh 保留对应材质槽；
- 共享一个 Renderer、骨骼数组、LOD 和 owner 生命周期；
- 不为静态 Mesh 替换创建兄弟 Partner。

额外 Renderer 只有在游戏原生描述和工厂能创建、登记和释放它时才能引入。

## 3. 当前实现边界

### 3.1 静态资源

当前生产路径在游戏已经创建 Renderer、尚处于原生装配回调的边界应用 Mesh 和材质。它不是理想化的全局 AssetBundle 缓存替换，但已证明世界、UI 和 NPC 能得到一致结果，并继续由游戏的 skin、Animator、LOD 和 owner 生命周期管理。

Mesh-only 规则不得修改 Animator 数据。源骨骼槽位按 2.3 的 v6 供体记录处理，不能按名称重建整个数组，也不能借用另一个模型实例的 Transform。

材质和贴图按声明的材质槽提交。未声明槽位保留游戏值；不能直接污染游戏或其他 Mod 共用的材质对象。

### 3.2 生命周期

DLL 只登记游戏创建的实例，并在游戏 owner 释放边界忘记登记：

- Prefab/UI/BaseModel/NPC owner 负责实例生灭；
- LOD 可见性继续由游戏决定；
- NPC 必须保留原生 `AssignSkin` 完成后的应用边界；
- 静态规则不创建或销毁 Partner、Animator、BoneCloth、Team 或碰撞体；
- 不使用逐帧轮询维护 Physics 或 Renderer 候选。

### 3.3 诊断 Hook

高频资源身份、PFB 扫描和装配探针默认关闭。它们只能在有明确问题、日志预算和退出条件的证据构建中启用。

生产基线只保留完成实际装配和 owner 注册/释放所需的 Hook。Hook 名称包含 `Trace` 不代表它一定是只读，审查以是否改变参数、返回值、游戏对象或插件登记为准。

## 4. 按键显隐

按键状态属于单个 Mod，同一 Mod 的所有实例共享状态。后续实现必须遵循：

```text
按键事件
  -> 更新 Mod 状态
  -> 计算目标 submesh 可见性
  -> 提交已验证的可见性输入
```

按键路径禁止：

- 重新读取与重建源资源代际；
- 改写 `bones`、`rootBone` 或 Animator；
- 创建、销毁或重新注册 Renderer/Partner；
- 创建、销毁或重算 Physics；
-触发 F10 资源代际。

首选游戏原生 submesh/部件可见性输入。当前验证路径在同一个已装配 Mesh 上原地更新目标 submesh 的索引：隐藏时写入空索引，显示时从资源代际缓存恢复原始索引。按键不更换 `sharedMesh`，顶点、骨骼槽位、bindpose、材质槽、LOD、`bones[]`、`rootBone`、Animator 和 Physics 均保持不变。

DLL 已将 Mod 按键输入泵与旧动画、GUI、相机和更新线程分离，并通过最小窗口过程把事件送到 Unity 线程。submesh 更新在 Physics 生命周期边界前返回，不创建或销毁 Partner。v1.0.0 已完成世界、角色 UI、NPC 三端实机验收。

## 5. F10 热重载

F10 是一次完整、可回滚的 Mod 资源事务：

```text
读取并校验新 Mod
  -> 构建不可变的新资源代际
  -> 发布新代际
  -> 通过已验证的游戏装配边界更新现有实例
  -> 新实例只消费新代际
  -> 游戏释放引用后退休旧代际
```

所有资源类型共享同一个代际，不为 Mesh、材质、贴图和 Physics 各写一套生命周期。解析或构建失败时保留旧代际，不能让一半实例使用新资源。

F10 不得把“恢复所有旧覆盖、逐 Renderer 重放、重建 Partner/Physics”作为最终实现。若没有可靠的现有实例刷新完成点，先保留待应用状态，让后续原生装配消费新代际。

当前已完成第一层事务保护：F10 在任何 Renderer、Skeleton 或 Physics 恢复前解析完整候选配置；任一已存在的 mod.ini 无效时拒绝整个候选，保留当前代际和已有实例。宿主测试已覆盖该回滚条件。

有效候选目前仍使用现有实例恢复/回放路径，因此尚未达到本节定义的最终原生刷新和旧代际退休方案。下一步根据实机 Mesh/Material 热重载结果，继续把全量恢复缩小为按变更资源提交。

## 6. Skeleton、Physics 和碰撞体

这部分延期到静态资源、按键和 F10 稳定之后。目标仍是修改游戏系统的输入，由游戏原生工厂创建、计算和释放：

```text
作者资源描述
  -> 游戏原生 Skeleton / BoneCloth / Collider / Team 工厂
  -> Animator 写入
  -> Physics 计算
  -> Renderer 蒙皮
```

进入实现前必须分别证明世界、角色 UI、NPC 是否存在实时 Physics、消费哪类描述对象、在哪个完成边界可以登记，以及如何随 owner 释放。UI 没有实时 Physics 时不能由 DLL 擅自补一套独立模拟。

## 7. 分阶段计划

| 阶段 | 内容 | 当前状态 | 通过条件 |
|---|---|---|---|
| 0 | 清理研究 Hook 和日志 | 部分完成 | 诊断探针默认关闭；仍有隔离的旧 Partner 与探针代码待清理 |
| 1 | 静态 Mesh/Material/Texture | 三端待回归 | 三条目标 Render、三端实例使用相同规则且稳定 |
| 2 | 按键显隐 | 历史版本已实机通过，当前候选待回归 | Mesh、Renderer 和 skin/Physics 指针不变，只更新目标 submesh 索引 |
| 3 | F10 统一资源代际 | 事务预检与同一执行器回放已实现；最终原生资源重建未实现 | 已有实例一致更新，失败可回滚，旧代际可退休 |
| 4 | 材质参数热更新 | 待验收 | 只影响声明槽位，不污染共享对象 |
| 5 | Skeleton/Physics/碰撞体 | 待取证 | 三端原生消费入口和释放边界均有证据 |
| 6 | Blender 完整适配 | 部分完成 | 导出格式与各已验收 DLL 契约一致 |

## 8. 验证矩阵

| 场景 | 操作 | 必须保持 | 必须变化 |
|---|---|---|---|
| 冷启动 | 进入世界、UI、NPC | 原生 owner、Animator、LOD 和释放链 | 命中实例使用 Mod 静态资源 |
| 多实例 | 世界、UI、NPC 同时存在 | 每个实例自己的 owner/LOD | 同一 Mod 规则和状态 |
| 按键 | 连续切换 | Mesh、Renderer、bones、rootBone、Animator、Physics | 目标 submesh 的索引和可见性 |
| F10 材质 | 修改材质/贴图 | Mesh、bones、Physics | 声明的材质和贴图 |
| F10 Mesh | 修改兼容 Mesh | owner 和原生装配顺序 | Mesh 及其原生装配结果 |
| F10 失败 | 提供无效资源 | 全部实例继续使用旧代际 | 错误状态和日志 |
| 卸载/传送 | 离开并返回场景 | 无残留引用、无周期任务 | 新实例消费当前代际 |

## 9. 禁止事项

- 不把低层 `sharedMesh` setter 当作唯一装配入口或通用热重载方案。
- 不按 Transform 名称重建源 Renderer 的整个骨骼数组。
- 不用独立 Partner 作为静态多部件替换的默认实现。
- 不由 DLL 创建独立 BoneCloth、Team 或碰撞体补齐未知原生链。
- 不使用逐帧轮询维护资源、Renderer 或 Physics 候选。
- 不用全局场景扫描代替 owner 生命周期。
- 不把日志、构建成功或历史版本号写成实机能力证明。
- 不把按键状态变化升级为 F10 资源代际事务。
