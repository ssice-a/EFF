# F10 热重载手段与当前证据

最新目标统一为 [完整渲染装配热重载](full-render-assembly-reload.md)：包括 Renderer 增减、Mesh/形态、每槽骨骼/权重、材质及贴图的组合变化。自然切换观察已完成，同数量重绑定草稿未部署。下面分资源路线与 v35c 测试优先级是此前调查记录，不替代新的完整契约。

2026-10-05。目标是重新导出 Mod 后按 F10 更新贴图、材质、Mesh、装配和控制，同时保持动画、残影与后续实例正确。用户已明确允许研究任何能实现目标的手段；此前文档将资源读取输入设为唯一替换入口的技术限制，不再约束 F10 研究。首次加载的已验收路径仍是离线准备合法数据，由游戏读取创建。

## 当前结论

完整原生重载生命周期尚未证明。调用若干原生函数并不等于复现完整换模型流程；v35b 的 T pose 和回滚日志已证明这一点。v35c 增补了实例化入口、动画配置恢复及武器/音效引用刷新，已部署但尚未完成实机验收。本次调查没有修改或重新部署运行 DLL，也没有改变用户 Mod 或 Blender 工程。

当前实现让只改一张贴图也加载候选闭包并重建角色。新资源内容使用独立 CAB/Bundle 身份，避免旧缓存复用；因此原资源与 Mod 版本可以并存。这不等于游戏缓存键原地失效，更不等于换地图清理。静态资源加载、场景实例、实例池和 GPU 资源是不同层次。

## 该游戏加载与缓存的证据

针对安装中的 GameAssembly 和 metadata，按方法 token 与 CodeGenModule 解出了下列静态调用链：

```text
BundleResourceManager._LoadAssetInternal
  → RuntimeManifestBinary.TryGetAssetInfoFromPath/GUID
  → Manager.LoadAsset
  → BundleProxyContainer.GetOrCreateAssetProxy
  → AssetProxy.GetHashKey
  → 查询 assetPools
      命中：返回原代理，必要时取消待卸载
      未命中：NewAssetProxy 并登记

Manager.GetOrCreateBundleProxy
  → 按 Bundle hash 查询 bundlePools
  → 未命中时 NewBundleProxy

AssetProxy/BundleAssetProxy 的 RetainHandle、ReleaseHandle
  → Bundle 引用管理与待卸载队列

ResourceManager.InstantiatePrefab
  → PrefabInstantiateProxyManager.InstantiateProxy
  → 资源已就绪后实例化 Prefab
```

`BundleProxyContainer.GetOrCreateAssetProxy` 的普通编译分支在 `0x303c952` 计算 key，随后查询表；命中和未命中具有不同分支。`GetOrCreateBundleProxy` 在 `0x3182268` 查询，只有未命中才调用 NewBundleProxy。这比仅凭函数名判断缓存更强，但仍只是静态证据；IFix 补丁分支和动态时序未在本轮执行。

因此不能断言游戏在启动时已把所有 Mesh、Texture 反序列化完。启动资源索引可以描述全部资源，实际加载请求再按身份取缓存或执行 I/O。相同角色的多个 Prefab 实例也不意味着多个 Mesh 资产都重新读取。资源卸载后再次加载、新版本采用新身份时，可以产生新对象，不能将“一个地图内不加载两次”当绝对规则。

## 可以研究的边界

