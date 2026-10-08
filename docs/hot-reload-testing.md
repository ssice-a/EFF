## 2026-10-09 当前验收结果

修正后的实机 PID 31820：06:24:47.979 请求 F10，06:24:48.086 提交，约 107 ms，迁移 Typhoea 与 LZY 两个世界模型。用户随后确认待机书恢复正常；日志所记录的刷新后观测窗口约 11 秒，没有再出现已用资源被销毁的记录。该结论结合用户视觉确认，不将这 11 秒日志描述为 180 秒观测，也不据此声明所有 NPC 或大量 Mod 的压力已通过。待机/特殊冲刺预加载分别约 0.56/0.55 ms，由游戏原生配置决定。

## 2026-10-08 未替换道具与待机表演生命周期

2026-10-09 PID 36684 再次实机复现，四个资源释放入口均成功绑定。06:08:47.527 请求 F10，06:08:47.633 提交（约 106 ms）。新的书 `-316740` 仍跟随新角色骨骼，Controller 为 `637966`；06:08:56.606 游戏对 `main/2bef987b4d686d02ad787edf.ab` 执行同步 `AssetBundle.Unload(true)`，在该调用内直接销毁这个仍使用的 Controller。06:08:58.356–.357 对 `main/673a32dee408d555f9e74c31.ab` 的相同调用销毁 43 个书 Mesh。后一次书实例再次发生相同销毁。调用栈经当前文件的元数据/PE 核对为 BundleResourceManager.LateTick → Manager._TailUpdateBundleWaitUnloadState → BundleProxy.UnloadSync，不是插件退役容器的 Unload(false)。这轮从弱引用存活前后确认了实际资源销毁，已超出上一轮仅 Controller=0 / Mesh=<null> 的相关性证据；没有修改或输出书资源。

原生调用链核对：ComplexAnimatorComponent.ChangeAnimationConfig 在 _ClearInternal 后调用虚拟 ReleaseInternal(false)，CharacterAnimationComponent 的实现仍清空该 Entity 的 born/custom perform preload。后续 _SetupAnimation/OnAnimationSetup 不重跑 CharacterAnimationComponent.OnStart；配置中的待机及特殊冲刺 perform preload 原本在 OnStart 调用 _PreloadSpIdlePerform/_PreloadSpDashPerform 建立。旧 F10 缺少这一步，使重建后继承的临时对象失去原有预加载持有，符合本次延后 Bundle 销毁的行为；首次角色启动与动画重绑的生命周期不能视为相同。

本轮修正：仅对真正的 CharacterAnimationComponent，在 ChangeAnimationConfig 成功且新 Animator 归属验证后，调用上述两个原生配置预加载函数；UI 展示部件和其他动画组件跳过。路径/列表及句柄由游戏选择和管理，不按角色、道具名或 Bundle 名硬编码，不额外保存附件清单、不阻止卸载，也不重跑会重复订阅事件的整个 OnStart。回滚同样恢复旧动画配置对应的预加载，调用耗时由 FULL-RELOAD-PERFORM-PRELOAD 单独记录。新增模拟回归先因重绑后的 preload 为空而失败，补齐后 v35/v36 全部通过；覆盖延后退出边界、一次调用与顺序、回滚、UI/其他组件过滤，以及不重置状态的连续 8 次 F10。这是已确认的流程遗漏修正，尚需本版实机证明书在延后待机切换后不再失效，不能将模拟回归代替原始复现验收。

v35 生产构建通过，游戏退出时仅部署 eff.dll，SHA256 `CEBF402B899A00E3D6515D3A7CCF40A145CCE6308BE8503CAD00381E5B6C3458`（2,197,504 字节）。65 个既有 Mod/配置/协议文件的集合与 SHA256 均未变，没有重导出、保存工程、修改书或发布 Release。定向弱引用观测暂留供本版冷启动/F10/延后待机验证；实机确认后再移除临时探针。

2026-10-09 实机 PID 29828 首次捕获实际书附件，用户确认复现。05:43:49.000 请求 F10，05:43:49.102 提交（约 102 ms，两个世界模型）。释放前书实例 `-251730` 的 Controller 为 `410594`，骨槽无缺失/模型外骨骼；表演退出后旧句柄无效且临时对象 go 已清空，但当前请求 source 仍为 2。重绑后角色模型为 `-293500`；约 1 秒后游戏创建新的 `toRelax` 表演与书实例 `-300640`，followCurrent=1、Controller=410594，5 秒时网格仍有效。约 6 秒切换到 `RelaxLoop`，7 秒时该书 Controller 变为 0；15 秒时书仍 active=1、挂点在新模型下、所有骨槽完整，但 Mesh 读数变为 `<null>`。约 22 秒另一次 `toRelax` 创建书实例 `-317232`、Controller=1094630，27 秒再次变为 0，30 秒时 Mesh 再次失效。后续待机继续使用此实例，观测完整持续到 180 秒。

该证据否定“新书仍跟随旧模型”及所测的“缺骨槽”预测，并将问题缩到表演切换后的资源/动画生命周期；Controller=0 本身不证明资源被销毁，`mesh=<null>` 也尚未区分空指针与失效的原对象。只读展开现有两份 compiled.bin 的描述，均没有书的 PFB 路径及 `CAB-5deb3789c8c7c910b53ae6df11a32dbe`，没有新增解包或修改作者资源。尚未确认哪个释放操作导致异常，不直接改骨骼、Animator Controller 或恢复参数。

下一诊断在实际附件中记住已使用 Mesh/Controller 的弱引用，通过当前游戏的四个 AssetBundle 原生释放入口（同步/异步 Bundle 和同步/异步单资产）对照释放前后的资源存活与调用栈。原接收者、参数、异步返回值和调用次数保持不变，不阻止释放、不发起加载。同步调用内的销毁单独记录；异步操作只观察游戏原有请求完成，并标记 correlationOnly，不能将同时完成的请求当作唯一因果。观测窗口结束后清除弱记录。

v35/v36 完整事务回归和 v35 生产构建通过；新增探针合同覆盖弱资源身份去重、同步/异步原参数与调用次数、异步返回值及完成观测、诊断异常时仍执行原释放。游戏退出时仅部署 eff.dll，SHA256 `E637B525C3C0E0B73796CFDA0AFBD37C861AEBCF77F3C4169653AEC886460728`（2,196,480 字节）；65 个既有 Mod/配置/协议文件的集合与 SHA256 均未变。该版本是只读诊断，没有改变表演、模型或资源释放行为，尚未修复书消失/扭曲。仍需本版冷启动后的完整复现以确认入口绑定及实际释放/资源失效的对应关系，探针合同通过不替代实机验收。

2026-10-09 道具身份更正：只读查询现有 AnimeStudio 索引，并在内存解析单个原始附件 Bundle `Bundles/Windows/main/2bef987b4d686d02ad787edf.ab`，找到独立书 PFB `assets/beyond/dynamicassets/gameplay/prefabs/attachment/att_widget_typhoea_book_postmodel.prefab`（CAB `CAB-5deb3789c8c7c910b53ae6df11a32dbe`）。其根为 `Att_widget_typhoea_book_postmodel`，包含 `S_item_widget_inner_*` / `S_item_widget_outer_*` 网格，以及 `book_rt_f_jnt`、`shudi_jnt`、`fengmian_*`、`shuye_*` 等书自身骨架。对角色本体导出 Mesh 的几何检查确认 cloth_06/07 是机械装置，不是书；此前这些网格及 Gadget 骨骼的正常读数不能用于验收待机书。没有新增全游戏解包、修改用户工程或 Mod，尚未确认实际待机句柄对这个附件的运行时持有和释放关系。

当前诊断替换为 `[DEBUG-perform-state-*]`：直接在 F10 必经的释放前、表演退出后、动画重绑后，读取角色动画组件的当前表演请求及表演管理器的主/子句柄，只接受属于当前 Entity 的句柄。跟踪这些句柄实际保存的 `TmpObjectInst`，记录资源路径、GameObject 显隐、跟随 Transform 对当前/旧模型的归属，以及附件自己的 Animator、Controller 和既有 Renderer 列表中的蒙皮状态。请求值大小和字段类型由当前 IL2CPP 结构取得；字典沿游戏自身使用的 Values/Enumerator 接口读取，并在异常时 Dispose。不安装表演事件钩子，不查询场景，不改变游戏字段或现有清理顺序。

观测使用弱引用，每秒检测状态变化，在 1/5/15/30/60/90/150 秒采样附件蒙皮，180 秒后释放窗口；读取失败只记录 GAP，不拒绝重载。撤除旧的本体网格延后观测，避免用无关机械装置判断书。生产入口回归覆盖实际集合数量约束、值类型枚举器、异常归还、表演请求装箱、其他 Entity 过滤及窗口结束释放；这些是探针合同测试，不是书异常的修复回归。v35/v36 完整事务回归和 v35 生产构建通过。

本轮游戏退出时仅部署 eff.dll，SHA256 `A130D20441A093FE9D3633B6FC79FC73949BABA10D7B98DA56B4B346C8A261E8`（2,182,144 字节）；65 个既有 Mod/配置/协议文件的集合与 SHA256 均未变。未改 Blender 工程、作者资源或 compiled.bin。新诊断尚未获得实际书的运行时日志，书消失/扭曲根因仍未确认；需冷启动、正常书待机、F10、延后消失和下一次扭曲的完整时段，COMMIT 和探针合同通过均不替代实机验收。

PID 26032 在 05:03:41.134 请求 F10、05:03:41.250 提交（约 116 ms，两个世界模型）。本体网格观测只有刷新前、刷新后、1 秒与 5 秒，最后一段日志止于 05:03:46；没有 15/30/60/90 秒的记录。该探针未识别上述独立书实例，也没有覆盖延后消失及下一次扭曲，因此这次 COMMIT 和本体骨槽正常都不能证明书正常。用户报告的视觉消失/扭曲仍未修复。

2026-10-09 补充实机顺序：用户确认 F10 后待机的前半段书仍存在且正常，稍后突然消失，下一次待机才扭曲。PID 30640 再次提交 generation=2，04:51:36.820；LZY 与 Typhoea 的 after-rebind 三种归属检查均匹配。立即动画归属丢失的预测已被两次实机观测否定；现有表演清理没有修复该延后异常。

新增临时 `[DEBUG-skin-state-*]` 只读诊断：在 F10 前后枚举确切当前模型中未命中作者控制声明的蒙皮网格，读取其公开 bones/rootBone/skinningRoot、显隐、包围盒与骨骼在模型空间的位置；刷新后保存弱引用，在 1/5/15/30/60/90 秒各读取一次既有列表，随后清除。没有全场景查找、按角色/书名称选取、骨骼写入或资源导出。该列表只覆盖当前模型已有网格，不能据此排除独立临时对象的问题。

生产入口回归验证完整骨槽、空槽、模型外骨骼及读取失败隔离，并验证观测窗口结束后释放引用。v35/v36 完整事务回归及 v35 构建通过。游戏退出时仅部署 eff.dll SHA256 `9381E19CE0D7E089904EBB9893F6B1C0FC81EB9CF2AB574BC3C8C7CA05FE7B49`（2,153,472 字节），65 个既有 Mod/配置/协议文件的集合与哈希未变。该版本仍为诊断，书异常尚未修复；下一轮需覆盖正常待机、F10、消失、再次待机扭曲的时段。

用户观察：Typhoea 的书在 UI 正常，切回大世界后消失，下一次待机出现撕裂；用户进一步确认冷启动后、第一次 F10 前正常，F10 后异常。当前 INI 只命中 body_01、cloth_01、cloth_02 和隐藏的 cloth_03，没有书的替换规则，不能归因于用户修改书。

对当前实际 Typhoea compiled.bin 回放并与原始来源比对：51 个未命中的 Mesh、1553 个未命中的 SkinnedMeshRenderer 字节保持原样；1845 个原脚本的 29680 个字段保持原样，仅允许原有 RenderHelper 的 customRenderers 引用列表扩展。35182 个原 GameObject/Transform 的身份、名字、父引用及 175910 个姿态/其他字段保持原样（新增组件/子节点列表由替换规则管理）。这是资源输出边界验证，不证明待机道具的运行时生命周期正确。

PID 46732 的 Player.log 记录 CharInteractPerform.TmpObjectInst.RefreshTimeScale 的 NullReferenceException，经 CharPerformHandleBase/CharPerformMainHandle 与 CharInteractPerformManager.OnTimeScaleChange 调用。该表演系统独立保存临时对象和挂点；旧 F10 只重绑动画、武器、音频和跟随特效，没有在拆除模型前结束关联表演，因此可留下被旧模型销毁的道具引用。这确认一处生命周期遗漏，尚不能单凭异常断言书撕裂的全部原因。

修正：按确切元数据读取 GameInstance.m_world、GameWorld.charInteractPerformManager，先调用游戏 ForceExitAllPerformOnEntity(entity, null)，再卸载武器和模型。null 不继承某次编队切换的表演句柄；游戏方法按该 Entity 清理主/子/跟随表演，未增加角色、Mesh 或道具名称特例。UI 展示部件不走世界表演清理，其他 Entity 不受该调用影响。当前待机/互动会正常结束，下一次由游戏流程重新创建，不手工修补道具或骨骼指针。

生产入口的模拟回归先复现拆除模型后临时道具已销毁但表演句柄仍存活的失败，再验证清理在模型/Animator 退役前发生、其他角色保留、展示部件不调用，以及连续 8 次不重置状态的 F10 每次都清理当轮待机道具。v35/v36 完整事务回归与 v35 DLL 构建通过；原有动画、武器、音频、异步 UI、拒绝及回滚仍覆盖。实机边界：修正版仍需冷启动后按 UI → 大世界 → 待机 → F10 → 再次待机验收，COMMIT 和模拟测试不替代书显示/变形的实际观察。未改变 .blend、INI、compiled.bin 或作者资源，无需重导出。

首版已在游戏退出时部署，eff.dll SHA256 `7F74E383D0E01EEBE5BAE333E2599D5E6B7395E0F9032806EED6332F92B2C6EF`（2,138,624 字节）。65 个既有 Mod/配置/协议文件前后哈希及集合校验未变。PID 19396 与 39900 实际记录 ForceExitAllPerformOnEntity 在模型退役前执行，随后两个世界模型提交；PID 19396 没有此前 TmpObjectInst 的空引用记录，但用户确认书仍异常。首版清理没有解决该显示/变形问题，COMMIT 不作为成功验收。

