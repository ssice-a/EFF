# EFF v2.0.12

Coordinated with EFF Blender 0.50.7, AnimeStudio 1.9.5, and EFF Mod Manager 0.2.3. This release adds explicit game-Prefab replacement by logical path and completes the standalone effect authoring route.

- Adds distinct mesh-attachment and game-Prefab replacement modes for effect Mods. Replacement routes the chosen logical Prefab path through the game's normal instantiate lifecycle and matches the original root activation state.
- Supports standalone effect-only Format 3 Mods, reusable emitter data, and native effect assets without copying game-owned resources into the Mod.
- Adds AnimeStudio author-owned particle Prefab creation from the game's TypeTrees, with Blender authoring/export and round-trip validation.
- Keeps source effect paths exact and path-based so game hash changes do not invalidate replacement targets.

Shared tools package: `EFF_Shared_Native_Tools_v2.0.12.zip`. Windows x64; runtime input remains Format 3.

# EFF v2.0.11

This coordinated release pairs EFF Blender 0.50.6, EFF Mod Manager 0.2.3, and AnimeStudio 1.9.4. It adds author-controlled bone animation resources and keeps effect preview/export data round-trippable across EFF, Blender, and AnimeStudio.

- Adds Format 3 Skeleton and sampled bone animation resources, with multiple author skeletons per Mod and explicit source-versus-author bone handling.
- Keeps animation controls on one timeline so shape keys, material animation, and author bone tracks can be exported together without changing demand-driven loading.
- Preserves effect preview VAT UV channels, colors, tangent handedness, native submesh metadata, and texture stream metadata for faithful inspection.
- Adds effect-block clipboard semantics for copying the complete visual block, including logical resource references, without bundling game-owned assets.

Shared tools package: `EFF_Shared_Native_Tools_v2.0.11.zip`. Windows x64; runtime input remains Format 3.

# EFF v2.0.10

本次修复多个武器 Mod 共用游戏原生材质输入时的误冲突。Windows x64，运行时仍只接受 Format 3；兼容 EFF Blender 0.50.5、EFF Mod Manager v0.2.3 和 AnimeStudio v1.9.2。

- 跨 Mod 材质输入现在按 CAB、资源身份、依赖和编译后字节判断是否相同；相同输入可以共享，不同内容或身份仍会明确拒绝，避免错误复用。
- 增加独立 View、跨依赖组共享及不同输入冲突的回归覆盖。

共享工具包：`EFF_Shared_Native_Tools_v2.0.10.zip`。本次仅更新 DLL 运行逻辑，无需重新导出现有 Mod。

## EFF v2.0.9

本次为多武器 Mod 材质修复版。配套 EFF Blender 0.50.5（continuous 通道）、EFF Mod Manager v0.2.3、AnimeStudio v1.9.2；Windows x64，运行时仍只接受 Format 3。

- 修复武器原生材质只存在于游戏 manifest 的“容器路径＋资源名”索引时，DLL 仅按路径哈希查询导致整个武器资源组回退的问题；两个游戏查询入口现在互为按需回退。
- 修复不同 Mod 编辑同一源材质时，共享节点因 `View` 指针不同而产生假冲突的问题。现在比较编译后字节；内容一致时共享，内容不同仍拒绝冲突。
- 增加逻辑路径索引回退、FBX 查询顺序、相同输入跨 Mod 共享及不同输入冲突回归。

共享工具包：`EFF_Shared_Native_Tools_v2.0.9.zip`。安装 DLL 更新后无需因上述逻辑路径修复重新导出 Mod。

## EFF v2.0.8

本次为剧情加载、武器替换及共享资源生命周期修复版。配套 EFF Blender 0.50.5（continuous 通道）、EFF Mod Manager v0.2.3、AnimeStudio v1.9.2；Windows x64，运行时仍只接受 Format 3。

