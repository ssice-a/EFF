# 特效生成与运行时装配路线

Status: implementation in progress. The native TypeTree codec and an author-owned, source-Prefab-free particle PFB workspace generator are implemented and offline roundtrip-tested. Logical game-resource linking and game runtime validation remain open.

## 现行链路

```text
游戏 PFB / Bundle
    -> AnimeStudio 导出完整 .efffx 来源基线
    -> Blender 显示层级、发射器、材质、Mesh 与预览
    -> .effedit.json 保存原生字段差异和作者资源
    -> AnimeStudio 重建受影响的 CAB，并写 UnityFS Bundle
    -> Format 3 保存私有 effect Bundle 与 EffectAttachment
    -> DLL 命中目标 Mesh 后加载 PFB、克隆并挂到 Renderer/Bone
```

- `.efffx` 是编辑/校验来源，不是游戏运行时 Mod。`source/manifest.json` 记录完整来源依赖及 CAB/stream 快照；AnimeStudio 用它保留未编辑对象和原生元数据。
- `.effedit.json` 表达来源对象的字段差异、发射器操作和作者 Mesh/Material/Texture additions。当前构建以已有 PFB 为基线；不是完整的零起点 Unity Prefab 作者格式。
- `EiemEffectBuildCommand` 重建含变更对象的 SerializedFile，再按来源 Bundle 生成 UnityFS 数据。`EiemEffectRuntimeWriter` 隔离被选中 CAB 的名字和 Bundle 身份；未修改的终端依赖继续由游戏原资源提供。被选中的 Bundle 组仍可能包含同 CAB 内的未修改对象。
- Format 3 的 `ResourceEffect` 保存 Unity 原生 Bundle 输入，`EffectAttachment` 保存目标 Prefab/Renderer/Mesh 证明和挂点参数。目标 Mesh 决定特效绑定到哪个运行实例，effect PFB 决定实例化的层级、粒子组件与资源引用。
- DLL 通过游戏 ResourceManager 保留来源 PFB/依赖，再用 Unity AssetBundle 加载器加载 Mod 的 PFB，克隆后挂到目标 Renderer Transform 或已验证的骨骼路径，并接入游戏特效生命周期。它不是替换目标 Mesh，也不是把特效挂载误当成错误路径。

Unity PFB 是带对象表、PPtr、类型数据和 Bundle 容器信息的序列化对象图。当前生成的 Bundle 是让 Unity 正常反序列化并实例化作者特效的载体，不等同于把所有游戏贴图和 Mesh 都复制进 Mod。需要重点审计的是被选中 CAB/Bundle 的内容范围、外部逻辑依赖和来源作者快照，而不是仅按 `.ab` 扩展名判断资源归属。

## 从零创建的边界

“从零”指作者不需要选取或继承一个游戏特效 PFB；作者应能创建自己的 Prefab 对象图、粒子组件与组件数据。它仍然需要生成 Unity 可读取的 SerializedFile 和 AssetBundle 容器记录，再将生成的 Prefab 放进 Format 3 的特效资源中。这个作者 Bundle 只承载新建对象和作者资源，不因此复制游戏已有的贴图、材质或 Mesh；游戏资源仍用逻辑 PPtr / 逻辑路径引用。

当前可从任一导出的 `.efffx` 选择游戏版本 TypeTree donor，由 AnimeStudio 只读取 schema、丢弃 donor 对象和依赖表，再生成独立的 GameObject、Transform、ParticleSystem、ParticleSystemRenderer、AssetBundle 容器和新的 SerializedFile。Blender 可导入生成的 workspace、编辑粒子参数并离线构建 native runtime 候选；构建产物仍标记 `gameVerified=false`。目前新建 Renderer 没有默认材质，逻辑路径到游戏资源的解析/外部引用接入仍未闭合，因此不能声称空白新建特效已能在游戏中可见。

## 设计目标