| 变化 | 优先研究的提交方式 | 需要证明的部分 |
|---|---|---|
| 贴图像素、尺寸或 mip | 新建 Texture2D，上传已经离线生成的像素，按旧 Texture 对象身份更新材质引用 | 正在使用、缓存、Prefab 模板、材质副本及特效引用的覆盖；后续新实例使用新贴图 |
| 材质属性或贴图槽引用 | 使用公共 Material 接口，或加载离线材质再更新对应渲染器的基础材质 | 游戏特效产生的材质副本/属性覆盖、材质槽变化与原版贴图保留 |
| Mesh 顶点/索引/形态、每 Mesh 骨骼槽 | 加载已准备的合法 Mesh，对现有 SkinnedMeshRenderer 使用公共绑定接口 | bones 必须按新 Mesh 的槽顺序逐项解析；rootBone、bounds、LOD、形态控制和捕获缓存同步 |
| 部件数或渲染器层级变化 | 比较局部视觉装配更新与完整游戏 owner 重建 | Renderer/helper 登记、额外骨骼映射、动画与 VFX 生命周期；不能将一条 setter 当作完整装配 |
| 原资源对象原地磁盘重读 | 继续调查已知 Unity native reload 候选 | 所有权、任务/GPU 时序与适用对象尚未证明，优先级低于公共接口 |

这些是研究方向，不是已经实现的 F10 行为。F10 对外仍是“重新加载 Mod”，按资源变化采用不同提交方式不要求用户改变操作。

## 统一以渲染装配为提交边界

用户提出统一销毁/创建对应渲染部件。这个方向优先于把贴图、材质、Mesh 分成三套独立的运行时生命周期。可以统一采用：

```text
按 F10 读取最新完整 Mod
  → 准备新 Mesh/Material/Texture 资源对象
  → 为受影响的源部件准备新的 Renderer 集及装配数据
  → 按每个 Mesh 槽映射到当前活骨架
  → 在游戏渲染登记边界替换部件集，保留实体、骨架和 Animator
  → 更新 LOD/自定义 Renderer 列表、材质、显隐、VFX、捕获和形态绑定
  → 后续实例选择新版本；旧对象按实际引用生命周期退休
```

统一的是资源版本、部件装配和提交生命周期。贴图引用仍写入材质，材质/Mesh 引用仍交给 Renderer；无需将贴图数据放入 Renderer。只销毁 Renderer 并重新创建不会使 ResourceManager 缓存自动失效，新 Renderer 必须明确取得已准备的新资源对象。

Renderer 组件、Renderer 所在 GameObject、完整模型根、骨架节点和游戏的 RendererInfo 登记不是同一个对象。原 Renderer 所在节点可能同时带骨架或其他组件，不能销毁整个节点来代替释放 Renderer。新增视觉节点只能携带需要的渲染组件，不复制第二套 Animator/实体脚本；允许的额外原生骨骼绑定回当前骨架。

已有静态证据确认 helper 重登记入口 `ReFindRenderersInChildren → ResetAll → InitAll`；Reset 会清理捕获并重置材质/显隐/VFX 状态，InitAll 会创建或重新初始化捕获控制器、材质/显隐登记与 VFX。这些函数没有提供加载修改后资源或创建新 Renderer 的证据。一次提交要按所属 helper 汇总受影响部件，不能每个 Texture 属性都重置 helper。

本次又核对到游戏有 `SkinnedMeshRenderer.set_skinningRoot`，除 `bones[]`/`rootBone` 外还需正确映射 skinningRoot。源码写出器保留 `m_SkinningRoot` 的逻辑引用，但运行时把候选 Renderer 移到旧骨架时，必须重绑定其实际 Transform，不能保留候选 Prefab 骨架指针。

完整重新创建 Renderer 是可研究的统一路线，不是已验证可行的单函数调用。仍要核对游戏是否持有直接组件引用、特定注册 ID 或渲染任务状态，以及如何恢复 helper 重置的显隐/效果状态。候选对象创建方式也要以游戏接受的合法数据及工厂为依据，不写私有 native 渲染内存。

当前 v35c 实际部署仍重建完整模型 owner，不等于该渲染部件方案。后续贴图/材质/Mesh 分项属于验收矩阵；统一装配方案允许只改贴图时也重新装配对应视觉部件，局部属性更新可在统一正确性通过后作为提速优化。

## 贴图路线的实际限制

游戏包含已编译的 `Texture2D.LoadRawTextureData`、`Apply`、`Reinitialize`、构造函数及 `Material.SetTexture`。