- 修复大型剧情 Prefab 检查时重复解压相同数据块、耗尽累计读取预算而导致转场黑屏的问题；按源位置读取所需字段，不增加缓存或读取上限。准备失败时仍保留游戏自身获取 BundleProxy 的调用语义。
- 补齐剧情 Timeline 直接克隆实例的控制登记，使其继承同一 Mod 的当前开关状态；保持游戏自身的隐藏状态，不扫描场景。
- 修复依赖型静态武器 Mesh 缺少公开资源入口时未登记路由的问题。Blender 导出精确的来源 Bundle 声明，运行时仍按完整逻辑路径和子资源身份验证。
- 修复武器详情页内嵌材质寻址，以及同包贴图回指原 Prefab 形成的依赖环；保留材质实际引用的本地资源闭包和流式贴图范围。本机同一武器详情页已由用户反馈替换生效。
- 修复不同消费者复用共享输入时丢失材质依赖声明的冲突；补齐游戏卸载合成 Bundle 时的直接依赖引用释放，继续由游戏管理共享引用生命周期。
- 继续只准备游戏实际命中的替换资源，不批量预载 Mod；新增逻辑路由、内嵌材质/贴图闭包、跨消费者共享及卸载生命周期回归。

共享工具包：`EFF_Shared_Native_Tools_v2.0.8.zip`。武器导出请同时更新 Blender continuous 插件和共享工具；新增 Bundle 路由声明需要本版运行时支持。

已知边界：部分 lx UI 粒子实例仍会记录 `renderInUI` 设置未生效的告警，粒子模拟可继续，实际绘制需单独核验；本版不宣称解决该问题。原剧情的完整播放、剧情/世界按键同步及长期切图回收仍需更多实机场景验证。Tangtang 水特效的完整技能时间轴、其它 VAT 类型与蒙皮预览不属于本次修复承诺；约 20 秒加载的责任归因也未完成严格对照。

## EFF v2.0.7

配套 EFF Blender 0.50.4（continuous 通道）、EFF Mod Manager v0.2.3、AnimeStudio v1.9.2；Windows x64。运行时仍只接受 Format 3。

- 修复 F10 后仍引用旧资源及角色消失的问题：按资源代际更新路由，补齐同步加载入口和已发布依赖图的就绪查询。
- 修复作者材质/贴图错误继承 Prefab 原生依赖；继续只准备游戏实际命中的替换资源，不批量预载整个 Mod。
- 修复游戏运行中直接覆盖 Mod 文件可能污染旧模型延迟读取流的问题：仅在 Mesh、DDS/贴图骨架及特效输入实际命中时创建临时磁盘快照，旧视图保持原字节，新代读取新文件，最终视图释放时自动清理。新增覆盖、截断、双代并发读取及未命中资源不加载回归；该项仍待游戏内复测。
- Blender 单材质导入同时支持 EFFSOURCE `.mat` 与 Format 3 `.effres`，分别绑定解包图片与 Mod DDS；修复 Mod 材质再次导出的原生来源选择。
- 改进 Manager 的 Mod 列表刷新、打开目录、更新缓存和安装暂存位置；大体积缓存使用非系统盘资源位置。
- AnimeStudio 构建支持独立输出目录；四端产物在本地构建、验证后上传，发布过程不重复触发云端构建。
- 增加资源代际、同步原生加载、材质导入/DDS 回读及发布包内容回归。F10 已由用户实机确认恢复；任意物理或特效结构变更仍需分别验证。

共享工具包：`EFF_Shared_Native_Tools_v2.0.7.zip`。Blender 通过插件更新检查获取 continuous 产物；Mod 材质再次导出需要对应的 EFFSOURCE 基线。

## EFF v2.0.6

配套 EFF Blender v0.50.3、EFF Mod Manager v0.2.2、AnimeStudio v1.9.1；Windows x64。运行时仍只接受 Format 3。

- 统一 F10 热重载入口，让 Format 3 Mod 控制与独立特效使用同一资源代际和刷新事务。
- 修正 Format 2 迁移的 Mesh 坐标约定：作者快照转换到 Blender 坐标，运行时 Mesh 保留游戏原生 UV；形态键压缩帧展开时保留原 UV 值。
- 迁移时复用内容和采样语义完全相同的贴图，避免同一 DDS 被重复写出。
- 增加 UV 原值、形态键帧展开、贴图行序和共享工具发布预检回归。