继续调查的三个假设为：道具跟随引用仍指向旧骨骼、动画网格分组显隐恢复遗漏、待机表演另有残留状态。临时 `[DEBUG-attachment-*]` 探针仅观察游戏 ComplexAnimatorComponent.SetTransformFollow 与 SetRendererGroupVisibility 的原调用；记录确切模型路径、根身份、跟随双方的名字/父节点/根归属/世界坐标与分组参数，不扫描场景 Renderer、不改变骨骼或显隐。首次 F10 前使用已有资源规则的目标集合过滤，提交后使用当前原生候选，保证能获得正常与异常两侧证据。生产入口回归覆盖返回 true/false、参数及调用次数保留、诊断读取异常隔离、其他角色调用、首次 F10 前的记录；v35/v36 事务测试通过。

只读诊断版已构建并在游戏退出时部署，eff.dll SHA256 `E44CAC35303293766FAC223F6CCA9E8080444B8815553914A59D628C6706BCDC`（2,145,792 字节），65 个既有 Mod/配置/协议文件哈希及集合未变。未改作者资源或工程，未重导出或发布 release。仍需本版冷启动待机、F10、再次待机的实际调用日志定位根因；当前书异常尚未修复，诊断完成后移除临时探针。

PID 38008 按冷启动待机 → F10 → 再次待机复现，18:32:36 成功提交 generation=2，用户确认完成操作；两个探针绑定成功，但均没有 EVENT 或 GAP。随后只读查看原 PFB 的 AnimatorMono 序列化处理器：PostAudioHandler、WeaponVisibleHandler、CharPerformHandler、FootStepHandler，没有 TransformFollowHandler/RendererVisibilityHandler。这说明首轮探针没有覆盖本模型实际的事件处理路径，不能据空日志排除挂点或显隐问题。

已撤掉这两个无效观测点，改为游戏 CharPerformHandleBase.ResolveEntityMountTransform(Entity, MountPoint) 与 SetCharMeshGroupShow(Entity, ModelPartEnum, bool)，观察角色表演实际解析的挂点 Transform 及显隐分组。两种枚举的实际类型、Int32 底层与四字节值大小在绑定时验证，不猜偏移；原请求、Transform 返回值、显隐调用次数及读取异常隔离通过生产入口回归。v35/v36 事务回归、v35 DLL 构建通过，游戏退出时仅部署 eff.dll SHA256 `1EF6F66055809146CDB567B24128EC9844638B881178ADCA2DDE70C0ED474568`（2,145,792 字节），65 个保护文件未变。当前仍是只读诊断，没有宣称书异常已修复；下一轮需对照角色表演实际的 F10 前后调用。

PID 11232 的第二轮表演探针绑定成功，18:55:14.548 的 F10 提交 generation=2，迁移两个世界模型，但仍没有 EVENT 或 GAP。两轮事件观测均未覆盖实际书的控制调用，不能据空日志判断归属、挂点或显隐正确。已撤除这些事件钩子，改为在必经的 CaptureConsumers / ReloadConsumers 边界只读记录当前 AnimatorMono：缓存的 ObjectMono/Entity、所属层级实际的 ObjectMono/Entity，以及绑定的 ComplexAnimatorComponent。该诊断只查询当前动画组件的父层级，不扫描场景 Renderer，也不改游戏字段。

生产入口回归验证 F10 前后都执行归属读取，能够区分“缓存归属为空、父层级归属正确”，并验证诊断读取失败不打断真实重载。v35/v36 事务回归与 v35 DLL 构建通过。游戏退出时仅部署 eff.dll SHA256 `263EF0743680BF43565469F4C9CF93164E6565679A37D7604E1C7CA8F3E496B5`（2,143,232 字节），65 个既有 Mod/配置/协议文件哈希及集合未变。该版本仍为只读诊断；尚未取得其冷启动后 F10 前后归属日志，书异常未完成修复或实机验收。

PID 40268 已取得该版本的实际归属日志：19:08:31.648 请求，19:08:31.746 提交 generation=2（约 98 ms，两个世界模型）。Typhoea 与 LZY 的 before-release / after-rebind 均为 cachedOwnerMatches=1、parentOwnerMatches=1、animatorBindingMatches=1；AnimatorMono 已换为新实例，缓存与父层级仍指向原 Entity。这排除了本轮所测的动画归属丢失预测，未证明书的显隐/变形正确。用户已回复重启，刷新后书的视觉观察仍待确认。

只读核对本机游戏原生流程：AbilitySystem._ApplyModeChange 的两处 ChangeAnimationConfig 都传入 tryResumeAnimations=true，与当前重载参数一致；实际 CharacterAnimationComponent.OnAnimationSetup 调用基类 OnAnimationSetup，后者通过 AnimatorMono.SetObjectMono 设置归属。BaseModelComponent.FinishLoaded 同时通知 RootComponent.InvalidateMountPointCache，不能把重复加载必定缺少挂点清理当作既定根因；本轮没有额外调用生命周期初始化或写入私有字段。

## 2026-10-08 F10 文件快照、Blender 导出与形态键文字

PID 44496 在 16:51:43 因准备期间文件变化拒绝候选，16:52:08 又因准备后的作者文件快照不一致拒绝提交；没有提交就不能把画面未变视为刷新成功。16:54:58 的后续请求已有 COMMIT，generation=2，迁移两个世界/UI 模型；这仍不替代用户对动作、衣服、武器和按键的观察。

相同编译内容的新文件时间戳原先只更新 Set，未更新 diskPrepared；Unity 帧又按相同候选键丢弃新的 Prepared，导致旧 authorFiles 一直参与提交校验。现在同键候选也更新已经验证的文件快照及控制声明，并保留已就绪的原生资源。先红后绿的入口回归复现该拒绝，验证刷新不会重复原生加载，真正的准备后文件修改仍拒绝。v35/v36 完整事务回归通过。只改 `[Mod] key_switch_enabled` 或无语义变化的 INI 时，用原先验证的声明/资源更新控制，Mesh/材质/贴图及 compiled.bin 的时间戳变化仍进入正常验证；单个真实 Typhoea 副本的控制刷新从约 2999 ms 降到 311 ms。隔离输入使用弱引用找回尚未消费的数据，不延长已消费数据的生命期。

Blender 编译器原先完成资源构建后串行重读、解压并回放全部来源来生成对象/重定位证明。现在在已有原始和编辑后成员仍在内存时生成相同证明，并复用现有工作线程；校验、完整来源及依赖保持不变。同一 Typhoea 独立副本强制完整编译：旧路径 47358.5 ms，新路径 27993.6 ms，compiled.bin 均为 SHA256 `7942C9D3A5C272220BF6B4D2BA1820CDC17D0C0F961AAA4818B800CC464188FA`，约减少 41% 编译耗时，没有增大输出。内容未变的单独编译复用为 2968.67 ms。

在打开的 `18324_autosave.blend` 中将当前 13 个选中对象导出到独立测试副本，生成 12 个作者 Mesh、6 个材质、13 张贴图：完整入口耗时 14.344 秒，其中作者处理 8.676 秒、来源准备/复用编译 5.018 秒、发布 0.650 秒。该测试没有改变网格内容，编译命中旧结果，不能当作修改网格后的全量导出耗时。选择、活动对象和工程路径保留，未保存工程或覆盖游戏 Mod。导出完成提示现在显示实际秒数，返回结果保留阶段耗时；形态键、录制按钮、面板标题和错误提示的乱码已修正，当前 Blender 只更新 Python 函数/面板，不重新注册保存控制变量的数据属性。31 项离线编译、导出合同及发布测试通过。

已部署单个 eff.dll，SHA256 `32A512CD6C00D908AF358F19C0473EFC41392C16C41169B4130DAF3FDCF01154`；Blender 自己的编译器 SHA256 `B4D0DA0172312A2BE74D1E58FBC41BBFEED7C75FC29AF7CDC1C5D05DEF1235E2`。65 个现有 Mod/全局配置及协议文件部署前后哈希不变。此版 F10 修正仍需冷启动后验证世界/UI 更新、动作、武器和按键；本轮没有发布 Release。

部署后实机 PID 46732：17:34:26.846 请求 F10，17:34:26.951 提交 generation=2，两个世界模型迁移，约 105 ms；17:34:33.216 再次请求，17:34:33.358 提交 generation=3，两个世界模型加一个展示模型迁移，约 142 ms。两轮均调用动画/武器/音频重新绑定；本会话没有 FULL-RELOAD-REJECT、STARTUP-DEPENDENCY-GAP 或 materialNull=1。LZY/Typhoea 同时响应各自小键盘绑定。两轮资源键相同，证明已准备候选能连续提交，不证明修改 Mesh 后的重新导出已经显示新内容；画面、动作、武器显现及有内容变化的导出仍待用户观察。

本轮独立测试副本位于 `bin/export-profile-151ea854af454c54879795788430a2a3`。校验绝对路径在当前工作区 bin 下后尝试清理，自动审批返回 `blocked by policy`，副本暂时保留；没有换工具绕过。

## 2026-10-08 首次加载的新增 Bundle 依赖

PID 24076 使用 `9EFE1...` 仍出现默认紫色和切换停顿。PooledList 绑定已成功，但 `main/03c21cd847f08573c23a8c9a.ab` 的键/名称校验失败；16:19:29.109 至 16:20:53.438 共重复记录 93,416 次，整图每次重建、异常与同步日志写入均位于资源查询路径。16:20:13 的世界模型完成记录仍有作者衣服 `materialNull=1`。这是依赖映射失败，不能通过修改作者贴图规避。

只读展开游戏随包 `manifest.hgmmap` 到内存后，目标包记录的实际键为 `7E369E5452DF7026`，与 VFS 文件记录一致；原先选用的 HashCodeEx4AutoLower 不生成该键。已改为从当前 RuntimeManifestBinary 持有的 ManifestDataBinary 读取现有映射表的 Bundle 名称与 hashName。数组与记录大小、名称引用及键偏移由 IL2CPP 字段/类型反射取得，所有读取范围受游戏自身 m_address/m_length 约束；只缓存编译图需要的键，并通过 GetBundleNameString 回查核验。没有游戏资源请求、额外清单文件、全游戏解包或 Renderer 扫描。

成功和失败结果均按当前 manifest、映射地址/长度与 Mod 图缓存，避免队列轮询重复解析/刷日志；清单换映射或 Mod 图变化后重新解析。回归先复现字符串哈希与存储键不同导致借用材质依赖缺失，再验证真实键/高位键、不同结构偏移、失败保留原列表、成功/失败各 1,000 次查询只解析一次，以及换清单后的恢复。v35/v36 完整事务回归通过。只读诊断从两个现有 compiled.bin 的结构取得全部 112 个来源 Bundle 名称，均存在于随包清单的 254,722 条记录中；只读取包清单，不展开这些包的资源。

该修正版 eff.dll SHA256 `855137C15F2276CA2A037C37A975F943505DD8F8068406AC6B291FFCC79DB8E3`，65 个 Mod/全局配置文件部署前后哈希未变，未重导出。PID 44496 实机仅建立一次键索引：255,842 个游戏记录、45 个所需名称，indexMs=61.871；没有依赖 GAP 或作者 materialNull=1。世界和 UI 的借用衣服 Shader 均为 HGRP/CharacterNPR、supported=1、passes=7，用户确认首次紫色已恢复正常。后续角色切换没有重复资源回放，材质实例 ID 复用，但没有帧时间对照，不能宣称所有切换卡顿已消除。35 次首次输入提交的 replay+upload 合计仍约 2.111 秒，分散在各次请求，不是一次切换或每帧开销。

PID 36948 再次确认 Typhoea 默认画面紫色、F10 后恢复。PID 41944 使用首版依赖修正 DLL，仍复现：14:39:32 出现 `STARTUP-DEPENDENCY-GAP phase=bind reason=game dependency list API absent`，增补模块没有启用；14:40:05 的模型完成探针直接记录多个作者部件 `materialNull=1`，14:40:18 F10 后借用材质才完整出现。此前合并的错误信息无法区分无参构造器和泛型 Add 哪个不可调用，不能声称已确认具体裁剪了哪个方法。原文件替换入口修改了序列化 external/PFB preload，但游戏 RuntimeManifestBinary 的 Bundle 依赖列表仍是原表；F10 的隔离资源加载则先注册了完整闭包。新增 external 不会自动成为游戏加载队列的依赖。

DLL 从已有 compiled.bin 的 before/after external 描述提取仅追加的 CAB 边，按同一清单的 CAB 所属来源映射到原 Bundle 名称，不新增文件或重编作者资源。当前真实双 Mod 集合为 22 个来源包、424 条去重的新增边。首版通过游戏 HashCodeEx4AutoLower 计算包键，再以 GetBundleNameString 回查；该计算入口已因上述实机证据撤掉，改用原清单的存储键。校验后补入 TryGetBundleDirectDeps/TryGetBundleDeps 返回的列表。原依赖顺序保留，间接依赖沿游戏原枚举函数补齐并去重；加载、排队、依赖就绪检查、RetainBundleRef/ReleaseBundleRef 均继续由原生流程处理，插件不主动调用 ResourceManager 加载来源，也不等待异步请求。

游戏 BundleProxy.Init 记录该实例使用的依赖图；ClearAll 完成释放后移除记录。F10 发布另一份清单时，已加载包的依赖释放规则保持一致，池中再次 Init 的实例使用当前图。按 manifest/hash 查记录，无场景或 Renderer 扫描。依赖枚举只读取现有 List 的实际元素范围，原游戏字段宽度和名称经反射确认，不设置资源数量限制。

PID 42056 使用 `9BE296...` 仍失败，14:52:43 记录的实际签名为 `.ctor(ObjectPool<PooledList<Int64>>, Int32)`；零参数和 capacity-only 构造器都不存在。该具体参数是游戏的 PooledList，不是普通 List。当前修正从原生列表的 m_objectPool 调用 Allocate，沿父类寻找 Add/IList.Add，并在查询成功或异常后调用 Dispose 归还临时列表。池化列表的往返验证在第一次收到游戏实际列表时执行，不创建另一套资源请求或队列。

此前回归覆盖普通列表、被裁剪的构造器/追加方法、父类上的追加接口、两参数池化构造器、Allocate/Dispose、异常归还、64 位键往返；依赖图覆盖首次借用材质/原生间接依赖、原顺序/去重/环、直接依赖引用归属、旧包释放与池复用、新清单和空图。`9EFE1DD131E6DA46DA1BDF14FE00CE2369A8AB4D2DB973458AB525A253C1A808` 是本轮前的部署，PID 24076 已确认失败。两个 Mod 的 INI 只新增按键启用字段，63 个资源、compiled.bin 与全局配置核验未变。不能把模拟依赖补齐或构建成功当作画面验收。暂留定向材质/武器诊断。

