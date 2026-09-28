# Physics 运行时重构边界

状态：设计与取证阶段。本文不是当前生产实现的承诺，也不把离线编解码、宿主构建成功或 Blender 预览当作游戏内 Physics 已生效的证据。

状态更新（2026-09-18）：P0 已在 DLL 代码中落地。默认 kEiemEnableExperimentalPhysicsRuntime=false 与 kEiemEnableNativePhysicsObservation=false；Physics 作者资源仍可解析，但不会进入自建运行时、不会调用 AddComponent/BuildAndRun，也不会安装高噪声原生观察钩子。已通过 18 项 Physics runtime 契约、53 项资源管线契约，并完成 DLL 构建；本次未部署到游戏目录。

## 1. 目标

物理资源需要能够表达和修改：

- 原生物理组、根节点和分支；
- 原生碰撞体及其参数；
- 已验证可修改的物理参数；
- 后续可由游戏原生工厂消费的新增骨骼和物理链；
- F10 热重载和多实例共享的 Mod 状态。

世界、角色 UI 和 NPC 必须遵循各自的游戏 owner、Animator、Physics、LOD 和卸载生命周期。DLL 的职责是提供资源输入、记录证据和请求已验证的原生刷新；不能建立第二套物理系统。

## 2. 当前证据与问题

已经确认：

1. 大世界和部分 NPC 运行时存在 `BeyondBoneCloth`、`ClothProcess`、Team 与 Animator 关系。
2. UI 实例存在没有原生 Cloth 的记录，三端不是自动等价的物理实例。
3. Physics 声明独立于 Mesh 部件表，原生 PFB 中存在 `BoneClothItem`、根节点、忽略节点、选择数据和碰撞体引用。
4. 作者 `.physics` 格式和 Blender 编辑器可以离线读写 v5 数据。

当前 DLL 仍有一条研究性运行时路径：

```text
Render 命中
  -> EiemCollectPhysicsIntent
  -> EiemReconcileModelPhysics
  -> 自建 EFF_Physics_<generation> GameObject
  -> AddComponent(BeyondBoneCloth)
  -> 写入 ClothSerializeData / Collider
  -> BuildAndRun
  -> DLL 自己保存、退休和销毁对象
```

这条路径的问题不是某个参数默认值，而是所有权边界：它绕过了游戏创建 `BoneClothItem`、owner、Team 和释放栅栏的路径。`BuildAndRun=true` 只表示接受构建请求，不能证明模拟已经就绪；Unity 对象弱引用也不能替代原生任务完成通知。因此这条路径不能作为 Physics 生产架构继续扩展。

## 3. 运行时对象分层

今后所有设计都区分四层：

| 层 | 内容 | 所有者 |
|---|---|---|
| Authoring data | Blender 导出的组、节点、曲线、碰撞体和参数 | EFF 资源代际 |
| Source graph | PFB/Prefab 中的 `BoneClothItem`、骨骼引用、选择数据、碰撞引用 | 游戏资源加载器 |
| Runtime native object | `BeyondBoneCloth`、`ClothProcess`、Team、Collider、Job | 游戏原生 owner/工厂 |
| Instance state | 某个世界/UI/NPC 实例的 active、LOD、卸载和当前代际 | 游戏实例 owner |

作者数据不能直接冒充 Runtime native object。DLL 不保存或复用跨实例的 Transform、Team、Process 或 Job 指针作为资源身份。

## 4. 允许与禁止的操作

### 允许

- 读取原生物理组件和描述对象，建立世界/UI/NPC 证据矩阵；
- 在已经证明安全的字段上做单字段写入并读回；
- 通过游戏已经存在的资源描述或原生工厂输入替换 Physics 配置；
- 观察原生完成通知、owner 释放和新代际接管；
- 在原生刷新完成后登记新实例，不自行认领其生命周期。

### 禁止

