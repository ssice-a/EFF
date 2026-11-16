# 2026-11-15 DLL 与 Blender 架构审查

本次检查当前工作区源码，包括已有未提交改动。范围为生产 DLL 的按需加载、F10、控制图、特效挂载及作者动画，以及 Blender 的特效作者状态、复制、导入缓存、预览、导出和配套 AnimeStudio 原生构建。下面保留初次审查的触发条件与证据；修复后的状态以本节记录为准。

2026-11-15 后续纠正：C1/C2 原先把全局索引纳入导出，导致真实冷导出约 23 秒。该实现已由[冷导出与运行时逻辑引用](effect-export-performance-20261115.md)替代。作者选择与导出均不读取 `.eidx`；来源 Prefab 也只需游戏逻辑路径，不要求本地导入。本文后面的索引修复和 23 秒记录仅描述当时版本。

同日进一步审查确认：C2 中保留旧原生身份的兼容分支尚未统一运行时重定位；B4 的作者新增资源裁剪也未覆盖来源 SerializedFile 内的所有旧对象/流。真实 `lizi` 包仍有游戏原图与作者副本并存，作者 Mesh/Material/Texture 尚未使用通用 INI 资源 ID。具体证据与修正路线见[特效资源装配审查](effect-resource-architecture-audit-20261115.md)。[材质与导出限制修正](restriction-audit-20261115.md)已独立部署，不代表上述装配问题已经修完。

## 逐项修复记录

| 编号 | 实现 | 验证 |
| --- | --- | --- |
| R1 | 生产启动、F10 共用 `PrepareInstalled`，一次装配模型与独立特效的变量、按键、面板和动画；模型发布图仍只含模型包 | 真实 `LoadControls` / generation Commit 回归：持久值迁移、移除独立包、重复 ID 拒绝 |
| R2 | Lease 区分请求结束与验证成功；失败结果不共享，不提交作者特效包 | 连续三次依赖失败均保留原特效，释放后新请求恢复；替换缓存与租约回归通过 |
| R3 | 附件代际签名使用作者树和关联动画的完整签名；显式克隆 completion 接入弱 owner 目录 | 仅修改 Clip revision，经实际 `RuntimeAttachments::Frame` 迁移作者树；旧实例退役前仍持有旧挂点 |
| R4 | 游戏返回的原生 Mesh、当前 manifest、精确 PFB 层级提供逻辑身份；删除同名回退 | 同名不同逻辑来源、共享 Mesh、多 PFB、静态/蒙皮、LOD、弱引用死亡与 F10；100 次缓存观察无新增 VFS 查询 |
| B1 | 复制携带当前作者 Material/Texture/Mesh 的最小可达集合；共享引用去重，游戏资源保持逻辑引用 | 真实工程亮度 50→50；纹理像素、网格/UV 保留；粘贴投影失败完整回滚 |
| B2 | 导入底稿缓存按已验证归档内容隔离；缓存文件损坏时重新提取 | 两个有效作者资源使用同一 ID、不同内容，底稿不串用；篡改缓存后恢复 |
| B3 | 游戏引用携带可移植逻辑来源与原生根证明；donor 预览目录可选 | 将所有 donor 预览目录改为不存在路径后，Blender 导入、编辑、导出和原生构建成功，材质亮度仍为 50 |
| B4 | 剪贴板、导出和选择范围共用资源可达性规则，移除不可达作者资源与游戏依赖 | 连续三次换图，活动作者资源数量保持 `[2,2,2]` |
| B5 | 播放帧确认作者结构变化后取消旧材质复用，下一稳定帧继续复用 | 通过实际 frame handler 打开 Mask 并渲染验证；下一帧节点身份不变；覆盖 UV、Alpha 与混合节点 |
| B6 | 导出请求保留根/发射器选择；裁剪新增声明和依赖，保留必要父级；原生 `EffectSetting` 同步过滤 | 真实工程单发射器导出→作者资源导回→再导出；新增父发射器仅作定位、子发射器单独构建；原生层级遍历与确定性重建 |
| C1 | 导出与资源选择移除全局索引查询；构建只校验实际作者来源、绑定声明与编译器 | 冷构建不启动索引进程；实际字节与选中绑定变化使构建失效 |
| C2 | 新游戏逻辑引用写入 INI，原生引用位由运行时当前游戏清单绑定；已导入来源保留直接原生引用兼容 | 真实未知路径构建、PPtr 字节位置、INI/Catalog 往返与不可变覆盖测试；来源 Prefab 无须本地存在；新路径实机加载仍需验收 |
| C3 | 作者底稿保留游戏 Shader 引用与预览名称，移除未修改 GPU 程序副本；同一导出事务只编码一次相同像素 | 同一原工程安装版冷导出 6.28→3.30 秒，作者文件 15.35→1.54 MB；真实三发射器导回再冷导出、选单发射器修改再导出均通过 |