## 2026-10-08 按键控制与面板

`[Mod] key_switch_enabled` 缺省/1 为启用，0 为停用，也接受 true/false。所有启用 Mod 都响应自己的绑定，同键允许共存；面板选择只决定显示哪个 Mod 的手动控件。Ins 独立勾选立即生效，并在工作线程只改写同一 INI 字段，保留其他声明、BOM、注释与换行；手改后 F10 生效，不需要重编作者资源。LZY/Typhoea 均写入 1。完整和局部导出会补齐缺失字段并保留现有值。Blender MCP 仅热更新 Python 导出/控制模块，工程、选择与形态控制数量保持不变。

面板使用内嵌 ImGui + 独立 D3D11/DirectComposition 窗口，Windows 11 Desktop Acrylic 背景；无游戏渲染器挂钩，仍只用快照/命令与 Unity 线程交换。设备/字体在首次打开 Ins 时创建，隐藏时停止绘制。真实窗口回归通过勾选、同时启用、选择、按钮、滑块与关闭，acrylic=1；模拟输入焦点与鼠标，不作为游戏窗口实测。真实 Blender 后台验证同键组/形态规划，25 个目录格式测试、11 个导出合同与 12 个发布测试通过。

## 2026-10-08 首次资源加载的重复工作

原调用链丢弃后台生成的首次加载结果，在游戏文件加载调用中又读取/解压来源并回放编译规则。现在后台保留已完成且实际变化的输入，以压缩包存储；首次请求只展开已准备的输入并传给游戏，之后复用原生流仍持有的 Byte[]，弱引用不会额外延长游戏数组寿命。未变化的来源回到原文件加载路径。没有新增磁盘缓存文件或修改发布资源。当前真实两 Mod 有 99 个变化输入、压缩缓存约 249.658 MiB，展开数据原约 887.24 MiB；这只计算新增缓存，不代表游戏、隔离候选或 Mod 的总内存。

生产入口回归验证缓存命中不读/回放来源、重复请求不分配另一份 Byte[]、未变化输入保留原 CRC/文件路径。真实集合验证缓存覆盖、解压哈希及快照共享；加载日志分别记录 replayMs、uploadMs、nativeSubmitMs 和 arrayReused。仍需实机对照量出进场景卡顿中资源展开、上传、原生提交及 Shader 创建的占比，不能据此声称几秒卡顿已完全消除。

## 2026-10-08 当前实机结果与武器重载修正

实机 PID 6724 在 13:31:06 成功提交两个世界角色的新模型，用户确认 F10 后衣服紫色恢复。首次启动仍紫，首次资源加载的新依赖注册尚未通过实机验收；候选材质的 Shader 名称、支持状态及 pass 数通过检查，不等同于首次依赖图完整。该会话随后用户反馈所有 Mod 的武器消失。Player.log 的 RayCastEffectAction 空引用签名包含角色/武器挂点参数，但尚未确定具体分支及所属实体，不能单凭这条异常认定丢武器的原因；消费者日志中的 weapons=1 仅表示调用了武器组件，不能作为武器显示成功的验收。

对照当前游戏 AbilitySystem._ApplyModeChange 的两条换模型分支，原顺序都是先刷新 CharacterAudioComponent 引用，再 UpdateAllWeaponMountPoint，最后 ReloadWeapon(true)。旧 DLL 将 ReloadWeapon 放在挂点更新前。现已对齐原生顺序，成功提交与失败回滚共用同一消费者恢复流程。回归先复现重载请求捕获旧挂点，再验证新根挂点、失败回滚及八次连续重载；v35/v36 完整事务回归通过。当前两个真实 Mod 的原生 GameObject/Transform 身份、名称、父引用和局部姿态审计通过：Typhoea 35,182 个节点/175,910 个字段，LZY 240,914 个节点/1,204,570 个字段。该审计不替代游戏内武器完成加载后的显示检查。

DLL 在游戏退出后已替换并核对 SHA256：`C0BF14CE01304ED8C247127C9860926C0E5A90012F9598E5A89F23076793F8EF`。既有作者、配置及配套文件校验未变，没有重导出、保存工程或发布 release。武器恢复仍待此 DLL 的启动、连续 F10 与攻击实测；临时 DEBUG-weapon-reload 日志区分原生 loaded/noWeapons 状态与函数调用，材质诊断继续用于首次紫色问题，实机问题确认后应移除临时探针。

## 2026-10-08 原生骨骼地址与候选材质验证

部分剧情/LOD PFB 的原生骨骼路径与作者工程不同，但真实骨骼槽引用正确。旧校验将作者路径强加到所有消费者，曾在 PID 36288 拒绝 F10。现在离线为每个命中 PFB 编译精确 Renderer 地址及骨槽 Transform 地址，两个构建上下文均生成该描述；运行时按实际地址验证，旧无地址描述继续走已有路径校验。没有按版本拒绝旧 Mod，也没有写入原版像素资源。

Typhoea compiled.bin 当前为 4,107,258 字节（约 3.92 MiB），SHA256 `5D61FF3E75A7E683CD728FCD81A39133C042A0AF6A2E51D2F499EC4AA263A105`；相比上一份的增加来自精确原生地址描述。全部 22 个根已包含描述；42 个非空纹理引用存在，6 个材质的原 Shader 引用、valid/invalid keywords 及禁用 pass 字段保留。两 Mod 组合为 112 输入、68 根，结果键 `3447E3571CD619B1BE23F4C3EBF624A8DE462B0DD127247596C7703BB221A5B5`。Blender 自有编译器 SHA256 `EE64801275ED51AAF5A7B87175EA00B9115021DDE68413720AFC5A08B461274C`；本次仅重建 compiled.bin，当前选择、作者文件和工程未改。

## 2026-10-08 LOD0 复用与跨角色材质完整依赖

旧工程仅导入 LOD0，原发现逻辑只查询 Blender Mesh，遗漏来源 manifest 已声明的低级 LOD。现在导出窗口与普通脚本入口都读取同一包的确切 Mesh 元数据；当前 13 个选择展开为 51 个视图，作者模板全部仍为原 LOD0，没有导入额外几何。四条替换规则覆盖实际存在的 15 个 Mesh 身份，所有级别复用 12 个作者部件、6 组切换和 2 个形态控制；cloth_03 的隐藏规则覆盖其已有 LOD0～2。复制对象按原作者名排序，避免 .001 对象在不同 LOD 中改变成员顺序。

实机日志显示外部狼卫材质的 _SpecRampMap 为空，F10 拒绝提交；旧 compiled.bin 的真实材质完整性检查也在佩丽卡 _DiffRampMap 处失败。旧来源准备仅保留修改纹理的模板，缺少继承纹理和 Shader。现在仅沿声明材质补齐原生依赖闭包的游戏文件范围，并把该材质原 AssetBundle 的预加载区间并入命中 Renderer 所属 PFB。编译结果仍保存规则和作者资源，不打包原版纹理像素。来源请求哈希包含新的选择语义，已有来源描述自然重新准备，不增加版本兼容限制。

全 LOD 实际编译另复现剧情 LOD3 原生调色板删骨后仅匹配 121/122 的失败。编译器汇集同一原生 rootBone 下命中 Renderer 的骨骼 hash/引用关系，补齐 LOD0 所需的原骨骼；跨层级和歧义引用继续验证。修复后当前工程实际导出成功，四规则 compiled.bin 为 2,334,470 字节，source-inputs.bin 为 33,052 字节，整个 Mod 为 285,335,788 字节。结果键 BDD5D1E1567CB44A97CADFB6BB1FC175BC1C3325FFDB8B8C34DD531E034B2703。第一次扩充来源并编译用时 104.4 秒；调整成员顺序后的导出用时 68.6 秒，这两次均有结构变化，不作为纯缓存命中速度。

35 个 Blender 逻辑/发布/离线回归、原生材质预加载扩展、静态来源与组合回归通过；真实结果的 42 个非空材质纹理引用全部存在于编译输入。与 LZY 组合为 2 Mod、112 输入、68 根；既有保护文件校验未变。Blender 自有编译器 SHA256 08E3951045D38BE2D33409128DA07C19C92FB06385C9E34E126B9EED6E8FB801，来源准备工具 C31738442E7A9BA80CBD3732BB2214EC1C000C2490BED07EC3D9B188016CA425。仅更新作者端及 Typhoea，DLL 读取结构未变，没有自动保存工程。游戏内紫色、远景 LOD、按键和 F10 仍等待修正版实测，不能以离线编译成功代替画面验收。

## 2026-10-08 旧工程迁移、跨角色材质与普通法线布局

用户当前工程 `G:/zmd/typhoeus/18324_autosave.blend` 的 13 个选中 Mesh 迁移到用户提供的 `chr_0034_typhoea_postmodel.eff` 来源；狼卫和佩丽卡材质分别关联同目录 `chr_0028_wulfa_postmodel.eff`、`chr_0004_pelica_postmodel.eff`。修复 36 个失效的原版贴图路径为确切游戏逻辑引用，自定义 G: PNG、几何、选中范围、形态键值和六组切换保留。没有自动保存 .blend。

Blender 原生导出器按材质自己的 eiem_author_package 读取结构，并在本次导出中复用同一个来源对象；图片筛选包含选中材质的包归属。纯逻辑回归通过；真实从佩丽卡包导入 .mat，校验所属包和原生 schema 后释放测试材质/图片，原选择不变。

塔菲 cloth_01 的原生 Mesh 使用普通 Float32 normal3/tangent4，旧导出器仅接受 0x31 packed frame。Python 编码及离线 C++ ApplyMesh 现在保留两种实际来源布局；普通 Float32 可保留 Blender 产生的有限零切线，不额外构造 packed basis。压缩帧解码精度、普通帧与零切线、未知格式拒绝回归通过。

真实剧情消费者另揭示原骨骼绑定限制：相同 Mesh 在剧情 PFB 的一根裙骨层级路径不同，严格按主模型全部路径查找失败（121/122 匹配）。离线编译读取原 Mesh 骨骼 hash/槽表，以该 Renderer 原生 m_Bones 对应恢复已有骨骼；新增槽继续在所属层级内按路径验证，重复候选按实际引用集合去重。原绑定姿态及原 Mesh hash 与新解包来源逐项一致。重排作者槽、新增槽及歧义/数量不符拒绝回归通过；不扫描运行中场景，不硬编码角色或剧情路径。

当前选中内容完整导出至游戏 `plugin/mods/Typhoea`：12 个部件、6 个材质、13 个纹理声明、4 条替换规则（隐藏的 cloth_03 保留 skip）、6 组按键切换、2 个形态控制；compiled.bin 为 1,841,518 字节。正常导出入口重复导出成功，用时 10.724 秒，作者文件及 source-inputs.bin 字节不变，仅重新生成 compiled.bin。整个 Mod 为 284,838,048 字节，主要为现有自定义贴图的 TEX 像素数据。

DLL 同入口实际读取单 Mod 通过：37 个输入、21 个根。与现有 LZY 组合通过：2 个 Mod、99 个输入、67 个根。Blender 自有编译器已更新，SHA256 `FD266682B4998565B16B8C06879D9D11FD1012600581165D3A30282AB695B381`；既有 34 个 LZY/配置/协议文件校验未变。实际画面、动作、跨角色材质和 F10 仍需游戏验收，离线成功不能替代实机结果。未发布新 release。

正常重复导出后的 compiled.bin 再次通过双 Mod 读取，SHA256 `0D9FF8F67BDC898A2C2508353177E21B23EC54CE30EE3DC2E8E6B6FDFAC0D078`。临时验证副本 bin/typhoea-migration 的清理也被自动审批以 blocked by policy 拒绝，暂留，未换工具重试。

## 2026-10-08 新增材质的原生依赖登记与独立 Mod 合并

庄方宜的 `M_actor_lod_zhuangfy_body_01` 位于 `CAB-ed1a0aec166350e23f7e189dd6fa75f1`。离线匹配出的部分 Mesh 消费者 PFB 原来没有依赖该材质，因此旧 SetReference 以 `new dependency requires serialized external registration` 拒绝导出。静态 Renderer 夹具先复现同类跨 CAB 失败，再验证按需登记外部依赖。

编译器保留原外部依赖表的顺序、GUID、类型和路径，仅追加所需 CAB 的 archive 引用；原有 FileID 不变。对象表后的 script references、ref types、user info，以及未修改对象的字节均保留。compiled.bin 已有 before/after 外部依赖字段，读取规则改为原表是新表的完整前缀，没有修改字节布局或按版本限制兼容。

DLL 组合多个 Mod 的同一原包时，合并并去重追加的依赖，依据 TypeTree 重定位修改对象及新增对象的 PPtr FileID，保留 PathID 和本地引用。单 Mod 继续回放已编译结果，不调用作者编译器。外部路径的隔离重命名按实际依赖记录定位；AssetBundle 名称字段按实际 schema 定位，不能沿用列表增长前的字节偏移。

LZY 与庄方宜的真实组合另暴露了原预加载列表合并的歧义：新增依赖可能与后面已有的依赖完全相同，旧序列算法误判为删除原区间。小型回归复现后，改为依据 m_Container、m_HashContainer、m_MainAsset 的原生区间边界分别合并，并优先识别保留原序列的插入。回归验证新增依赖仍落在所属 PFB 的区间，删除/显隐控制等非依赖列表规则仍按原有语义处理。

真实导出还发现 Python 包检查遗漏 compiled.bin，把编译产物当作额外文件拒绝。canonical ini_reader 与本机 Blender 配套模块已登记该文件；新增首次/重复发布回归，并在发布失败回滚用例中同时检查旧 compiled.bin 与作者资源的恢复。

当前验证：跨 CAB 材质、不同 Mod 的外部依赖顺序/去重、修改及新增对象的引用编号、原 GUID/未改对象保留、删除成员和空集、重复依赖与 PFB 区间，以及包名称字段移动回归通过。Blender 当前选中的三个庄方宜对象在隔离目录完整导出成功，保留工程的现有 skip 规则，生成 compiled.bin 4,190,371 字节；DLL 相同读取入口验证单 Mod 的 26 个输入/21 个根、移目录和空集恢复。现有 LZY 与该结果的隔离组合通过：2 个 Mod、84 个输入、63 个根、4 个共享输入。没有把测试 Mod 发布到游戏目录，也没有保存用户工程。

