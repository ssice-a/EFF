# 离线目标清单与运行时规则应用

作者规则仍只写在 `mod.ini`，资源仍是独立 `.mesh/.mat/.tex`。自动生成的描述不要求用户编辑，不含原游戏资源副本。

## 找到目标

首次导出、来源声明变化或主动刷新时，工具复用 AnimeStudio 已有的 `index/endfield_assets.eidx`。按路径、资产名与类型确定原 Mesh，沿 CAB 反向依赖选全部候选包；再读取真实 `m_Mesh` PPtr、容器与层级确认所有 Renderer/PFB。材质、贴图只加入模板依赖，不扩大替换目标。

每 Mod 的 `source-inputs.bin` 保存游戏相对物理路径、offset/size、SHA、资源归属和相关依赖。只选反向消费者、声明来源与直接模板依赖，不复制全局索引，不递归打包全部正向依赖。声明未变时复用描述；首次准备新来源需要已有索引。DLL 不读全局索引，不查询未知 PFB，不扫描场景 Renderer，不按角色名、当前角色或世界/UI 后缀筛选。

## 小型输出

编译器读取完整 mods 集合，从 INI 生成规则，记录每个原 Renderer 的 CAB+PathID、原 Mesh 身份、所属输入和 PFB 根。共享 `resource-reload-baseline` 只保存原游戏范围与摘要，保留退休 Mod 的必要范围用于集合恢复。

新候选的 `manifest.bin` 保存规则、来源、作者文件引用/摘要、控制和装配预期，使用 XPRESS-Huffman 压缩。离线构建最终成员后，记录来源成员或作者资源的复制范围、少量变化字节和最终摘要；CAB、外部引用和 stream 路径的修改位置也离线验证。Mesh/贴图大段字节引用独立作者文件，原游戏成员引用原范围。候选不写完整 PFB、资源包、`startup/` 或完整几何/外观/命名阶段副本。全部核验成功后才原子更新 `current.tsv`，失败保留旧候选。

编译器仍在内存应用规则，核验 TypeTree、引用、骨骼和完整输出摘要，因此首次导出仍有构建成本。描述没有人为容量上限。

## 加载与 F10

游戏正常请求已知文件范围时，DLL 核验来源摘要，按离线复制范围重建成员，核验成员和完整包摘要，在 Unity 创建资源及实例前提交内存中的 Bundle。新清单不再运行 TypeTree 资源索引、Renderer 匹配、PFB 层级和骨骼解析；旧清单继续使用完整审计构建器。首次加载保留原容器/CAB/Bundle 身份。完整字节包在只读 MemoryStream 中，经 Unity 的同步/异步归档流入口加载；managed 方法裁剪时按实际 engine icall 名绑定，并核验托管 Read/Seek/Length 回调。流和数组保留到 Bundle 卸载，而非请求完成即回收。不覆盖安装档案，不主动加载未知来源。PID 33924 已确认流入口实际加载并完成 F10 提交，但该旧 DLL 的动画和延迟验收失败。

命中 Renderer 对应完整 0/1/N Part，未声明的 Renderer 保持原内容。借用材质生成作者材质对象，原材质和原贴图模板保留。其他依赖仍由游戏正常加载。

编译器持续准备完整 mods 集合。DLL 工作线程按离线清单准备隔离字节；游戏已有模型完成加载后，可提前注册候选 Bundle，并只为已经由游戏加载的 PFB 准备实例模板。提前准备不发布替换路由、控制或游戏代际。F10 核验当前完整集合与候选，未变时复用准备结果，增删改时调用同一集合编译流程。候选就绪即执行 owner 释放、路径池清理、模型加载及动画、武器、角色音频重绑定；未就绪则等待当前候选，不能保证此时毫秒完成。