通用回归包括完整资源热重载 smoke、完整特效生命周期 smoke、作者挂点运行时、Python 作者/更新测试、AnimeStudio 原生特效与索引测试，以及独立后台 Blender 资源、材质渲染和单发射器往返测试。证据位于 `diagnostics/architecture-audit-20261115/`。测试未修改用户工程或覆盖 `mods/lizi`。

这些验证包含真实游戏文件的离线解析和模拟原生 API 的生产调用链；本轮尚未启动游戏验证视觉效果。裸逻辑路径发现的 Prefab 仍需游戏加载器确认公开入口，不能把离线根与闭包验证等同于实机加载成功。首次遇到一个 PFB 时的元数据检查延迟也仍需实机测量。索引首次解析会有冷启动成本，不能把温缓存结果说成冷导出耗时。

本轮本机部署记录在文末；没有提交、推送或发布 Release。

总体判断：INI 装配、DDS 像素独立、逻辑身份命中、游戏反序列化和作者资源按需准备的方向可以保留。具体问题集中在同一概念拥有不同的装配、失效或生命周期规则：完整控制图的装配留在另一条路径，生产启动与 F10 均有遗漏；作者材质在复制与导出时归属不同，来源缓存代替真实文件成为权威，可选预览目录又变成构建必要依赖。以下是可以落到触发条件和代码位置的发现。

## 先处理的运行时问题

### R1 · P1：独立特效控制未接入生产启动与 F10 装配