补丁已部署：游戏 eff.dll SHA256 `7A91C0E8DAA213C079734F02304A892D531A812E07AD2CE61EF5A60073098CAE`，Blender 自有 eff_resource_pack.exe SHA256 `5D5528318AD827FAF2FC8F06A13A67BC13F1B6F81353246EB72958EEDECA950B`。生产 v35 构建、v35/v36 完整重载事务回归、静态组装和组合回归通过；34 个既有 LZY/配置/协议文件再次校验未变。隔离验证副本的清理被自动审批以 blocked by policy 拒绝，暂留 bin/external-dependency-verification，未换工具重试。

实机边界：上述是离线编译和 DLL 磁盘入口的真实资源验证，不能替代庄方宜新 Mod 的游戏画面、动作、按键和 F10 验收。仍需正常重新导出到游戏 mods，再启动游戏检查。

## 2026-10-08 UI 页面项类型与角色切换

编码候选的实机 PID 32416 已启用全部 UI 接口，10:28:13 世界 F10 提交 generation 2；随后 UI 根 -342026 登记 controlsReady=15。10:28:16 的 UI F10 已记录 gameCodec=1、roundTripVerified=1、factoryReturned=1，说明编码与工厂加载通过；下一次 Lua 调用抛出 managed exception，仍未提交 UI 事务，原代 2 保留。此次日志尚未包含异常原文，不能据此写为 UI 显示、动作或刷新通过。

只读游戏脚本确认 m_phaseItems 是混合集合，含 PhasePanelItem、PhaseGameObjectItem 与 PhaseCharItem；PhasePanelItem 未声明 go，HL 的类型检查不等同于普通 Lua table 的缺字段返回 nil。旧适配器逐项读取 candidate.go。加入混合项与严格成员读取的生产 Lua 回归后复现 `HyperLua undeclared panel member: go`。这锁定了真实集合结构下的一条失败路径，但此前实机泛化异常未能证明其完整内容。

新候选使用游戏 HL.Is 与真实 PhaseCharItem 类型筛选，再核对模型、页面和加载器；栈按实际 BottomIndex/TopIndex 遍历，灯光映射通过游戏 HL.TryGet 读取可选成员。LuaFunction.Call 的异常通过 System.Exception.get_Message 保留原文，失败前仍保留旧人物。回归覆盖混合页面项、非默认栈下标、没有灯光字段的页面、连续绑定、显隐/动画和失败回滚。

用户提出切换角色再切回：游戏 PhaseCharInfo._RefreshCharModel 确实先 RemoveAllPhaseCharItems 再 CreatePhaseCharItem，已提交资源会在重新创建时使用。PhaseCharFormation 则使用 m_charModelCache，切换可能复用旧实例。手动切换不等于提交新 Mod 集合，也不能替代编队缓存与当前 UI 的 F10 验收。

类型筛选候选已在游戏退出时部署：eff.dll SHA256 `752B5861A4559B316AEF642FF5B6681DBACED892DE2044D61EC6566A1A2F5E85`（1,479,680 字节）。生产 v35 构建、v35/v36 完整事务回归、生产内嵌 Lua 混合页面项回归与 diff 检查通过；34 个 Mod/配置/协议文件前后校验未变。未重导出、未保存工程或修改游戏脚本。UI 画面和 F10 仍待实机，v1.3.2 未发布。

## 2026-10-08 UI 适配器脚本加载格式

签名修正版的实机 PID 36172 已启用 UI-LIFECYCLE 与 UI-PAGE-API；10:18:03 的 UI-READY 记录实际根 -322454，controlsReady=15。后续小键盘按键触发这些 UI Renderer 的显隐写入，与世界实例共用控制值。这证明 UI 控制登记与写入已发生；用户仍未确认最终视觉结果。

10:18:12 的 F10 在任何实例修改前拒绝，reason=reload native reference array coverage/limit，未提交新代。对应 Player.log F:2818 报告 LuaCypher.DecryptLua 对适配器明文发生 Base64 FormatException；实际 LuaDLL.xluaL_loadbuffer 会先调用游戏解密，随后 DoString 没有返回数组。该错误与 Mesh、Mod 编译结果及容量无关，原代继续使用。

新候选通过游戏 LuaCypher.GetKey 和 Security.XXTEA.EncryptToBase64String(String,String) 编码内嵌适配器，再用游戏 DecryptLuaString 与完整 String.Equals 验证往返内容一致后提交给 LuaManager.DoString。编码与工厂创建仅首次执行；不保存脚本、不硬编码密钥、不修改游戏解密流程。页面定位和绑定调用仍使用已缓存 LuaFunction，世界模型继续使用原组件生命周期，不经过此 UI 适配器。

回归先复现强制解密加载器拒绝明文、DoString 返回 null，再验证真实调用接收者、动态游戏编码 API、完整内容往返、不重复编码，以及内容不一致时保持旧页面且不执行工厂。各编码方法的实际签名与现有本机游戏元数据核对；模拟成功仍不能替代实机页面刷新验收。

编码候选已在游戏退出时部署：eff.dll SHA256 `19E069237A8EB98872D34CA7A1C2DB7558A33AB64D1DA8497A4C55DA079EFB20`（1,479,680 字节）。生产 v35 构建、v35/v36 事务回归、生产内嵌 Lua 页面回归与 diff 检查通过；34 个 Mod/配置/协议文件校验未变。未重导出、未保存工程、未修改游戏脚本。实机页面接管、显示和动作仍待验证，v1.3.2 未发布。

## 2026-10-08 UI 控制初始化签名

页面绑定候选的实机 PID 35724 在 10:06:04 记录 `LuaManager::DoString(System.String)->System.Object[] static=1` 签名不存在。该异常发生在 UIModelLoader 生命周期 hook 安装前，因此这次没有 UI-READY，也没有把 UI 网格置为 modelReady；用户观察到 UI 显示全部网格，按键无效。按键日志仅证明已登记实例的控制值在变化，不能证明 UI 已接管，更不能证明上一轮页面绑定成功。

只读元数据确认 DoString 是实例方法（flags=0x1e6），get_instance 是静态属性（flags=0x896）。修正为从游戏 LuaManager.get_instance 获取对象再调用 DoString。回归使用同样的真实签名布局，先复现初始化失败再通过，并检查实际调用接收者；此前仅注入模拟 API 句柄，漏掉了初始化绑定。

普通 UI 生命周期和按键登记独立于 Lua 页面接管 API：页面 API 缺失时仍登记 UI 控制，匹配它的 F10 在修改前明确拒绝。回归覆盖 UI 同步/延迟/即时完成的显隐、游戏禁用意图和 API 缺失保护。UI-READY 增加实际 rootId 与 controlsReady 数量，统计复用该实例加载完成时已有的子节点登记，不增加场景 Renderer 扫描。此次修正仍需实机确认 UI 显隐及 F10 页面接管。

本候选已在游戏退出时部署：eff.dll SHA256 `77434B3A7F83D60F3858380D87F4C046D608F789182903331D1CEC3F3C67D868`（1,476,096 字节）。生产 v35 构建、v35/v36 事务回归、生产 Lua 页面绑定回归和 diff 检查通过。34 个 Mod/配置/协议文件前后校验未变；未重导出、未保存 Blender 工程。实机显隐与 F10 仍待用户测试，未发布 v1.3.2。

## 2026-10-08 UI 页面接管与一次性协程

实机 PID 32072 的代理路由候选在 09:30:51 校验了新 Mesh/Texture，世界和 UI 共提交 generation 2，requestToCommitMs=90.1988，UI 新根 hierarchyActive=1。用户仍确认人物消失。因此此前“COMMIT/加载器字典登记即页面接管”的假设错误，不能写为视觉通过。

只读查看游戏 VFS 中的相关 LuaScripts：PhaseBase.CreatePhaseCharItem 的 LoadModelAsync 回调仅设置 modelGo 并调用 onComplete；coroutine.waitAsyncRequest 在 corInRequest 已清除后直接返回。BindGameObject、ChangeLayer、装饰/特效和页面初始化都位于协程后半段。回调重放不会再执行这些代码。诊断仅在内存中读取目标脚本，没有导出或修改游戏脚本。

新候选移除 F10 的一次性回调重放。通过游戏 PhaseManager 的实际页面项、加载器和原根找到 PhaseCharItem，以它的 BindGameObject 接管新根，页面项及编队缓存身份保持不变；公开模型/Animator/Transform API 恢复当前层、姿态、显隐、参数/播放状态、武器、装饰和已有灯光组。未知页面项在变更前拒绝。原生完成必须接受句柄且页面绑定必须返回成功，之后才卸载旧根；部分 UI/世界提交失败会回滚。

回归使用生产内嵌 Lua，复现一次性回调造成空白，再验证连续绑定、编队移动后的父节点、隐藏缓存、层/位置/动画/武器/灯光、页面退出和初始化失败回滚。原生事务模拟将回调改为只绑定一次，验证后续 F10 不重放它；页面拒绝新根时不提交且卸载新请求、保留旧人物。实机 UI 显示、切换及动作仍待验证，v1.3.2 未发布。

页面绑定候选已在游戏退出时部署：eff.dll SHA256 `F5BA2B1B599DE05A9882A68CBE8746D45A7422AD2835268BEF38551952BB70E5`（1,474,560 字节）。生产 v35 构建、v35/v36 完整事务回归、生产内嵌 Lua 页面绑定回归及 diff 检查通过。实际 UIModelLoader 完成闭包对空回调有显式判断，因此 F10 可不传旧协程回调。32 个 Mod 文件与 eff.ini/package-protocol.json 共 34 个文件部署前后 SHA256 一致，未重导出或保存用户工程。此候选尚未获得实机画面验收。

## 2026-10-08 UIModelLoader 控制与重载适配（历史候选）

v1.3.1 实机 PID 3184 连续世界 F10 已提交 generation 2、3，耗时分别为 64.5493、56.5488 ms；角色详情/编队 UI 的 F10 因缺少独立 UIModelLoader 生命周期拒绝，控制网格未进入 modelReady 状态。

候选 v1.3.2 接入 UIModelLoader 同步返回、异步完成、卸载、取消及 Dispose，按实际路径、父节点、委托结构和加载器字典记录所有者，不扫描场景 Renderer。世界和 UI 共用 Mod 控制值。没有可重放回调的同步 UI 实例仍在变更前拒绝 F10。

两次实机失败：PID 40880 的同步 LoadModel 加回调重放虽提交世界/UI 两个实例，人物消失；PID 36808 改用 LoadModelAsync 加 LoadImmediate，08:43:40 日志证明异步创建仍未完成，旧 UI 已释放，回滚失败，用户再次确认人物消失。两条路线均已移除。正常 UI 完成时 hierarchyActive=0，不能用这一次读数宣称画面失败或正常。原模拟将异步完成设为立即完成，漏掉了实机的跨帧执行。

当前候选先保留旧 UI 人物和页面引用，再提交原 LoadModelAsync；游戏完成后暂存实际完成委托与未跟踪句柄、校验所有 UI 候选，再执行原生完成逻辑。原生代码负责父节点、组件和页面回调；随后发布控制并卸载旧 UI 根。等待期间交还帧循环、恢复公开资源路由。取消、关闭页面、验证失败或新 F10 覆盖时保留原显示，原取消分支负责新请求的句柄释放；未完成的原生任务阻止候选退休。未写私有 UI/Animator 字段，未改作者协议。

回归先红后绿覆盖延迟数帧、原人物/控制保留、完成后才移交页面、候选验证失败、取消后迟到回调、关闭页面、混合世界/UI 的真实 Frame 推进、无关模型出现、新 F10 覆盖与请求排空；连续世界重载、动画/武器/音频、连续 UI、显隐及既有回滚保留。**候选仍待实机画面验证，未发布 v1.3.2，不能把原生模拟或 COMMIT 当成视觉通过。**

上一失败候选 eff.dll SHA256 `467DFAEA01FABCB144476052E5A20CBF618D80596E677766729C54BBF1AC3771`。跨帧保护候选曾在游戏退出时部署，SHA256 `C3239B5B08231ACCB2F3FC52CDA2D537F9A42CABB41A225224FE4D1FCAA1D93C`（1,454,080 字节）。生产 v35 构建、v35/v36 完整事务回归及 diff 检查通过。32 个 Mod 文件与 eff.ini/package-protocol.json 共 34 个文件部署前后 SHA256 一致。未重导出、未修改或保存 .blend，其实际结果见下方 PID 36636。

跨帧候选的实机 PID 36636：09:14:16.422 进入 UIWait，09:14:16.435 收到游戏完成回调；随后校验发现新 UI 根仍引用旧 Mesh/Texture，在任何原实例卸载前拒绝，代际仍为 1。用户只确认按了 F10、无法判断是否刷新，未报告该次人物消失。日志表明仅在 LoadModelAsync 提交期间提供候选不足以覆盖稍后执行的代理实例创建。

后续候选通过 FAssetProxyHandle 的公开完成注册及实际闭包关联这次 PrefabInstantiateProxy，在其稍后的 _PreloadPrefab 同步/异步流程中按实际代理身份和游戏路径哈希提供已校验的候选；普通代理和公开路由保持原状态。候选强引用持续到原生请求完成。补充回归先复现提交范围结束后实例取旧资源，再验证后续代理使用新 Mesh/Texture，其他代理/Clone 不受该临时路由影响。代理路由候选已在游戏退出时部署，eff.dll SHA256 `74346BADE87041879AA8A326B765DDFE47121661385CD75A828C79C6823195A8`（1,463,296 字节），34 个受保护文件前后 SHA256 一致。生产构建和 v35/v36 回归通过，实机提交、UI 显示及动作仍待验证。

## 2026-10-08 独立 Mod 编译与三端隔离

当前源码由 Blender 自带编译器为单 Mod 写 compiled.bin，DLL 不启动内嵌或外部作者编译器。真实 LZY 离线样本：62 输入、46 根，131.173 MiB 原资源 + 11.245 MiB 编译规则。原资源/INI 保持不变。已部署 eff.dll SHA256 6086A3DE0415A2145A6948ABA32B33DA576D43E275BED420632232E9505EECF5、LZY compiled.bin 758A7C4D31130C895BE775BC8DB61C43E72662BEFB2BB675273C776E713A6794、Blender 编译器 9BA8208CBA319E2688D9C760642DE436A54209D64542B20440DECFD2E704E73F。31 个现有 Mod 文件核验未变，未保存 .blend。旧工具目录清理被自动审批拒绝，仅替换新版文件，旧目录保留且不被新版调用。

回归覆盖同 PFB 两个 Mod 的新增对象/子节点、LOD/helper 列表、preload 区间、流偏移与对齐、删除恢复 pristine 及冲突拒绝；真实独立清单回放一致。相同作者内容的编译复用实测 1.69 秒，仅为编译器计时，非完整 Blender 导出或 F10 耗时。DLL ZIP 0.63 MiB、Blender ZIP 8.81 MiB、AnimeStudio net9 ZIP 12.23 MiB。原生模拟连续世界/展示重载、动画/武器/音频、回滚与退休回归通过。源码与离线证据不等于新 DLL 和新规则实机画面验收，不能把旧二进制成功结论套用到本轮。

