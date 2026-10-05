# 完整渲染装配热重载

最新单项验收：同路径贴图正向更新已在 PID 39444 通过世界实例提交和用户视觉/动作检查；重载键改为小键盘减号。反向恢复、展示实例及完整装配仍待验证，见 [当前热重载测试](hot-reload-testing.md)。该结果没有改变下面的完整目标，也没有部署 v36 草稿。

2026-10-05。用户确认的完整目标和实施规则。功能尚未完成；安装目录仍为 v35c。同 Renderer 数量的重绑定草稿已撤出生产代码，保存在 diagnostics 下，没有部署。后续实现以本文为准，不先发布“仅贴图/同数量”的完成版。

当前主源码的离线清单 ABI 为 `EFF_NATIVE_PACK_V36_ASSEMBLY_1`，游戏目录仍安装 v35c 编译器与其已发布代际。不能直接用主源码 DLL 覆盖旧安装。v35c 兼容只读生命周期探针在重启会话 pid 26220 生效，观察记录见 `diagnostics/f10-native-lifecycle-audit-20261005/restart-pid26220-observation-v2.json`。该轮未按 F10。探针初版把返回 `System.Collections.IEnumerator` 的 `BundleResourceManager.UnloadUnusedAssets` 误写为 `System.Void`，所以此边界未安装；2026-10-05 已修正，并增加代理的 `ReleaseHandle/Unload/Clear/Dispose` 与管理器 `OnProxyDispose` 观察，重新部署 v35c ABI 的 DLL，SHA `5B5F7F0BA06422B538BBBC341D963967056ABE63D14239B6543DC394AF51F812`。原 DLL 及受保护文件校验见 `diagnostics/f10-native-lifecycle-audit-20261005/probe-v35c-deployment-v2.json`。v2 尚待一次游戏会话验证。上一轮地图与 UI 日志已保存，不需重做完整自然切换矩阵。

## 对外规则

Blender 导出完整版本，按一次 F10，更新当前及未来实例。允许一次导出同时改变 Mesh 顶点/索引/形态、骨骼槽顺序和权重、Renderer 数量、材质参数/引用、贴图尺寸/像素及 INI 控制。资源变化类型不决定不同的生命周期路线。

统一单位是一个源渲染部件对应的完整部件集及登记关系。原生 Renderer 对应 0/1/N 个 Mod 部件都是正常输入；增删、排序、改名和回到原版必须支持。世界、UI、LOD、shadow proxy 等每个实际消费者分别装配，不能套用 LOD0 的列表或骨骼表。

骨骼变化包括每个 Mesh 的槽、bind pose、权重和额外游戏原生骨骼引用，仍绑定该实例的原生骨架。本轮不把修改 Animator 动画/控制器、插入新骨架或重建物理模拟等未请求功能混入。

## 实际证据

无改动 F10 会话 pid 35748 已确认 v35c 的候选加载及当前世界实例替换路径可以提交：18 个独立候选 Bundle、世界/UI 两根候选 PFB 预检、一个世界 owner 经游戏的 `RemoveMainPart → LoadMainModelSync` 和 `Internal_InstantiateAsyncImmediatelyWithParent` 路由实例化，动画配置重绑返回成功，generation 由 1 到 2。旧根和新根 ID 不同，提交无拒绝或回滚。UI 候选已准备，但 F10 后 UI 实例未被消费；此次资源内容未变化，编辑后实际画面、Renderer 0/1/N 变化及旧代回收尚未验收。证据见 `diagnostics/f10-native-lifecycle-audit-20261005/unchanged-f10-pid35748-resource.json` 与 `unchanged-f10-pid35748-observation.json`。这为用户提出的游戏自有 PFB 创建/替换路线提供实机支持；是否继续沿用完整 owner 重建，取决于动作视觉反馈和真实资源改动测试。

重启会话 pid 26220：17:03:21.773 世界 `input-033.vfs` 的 GameObject 返回；17:03:53.505、17:03:55.267、17:03:58.028 UI `input-068.vfs` 的 GameObject 返回，三次返回同一托管对象地址，不能仅凭地址推断缓存/生命周期。记录到 83 次 `PrefabInstantiateProxy.LoadImmediate`、84 次 `BaseModelComponent.OnModelLoaded`、19 次 helper `ReFindRenderersInChildren`，17:04:03.217 管理器 `Release` 一次，随后多个 helper `OnRelease`。采样达到每边界 128 上限，部分事件只给下界。783 个已记录进入事件均有返回，未记录原函数异常。没有 F10 请求或提交；没有 `AssetBundle.Unload` 记录。`PrefabInstantiateProxy.Release` 未触发，但代理实际退出路径可能经 `ReleaseHandle/Unload/Clear/Dispose`，初版未观察这些边界，不能断言代理未释放。日志未提供明确退出原因。该轮只证明自然加载及局部登记时序，不能证明完整热重载。

进程 4448 加载 v35c，没有 F10 请求。世界输入为 input-033，UI 为 input-068。日志记录多次 Renderer 绑定变化及 helper 释放/初始化；UI 输入五次实际异步 Bundle/Asset 返回，世界输入两次。操作没有逐次时间标记，不能给每次事件强行命名。五种边界达到 128 次采样上限，缺失的晚期事件不作未执行的证明。

完整候选序列化图：世界含 601 个 GameObject/Transform、107 个 SkinnedMeshRenderer 和一个 LODGroup；UI 含 507 个 GameObject/Transform、25 个 SkinnedMeshRenderer 和一个 LODGroup。这包含原版与新增对象，不是全部受 Mod 修改。Mod marker 身份数分别为 43/12；marker 数不等于 Renderer 数。