- 位置：[生产启动](../src/eiem_resource_entry.cpp#L548)、[启动控制初始化](../src/eiem_entry_runtime_shapes.h#L939)、[F10 Commit](../src/eiem_entry_demand_generation_reload.h#L365)、[独立包分类](../src/eiem_entry_demand_loading_probe.h#L740)、[另一条装配路径](../src/eiem_entry_full_resource_reload.h#L993)。
- `InstalledGeneration.packages` 明确不包含 `standalone`；`ReadInstalledPackages` 返回的也只是 `packages`。生产启动将其交给 `LoadControls(graph)`，F10 则只对 `packages` 调用 `RuntimeControls::Prepare`。完整的 `AppendPackages(standalone)` 留在另一条 `CompiledRuntimeControls` 路径，当前生产入口没有复用它。
- 触发：安装独立特效 Mod 后启动或执行 F10。特效挂载管理器仍持有独立包，但 INI 变量、快捷键、Ins 项和动画播放器没有装入统一控制图；只有挂载默认播放仍可能运行。[作者挂点动画](../src/eiem_entry_author_transforms.h#L71) 找不到相应 package/player 后跳过执行。不能把问题仅描述为第一次 F10 才出现。
- 证据级别：生产调用链确认，未启动游戏复测。
- 收敛方向：启动和 F10 共用一个完整安装实例控制图的装配入口，统一校验、状态迁移和发布；不能只在新入口补一个长期易遗漏的调用。

### R2 · P1：游戏依赖请求失败后可能绕过下次验证

- 位置：[GameLeases](../src/eiem_effect_debug.h#L63)、[请求完成与校验](../src/eiem_effect_debug.h#L79)、[替换缓存重试](../src/eiem_effect_debug.h#L118)。
- 当前先写 `lease.done=true`，再检查加载错误、资源存在及名称。任一检查失败时，替换缓存仍保留该 Lease；下次请求因 `done` 直接跳过检查，同源资源还可能共享该失败 Lease。
- 触发：游戏来源 Prefab/资源请求结束，但报告错误、返回空对象或名称不符；随后再次命中相同特效替换。
- 证据级别：状态转移和异常保留路径确认，未进行实机故障注入。
- 收敛方向：请求结束、验证通过和释放完成必须分开表示。只有已验证资源进入可复用状态；失败保留旧游戏特效，重试不能把失败当成功。

### R3 · P2：只改原生挂点动画时，F10 不迁移已有作者树

- 位置：[AttachmentStamp](../src/eiem_entry_effect_attachments.h#L30)、[重建条件](../src/eiem_entry_effect_attachments.h#L174)、[作者树签名与不兼容处理](../src/eiem_entry_author_transforms.h#L58)。
- 挂载差异只记录层级、特效载荷及挂载参数；作者树签名还包含关联动画时长、输出文件、资产与 revision。仅修改这些动画内容时，挂载差异可能不变，已有 Tree 不重新 Acquire；Frame 随后发现签名不兼容，仅停止采样。
- 触发：同一 owner 上修改动画时长、原生 Clip 映射/内容，其他特效与层级不变。完整模型重建可能掩盖问题，独立特效尤其容易暴露。
- 证据级别：生产差异/重建调用链确认。现有作者树测试直接 Acquire 新树，没有经过 F10 和 CaptureStamps。
- 收敛方向：作者层级与关联动画共用装配签名；差异出现后，在同一 owner 上迁移作者树和资源租约。

### R4 · P2：独立特效挂载存在以名称替代逻辑身份的匹配

- 位置：[DeclaredRendererMatches](../src/eiem_entry_effect_attachments.h#L9)、[无 Part 的回退](../src/eiem_entry_effect_attachments.h#L116)。
- 没有 Part 证明时，允许仅凭 Mesh 名称与 Renderer 相对路径匹配；该分支没有验证 `target.path`。
- 触发：两个不同逻辑 FBX 内的资产名和 Renderer 层级相同。独立特效可能同时挂载到不应命中的实例上。
- 证据级别：条件分支确认，未构造双模型实机场景。现有测试从 `sourceProof=true` 开始，未覆盖同名不同逻辑来源。
- 收敛方向：由已观察的原生 Mesh/游戏清单提供完整逻辑身份证明。继续允许同一个 Mesh 被不同 PFB 复用，不能用固定单一 PFB 限制代替逻辑匹配。

## 作者数据与导出的实证问题

### B1 · P1：复制发射器丢失已修改材质

- 位置：[复制对象范围](../tools/Blender/eiem_effect_clipboard.py#L204)、[资源引用转换](../tools/Blender/eiem_effect_clipboard.py#L173)、[粘贴恢复](../tools/Blender/eiem_effect_clipboard.py#L370)。
- 剪贴板携带 GameObject/Transform/ParticleSystem/Renderer，把外部材质转换成原游戏逻辑引用；没有携带当前作者材质参数。粘贴从原来源重新读材质。
- 用真实工程数据经过生产 `copy_emitter → encode/decode → paste_emitter` 复现：源材质亮度为 **50**，粘贴后为 **31.2**。复制的原生类只有 1、4、198、199。无逻辑来源的已导回作者资源跨文件复制还可能直接报缺少外部资源。
- 收敛方向：复制特效表现时携带作者资源的最小可达集合；未改游戏资源仍只带逻辑身份。剪贴板与导出共用资源归属规则。

### B2 · P1：导入缓存覆盖真实作者包的新底稿

- 位置：[缓存目录及命中条件](../tools/Blender/eiem_format.py#L809)、[全局缓存调用](../tools/Blender/eiem_blender_addon.py#L1538)。
- 仅用 `ResourceEffectAuthor` ID 建缓存目录，两个入口文件存在就跳过解包，不比较来源归档摘要。[该 ID 仅由相对文件路径生成](../tools/nativepack/author_format3.py#L1497)，不同 Mod 采用默认 `effects/author/Resource.effres` 时会得到相同 ID，同 ID 的新包或另一个 Mod 可读到旧底稿。
- 小型复现调用真实 materialize 函数，在解码后的归档接口输入同 ID 的 old/new 两个内容，结果依次为 `expected=old actual=old`、`expected=new actual=old`。进一步用真实 writer 生成两个校验有效、同资源 ID 的独立封装，第二个封装仍返回 `first-mod-source`。没有改动用户缓存。
- 收敛方向：缓存由已校验来源内容/代际确定，使用不可变目录。缓存失效或缺失只影响速度，不能改变导入结果。

### B3 · P1：跨来源作者包仍依赖原作者机器的绝对目录

- 位置：[作者归档](../tools/Blender/eiem_format.py#L730)、[强制读取 previewSource](../tools/Blender/eiem_effect_publish.py#L61)、[借用资源读取](../tools/Blender/eiem_effect_native.py#L48)。
- 作者包只归档主来源，跨来源引用仍保存绝对 `previewSource`/`gameSource`。导回可以降级为逻辑引用，但下一次构建仍无条件读取 donor 的 manifest。
- 直接读取当前 `mods/lizi` 的作者资源，Shader/Texture 均依赖 `E:\EIEM_Workspace\templates\basic-particles-20261113\sources\...\p_fxbat_liino_attack_05_star_hit_small.efffx`；此 donor 不在主归档里。现有同机器往返通过不能证明移机后可复用。
- 收敛方向：逻辑来源证明应可移植，离线预览位置应可选，原生结构模板应由作者数据明确拥有。不能为修复路径依赖而把游戏资源载荷全复制进运行包。

### B4 · P2：替换贴图后保留不可达旧材质和依赖

- 位置：[所有历史作者资源合并](../tools/Blender/eiem_effect_assets.py#L791)、[逻辑绑定枚举](../tools/Blender/eiem_effect_game_resources.py#L140)、[全部资源进入编译](../tools/AnimeStudio/AnimeStudio.GUI/EiemEffectBuildCommand.cs#L99)。
- 导出始终带上全部历史 `author_assets`，没有从当前特效根计算实际使用集合。
- 在真实 Mod 导回后，仅替换一次星芒的 2×2 主贴图：作者资源 **6→8**；旧 Material 已不可从任一场景组件到达，却仍在 `assets` 和 `gameReferences` 中。重复导回和换图会继续带走旧资源及相应游戏来源租约。
- 收敛方向：以当前特效表现为起点，统一计算资源可达集合，处理共享、替换和退休；作者保留的未使用资源若要存档，应与活动运行资源区分。

### B5 · P2：播放中编辑材质结构后，预览仍复用旧节点

- 位置：[帧刷新](../tools/Blender/eiem_effect_preview.py#L984)、[提前复用材质](../tools/Blender/eiem_effect_preview.py#L275)、[播放时延后后台刷新](../tools/Blender/eiem_effect_preview.py#L927)。
- frame 已发现并同步真实作者修改后，仍设置 `_reuse_materials=True`；材质因此跳过结构签名，仅更新有限数值。换贴图、Mask/Blend 开关、UV 旋转等需要重建节点的内容可能维持旧结果；待办随后被清除。
- 证据级别：调用链确认，未操作用户 live Blender 验证。
- 收敛方向：PreviewSession 区分作者结构失效与逐帧动画数值。作者结构没变时复用，真实编辑后重建受影响部分。

### B6 · P2：选中单发射器仍导出整个特效根

- 位置：[export_roots](../tools/Blender/eiem_effect_editor.py#L90)、[ExportBatch](../tools/Blender/eiem_effect_editor.py#L1225)。
- 第一步便将所有所选对象替换为根，随后只保存根并导出完整文档。用户要求的“选择发射器仅导出它，选择根导出整根”没有实现；额外发射器还会干扰游戏验证和性能判断。
- 证据级别：生产导出入口与数据流确认。
- 收敛方向：导出请求保留根与选中发射器范围，统一求解必要父级、子发射器及资源依赖；不修改场景启用状态来临时模拟选择范围。

## 跨端缓存与资源入口

### C1 · P2：原生构建缓存没有包含当前游戏解析索引

- 位置：[inputs_signature](../tools/Blender/eiem_effect_publish.py#L22)、[cache hit](../tools/Blender/eiem_effect_publish.py#L83)、[编译器的索引校验](../tools/AnimeStudio/AnimeStudio.GUI/EiemEffectGameReferences.cs#L79)。
- 签名包含来源和编译器，但不包含用于逻辑资源解析的 EIDX 内容/代际及共享索引选择。命中缓存后不执行编译器索引兼容校验。
- 小型复现只改变 `sourceIndex` 指向文件的内容，`inputs_signature` 和 Build key 均不变。新游戏/索引可能继续得到旧 CAB/PPtr；冷构建本应要求更新来源。
- 收敛方向：由同一个逻辑资源解析上下文提供已验证索引代际，参与构建请求和缓存身份；不能通过每次散落地读取其他全局配置来补洞。

### C2 · P2：游戏逻辑资源的两个编辑入口规则不一致

- 位置：[材质贴图入口拒绝新逻辑路径](../tools/Blender/eiem_effect_material.py#L191)、[PPtr 逻辑绑定](../tools/Blender/eiem_effect_game_resources.py#L111)、[来源闭包校验](../tools/AnimeStudio/AnimeStudio.GUI/EiemEffectGameReferences.cs#L20)。
- PPtr 入口能保存新逻辑路径；材质贴图路径入口却要求新值必须是本地文件。新路径不在默认来源闭包时，后续 SourceLease 也不能自动发现可加载入口。
- 这是能力缺口和重复规则；不能声称任意游戏逻辑资源都已可用，也不能清空 PPtr 或静默打包游戏贴图绕过校验。
- 收敛方向：两种 UI 使用同一资源选择/解析能力，明确区分作者文件、已验证游戏引用和缺少来源证明的待解析引用。

## 需要继续测量的风险

- 原生 Clip 的 `Tree.loaded` 在停止后继续持有，直到作者树退休；数字样本的停止/owner 规则不同。需要明确缓存预算及租约规则，不能仅凭此静态现象认定内存泄漏。
- 原生 Clip 共享缓存查找前仍先 `open()` 和计算整份 Digest，多 owner 首次播放存在重复读取。应通过实际 Clip 大小和 request-to-ready 测量决定优化。
- 混合数字样本与 native 输出的 Clip，其时钟受样本 owner activity 门控；该租约目前主要从 Mesh demand group 获得。需要验证 Mesh 消费者退休但作者 Empty 仍可见的组合。native-only Clip 不受此条件阻止，不能笼统报告为全部动画冻结。
- 导出仍会验证来源、编译器文件并重新投影作者数据。这些成本存在，但本次没有把 6～8 秒全部归因于哈希，也未重新量化游戏内存/GPU 成本。
- 生产默认编译的诊断开关与生产 DLL 检查仍有价值。带 `debug`/`compiled` 名字的文件可能服务当前运行或兼容读取，不按名称直接删除。

## 建议实施顺序与回归接口

1. **完整安装实例与代际发布**：先解决 R1、R2、R3。回归必须从生产 F10 发布入口出发，覆盖独立/模型包、变量和动画、多 owner、失败请求重试；不能只测试最后的匹配或 Acquire 辅助函数。
2. **作者特效资源集合**：集中处理 B1、B3、B4。复制、导出、导回共享“当前表现拥有的作者资源与游戏引用”规则；增加跨根复制、移除 donor 目录、重复换图后资源数量稳定的回归。
3. **缓存和逻辑资源解析**：处理 B2、C1、C2、R4。增加同 ID 新内容、不同 Mod 同 ID、新索引、同名不同逻辑来源等测试；缓存永不代替真实文件权威。
4. **预览与选择范围**：处理 B5、B6。播放中改图/混合方式的预览应及时收敛，选中导出应准确；以实际运行包里的发射器、材质和引用核验结果。

这些收敛可以沿现有格式、模块和调用接口逐步完成，不需要新增另一份控制清单、动画时钟或运行时资源格式。正确的 seam 应让调用者提交一个完整请求，内部完成统一规则；拆出更多转发文件本身不能解决上述遗漏。

## 证据与本机状态

诊断目录 `diagnostics/architecture-audit-20261115/`：

- `repro_blender_author_seams.py` / `blender-author-seams-output.txt`：缓存旧底稿、材质复制丢值、当前包的绝对 donor 路径。
- `repro_effect_cache_index.py` / `effect-cache-index.log`：索引变化未使构建键失效。索引文件为接口夹具，不是实际游戏更新重放。
- `repro_effect_asset_growth_blender.py` / `effect-asset-growth.log`：独立后台 Blender 导入真实 Mod，仅修改临时图片后检查作者资源可达性；未保存工程、未编译或覆盖 Mod。
- `repro_effect_source_cache_collision.py` / `effect-source-cache-collision.log`：两个由真实 writer 编码、decoder 校验通过的同 ID 资源，经过生产 materialize 接口复现跨包旧底稿复用；来源归档使用最小接口夹具。

初次审查阶段完成此前已授权、已构建的 0.50.16 部署：用户退出 Blender 后使用现成包安装，64 个插件文件、126 个共享工具文件摘要通过，回执为 `bin/local-deployments/20261115-073454-710/Blender.json`。该版本解决上一轮导出报错，不包含后续架构修复。

## 修复后的本机部署与验证

2026-11-15 已完成部署，三个组件均核对实际安装文件 SHA256：

- 正式 DLL：`D:\Hypergryph Launcher\games\Endfield Game\plugin\eff.dll`；SHA256 `8E364BBA6B3204806340B2829C145233473C3F685FEC11D2E3ECE750096ABBDB`；回执 `bin/local-deployments/20261115-085703-580/Dll.json`。生产验证确认未包含捕获探针。
- AnimeStudio：`E:\AnimeStudio-net10-1ccfbc16bf7fe625e8295bac8074ac3b1b9a065b`；回执 `bin/local-deployments/20261115-090403-396/AnimeStudio.json`。已在共享设置注册实际部署的 GUI；旧工程可能显式保存的本机 Debug 构建也已从当前源码重建。
- Blender 0.50.17：标准 Blender 5.2 的 `scripts/addons/EFF_Blender`；回执 `bin/local-deployments/20261115-090524-770/Blender.json`。66 个包内文件全部与回执一致，Python 源文件与当前工作区一致。
- 共享工具：`E:\EIEM_Workspace\tools\3d6909edcb162847c3c82616a5aec3cb3c62a860140ff47b92d0e43d4e889448`，126 个文件校验通过；回执在同次部署的 `shared-author/SharedAuthor.json`。

安装验证汇总为 `diagnostics/architecture-audit-20261115/deployment-verification.json`。四个仓库均在 main，部署前 fetch 后均与 origin/main 相等。本轮未提交、推送或发布 Release。

最终验证：Python 229 项（1 项跳过）；AnimeStudio EffectsTests 75 条断言；IndexTests、UpdateTests、完整 DLL 资源热重载与特效生命周期套件通过。后台 Blender 使用**实际安装目录**的插件和 AnimeStudio，完成单发射器导出、缓存复用、作者资源导回、再次导出，以及新增父级仅保留定位变换的原生构建。

安装版真实夹具的首次导出约 23.2 秒，其中约 18.5 秒用于首次索引解析及输入整理；同请求再次导出命中原生缓存，约 5.27 秒，未重新编译。换机器底稿或首次新增逻辑资源仍有冷解析成本。本轮保证正确的索引/资源失效规则，尚未达到“参数调整后即时导出”，后续优化应以这些分段数据为基线，避免用跳过验证换取速度。