# 热重载当前验收状态

## 当前静态 Mesh 配套部署（2026-10-08）

已部署生产 v35 DLL、游戏与 Blender 两处编译器，更新 Blender 的原生 Mesh 编码模块。插件通过既有 Junction 使用最新 Python 源码。32 个 Mod 与配置文件逐一 SHA 核验未变；检查时游戏目录下尚无资源候选缓存/current.tsv，未启动游戏或编译器发布新候选。未重导出、保存用户工程或写部署备份。协议与 INI 数据结构保持原样，AnimeStudio 已有静态 Mesh 来源字段，无需重部署解包器。新编译器的构建内容标识更新，防止复用缺少静态装配结果的缓存，不限制作者版本。

当前安装 SHA256：

- `eff.dll`：`84FBAE040151C388B16EAD7167161B8B3FD2C5CB96437D2E71D48B82736DDC1D`
- 游戏与 Blender 的 `eff_resource_pack.exe`：`A8854C8BD10F9BE027FF4AA61E6DBB702D148BBF32883B611B3AEA6BC9EA0811`
- 两处 `package-protocol.json`：`5B63FB0A4AD0AF985BC210A40EAB94514DFC14BC589BF58F38910ECDC43B443F`
- Blender 的 `nativepack/mesh.py`：`FC905D1A304A9EFA1D08E7475C668428F5FB9CAB06843A753E7E9520EAB07FA5`

静态目标按 MeshFilter 的真实 Mesh 引用、GameObject 组件表和唯一 MeshRenderer 配对，命中全部所属 PFB。无 MeshRenderer 的单独 MeshFilter 不算视觉消费者。0/1/N 部件、作者材质、preload、LOD/helper 列表和显隐沿用原流程。第一部件保留锚点，额外部件拥有自己的 MeshFilter；不伪造骨骼。混合静态/蒙皮节点保留原 SMR 地址序号，扩展 Renderer 用构建时的精确对象身份归属，避免把同节点的另一个原 Renderer 的部件重复归入。

通过的检查：真实序列化夹具先复现漏命中，再验证两个 PFB 的全部静态使用者、0/1/N、材质克隆、LOD/helper/preload、原静态 batch/额外流清理、未声明对象保留、孤立 MeshFilter 与混合组件归属；v35/v36 完整事务回归包含静态原生验收与显隐，原连续 F10、动画恢复、失败回滚也通过。原形态桥回归通过。5 项 Python 蒙皮流测试、10 项导出核心测试通过。读取已有游戏源包的真实 Mesh TypeTree，验证无 UV/有 UV 的静态编码和完整序列化回读；Blender 5.0.1 后台工厂场景验证实际 snapshot → 原生 Mesh 编码，包含清除未使用源骨骼元数据，不打开或保存用户工程。

**静态 Prefab 的游戏显示与 F10 尚未实机验收。** 这些夹具和真实 Mesh 数据结构检查不能代替游戏画面。静态 Renderer 不支持运行时形态键；资源构建支持不等于独立场景物件或人群 NPC 的 F10 owner 生命周期已适配，边界见[命中流程](runtime-resource-discovery.md)。

## 小型规则部署与来源记录（2026-10-07）

游戏 DLL、游戏与 Blender 编译器/协议、两处来源工具已同步小型规则链路。INI 保持唯一作者规则，离线确定全部目标，运行时读原游戏范围并在 Unity 创建资源/实例前应用。未做运行时来源查询、场景 Renderer 扫描或未知来源主动请求。Blender 插件为指向 `tools/Blender` 的 Junction，Python 修改直接生效；原生工具另外同步。

该次安装 SHA256：

- `eff.dll`：`B3411FFD8727550F98BF4F44216AC868C6745A2D1CFB710F96F4C855D113F6A3`
- 游戏与 Blender 的 `eff_resource_pack.exe`：`5BF111366B6F542CC0285F69584D9795A0368E2D7AD64A119D9488CA018C1015`
- `package-protocol.json`：`5B63FB0A4AD0AF985BC210A40EAB94514DFC14BC589BF58F38910ECDC43B443F`
- `mods/LZY/source-inputs.bin`：`002AA6B762053CB8C26664442B0D74349A6F411CBFBFC5F937F7E526B7CB606C`（63,063 字节）
- 来源工具 `EndfieldVfsProbe.dll`：`0867E41098B69A6803281AB37C3B4422807565AA5BA000589977496BAF3E24B3`

该次 current key：`63B98D54F667330E9A2C1E6D70CE715A1774B3676458AC4AA4087BAB3C204198`；manifest SHA：`B3A5ED48C573715F1AC21AA6CB5025E3410493F831F7C940EF227A41F3EAD988`。含离线复制指令的规则 4,416,538 字节（约 4.21 MiB），来源描述 63,063 字节。新代目录只有 manifest 与 ready 摘要，没有完整 .vfs 或 startup 目录。该次有 15 个作者 Mesh、6 个材质、11 个贴图声明、7 组规则。来源描述不复制全局索引。

游戏与 Blender 两处编译器已同步。本轮按用户要求迁移旧 Blender 工程并重新导出 LZY，更新为上述候选。部署 DLL/编译器期间 34 个作者、配置、协议及 current 文件逐一 SHA 核验未变，未写部署备份；随后作者资源及 current 由正常导出流程发布。

## 来源迁移与独立材质副本（2026-10-07）

用户提供新包 `E:/EIEM_Workspace/assets/beyond/dynamicassets/gameplay/actors/postmodels/characters/chr_0032_lizhiyan_postmodel.eff`。旧 `.eiem` 来源目录已不存在。通过 Blender MCP 按精确源 Mesh 身份迁移 80 个对象的来源与骨骼元数据，将 113 处原版贴图属性改为新包短路径；保留自定义 body/clothes PNG。所选 17 个对象、顶点坐标、拓扑数量、网格组与 Armature 绑定核对未变。

`MeshS_actor_lizhiyan_cloth_03_lod0_3.clothes` 单独使用 `MaterialM_actor_lizhiyan_cloth_03_original`。原游戏该材质确实引用 cloth_02 的 D/N/P/M 贴图及通用 ramp；其他部件的作者材质保持独立配置。

第一次完整导出暴露同一来源的两个材质副本被组合器拒绝。回归复现后，组合器按来源与完整副本内容复用相同对象，允许不同副本独立生成；模板查询遍历全部作者副本，序列化构建保留原材质并写出各独立 PathID，热重载材质预期按作者 ID 保存。原 Mesh 目标冲突仍拒绝。相同副本复用、同 Mod 原版/自定义副本、多 Mod 不同副本及冲突 Mesh 回归通过。

重新导出成功：15 个实际部件、6 个材质、11 个贴图声明、7 组替换规则、5 个切换组、LOD0–3。另 2 个选中对象按当前作者设置生成隐藏规则。完整导出含离线编译共 49.203 秒，不作为单独 Blender 编码耗时或 F10 耗时。作者目录完整回读及候选 SHA 核验通过；真实规则重建核对两个材质副本的完整字段、贴图绑定和各自槽元数据，保留原材质/未声明 Renderer，验证错误复制范围拒绝。规则回放载荷总计 411,933,317 字节，未保存完整包。

迁移工程另存为 `G:/zmd/lzy/1-eff.blend`（312,177,120 字节），旧 `1.blend` SHA 核验未变。游戏未自动启动，本轮新服饰材质、启动画面及 F10 实机结果仍待用户验证。此前世界 F10 的用户反馈不能当作本轮新候选的视觉验收。

## 当前工程路径与袖子切换复查（2026-10-07）

用户反馈导出报错，游戏小键盘 1 切换时只缺少 `.xiu` 袖子。当前打开的 `G:/zmd/lzy/1-eff.blend` 中重新发现旧 `.eiem` 对象来源、旧 baseline 目录、116 个缺失原版贴图属性，原版 `.clothes` 独立材质也不在当前场景；尚未确定这些旧值如何恢复。调用正常材质导出校验，在所选 5 个材质上复现 `texture path does not exist`。

按提供的新包精确 Mesh 来源和载荷名称重新迁移 80 个对象及骨骼来源元数据，修复全部 116 个缺失路径，保留现存自定义 PNG，恢复 `.clothes` 的独立原版材质。当前选择 16 个对象，输出 14 个实际部件、2 个隐藏规则、6 个材质、11 个贴图、7 组规则，完整导出及保存 48.922 秒。坐标、顶点/面数量、组名称/索引和选择核对未变。保存工程后用独立后台 Blender 从磁盘重新打开，确认缺失贴图属性为零、`.clothes` 原版材质和 `.xiu` 新来源已持久化；不依赖当前 MCP 场景状态宣称保存成功。

实机 PID 32856 使用上一候选 `284BE5...`。日志确认 `.xiu` 已绑定且初始关闭；10:52:00 小键盘 1 已改变状态为 1，其他衣服关闭，但没有袖子重新启用的记录。因此不能将问题归因于未导出或按键无效。真实 LZY 按键回归通过（三状态、两个实例、原生禁用、晚加入实例、偏好持久化）。曾构建未部署的袖子诊断代码；随后原 DLL 的新日志已显示袖子实际启用/关闭，移除该临时观察，不增加运行时持续日志。袖子画面仍未宣称通过用户验收。

随后用户要求再次导出。MCP 读取当前 17 个选择，所用 6 个材质没有缺失贴图，`.xiu` 包含在 NUMPAD1 状态 1 且没有隐藏导出标记。相同完整导出入口成功，15 个 Mesh、6 个材质、11 张贴图、7 组规则，11.291 秒，选择未变；current 为 `63B98D54F667330E9A2C1E6D70CE715A1774B3676458AC4AA4087BAB3C204198`，manifest SHA `B3A5ED48C573715F1AC21AA6CB5025E3410493F831F7C940EF227A41F3EAD988`。11:04:44 上一候选 F10 已提交 generation 2；11:06:39 新候选 F10 被 `reload model part load ownership was not recorded` 拒绝，仍保留 generation 2。11:06:41–42 袖子 5 个 Renderer 记录了 enabled=1/0 的实际循环，按键与绑定此时有效，尚无用户画面确认。不能将成功导出等同于新候选实机提交，也不能认为全部 F10 已通过。

## LoadModel 清理阶段误删新归属（2026-10-07）

当前游戏文件的只读元数据/原生代码核对：`BaseModelViewPart.LoadModel` 在写入新 config/callback 前，以虚调用槽 23 调用 `ReleaseModel`；Base 与 Complex 的 `ReleaseModel` 均为槽 23。`SetupData` 仅写配置，没有同一释放调用。这些 RVA/槽检查只用于诊断，不进入产品地址解析或角色规则。

DLL 的 `PartLoading` 在原调用前记录新委托所属组件，而 `PartReleased` 无条件删除整个记录；真实清理顺序导致随后完成回调留下 root/path 却没有 ownerRecorded，F10 因而拒绝。此前 mock 的原 LoadModel 没有模拟该内部释放。增加同步/异步、世界/展示四种组合的真实清理顺序回归，先得到 `LoadModel's cleanup erased its pending ownership record` 失败，再修复。加载调用的局部作用域只保留当前新请求的归属；旧 root/path 与控制登记仍退休，独立 ReleaseModel 仍删除完整记录。作用域支持嵌套加载，不按角色、场景、Mod 或路径例外处理。

v35/v36 事务回归通过：异步清理后仍阻止未完成加载，完成后的世界/展示归属正确；每种组合连续两次 F10 提交，世界动画两次重新绑定；独立释放无活跃记录残留。保留缺失归属拒绝检查。当前实机旧日志没有指出具体部件 ID，因此确认的是与拒绝完全一致的 DLL 缺陷及修复回归，尚需新版实机验证是否仍存在其他漏记入口。生产 v35 DLL 构建成功，用户退出游戏后已部署，SHA256 `2A2AAE7173E7C3B4A4576796FEC9406B9EF9D0DAD3F22E4E9E49D757FE75745E`。部署只替换运行时 DLL，36 个 Mod、偏好、配置、两处编译器、协议及 current 文件逐一 SHA 核验未变，无备份；游戏未自动启动。下一步为角色详情/编队页与世界场景连续两次 F10、袖子三状态切换及动画实机核验，不能以 mock 代替。

## 初始显隐污染游戏 Renderer 基线（2026-10-07）

用户冷启动后反馈 NUMPAD1 不显示 `.xiu`、NUMPAD4 无法切换 `.dou`。实机 PID 32200（构建 11:28:48）日志记录两个部件均已绑定、初始作者状态为隐藏；NUMPAD1 的 0/1/2 与 NUMPAD4 的 0/1 按键值正常变化，控制绑定持续为 70，没有相应重新启用记录。此会话未记录 F10 提交，不能作为上一归属修正的连续热重载实机验收。

只读检查当前游戏代码，BaseModelViewPart.PostDealLoadedModel 读取 Renderer.enabled 并保存初始数组，后续显示/恢复依据此数组。原 DLL 在更早的 EntityMaterialController.Init 中就应用作者显隐，有机会使游戏把默认隐藏的部件保存成原生禁用。针对真实 LZY 作者包，回归调用生产 Init，再模拟游戏保存/重放 enabled 状态，先复现 `material Init hid an author part before the game's initial Renderer snapshot`。

修正将材质初始化登记标为尚未完成模型；这一阶段的帧更新、原生 setter 及仅控制配置重载均不施加作者显隐。游戏完成回调登记准确模型根后，采样最终原生 enabled 状态再应用作者规则。重复 Init/完成回调不重新采样作者已隐藏的状态，真正的原生禁用仍保留。此处理没有按角色、部件名或按键例外，也没有新增场景扫描或修改游戏私有初始数组。

真实 LZY 控制回归覆盖提前 Init、完成前帧/配置重载、直接原生禁用、完成时采样、重复初始化，以及两个模型实例的全部 NUMPAD 状态、三值循环、偏好与晚加入实例，均通过；v35/v36 的连续世界/展示重载、动画消费者、回滚和归属回归也通过。生产 v35 构建成功。确认游戏已退出后，仅部署 eff.dll，SHA256 见顶部；39 个作者、偏好、配置、编译器、协议、current 与当前候选文件逐一核验未变，无备份。Blender 工程与导出资源未修改。冷启动的 xiu/dou 视觉以及 F10 后按键/动作仍待用户实机确认，回归通过不能代替画面验收。