本轮读取当前发布 generation 的 18 个候选闭包 Bundle，发现 **11/11 张新增 Texture2D 都是 `m_IsReadable=0`**。写出器沿用了模板的可读性标志。

`LoadRawTextureData`、`Apply` 和 `Reinitialize(width,height)` 的编译代码会检查可读性并进入异常分支。其他重载的限制也应独立核查，不能因为名称相同就假设行为相同。因此当前非可读资产不能直接套用“写像素再 Apply”的方案。

可以将 Mod 贴图改为可读，再保留对象身份更新；代价是 CPU 侧像素副本。本轮更倾向先验证新建 Texture + 引用更新，避免要求所有 Mod 贴图长期保留 CPU 副本。构造时尺寸、色彩空间和 mip 直接来自自描述 TEX，CPU 侧 mip 已生成；不会在 F10 解码 PNG 或重建 Mesh。能否可靠覆盖全部消费者需要实机证明，上传成本还需测量，不能先宣称毫秒级。

更新必须按声明和实际对象引用处理，不能按贴图同名或统一 renderer mask 盲目覆盖。特别是 toushi 等保留原版贴图的材质，不应被身体贴图更新覆盖。旧 Texture 的释放必须等到相关引用不再使用它；全局 UnloadAllAssets、Destroy 全场资源不能当成 Mod 刷新。

## Mesh 和原生 helper

安装中的 `SkinnedMeshRenderer.set_sharedMesh/set_bones/set_rootBone` 均有公共绑定入口。合法 Mesh 可以继续通过已验证的游戏资源读取创建；使用运行时 setter 更新绑定不要求自行写 native 私有顶点/索引内存。

这些入口存在并不证明完整热重载已经安全。每 Mesh 的槽顺序可能不同，单独更换 sharedMesh 而保留旧 bones 会重新制造撕裂。残影控制器、形态快照缓存、材质实例与 Renderer/helper 登记也必须和新绑定一致。

`EntityRenderHelper.ReFindRenderersInChildren` 已核查为 `ResetAll → InitAll`，并带 VFX 恢复动作，不是纯重新枚举。它可能重置材质/可见性/特效状态，不应每更新一个属性就调用。它没有替代 Animator 配置恢复的证据。局部绑定和完整 owner 重建需要分别验证。

## 下一步和验收界线

详细的当前版本测试顺序、用户操作、失败分流及后续资源矩阵见 [F10 执行与验收计划](f10-test-plan.md)。先对已部署且未验收的 v35c 做一次明确 A/B 的实机测试；下面贴图局部更新属于它失败后的优先实现路线，不应当作已经部署的功能。

先建立可执行的贴图更新验证，检查修改 body 后 F10 的像素变化，并同时确认当前 root/Animator/Mesh 对象身份保持、后续实例贴图更新、原版头饰贴图保留及尺寸变化。这个步骤应避免调用 RemoveMainPart/LoadMainModelSync。

然后验证 Mesh 更新，覆盖顶点数/索引变化、不同骨骼槽表、形态和残影；最后处理渲染器数量变化。完整 owner 重建保留为候选，不能因静态接口或模拟测试通过就宣布所有资源验收完成。

## 本轮可复现调查

- `diagnostics/f10-native-lifecycle-audit-20261005/audit_reload_alternatives.py`：只读反汇编与方法身份核对，输出 `reload-alternatives-static.json` 和调用摘要。
- `diagnostics/f10-native-lifecycle-audit-20261005/inspect_texture_reload_inputs.cpp`：只读实际离线 generation，核对新增 Texture 的尺寸、mip、格式、可读性和色彩空间。
- `texture-reload-inputs.txt`：generation `C8FBDF7DB398112FAE82C3754AD01F46CAAF3F2E2A955D9C1DB261B18966E11C`，11 张新增贴图，可读数量为 0。

这两项调查已执行成功；它们没有执行游戏运行接口，不作为视觉或运行时稳定性验收。