离线清单始终覆盖全部目标。尚未访问的角色不提前加载 PFB 资产；其后游戏自己的 `PrefabInstantiateProxy._PreloadPrefab(bool)` 或原始 AssetProxy 读取入口命中精确来源时，才从已注册候选取对应模板。通过游戏 StringPathHash 和实际来源对象定位，不扫描场景，不把实例句柄当资源模板。提交后的隔离 Bundle/流随原生代际保留，支持后来出现的其他角色。

增删改和空集合共用流程，失败保留旧实例和规则。owner 记录游戏 `BaseModelComponent.AddPart` 的真实主部件归属，并结合加载 delegate；同步 `CreateMainPart -> AddPart -> SetupData -> StartSyncLoad` 不依赖加载回调存在。世界模型不会因同步加载没有 delegate 而降级为展示模型。

展示部件同时观察 `BaseModelViewPart` 和 `ComplexModelViewPart` 的完成入口，释放时注销该根的控制绑定，避免已归还路径池的存活 Renderer 被误认为活动实例。调用 ReleaseModel 时通过 IL2CPP 的实际虚方法解析派发到当前部件的实现，不固定调用 Complex 部件。已提交候选再次 F10 预检失败时仍保持活动代际及其 Bundle，不能把活动图标为待回收后重新加载同 CAB。

## NPC 实例覆盖边界

目标清单按 Mesh 引用确定，与实例控制器无关。当前 46 根样本包含 `actors/postmodels/npc/chr_0032_lizhiyan_postmodel.prefab`，也包含世界、UI、技能、剧情及抽卡根。离线命中 NPC PFB 不等于已加载 NPC 的 F10 生命周期适配完成。

只读检查当前游戏 metadata 和 native 调用确认：人群 NPC 的 `NPCCrowdModuleManager.InitNPCEntityComponent` 经 `NPCAvatarManager.CreateAvatar` 建立 Avatar，并初始化 `NPCUnionAnimtor`、LOD 和相关组件；PostModel 分支使用 `CreateNpcComponentByPostModel`。这条流程不是 BaseModelComponent 的主部件流程。当前 DLL 的 liveParts 只记录游戏模型部件，不包含 NPCAvatar 的独立 owner，因此不能宣称所有人群 NPC 已支持 F10 原地刷新。使用普通 ModelComponent 的实体与人群 NPC 需区分其实际生命周期，不按角色名或 PFB 路径后缀分类。

后续适配须记录游戏自己的 NPC 装配/释放边界，用真实资源身份关联受影响实例，再走其 Avatar、动画、LOD 和池的重建链。不能直接把 NPC 当展示部件释放，也不能为一次外观刷新直接执行整个 NPC 实体释放/初始化并声称行为状态保持。尚未覆盖的活动实例继续拒绝事务并保留旧代际；这不表示已自动刷新该实例。

## 验证边界

当前 LZY 样本有 15 个作者 Mesh、46 个 PFB、1440 个唯一序列化 Renderer，来源描述 63,063 字节、62 个必要输入，包含离线复制范围的压缩规则 4,414,466 字节（约 4.21 MiB）。完整包回放 412,388,711 字节，复制规则约 2.3 秒；先前 13 Mesh 样本中，普通规则构建约 28.5 秒，复制输出全部与完整审计输出逐字节一致。这是离线回放结果，不是实机 F10 耗时。以上是样本结果，不是协议常量。验证与实机边界见[当前验收](hot-reload-testing.md)。

Unity 内存加载仍需要完整可解析载荷；清单大小不等于运行时内存，本次没有解决不同 LOD/模板下作者像素的重复构建。旧全量缓存可能仍在磁盘，不参与新候选。静态 MeshFilter、跨文件 PFB 根、persistent pool 与未知 owner 生命周期仍明确拒绝。

游戏更新使来源范围或 SHA 不匹配时须重新准备描述；兼容性依字段、引用和内容判断，不按产品版本号判断。

```text
EndfieldVfsProbe --prepare-static-sources <game> <index.eidx> <Mod> <compiler> [--refresh]
eff_resource_pack --mods <mods> <baseline> <cache>
```