1. 目标仍是“命中逻辑 Mesh/模型实例 -> 装配作者特效 PFB”；世界与 UI 实例沿用同一份逻辑目标证明。
2. 作者新建或修改的 Prefab 对象、ParticleSystem/Renderer 数据、曲线和自有 Mesh/Material/Texture 由 Mod 管理。
3. 游戏已有资源通过逻辑路径和资产身份引用；不把游戏 Bundle 中的 Mesh、材质、贴图作为作者 additions 重复发布。
4. Unity 继续负责解析 Prefab 对象图和原生组件。运行时只在目标命中后按需加载特效输入，不扫描或常驻加载所有特效。
5. 能从零创建 effect PFB，并支持跨文件复制完整发射器表现及其资源引用；编辑块的语义与 Unity/游戏对象图一致。

## 建议路线

1. 先对现有真实 build 做归属审计：逐个列出生成的 Bundle、CAB、stream、对象数和字节数，标识被修改对象、同 CAB 未修改对象、作者 additions、仍由游戏解析的 external PPtr。用数据判断哪些复制是 Unity 原生容器要求，哪些只是当前打包范围过宽。
2. 固定作者/运行时契约：EffectAttachment 继续只负责逻辑 Mesh 命中与挂点；Effect runtime resource 负责可由 Unity 实例化的特效 PFB；作者资源 additions 与游戏逻辑引用分开声明。不同 Mod 的特效对象命名空间必须独立。
3. 收敛编译产物：优先让 UnityFS Bundle 只承载作者特效 PFB 必需的对象和自有资源。游戏资源保留可验证的逻辑 PPtr/external dependency，并由游戏 ResourceManager 提供。若序列化器必须重建某个 CAB，报告并限制同 CAB 未修改对象进入产物的范围。
4. 打通零起点对象图：Blender 创建稳定 ID 的根、子节点和受支持组件；AnimeStudio 使用当前游戏 TypeTree 初始化对象数据，为作者对象分配独立 PathID，补齐对象表和 Prefab 容器，再执行原生往返校验。先支持 GameObject、Transform、ParticleSystem、ParticleSystemRenderer 及可解析资源引用，不在第一步承诺未知脚本或任意 Unity 组件。
5. 保持当前按需运行时装配生命周期：目标 Mesh 未命中时不加载 effect Bundle；命中后异步准备、克隆、挂载和注册；F10 换代只替换该 owner 的实例，失败时保留旧实例。
6. 最后再缩小可编辑作者快照。当前完整 `.efffx` 依赖快照为离线重建提供基线，不能在没有来源解析器时直接删掉。需要先建立逻辑 Prefab + VFS 指纹到本地来源的解析/重建路径，或改为携带最小可编辑基线；运行时 Mod 与 Blender 作者回导分别验收。

## 验收边界

- **来源/构建：** 同一编辑重复构建结果确定；只改一个发射器时能报告对象级变更；资源引用仍指向游戏逻辑资源；作者 additions 可导回 Blender。
- **资源归属：** 产物清单明确区分作者拥有的字节与游戏拥有的逻辑引用；没有因为共享 CAB 而静默复制无关 Bundle 依赖。
- **装配：** 同一个命中 Mesh 的世界与 UI 实例都只在各自 owner 上实例化；重复 Mod 的资源身份隔离；卸载/F10 后 Bundle 与特效实例按生命周期释放。
- **编辑闭环：** 零起点创建、保存、发布、清空场景、导入、编辑、再发布；跨文件复制完整 emitter 表现并校验所有资源引用。
- **游戏验收：** 运行时效果、材质、粒子时间、目标实例、UI/大世界、重复加载和失败回退都需在真实游戏确认。离线 Blender 和 UnityFS 往返测试不代表游戏通过。

## 仍需验证

- 当前受影响 CAB 的实际 Bundle 字节占比及同 CAB 非作者对象数量。
- Unity 对作者 CAB 中逻辑 external PPtr 的加载约束，以及来源 ResourceManager lease 能否覆盖不同生命周期和 UI 场景。
- 完整从零创建所需的 TypeTree/Prefab container/对象表元数据，哪些能由现有模板安全克隆。
- emitter 复制跨来源时如何重映射对象身份，同时保持逻辑资源路径、材质槽、PPtr 类型和父子关系。