共享工具包：`EFF_Shared_Native_Tools_v2.0.6.zip`。

## EFF v2.0.5

配套 EFF Blender v0.50.2、AnimeStudio v1.9.1；Windows x64。运行时仍只接受 Format 3。

- 接受 `packages/` 命名空间中的 Mesh 目标及 Material/Texture 来源身份，与 `assets/` 使用同一逻辑路径规则。
- 继续拒绝路径穿越、反斜杠、盘符和空路径段；不会把游戏资源身份误当作本地贴图文件。
- 增加运行时清单、作者工具和 Blender 导出回归测试，覆盖 package 资源路径及非法路径。

共享工具包：`EFF_Shared_Native_Tools_v2.0.5.zip`。

## EFF v2.0.4

配套发布修复共享 Format 2 迁移工具，支持旧 Mod 目录中未引用的回滚 `.bak` 文件，不再因此拒绝整批迁移。

- Format 2 迁移仍验证所有声明资源；仅忽略未被清单引用的回滚副本。
- 增加迁移回归测试，覆盖含 `.bak` 文件的真实目录模式。
- Mesh 导出保留作者 UV 与切线手性；贴图行方向在离线 DDS 编码阶段统一处理，迁移与 Blender 导出使用同一规则。

共享工具包：`EFF_Shared_Native_Tools_v2.0.4.zip`。运行时仍只接受 Format 3。

## EFF v2.0.3

本次为 Format 3 架构重构，配套 EFF Blender v0.50.0、AnimeStudio v1.9.0、EFF Mod Manager v0.2.0。

- DLL 只接受 Format 3 自描述资源目录；旧格式转换统一由 Mod Manager 离线完成，运行时不增加旧格式兼容分支。
- 改为按游戏请求准备原生资源输入，复用当前 CAB/path ID、依赖与反序列化流程；无需在 Mod 内携带 compiled.bin 或 source-inputs.bin。
- Mesh、材质、贴图、物理、特效、控制资源独立保存；公共来源索引与作者工具使用共享位置和 SHA256 校验。
- 修复变身、大招换模、回收和跨场景后切换组丢失状态：游戏原生显隐意图与作者过滤后的实际显隐分别管理，UI 与大世界共用控制值。
- 修复特效挂载 Part 标识、原生材质外部 CAB 的直接依赖解析、材质属性移除与热重载生命周期。
- 修复按需加载下特效挂载骨骼表未发布的问题；lx 特效与大招换模后的按键切换均已由用户实机确认。
- Blender 导出只保留实际使用的骨骼槽位；新增可撤销的空顶点组清理，保留所有正权重。

## 安装与迁移

退出游戏后，将 EFF_v2.0.10_dll.zip 解压到游戏根目录。保留个人 plugin/eff.ini、plugin/mods 与 state.ini。旧 Mod 先用 EFF Mod Manager v0.2.3 迁移为 Format 3，或用配套 Blender 重新导出。共享作者工具供离线作者与迁移工具使用，无需安装到游戏目录。

## 验证

按键切换问题已由用户在游戏中确认修复。发布工作流继续验证依赖图、材质移除、模型显隐生命周期、热重载事务、DLL 加载、索引更新和共享工具收据。资源权重修复属于本地 Mod 资产修改，不包含用户 Mod 或工程到公开发布包。

- Release preflight now provisions pinned DirectXTex and NumPy. Offline composition fixtures follow the current overlap and schema contracts; the runtime remains Format 3 only. Tags v2.0.0 through v2.0.2 were not published.

## English

Format 3 is the only runtime format. Legacy conversion runs offline in Mod Manager. Demand-driven native inputs retain game-owned deserialization and bounded dependency discovery. Fixes native visibility snapshots after transformation, effect Part bindings, direct material dependencies and reload lifecycle handling. The Blender exporter compacts unused skin palette entries and provides undoable empty-group cleanup. Preserve personal settings and migrate older Mods before upgrading.