用户随后回复“现在没问题了”，确认本轮 xiu/dou 切换问题恢复。新会话 PID 25692 的日志也记录两者 authorVisible=1、nativeEnabled=1、enabled=1，仍为 70 个控制绑定。本次确认不扩大为其他角色/NPC、多 Mod 性能或所有生命周期分支的验收。发布 v1.3.0 构建会更新版本元数据，但不自动覆盖已安装游戏 DLL。

## 本轮 37 秒与 T pose 修正

旧 DLL 实机 PID 33924 在 05:33:30.931 收到 F10，05:34:05.789 完成 native 加载，05:34:05.861 提交。日志给出的请求至提交为 37,106 ms，其中 nativeLoadMs=34,844；一次主线程提交约 46.9 ms。旧实现为 46 个离线 PFB 全部请求来源租约和候选模板，包括未访问角色；验证累计主线程约 2,028 ms。实机流入口已经成功，不能继续将此状态记为加载入口待确认。

同一会话世界模型记录 `currentWorldOwners=0 liveDisplayOwners=1`，没有动画重绑定；Player.log 持续出现 CharacterAnimationBlackboard._CalClothAddParamater 的空引用。静态检查游戏真实同步流程为 `CreateMainPart -> AddPart -> SetupData -> StartSyncLoad`，不调用携带 delegate 的 LoadModel。增加真实 AddPart 主部件归属记录后，模拟该完整同步链的测试先失败后通过，连续 F10 仍按世界流程恢复动画。尚不能从 mock 证明实机 T pose 已修复。

新实现在已有模型完成加载后提前准备 native 候选，仅加载游戏已经访问的 PFB 模板，F10 前不发布路由或代际。未访问目标仍保留完整离线清单，在游戏自己的 PrefabInstantiateProxy 加载入口按精确来源 hash 和对象处理；资源句柄与实例句柄明确区分。骨骼验证复用同一次验证中已强引用的节点，避免重复托管存活检查；256 轮 mock 验证的 Alive 调用从 5,120 降至 0，不能将 mock 毫秒当实机性能。

离线编译器记录最终成员的来源/作者复制段、差异字节和摘要，并记录 CAB/外部引用/流路径等宽修改位置。DLL 对新描述不再建立 TypeTree 资源索引或运行 Mesh/Renderer/材质构建器。13 个作者 Mesh 的真实样本回放：旧路径索引约 3,178 ms、组装约 28,502 ms；复制路径初始化约 0.20 ms、组装约 2,292 ms，同一轮完整审计路径约 27,881 ms。62 个隔离输入全部逐字节一致，载荷合计 411,236,479 字节；原材质及未声明 Renderer 对象原字节保留、错误复制范围拒绝。用户新导出的 15 Mesh 集合回放初始化 0.164 ms、组装 2,288 ms，载荷合计 412,388,711 字节，作者资源 137,467,902 字节；作者字节及资源元数据核验通过。纯回放不调用 Unity。

13 Mesh 样本的完整结果逐包 XPRESS-Huffman 压缩试验合计 111,048,001 字节（约 105.90 MiB），未保存该完整副本，也未将通用压缩封装当 Unity 可加载格式。该复制方案降低运行时解释成本，但仍需要原包解码、内存复制、完整摘要与 Unity 加载，预热未完成时不保证 F10 毫秒完成。

v35/v36 完整事务模拟（连续八次 F10、同步/异步主部件、展示部件、后续角色的 native proxy 加载、实例句柄不路由、回滚、资源退休）通过；命名修改、压缩清单往返、旧规则兼容、复制范围、展开顶点流与损坏摘要回归通过。生产 v35 构建并部署完成。

实机 PID 13672 确认 `FULL-RELOAD-READY nativePrepareBeforeShortcut=1 unusedRootsLazy=1 nativeMainPartOwners=1`，流/新代理及 AddPart 绑定成功；后台准备 3,822.7 ms。提前准备只选择游戏已加载的 1 个 PFB，完整离线目标仍为 46；nativeLoadMs=12,125，发生在 F10 之前，没有提前发布路由或交换代际。

该会话第一次 F10 请求至提交 2,888.65 ms，提交帧主线程 48.97 ms；重新导出后的第二次为 4,952.17 ms，提交帧主线程 50.88 ms。两次均记录 `currentWorldOwners=1 liveDisplayOwners=0`，动画及武器重绑定已执行，提交 generation 2/3；不能由提交日志推定视觉动作正常。总延迟仍含 F10 每次启动外部编译器的 2,812/3,046 ms，第二次还含新候选 native 加载 969 ms。因此这些结果尚未满足完整 F10 毫秒验收。

最终部署 DLL 已移除上述重复编译：F10 先核验完整当前集合与已准备的候选，未变时直接请求提交；成员增删或作者变化仍调用同一集合编译流程。新增回归以无法启动的占位编译器证明有效候选不启动进程，并确认新增 Mod 不能误提交旧集合，先失败后通过。

上述会话同时出现 CharacterAudioComponent.SetAudioObjectPosition 空引用，记录 `audio=0`。游戏自己的模式切换使用 `Entity.TryGetComponent<CharacterAudioComponent>`，通用 `get_audioCom` 返回的是另一种 AudioComponent。最终 DLL 改为按实际 `System.Type` 查询 CharacterAudioComponent 并调用 InvalidateTransformReference；通用组件与角色音频组件不同的回归先失败后通过，v35/v36 事务回归和生产构建通过。

最终 DLL 的新冷启动 PID 9716（构建 06:48:22）已确认全部重载绑定就绪，包括新增类型查询。后台字节准备 3,553.88 ms，native 提前加载 1 个已访问 PFB，9,922 ms 后完成；这些工作发生在 F10 之前。06:54:05.271 请求、06:54:05.338 提交，完整请求至提交 **67.49 ms**、提交帧主线程 **44.29 ms**，此次 F10 没有启动外部编译器。世界 owner 仍正确为 1，`animation=1 weapons=1 audio=1`，generation 2 提交成功；检查时 Player.log 没有先前的角色动画、角色音频空引用。

用户随后反馈世界模型 F10 正常。该反馈与上述世界提交相符，但不能代替 UI、NPC 或其他控制器验收。native 提前准备仍有成本，用户在其完成前按 F10 仍需等待，不应宣称任意时机均为毫秒。

## 角色详情/编队 UI 与活动候选重试

同一 PID 9716 在 UI 切换后，06:55:53 的 F10 因 `live Renderer has no recorded game model-part lifecycle` 拒绝；06:55:57 再次请求，06:56:03 因 `reload candidate Bundle load failed` 拒绝。两次都没有提交，不能把 UI 自身刷新记为 Mod 热重载成功。

当前修正版观察基础及 Complex 部件完成，并在游戏 ReleaseModel 后清除该根的控制登记；原生捕获持有的快照仍保留到自己的释放入口。展示释放使用 `il2cpp_object_get_virtual_method` 派发到实际部件实现。另修正已提交候选重复 F10 预检失败后被标记 Reject/回收的状态错误：活动图保持 Passed，下一次请求复用原生资源。对应回归覆盖已提交图预检拒绝/重试、仍存活的池根注销以及真实派生释放方法，v35/v36 完整事务测试通过，生产 v35 构建并部署完成。

**这版的角色详情/编队 UI 实机验收仍待冷启动验证。** NPC 目标 PFB 已在离线范围，但人群 NPC 使用独立 NPCAvatarManager 装配链，当前 F10 未接入该实例链。不能以世界模型或 mock 通过宣布所有 NPC 已覆盖；实际区别及后续要求见[来源与生命周期边界](runtime-resource-discovery.md)。

## 启动就绪顺序修正（2026-10-07）

用户反馈失败后，实机 PID 29420（04:53–04:54）确认规则候选已读入，目标文件请求也已命中，但 `SHAPE-RUNTIME-READY enabled=0`、`generation public load APIs unavailable`，随后 18 次命中均报 `runtime rule memory bindings not ready`，实际提交为 0。故障发生在资源提交之前，不能归因于新增 Mesh 或规则目标缺失。

`InstallHooks` 在安装捕获完成 trampoline 之前调用 `RuntimeShapes::Arm`，缺少这一必需绑定使控制模块保持禁用，随后的 `NativeBundle::Install` 也拒绝启动。修正将捕获完成绑定放在 Arm 之前，内存加载绑定仍在文件加载钩子之前。`ENTRY-READY` 增加 `ruleMemoryArmed`，并将规则加载就绪计入 `replacementArmed`，避免只统计旧文件替换表。

新增 `startup_readiness_smoke` 调用生产 `InstallHooks`，以已准备的控制桥和真实待安装的捕获完成元数据模拟该日志状态；原代码先复现控制模块仍禁用，修正后 v35/v36 均通过，确认 sync/async 内存绑定及控制模块在文件钩子启用前就绪，并且绑定过程未主动发起原生请求。它不代替真实客户端的全部接口绑定或视觉验收。原 v35/v36 完整事务回归通过，生产 v35 构建成功。

游戏停止时仅替换 `eff.dll`，29 个 Mod 文件及 eff.ini、编译器、协议、current 指针共 33 个文件 SHA 保持一致，部署暂存已消除，无备份。该安装的实机效果和 F10 验收仍待冷启动检查，无需重导出。

## 内存入口拒绝游戏归档与加载页等待（2026-10-07）

PID 5600（05:07–05:09）确认上述启动顺序修正生效：控制、NativeBundle 与完整重载均就绪，提交 17 个规则请求。但 Player.log 随后报 `Failed to decompress data for the AssetBundle 'Memory'`、`mem:/CAB-.../__data` 归档头读取失败、资源包加载失败，最终进入 `WaitCharacterTimeout`。没有收到 F10；离线准备的候选仍在 AwaitRequest，不能将这次卡住归因于 F10 或待构建的清单。

只读核对当前 UnityPlayer 调用链，内存入口先进入整包解压，再通过临时 Memory 归档读取；流入口读取可寻址归档。现将运行时构建的原游戏格式字节包在只读 `System.IO.MemoryStream` 中，经 `LoadFromStreamInternal`/`LoadFromStreamAsyncInternal` 提交，启动与 F10 共用此路径；不生成完整 .vfs/PFB 文件或修改原包，不改规则或作者数据。当前客户端裁剪了对应 managed 入口，按游戏已有 engine icall 名绑定。只读 metadata 和代码模块表另确认 MemoryStream 构造函数、ManagedStreamHelpers 的 Read/Seek/Length 均有编译实现；产品绑定按字段/签名核验，不使用诊断 RVA。

流和其数组在异步请求期间保持强引用；请求完成后交给 Bundle 生命周期保留，直到 Unity 对象失效后回收，避免完成即释放导致后续定位/读取失效。完成轮询在调用托管 getter 前释放锁，允许回调重入。失败完成记录 `ENTRY-RULE-LOAD-FAILED`；此轮没有声称解决任意损坏 Mod 的异步失败回退。

生产启动/运输回归先在原入口复现 Memory 解压错误，再验证流入口、Byte[] 数据、只读构造、三参数 icall、异步及 Bundle 延迟读取生命周期、卸载回收、完成回调重入。v35/v36 均通过，原完整事务回归也通过，生产 v35 构建成功。游戏停止时只替换上述 DLL，33 个受保护文件 SHA 未变，无备份或部署暂存。**修正版尚未取得实机冷启动及 F10 验收，模拟和静态接口存在检查不证明视觉成功。**

文件快照现在只针对显式选择的诊断范围，普通启动不再自动写四个资源副本。本次 run-5600 的 input-1 至 input-4 清理被自动审批拒绝（工具仅返回策略阻止），仍在原目录；没有改用其他方式删除，也没有将它们记为已清理。

## 本轮验证

- 真实 LZY 样本：46 个 PFB，1440 个唯一序列化 Renderer 地址；62 个必要范围均能在运行时用同一构建器还原启动/隔离字节，与离线 SHA 一致。原材质与未声明 Renderer 的原对象字节保留；错误 Renderer 地址拒绝。
- 规则引用作者资源，完整 Mesh/贴图字节不嵌入清单。借用材质克隆为作者对象，原材质模板保留。真实规则还原出的完整 native 载荷合计 411,296,181 字节，这是内存构建载荷，不是规则文件大小。
- 真实资源隔离测试：完整作者首次生成小型规则；同内容复用保持 current；拆成两个非冲突 Mod 合并共享 CAB/PFB，删除成员及空集合由保留的原范围统一重建。未修改生产作者目录；这不是第二角色实机验收。
- v35/v36 完整事务 mock 通过：归档流参数与内容、裁剪后的 engine icall、异步及 Bundle 存活期间的流保留/卸载回收、提交后释放重复 worker 字节、未命中/非零 CRC/原来源缺失的原文件请求保留、public/internal 加载入口；连续八次 F10、模型回调归属、动画重绑定、rollback 和旧容器退休。
- Blender 11 项离线调用（含新编码拒绝旧 reader）与 11 项发布回归通过。兼容判断按编码与字段，不按产品版本号。
- 正式磁盘冷核验约 58.9 秒，隔离重复冷核验约 57.8 秒；同内容复用约 1.74 秒。首次或修改后的规则生成仍做完整内存核验，没有宣称导出瞬时完成。
- 当前客户端的归档流 managed 方法被裁剪；UnityPlayer 的 sync/async engine icall 和托管流回调有对应编译实现，按名/签名绑定，未写死 RVA。下一次日志需确认 `NATIVE-BUNDLE-READY archiveMemoryStream=1` 及实际 Bundle 成功返回；旧 Memory 入口的“绑定成功”不证明该加载方式适合游戏包。

## 剩余验收

**当前小型规则版尚未完成实机验收。** 已请用户冷启动查看首次效果，再连续两次 F10，核对模型、动作与按键。候选发布、内存规则一致和 COMMIT 都不单独证明视觉成功。现有 LZY 无需重新导出。

静态 MeshFilter、跨文件 PFB 根、persistent pool、未知生命周期和多角色实机边界见[命中流程](runtime-resource-discovery.md)。不同 LOD/模板中的作者像素重复构建仍存在，清单小不代表运行时内存小；提交后释放 DLL 的 worker 字节，托管流及其数组保留到 Bundle 卸载，原生资源仍由 Unity 生命周期管理。

旧全量基线 inputs、阶段缓存和旧代目录仍在磁盘，新规则不读取它们；尚未将磁盘残留当作已清理。当前没有新增完整原包/PFB 副本。

## 下列为历史动态试验与验收记录

下列旧哈希及“当前”表述仅属于当时记录，以本页顶部静态部署为准。