- 为每个 Mesh 或 Renderer 创建一个物理 host；
- 从 `Render` 命中直接 `AddComponent(BeyondBoneCloth)`；
- DLL 自己调用 `BuildAndRun` 并把返回值当作 ready fence；
- 以 Mesh 显隐、LOD 变化或按键状态为理由销毁物理节点；
- 在没有原生释放栅栏时销毁旧 Physics、Collider、Transform 或 Process；
- 用 Team ID、对象地址或 Renderer 地址作为跨代际资源身份；
- 因为作者文件包含新骨骼就静默创建一套不属于游戏 owner 的骨架。

按键 submesh 显隐必须跳过 Physics。F10 也不能用“逐 Renderer 恢复旧覆盖并重放 Physics”作为最终协议。

## 5. 分阶段计划

### P0：冻结生产行为

- 保持 v1 Mesh/材质/贴图/submesh/F10 行为不变。
- 物理作者文件继续可离线读写，但不自动创建运行时对象。
- 将现有 Physics runtime hook 标记为实验路径，默认关闭其创建、`AddComponent`、`BuildAndRun`、退休和销毁分支。
- 只保留限流的原生 Physics 摘要日志。

通过条件：无 Physics 写入时 v1 行为不变；日志能明确区分 `native-observe-only` 和实验路径。

### P1：原生装配取证

对世界、NPC、UI 分别记录同一角色的：

```text
owner -> model -> Animator
BoneClothItem / BeyondBoneCloth 数量
rootBones / ignored roots / selectionData
sourceRenderers / skinning bones
Collider 引用与父节点
ClothProcess.Init / StartRuntimeBuild / BuildAndRun / Dispose
Team 注册、ready、释放
```

只读，不创建、不修改、不销毁。必须确认 Physics 是由同一类原生工厂装配，还是三端各有分支；UI 没有 Cloth 时要记录为能力差异，不能假定是日志缺失。

### P2：单字段原生写回

对一个已存在、由游戏创建的 Physics 组件，在一个端、一个实例、一个字段上做实验，例如 `blendWeight` 或 `animationPoseRatio`：

1. 读取原值；
2. 写入一个可逆值；
3. 通过原生通知接口；
4. 在下一次物理更新读取实际状态；
5. 恢复原值并确认 owner 仍拥有该组件。

若只读回成功但画面/物理行为不变，记录为“字段可写但尚未证明被消费”，不能进入配置替换。

### P3：原生配置替换

只替换已经在 P2 证明可消费的配置字段，复用游戏已有 `BeyondBoneCloth`、Collider、Process 和 Team。拓扑、骨骼数量、碰撞体数量变化一律拒绝热应用，等待下一次原生模型重建。

### P4：新增骨骼与新增 Physics 链

只有在 P1 证明原生工厂接收新增 Transform、rootBones、selection 和 Collider 引用后，才设计新增链。世界、NPC、UI 必须分别通过；不能因一个端成功就推广到另外两个端。

### P5：热重载

- 参数变更：仅在 P2/P3 允许的字段上原地写回；
- 资源或拓扑变更：发布新代际，等待游戏原生 owner 重装配；
- 无法找到原生刷新完成栅栏时，F10 标记“待下一次原生重建”，不创建第二套 Physics；
- 旧代际由游戏释放后才退休，DLL 只观察和清理自己的索引。

## 6. 证据等级与验收

每条结论标记为：

- `E0`：静态元数据或离线文件观察；
- `E1`：宿主/调用返回值验证；
- `E2`：运行时对象字段读回；
- `E3`：游戏内行为和生命周期验证；
- `E4`：世界、NPC、UI 三端重复验证。

“文件字段存在”“setter 读回成功”“BuildAndRun 返回 true”最高只能得到 E1/E2，不能宣称 Physics 已经生效。生产能力至少需要 E3；跨三端能力需要 E4。

## 7. 退出条件

在 P1/P2 未完成前，不修改 `src/eiem_native_physics_runtime.h` 的生产路径，不部署新的 Physics DLL。完成 P3 后再决定是否保留旧实验代码；若原生工厂无法接收新增拓扑，作者工具仍可作为离线编辑器，但运行时明确拒绝该能力。