写出器给额外部件增加只带 Transform/SkinnedMeshRenderer 的节点，原 Renderer 节点仍可能带其他组件和骨架后代。LOD 引用被展开。helper 的 customRenderers/vfxAssets，以及 VFX 派生类型配置缓存，需要纳入更新。

静态普通分支已确认：ReFindRenderersInChildren 收集部分效果状态，调用 ResetAll/InitAll 后恢复部分状态。ResetAll 释放材质/显隐/VFX 登记；_InitRenderAndMaterial 重新枚举或使用 customRenderers，并调用 EntityVFXAsset.GetRendererTypeConfigs(false)。后者可返回 m_rendererTypeConfigs 缓存。IFix/运行时分支仍需验证。

因此不能先在旧材质副本上设置新贴图，再让原生释放步骤恢复旧材质；“重新查找”也不等于清除全部派生配置。

## 一条完整事务

1. **读取版本**：Blender 共用离线编译器生成完整资源与装配描述，F10 取得完成的版本。纯贴图导出可以保留未变几何，但描述仍须完整；无需手动 EXE。
2. **准备目标集**：通过 Unity 资源读取创建 Mesh/Material/Texture；准备每个源消费者的 Renderer 集、逐槽骨骼、rootBone/skinningRoot、bounds、LOD、helper/VFX 配置及控制。准备完成前保留当前画面。
3. **映射实例**：用源 Prefab/源 Renderer 稳定身份匹配，不以当前 Mod part 名或 Mesh 名作为唯一身份。保留原生 Transform/Animator；有直接组件引用时保留原生 Renderer 作为锚点，额外视觉部件才新增节点。改名/删除不丢源匹配。
4. **释放并切换登记**：主线程释放旧登记、切换完整引用集，然后重新登记材质、显隐、VFX/捕获及控制。同一 helper 一次处理所有受影响部件；材质副本、配置、控制保持实例隔离。
5. **发布版本**：当前实例一致后发布。后续 Prefab 创建及闲置实例池使用同版本；隐藏实例也在范围内。未修改时再按 F10 保持相同语义。
6. **退休旧版本**：解除旧部件在 helper/LOD/控制/池中的引用，按实际消费者生命周期释放。原生节点不随 Mod 删除。插件独有扩展节点在消费者退出后销毁。Unload(false) 不证明全部 GPU 对象已销毁，Reset 不证明残影已经退出。

准备失败保留旧版本。提交失败恢复完整旧部件集及配置登记，不重建角色掩盖失败；多实例回滚不能只恢复其中一部分。不为贴图、材质、Mesh 分别维持刷新生命周期。

## 同步迁移的关系

| 关系 | 要求 |
|---|---|
| 源消费者 → 目标集 | 支持 0/1/N；增删/改名保留源锚点 |
| Mesh → 骨骼槽/bindpose/权重 | 每 Mesh 独立槽序，Transform 属于当前实例，rootBone/skinningRoot 一致 |
| Renderer → Material → Texture | 实际候选引用，原版与自制贴图混用、不同尺寸，包括 toushi |
| Renderer → LOD/shadow proxy | 按每个 Prefab 关系展开/移除 |
| helper → Renderer 列表 | 原始/自定义列表及动态子 helper 不留旧扩展部件 |
| VFX 配置 → 派生缓存 | 必要时按原生接口重新生成，不能只换输入列表 |
| 材质控制 → 副本/RendererInfo | 先释放、再写基础材质、最后登记，避免旧恢复覆盖新资源 |
| 形态/残影 → Mesh/实例 | 新几何与当前权重；已捕获残影保留自己的快照到原生 Release |
| 场景/展示/池 → 版本 | 当前、隐藏、后续及取池实例一致，启动与 F10 共用装配描述 |

不能 Destroy 原 Renderer 的整个 GameObject 来代替清理。直接引用消费者先分类：保留源锚点能保持的引用不重建；成员列表按各自原生语义展开。未知引用不能用字符串替换或写私有 native 内存处理。

## 离线描述与实现顺序

现有 manifest 的 root/marker、Mesh 骨骼路径和资源槽不能完整表达源 Renderer → 多部件及 LOD/helper 关系。扩展编译器装配描述，包含源身份、原生/扩展节点归属、每消费者完整目标集、逐槽骨骼引用和 LOD/helper/VFX 关联。来源是原生序列化图与导出规则，不增加手填字段；F10 不重算顶点、权重、mip 或 Bundle 编码。

按完整流程串通：装配描述 → 实例映射/目标集构建 → 完整登记切换/回滚 → 未来实例/池 → 引用退休。先覆盖复杂输入，再优化性能。same-count 不能作为生产前提；拓扑变化不能退回 RemoveMainPart/LoadMainModelSync 形成另一套路线。

## 验收

主用例在同一导出同时改变：一个 Mesh 的顶点/索引/形态、另一个 Mesh 的骨骼槽/权重、一个源消费者从一部件变三部件并删除另一部件、材质槽/参数和贴图尺寸/像素。一次 F10 后完整出现，旧部件不重复，动画/平 A/残影/控制正常，原版 toushi 保留。反向导出再 F10 完整恢复；世界/UI/后续生成/地图切换保持同版本。

本地回归覆盖同名不同来源、源部件改名、0↔N、不同槽表、重复 F10、owner 在准备时消失、创建/注册失败完整回滚、旧残影仍存活、多实例及闲置池。离线/模拟通过不替代游戏验收。

导出、离线编译、原生加载、主线程装配、请求到可见提交分别计时。正确性通过后优化不变资源复用和依赖编译粒度，不用限制部件数换取快。旧路径约 29 秒贴图准备与约 1.16 秒候选加载属于不同阶段，不是新方案延迟承诺。