当前安装 DLL SHA256：`1809815463871A45AD5CC445B3A4C86EDEC126FB315E60EE177E37D7087312F5`。已撤掉新增的主动来源加载及代理绑定，恢复只读游戏清单/VFS 查询；保留既有模型重载租约和查询完成前的清单摘要核验。生产构建、只读来源回归及 v35/v36 事务回归通过，只替换 eff.dll，31 个 Mod、配置及配套文件哈希未变，无部署备份或暂存文件。

最新实机 PID 18252（19:43–19:45）未生成或提交候选，用户反馈停在加载页。新增主动请求报同一身体材质包加载失败，Player.log 也记录错误，栈进入场景加载的 `WaitForAllAsyncOperationsToComplete` / `_ForceOperationFinish`。撤回旨在消除对共享加载队列的干扰；卡住的唯一原因及撤回后的实际表现尚未验证。目标包 VFS 映射缺失、Mod 无效果仍未解决，本轮不能标为成功验收。

最近实机 PID 15816（2026-10-06 19:04–19:06）仍无 Mod：游戏路径处理前后相同，身体材质包键 399D7E8C974C2918 无 VFS 映射，submitted=0；未发现新的 Windows 崩溃事件。因此前一版“补路径处理”没有解决当前故障。下文前轮 DLL 摘要及模拟通过只记录当时证据，不是当前实机验收。

本轮新增[运行时按需来源发现](runtime-resource-discovery.md)：从游戏现有 RuntimeManifestBinary 查询 INI 路径、包级反向候选、正向依赖与 VFS 范围，仅解析相关包确认 Mesh/Prefab。全游戏扫描代码已撤掉；其诊断索引的删除被自动审批拒绝，留在本地 `bin/source-discovery/real-cache`，不参与新流程或部署。

用户反馈 Mod 无效果后，PID 37504 日志直接确认：旧 DLL 读取了 LZY，但启动 `ENTRY-SUBSTITUTIONS absent`、`nativeSubmission=0`，F10 只执行控制配置提交，`submitted=0` 和 `liveRenderers=0`。当时编译器、来源表和 current 清单均缺失，资源替换没有启用。用户退出后已同步新 v35 DLL、游戏/Blender 编译器与协议，并同步 Blender 来源等待处理模块；29 个作者/配置文件哈希未变。下文旧集合记录不证明当前候选已构建。

PID 26440 的针对性诊断确认 StringPathHash 数值转换一致，托管/原生 GetFileInfo 对身体材质包清单键都返回未找到，managerInitialized=1；这不等于证明对应 VFS 条目已挂载。PID 18312 的完整路径重算仍失败。随后只读两个指定 BLC 的目录元数据，确认同一包的存储键就是清单 hashName=399D7E8C974C2918，原始名称 Data/Bundles/Windows/main/...ab；此前重算值 FDE40BD4AE70C1BC 不符合该记录。已撤销“补包根重算身份”逻辑，恢复游戏 BundleProxy 实际使用的 hashName 原值查询。对应回归先复现拒绝再通过。17:58 PID 41068 在新增私有集合诊断中闪退，当前已移除该读取，只保留已绑定方法及同步加载上下文对照。此前多轮部署未恢复 Mod；当前仍没有首次 current 或真实资源提交。启动和 F10 都调用同一 Prepare → PollOffline → 编译器 → Unity Frame 提交链。此前已成功的贴图会话使用预先准备的来源和候选，没有经过新增运行时查询。

随后用户确认 PID 41956 正常进入但仍无 Mod 效果。18:13:03 的 DEBUG-catalog-context 证明查询实例与刚成功同步加载包的实例相同，且已加载键 D5034A08B83E6580 再查为真；目标材质包键 399D7E8C974C2918 仍为假。本次会话未见 F10，尚无来源表、current 或资源提交。用户说明材质可借用其他未出场角色的资源：查询不应依赖场景角色或 Renderer。

静态核对游戏的字符串 Load/LoadAsync 入口，确认会先执行 BundleResourceManager._PreprocessAssetPath，再查清单；此前来源查询跳过了这一步。现调用游戏相同方法，使用与重载资源租约相同的实际 RootCategory.Main 枚举字节，不自行拼平台/语言路径。借用未使用材质的 mock 回归先复现原路径关联未映射基础包的拒绝，再验证游戏处理路径关联的有效来源；该夹具没有模型或 Renderer。来源查询回归、v35/v36 事务回归及 v35 生产构建通过。游戏停止时仅替换下列 DLL，31 个 Mod、配置、编译器及协议文件哈希一致。新增 RESOURCE-CATALOG-ASSET 记录原路径/游戏处理路径/包键；本机该材质是否发生路由变化、是否恢复来源准备与 Mod 效果仍待实机验证，不能用夹具结果代替。

2026-10-06。游戏安装目录为 `D:/Hypergryph Launcher/games/Endfield Game/plugin`，原有 `reload=F10`、`gui=INSERT` 配置保留。当前已部署贴图属性接口、连续重载模型归属、游戏加载入口记录及资源路径处理修正的 v35 DLL，F10 所有者发现不再扫描场景 Renderer。编译器和协议结构保持兼容。Blender 安装目录为 `C:/Users/25487/AppData/Roaming/Blender Foundation/Blender/5.0/scripts/addons/EIEM_Blender`，此前 151 个配套文件逐一校验一致，包含 AuthorCore DLL、编译器与依赖。构建、磁盘准备及后台全部资源导出验证通过；当前路径修正尚待新游戏会话验证，不把事务提交或 mock 通过当成实机动画验收。主源码默认 v36 的装配地址清单未部署。

当前安装文件 SHA256：

- `eff.dll`：`33AA615AFFAFC4E13182DE19E20D5C425C92EB46D9749BA2E85F810072D80F81`
- `tools/eff_resource_pack.exe`：`40D80AF68304D104E565454C537FC3C278A82FB42CB057BE9C73B68CED80024F`
- `package-protocol.json`：`C8B074C398B6AA8F5383BE74BF0AFA0AB69389093758A43D8D4F07D8D8C1EADD`
- 游戏根 `d3dcompiler_47.dll`：`486DAF907D78D19093557BAF6AA2CF11BCDC952DB5E185387FFC28388844658A`
- 游戏根 `vulkan-1.dll`：`7561CCA3A7BEEEC8D3971535AB95BE92118938475EF238A51CCB9D2E028F2911`

历史安装集合 key 为 `21C929971382CD6B6DCDDDE52F73B981F322F7C214968F4E022B952CE382BD59`，本轮安装首次 current 尚未生成。更早集合 `7936CDE53F3DBADB4A806317533898C7C44B5289ED67DFE9FD616958BA9EB833` 的安装磁盘入口验证完整 68 个输入、60 个启动替换输入、18 个锁定依赖与编译控制图一致性，`UnityCalls=0`，初次验证 1027.79 ms。20 次无改动请求的后台准备平均 0.879 ms、最慢 1.148 ms；这些数值只对应此前输入，不包含原生模型加载或游戏提交。

恢复存储包键的来源查询回归通过，加载入口回归使用临时的 UnityFS 签名字节夹具验证原调用、返回值、TLS、同步加载上下文和失败加载不覆盖上下文；夹具已清理，没有使用 Endfield 加密包冒充标准 UnityFS 夹具，也不把入口模拟测试当成真实包解码。旧序列化负例的提示断言从人为 count limit 更新为真实 input extent，生产字节范围检查未改。

17:58 PID 41068 的 Unity 符号栈直接定位到 MappingCollectionCount → Exact → il2cpp_class_get_methods；Windows 事件为 0xc0000005、GameAssembly.dll 偏移 0x408f2。闪退由新诊断引入，尚未进入候选构建或游戏资源提交。已移除未经类型核验的私有集合字段读取及计数日志，不猜测字段布局。生产查询代码的 mock 回归先拦截并复现这段读取，再验证移除后返回来源映射错误，诊断不能提交候选，后续有效请求仍正常完成。v35 生产构建及来源查询回归通过；游戏进程停止时原子替换上述 DLL，31 个 Mod、eff.ini、编译器及协议文件哈希未变，无备份或遗留部署暂存文件。实机不再闪退及原 Mod 效果仍需分别验收。

游戏 PID 14600 的实机日志确认旧 DLL 在初始化时拒绝缺失的 `Material.GetTexturePropertyNames()->String[]`，随后两次收到 F10（`vk=121`）均以 `requiredNativeBindingsUnavailable` 拒绝，因此停留在 generation 1；导出候选本身已更新。当前游戏元数据仅保留带 List 参数的名称接口，同时保留 `GetTexturePropertyNameIDs()->Int32[]` 与 `GetTexture(Int32)->Texture`。修正使用后两个公开接口，仍完整比较候选中非作者部件的 Shader/Texture 对象身份，不硬编码贴图属性名或跳过验证。先以缺失无参接口的元数据夹具复现同一错误，再修正；v35/v36 事务回归通过，覆盖精确重载接口绑定、有符号/零 ID、空属性、null Texture、多属性变化、重复 ID 和数组元素布局拒绝。游戏退出后原子替换 DLL，并确认当时 27 个作者文件、eff.ini 与 current.tsv 哈希保持一致。随后 PID 36120 已确认初始化 `[FULL-RELOAD-READY]` 及两次 F10 提交；该会话出现下述动画问题。

## F10 后整个人物 T pose

用户确认症状发生在游戏按 F10 后，整个人物不播放动作。Blender 当前导出的 16 个 Mesh 都有启用的 Armature，实际使用的权重组没有缺失骨骼；这不能证明所有几何与权重正确，但没有发现新增网格未绑定骨骼的证据。

PID 36120 的 09:13:58 首次提交捕获 1 个世界所有者，并执行 `ChangeAnimationConfig(data, true)`。09:16:01 第二次提交却把同一个世界模型根当成展示部件：`currentWorldOwners=0 liveDisplayOwners=1`，没有动画重绑定记录。之前的重复 Frame 测试每轮重置模型根、动画与 helper 状态，漏掉了这种连续重载的归属变化。

本次回归保留前一次提交的模型与动画状态，模拟新 helper 没有 entity 链接，先复现“第二次重载提交但动画仍绑定旧根”。修正从游戏 `BaseModelViewPart.LoadModel` 的完成委托公开 `Target` 记录弱的组件归属，随后以组件/entity、当前 mainPart 与模型根交叉核对。游戏会在完成通知前清空委托，因此在加载入口记录，在 `PostDealLoadedModel` 完成根记录；未完成的异步记录保持弱引用。世界所有者统一执行已有动画、武器、音频与跟随特效生命周期，普通展示保持其部件生命周期，不使用角色名或路径后缀分类。委托按实际声明类型与 `Invoke(Boolean, BaseModelViewPart)` 结构绑定。

v35/v36 原生 mock 事务回归通过，包括连续 8 次不重置状态的重载、异步完成、普通展示委托、已退休所有者拒绝、当前 mainPart 核验、不同委托参数结构拒绝及已有失败回滚。生产 v35 构建成功，仅部署新 `eff.dll`；游戏作者文件、eff.ini、current.tsv、编译器与协议共 34 个文件哈希一致。没有修改或保存用户 blend，也没有重导出覆盖资源。

实机验收仍待：冷启动游戏，先确认动作正常；在同一角色世界场景按 F10，提交后检查走动与攻击，再按一次 F10 重复检查，随后进入展示并返回世界。日志应始终把世界实例记录为世界所有者并执行消费者重绑定；COMMIT 本身不代表动画播放通过。如果第一次 F10 仍停止动画，需要继续核对实际动画播放与模型完成通知，不能凭本次 mock 结论宣称已排除。

## 重新导出后 F10 被归属预检拒绝

PID 20160 于 11:51:40 已确认加载/完成钩子和 `[FULL-RELOAD-READY]`。11:54:36 离线准备新集合 `21C92997...`，11:54:39 收到同集合请求，11:54:40 却以 `reload helper/model owner differs` 拒绝。导出已经发布；失败发生在场景 Renderer 的 helper 反推预检，游戏继续保留旧代际。没有把此拒绝解释为编译、按键或导出发布失败。

先用附属 Renderer/helper 夹具复现同一拒绝，再移除 F10 的全场景 Renderer 发现及 helper 反推。所有者统一取自游戏加载入口的弱记录：世界加载通过 LoadModel 委托 Target 定位组件；展示的直接 SetupData → StartSyncLoad 路径也记录。记录覆盖无 Mod 的角色，按当前候选的精确资源路径选择需要迁移的实例，不依赖当前角色、当前控制目标或已有作者 Renderer 绑定。Prefab 内 Renderer 和资源身份的校验仍保留。

v35/v36 原生事务回归通过：附属 helper 不再干扰世界归属、未记录入口的完成记录拒绝、直接 SetupData 展示路径、两个角色先以原版加载后新增第二角色 Mod、仅第二角色候选，以及连续 8 次世界重载和原有回滚。另有闲置路径池清理和提交后原生实例化路由回归。生产 v35 构建成功，仅复制新版 eff.dll；32 个作者/config/current/编译器/协议文件 SHA256 保持一致，没有修改 blend 或重导出游戏资源。

冷启动后仍需实机核对本次集合能提交、连续 F10 动画和世界/展示转换。多角色夹具通过不能替代第二角色实机验收；当前 pristine 来源仅覆盖 LZY，新增其他角色须补齐对应来源资源和精确 Prefab 根。没有按角色名分类或增加角色数量上限。

同日继续优化离线发布：原命名空间的 startup 输入从已校验的不可变外观缓存创建同盘硬链接，链接后逐字节复核；不能链接时回退普通写入。隔离命名空间仍完整编码、解码核验、计算哈希并写出。缓存原子替换和删除不会改变已发布 startup；变化的缓存内容会拒绝候选。v35/v36 编译和缓存/清单回归通过，后台 Blender 两次换像素导出为 4.93 / 4.93 秒，详见[导出测量](blender-scoped-resource-export.md)。优化编译器已同步到游戏 tools 和 Blender nativepack，两处 SHA256 与源码产物一致；部署前后 32 个作者/config/current/DLL/协议文件未变，无需重导出或保存 blend。

## 游戏加载完成登记控制；异步模型边界

继续检查发现 RuntimeShapes 仍每秒使用 FindObjectsOfType 扫描场景，F10 提交清空绑定后依赖下一次扫描恢复控制。新增模型完成回调回归先复现未登记，随后改为完成回调按该模型根获取 SkinnedMeshRenderer 子节点（含 inactive）。原生 MaterialController.Init 的显式 Renderer 列表登记保留，移除其嵌套 Update，避免模型构建中重入帧事务。构建期间暂停控制登记，成功发布新控制图后立即登记新模型；失败时恢复原控制图下的模型登记。删除全场景定时扫描、相应方法绑定及 2048/65536 人为列表上限，实际数组范围校验保留。

原生 mock 覆盖完成回调、F10 后立即控制绑定、模型内 0/1/N 网格与失败回滚；v35/v36 均通过。当前真实作者包控制集成通过 9 个可切换部件、两个模型实例、全部 NUMPAD cycle（含三值）、按住不重复、修饰键/press、显隐读回、原生 enabled 意图、后续实例及偏好往返。测试基于隔离作者副本，生产文件未改。

另一回归先复现受影响的异步模型尚未完成时仍允许提交。现仅对候选精确路径的未完成加载在变更前拒绝，请在加载完成后重新按 F10；保留弱的加载归属，不强行重建未完成模型。无关角色的加载不阻止事务，没有当前实例时仍提交候选、清除对应闲置池并路由后续创建。完整 Frame、连续世界重载、回滚和后续实例路由测试通过，实机尚待验证。

使用本轮优化编译器及 DLL 磁盘入口对真实资源隔离副本重新检查：0→1→2 作者集合、mods 目录缺失、删除成员/最后成员、重新添加、冲突失败保留 current、共享 CAB/PFB 和零替换表均通过。F10 调用编译器后选择准确候选，原生提交前控制/UI 不变；独立检查无 GameAssembly 文件仍能编译和准备。该测试把 LZY 的非重叠目标拆成两包，不能声称第二角色已经实机通过。临时输入自动清理。

最终仅部署新版 v35 DLL（SHA256 见顶部），32 个作者/配置/current/编译器/协议文件哈希不变。新冷启动会话尚未运行；需要核对连续 F10 动画与按键、展示/世界转换和后续角色加载。

本轮导出报错 `offline cache belongs to another Mod/source directory` 的根因是 Blender 会话仍运行旧导出函数，向已属于完整作者集合的公共缓存执行单作者命令；旧安装编译器还使用四路径 CLI。旧命令在隔离缓存中连续两次复现，未删除生产缓存或放宽归属检查。新编译器明确声明三路径参数，Blender 按实际参数声明调用，并兼容已知旧参数布局；公共缓存统一使用 `--mods`。针对旧安装协议与集合缓存的回归已覆盖，30 项相关 Python 测试通过。

使用 Blender 5.0.1 后台读取用户保存的 `G:/zmd/lzy/1.blend`，加载新安装插件，通过实际 `eiem.export_package` 操作符以“全部资源”、全部已发现 LOD、包含按键切换导出。仅将目标发现重定向至 E 盘隔离目录，实际发布、已安装编译器及集合缓存检查均执行：14 个所选 Mesh 生成 12 个 Mesh 资源、5 个 Material、11 个 Texture、7 条替换规则，覆盖 LOD 0–3，导出含离线编译共 21.99 秒。操作符返回 `FINISHED`，`offlineReloadPrepared=true`，实际命令为 `--mods`，没有再出现归属错误。工程文件、游戏作者文件、eff.ini 与生产 current.tsv 共 29 个文件前后 SHA256 一致。该结果证明后台导出及离线准备成功，游戏冷启动和 F10 提交仍待实机验收；隔离输出不替换用户游戏内资源。

2026-10-06 的 body 残影消失反馈尚未复现或修复。先前贴图测试的动作/残影视觉通过只作为当时的样本。本轮部署没有覆盖游戏内作者资源或修改 GFX 配置；新 DLL 的冷启动、形态权重和重载影响仍需分开检查。历史抓帧、备份与诊断产物已按用户要求清理。

部署前核对的作者 PNG 和游戏 TEX 已恢复原图。历史 PID 42344 曾冷启动原图代际 `08652A09...`；这不能证明做过反向热重载，也不是本轮新 DLL 的运行记录。

## 已通过：同路径贴图像素正向更新

会话 PID 39444 在游戏启动后，通过 Blender 仅贴图导出将身体贴图更新为青绿色，再触发一次重载。日志确认当前世界角色从旧根迁移到新候选根，提交 generation 2；没有拒绝或回滚。请求到提交为 919.54 ms，提交帧主线程约 63.97 ms。导出准备时间单独计算。

用户确认身体变为青绿色，走动、平 A、残影和头饰正常。部署 TEX 与作者 PNG 的全部 mip 字节一致，像素与原图确实不同；尺寸保持 1024×1024、11 个 mip，mod.ini 字节未改变。已提交代际的原生载荷核验覆盖 43 Mesh、5 Material、11 Texture，并确认 11 张原生贴图保持不变。

该历史会话通过的是当前世界实例的正向贴图更新；不能替代修正版、反向恢复、展示实例、几何或部件增减验收。

## 快捷键

当前源码默认值与项目 `config/eff.ini` 设置为：

```ini
[Hotkeys]
reload=F10
gui=INSERT
```

F10 重新核对整个 `plugin/mods` 目录及每个作者资源，以当前目录的完整集合生成并提交候选；未改变的资源允许复用经校验的结果。新增、删除、修改以及空集合都是同一条目录扫描、集合组合、原生加载和提交流程，没有按操作类型或特定角色的恢复分支。候选始终由来源基线叠加当前集合生成，无声明的目标自然使用基线。当前安装配置为 F10，减号不是热重载键。导出/复制结束后再按 F10；失败时保留当前代际，修正文件后再次按 F10。

当前安装已接入 `gui=INSERT` 内置控制面板及所选 Mod 按键限制。默认选择首个有按键/形态的 Mod，保留同作者目录的重载选择；面板打开时用按钮/滑块调整，关闭后恢复所选 Mod 的物理按键。控制层回归和原生 Windows 控件消息检查已通过，后者注入前台观察值。Ins 开关、鼠标/失焦、款式按钮、形态长按/滑块以及重载后选择保持仍待实机验收。Lua 自定义面板仍未接入。

## 多角色 Mod 增删热重载

多个角色各自有对应 Mod；无对应 Mod 的角色保持原版。当前源码通过 `--mods` 编译完整作者集合，共享 CAB/PFB 一次构建，F10 才提交游戏代际，Ins 仅选择按键控制目标。对应角色的 pristine 基线必须预先覆盖，当前真实游戏基线仅为 LZY 世界/UI，不能据此宣称已测试其他角色。

隔离真实资源副本检查覆盖 0→1→2、删除成员、删除最后一个恢复 pristine startup、零替换表、重新添加和冲突失败保留 current；原生 mock 覆盖两个不同角色的原版模型加载后新增第二角色 Mod、仅第二角色候选、无旧绑定的当前原版世界实例、世界/展示恢复、原生 Mesh/Texture 身份、过期成员与失败回滚。实机仍需验证：运行中放入第二角色 Mod 后按 F10、Ins 同键隔离、删除所选 Mod 后按 F10、清空后按 F10 恢复原版，再新增后按 F10 接管当前实例；全过程检查动画、残影和后续展示实例。

## 本轮操作边界审查

| 用户操作 | 当前处理与检查 |
| --- | --- |
| 新增、删除、重新添加角色 Mod；清空整个 mods 目录 | F10 编译并提交完整集合；目录不存在也视为空集合，不擅自重建该目录。隔离资源/F10 路径回归覆盖。 |
| 删除当前控制目标 | 成功提交后自动选择剩余可控制 Mod；无 Mod 时面板为空。失败不改变选择。 |
| 多角色使用同一快捷键 | 只更新 Ins 选中的 Mod；同一 Mod 的实例共享数值。 |
| 按住键时切换目标、重载或恢复焦点 | 持续采样按键边沿，cycle/press 不产生额外按下；hold 按实际游戏时间推进。 |
| 面板打开、失焦、关闭；排队后重载 | 物理 Mod 按键暂停；UI 命令检查图和选择版本，过期命令丢弃。真实 Win32 控件消息测试通过，桌面前台观察为注入值。 |
| 热新增/重新添加有 state.ini 的 Mod | 磁盘候选读取合法保存值；提交时同 ID/目录的合法现行值优先。候选失败不改写偏好缓存。 |
| 缩小形态范围、变更 cycle 值域、损坏 state.ini | 非法旧值回退作者默认值；不把无效数值写入新控制图。 |
| 同目录替换成其他 Mod ID | 运行中缓存按 ID 隔离；保存文件增加 ID 注释，旧 ID 的排队保存检查当前作者归属。旧无 ID 注释的文件继续兼容。 |
| 复制/导出尚未完成、重复 ID、冲突目标、缺资源、非法 INI/配置 | 拒绝候选并保留原代际；完成或修正后再按 F10。编译最多重试一次当前作者集合，避免递归追逐持续变化的作者。 |
| 连续 F10、较新磁盘候选或准备失败 | 只提交最新匹配的候选；准备失败撤销票据，修复文件不会沿用失败请求自动提交。原生 mock 覆盖提交前再次换请求及回滚。 |
| 一次编译超过两分钟 | 取消请求并停止此次启动的一次性编译子进程；独立监视器不被终止。超时分支为代码审查，未运行两分钟故障注入。 |
| 运行中清理缓存 | 启动替换文件与候选闭包持续持有读取句柄，阻止删除/覆盖；其他缓存被破坏时拒绝新提交。隔离启动测试核验 Windows 共享冲突。 |
| 切场景、展示、动作/残影、原生请求永远不完成 | 世界/展示迁移与失败回滚有 mock 检查，实机仍待验收；原生未完成请求保留依赖，可能阻塞后续重载。完整池/Job 退出契约仍未验证。 |

v35/v36 构建、原生事务 mock、控制/偏好回归、Win32 控件消息、作者集合契约和 Blender 离线选择测试通过。真实输入使用 LZY 的非重叠目标拆分成两个作者包；它验证共享 CAB/PFB 组合，不代表第二角色实机通过。真实资源副本与偏好测试在 E 盘并自动清理。

## 下一轮优先：冷启动与连续 F10 动画

保持当前作者资源，按上文步骤先检查冷启动动画，再连续两次 F10 检查走动与攻击。通过后，以形态权重 0 平 A，再调高形态权重后平 A，继续核对 body 残影反馈。结合新增分支/赋值观察区分冷启动、形态权重和重载影响；这一步不以历史贴图视觉通过替代。body 残影问题仍没有本轮实机修复结论。

## 后续：青绿色 → 原图

当前文件已经恢复原图。独立开展反向贴图测试前，需重新准备青绿色 TEX/完整候选，再冷启动确认青绿色，才能执行下面步骤；普通启动不会自动回到青绿色。本轮没有修改或重新导出测试资源。

1. 先启动游戏进入同一角色世界场景，确认冷启动仍显示青绿色；此时保留青绿色作者 PNG 和 TEX。
2. 游戏运行中将 `G:/zmd/lzy/body.png` 恢复为事先准备并核验的原图。已清理的历史 diagnostics 备份不再作为操作输入。
3. 使用 Blender 对原身体选择执行“仅贴图”导出到已有 LZY；等待离线编译完成。
4. 在同一世界场景按一次 F10，确认身体恢复原图，走动、平 A、残影、头饰及已有按键正常。
5. 再按一次 F10 检查无改动重载，再进入角色展示并返回世界，检查后续实例使用原图；保存本会话日志。

反向测试会话里，原图不能在恢复操作前就已经加载，否则无法观察反向热重载。源图恢复脚本执行前要求当前游戏已加载青绿色版本；脚本只恢复作者 PNG，TEX 仍由 Blender 导出。

## 后续范围

| 用例 | 状态 |
| --- | --- |
| 同路径贴图像素 A → B | 世界实例通过；用户视觉与日志一致 |
| 连续 F10 后整个人物 T pose | 模型归属修正已部署，连续 8 次 mock 回归通过；实机待验证 |
| 当前 body 残影消失反馈 | 待冷启动/非零形态权重复现；观察已准备，不宣称修复 |
| 贴图 B → A；F10 触发 | 待实机 |
| 未修改时重复重载；展示/未来实例 | 连续世界归属及展示 mock 通过；实机待验证 |
| 贴图尺寸与材质属性/引用变化 | 离线已有检查，待实机 |
| Mesh 顶点/索引/形态、骨骼槽/权重 | 待实机 |
| Renderer 0/1/N、部件增删改名与多资源组合 | 按 [完整装配契约](full-render-assembly-reload.md) 继续实施与验收 |
| 多代资源退休、闲置池和完整生命周期 | 尚未验证 |

贴图单项通过不等于完整热重载完成，不另设贴图专用的运行时提交路线。
## 2026-10-08 UI F10 空委托调用崩溃

实机 PID 39824 在 10:38:09 完成 UI 页面定位并捕获世界/UI 两个所有者，下一帧尚未记录 UI-REQUEST 就崩溃。游戏生成的 Player.log 原生调用栈明确落在 `LoadUIModel` 的 `Delegate.get_Target` 调用（旧第 59 行），随后是 Rebuild 与 Frame；游戏已有 crash.dmp 的异常记录为 0xc0000005、读取地址 0x20。F10 为避免重放一次性协程刻意传入空回调，旧诊断日志却仍用它作为实例接收者；该原生调用不能靠 C++ 的 std::exception 捕获恢复。

原事务模拟对空委托接收者返回普通目标，漏掉了这条崩溃路径。先补强模拟以记录并拒绝空接收者，再通过真实混合世界/UI 的 Frame 回归复现失败：`mixed F10 inspected a null page callback through Delegate.get_Target`。修正移除仅供日志使用的委托目标读取，日志直接记录 callbackProvided；游戏异步创建、页面绑定、取消和回滚流程保持原有调用方式。

生产 v35 构建、v35/v36 完整事务回归、生产内嵌 Lua 页面回归与 diff 检查通过。修正版已在游戏退出时部署，eff.dll SHA256 `04C4B069778A563E302421426CB8FEFD9B2A24086AAA1D9CD69F8731405F1A0F`（1,478,144 字节）；34 个 Mod/配置/协议文件前后校验未变，compiled.bin 仍为 `758A7C4D31130C895BE775BC8DB61C43E72662BEFB2BB675273C776E713A6794`。未重导出、保存工程或修改游戏脚本。

部署后的实机 PID 39344 在 10:59:00—10:59:03 连续四次 F10 提交 generation 2、3、4、5，每次均迁移世界和 UI 两个所有者，并记录 pageItemRebound=1、oldContinuationReplayed=0。请求到提交分别为 96.642、100.068、96.550、97.148 ms；最后提交帧的主线程工作约 56.6—61.2 ms。日志确认真实请求、候选校验和页面引用移交走通；四次使用相同 Mod 集合键，不能据此宣称新导出内容已经实机覆盖。真实 UI 最终画面、动作及显隐仍待用户确认，此边界随 v1.3.2 发布说明保留。
