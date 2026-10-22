# DLL 发布前性能审计与实机测试计划

日期：2026-10-21。范围：当前 `src/eiem_resource_entry.cpp` 生产入口及其运行时模块；不包括 Blender 导出提速。目标是先保存可复现基线，按多角色、多 Mod、UI、地图、NPC、战斗的实机数据确定优化顺序。

**当前状态：已完成源码热点审查、补充测点及测试工具；尚未取得这一版的游戏性能结果。下文“热点”均是待验证候选，不代表已经证明拖慢游戏。此次没有据静态猜测修改调度、缓存、资源校验或释放策略。**

## 1. 成本如何分布

```text
启动 / 磁盘 worker
  已安装 Mod 枚举、编译记录校验 / 合并、路由准备
  每秒检查磁盘变化；F9 读取特效快照；F10 准备完整事务

游戏加载 / 模型生命周期回调
  File 输入替换 → 原生依赖查询 / 首次索引 → Prefab 资源取得
  世界 / NPC / UI 模型完成 → Renderer 登记 → 材质与特效绑定
  实例化入口另有 source → candidate 路由

Unity 每帧
  Update 入口去重
  重载 / 异步完成 / 退休资源维护
  特效 owner 核对、显隐、加载与创建、渲染层跟随
  按键与面板命令 → 共用动画频道采样
  动态材质 → 形态权重 / 部件显隐

战斗 Capture 入口
  CPU 形态求值、当前骨骼矩阵、临时 Mesh 克隆 / 写入 / 验证
  原生 consumer Release 后回收临时对象

独立线程
  Ins Win32 / D3D11 面板
  持久状态写盘、性能汇总日志
```

`plugin-frame` 只覆盖每帧维护入口。模型完成回调、原生资源查询、实例化路由和 Capture 可能在该入口之外执行，不能把 `plugin-frame` 当成 DLL 全部主线程工作。各阶段计时包含其子阶段，不能相加当作独立 CPU 时间。

区分四种现象：稳定帧持续成本、加载/切换尖峰、资源驻留增长、引擎/GPU 额外负担。DLL 调用 Play 或提交 Bundle 所花的时间，不包含之后的粒子模拟、Shader 编译、GPU 上传或透明片元绘制。

## 2. 规模变量

| 变量 | 含义 | 为什么单独记录 |
| --- | --- | --- |
| M | 已加载人物 Mod 与独立特效包数 | 未出场 Mod 也增加控制/来源元数据，通常不意味着所有角色原生模型已经加载 |
| O | 游戏登记的模型 owner 数，UI 分开 | 同一个 Mod 的多个 NPC 是多个独立 owner |
| R | 已登记作者 Renderer 数，含各 LOD | 与人物数、拆分 Part 数、LOD 数共同变化 |
| C | 每个 Renderer 的受控形态通道数 | CPU 采样可共享，原生权重读取/写入仍按 Renderer 执行 |
| A / T | 活跃动画组 / 输出频道数 | 空闲组与正在采样的组应分开 |
| B / P | 动态材质 Renderer / 属性数 | 同一个材质的多个实例仍有各自 property block |
| F / L | 特效绑定数 / 每绑定覆盖的 Renderer 数 | 一个骨架共用特效实例，但当前显隐检查仍遍历全部 LOD 候选 |
| G / Q | 特效资源代际数 / 等待退休或隔离的对象数 | F9/切场景后是否回到平台值，比单次总内存更能说明问题 |
| V / S | Capture 顶点数 / 同帧快照数 | 战斗尖峰不能仅按 Mod 包数解释 |
| D | 作者目录文件数 | 每秒磁盘轮询与启动准备的规模因素 |

`runtimeWorkload` 每五秒读取已有 C++ 集合大小，没有新增 Unity 场景查询。它不是活跃可见 draw 数；parts/uiModels 是登记表大小，可能含尚待核对的记录；`shapeBindings` 可以包含不可见 LOD、等待弱引用清理的 Renderer；`effectArchiveBytes` 仅统计 Generation 所引用的 CPU 原生归档字节，不是 DLL、Unity、GPU 总内存。跨线程汇总边界近似，负载标签带自身 sampleTickMs/sampleAgeMs，不能当作与每个慢帧精确同步的瞬时快照。

## 3. 候选热点台账

优先级表示“先测什么”：P0 最容易造成明显尖峰/随实例增长；P1 次之；P2 只有数据显著时再处理。所有条目当前都等待本轮实机测量。

### H01 · P0 · Update 去重之前的托管调用

位置：[Update / Tick / FrameTick](../src/eiem_entry_runtime_shapes.h:720)。多个游戏回调都调用 Update。虽然主体通过 `frame == lastFrame` 保证每帧一次，但去重之前仍执行 `Main()` 和读取 `Time.frameCount`，两者经 `il2cpp_runtime_invoke` 取得值；因此主体去重不等于全部入口开销消失。

预测：NPC/角色增加后，`update-entry changed=0` 的调用数随回调次数上升，`plugin-frame` 调用数仍接近实际帧数。战斗或 UI 切换也可能增加入口次数。

测点：`update-entry`、`plugin-frame`、`managed-invoke`。若成立，再核对是否能依赖已确认的线程身份和唯一全局帧入口减少门口调用；必须保留线程判定与重入安全，不能未经验证删除 hook。

### H02 · P0 · 形态绑定读取按 Renderer 与通道放大

位置：[每帧控制绑定](../src/eiem_entry_runtime_shapes.h:755)、[ShapeValue](../src/eiem_entry_controls.h:239)。每个有受控形态通道的 Renderer 每帧检查对象/源 Mesh，并读取每个通道的原生权重；值变化时执行 setter 和读回校验。当前不会因为 Renderer 不可见就跳过这些通道。

规模约为 R 的遍历加上全部受控通道数，主线程托管调用数比字符串查表更值得关注。多实例、多个拆分 Part、全部 LOD 使用同一高模会增加该项。同 Mod 共享一次曲线采样，并不能消除每个实例的权重同步。

测点：`control-bindings`、`shape-weight-read/write`、`shapeBindings`、`animationChannels`。对照：同场景分别只静态网格、常量形态、播放形态动画；再增加相同 NPC 数量。若读取远多于实际变更且耗时显著，再设计通道版本/原生改写检测，不能直接把游戏可能改写的通道当成永久缓存。

### H03 · P0 · 动态材质整理与原生 property block 交互

位置：[材质 Frame / Values](../src/eiem_entry_material_animation.h:43)、[原生 block hook](../src/eiem_entry_material_animation.h:25)。Frame 为每个绑定收集当前输出到临时 `std::map`，再与 `lastValues` 比较；数值变化时从游戏基线 block 重建工作 block、写入各属性并提交。游戏自身 SetPropertyBlock/GetPropertyBlock 的调用还会进入相应适配逻辑。

预测：即使两个 NPC 使用同一条动画，临时 map、原生读写和 block 提交仍按实例增加；人物 UI 或战斗游戏材质写入频繁时，hook 路径也可能放大。动态值不变时已有 `current == lastValues` 跳过写入，不能误记为“所有材质每帧无条件重写”。

测点：`material-frame/register`、`material-slot/global/read-hook`、`material-value-write`、`materialBindings`。对照：停止、暂停、播放同一组；再由单实例增到多实例。若成立，优先考虑准备好的属性索引/紧凑值快照和输出版本，必须保留游戏基线、四分量完整所有权及停止后的恢复。

### H04 · P0 · 特效每帧显隐检查与 GC handle 周转

位置：[RuntimeAttachments::Frame](../src/eiem_entry_effect_attachments.h:99)、[FollowLayer](../src/eiem_effect_debug.h:145)。每个启用特效绑定检查 owner、挂点、clone，然后遍历该绑定的所有 Renderer，读取 alive/enabled/GameObject/activeInHierarchy；可见实例还检查层。临时 `Ref::Make` 会创建、释放 GC handle。

规模接近 F×L 的原生桥接调用。当前角色即使一帧只显示一个 LOD，也会检查其余候选；多个 NPC 各有独立绑定。层不变时已经跳过整棵特效树的 layer setter，主要待测的是检查与临时句柄，不应把它描述为每帧重写所有子节点的层。

测点：`effect-frame`、`effect-renderer-test`、`managed-invoke`、`gc-handle-create/free`、`effectBindings/Instances`。对照：同一模型关闭/开启特效、不同实例数、近远 LOD。若成立，再考虑缓存可见来源并由模型显隐事件失效，仍保留 UI 后续层切换、原生销毁恢复和全部 LOD 语义。

### H05 · P0 · 首次/重复模型登记中的骨架与层级遍历

位置：[RegisterModel](../src/eiem_entry_runtime_shapes.h:653)、[ModelReady / Parent](../src/eiem_entry_effect_attachments.h:38)、[BoneHierarchy / AssemblyTree](../src/eiem_entry_full_resource_reload.h:467)。模型登记先枚举 Renderer；特效绑定再枚举一次。对包/特效/匹配 Renderer 的循环中，骨骼 palette、祖先链、Transform 名称、完整 owner 子树可能重复构造。

这不是稳定帧里每次都扫描整个场景，而是对精确 owner 在完成、重载、恢复边界执行。多个 owner 同时进入视野、UI 连续换角色或新 NPC 集中生成时，更容易形成突发。骨架数 B、路径深度与 Renderer/LOD 数比文件夹数更关键。

测点：`model-register`、`ui-register`、`effect-bind`、`effect-mount` 与对应原生调用/句柄数量。若成立，可在同一次 owner 登记中共享层级索引、按实际骨架缓存挂点并按 owner 代际失效；不能改成只按骨骼名或宽松骨骼数量匹配。

### H06 · P1 · 缺少绑定时的每秒 owner 对账

位置：[特效 Frame 的 reconcileClock 分支](../src/eiem_entry_effect_attachments.h:113)、[Owners](../src/eiem_effect_debug.h:58)、[Field / ReadObject](../src/eiem_entry_full_resource_reload.h:219)。每秒从已有 liveParts/UI owner 记录生成临时 map，验证游戏仍持有这些实例；没有特效绑定的 owner 会重新走 ModelReady。没有目标部件的角色也可能被反复检查。对象字段辅助函数每次会按类型层级查找字段。

预测：当只给一个角色配置特效、附近存在许多其他角色/NPC 时，可能出现接近一秒周期的小尖峰，与实际粒子数量无关。

测点：`effect-owners`、`effect-bind changed=0/1` 的窗口调用数与最大值、`parts/uiModels`。若成立，可建立“本 owner/模型代际已经确认无目标”的索引，或减少重复元数据查找；负结果必须在骨架晚重建、控制图变化或原生 owner 更新时失效，避免恢复问题重新出现。

### H07 · P0 · 特效集中创建不受加载软预算硬限制

位置：[CreateInstance](../src/eiem_effect_debug.h:151)、[绑定 Frame](../src/eiem_entry_effect_attachments.h:99)。1ms 条件约束的是准备步骤，不能中断单次 Unity 调用。准备完成后，同一帧多个绑定可以分别克隆、枚举子树、继承层、注册并 Play；这些创建操作并没有共同的严格 1ms 硬上限。原生参数观察还会做有限次读取与字符串整理。

预测：共享 Bundle 已经热起来时，一批相同 NPC 同时获得特效，仍可能出现与 clone 数成比例的尖峰；不能仅凭资源共享就认定这一帧廉价。

测点：`effect-create`、`effect-load-step`、创建/退休 operation、`effectInstances`。分别测试资源冷加载和资源已驻留后的批量实例。若成立，再考虑公平的创建队列/渐进提交；保持 owner 失效取消和不同实例独立，不能偷偷合并成一个全局特效实例。

### H08 · P1 · 特效退休、隔离代际与长期驻留

位置：[Collect / Frame](../src/eiem_effect_debug.h:191)、[Stop / Release](../src/eiem_entry_effect_attachments.h:28)。退休实例逐帧检查原生对象是否真正销毁；Generation 是否仍使用，会与 retiredInstances 交叉核对。Q/G 较大时可能形成嵌套扫描。失败隔离和未完成原生请求会保留资源，属于所有权契约。

测点：`effect-collect/retire`、`effectGenerations/Retired/Quarantined/ArchiveBytes`、`nativeLifetime`。UI 开关、换地图、F9 多轮后记录是否平台化，以及回收所需的实际延迟。若确认账本扫描成本高，可维护使用计数/退休索引；不能用超时强制卸载未知仍被使用的资源。

### H09 · P0 · 战斗 Capture 的大块 CPU 求值与读回

位置：[SnapshotFor](../src/eiem_entry_runtime_shapes.h:459)、[CapturePaletteFor](../src/eiem_entry_runtime_shapes.h:410)、[Evaluate](../src/eiem_shape_snapshot.h:53)。代码读取当前权重、CPU 求值几何、读取每骨骼矩阵、Instantiate Mesh、清形态并写顶点/法线/切线、重算 bounds，再读回几何、indices、bindposes 和 boneweights 做验证。首次 Read 还取得完整形态数据。

规模取决于 V、有效通道和同帧 S。缓存来源几何后也仍有快照生成和读回；它不是每帧给普通人物重算基础网格。高速连招、多角色技能叠加、残影消费者集中创建需要单独覆盖。

测点：`capture-snapshot/retire/matrices`、`captureGroups/Snapshots/shapeCache`、原生调用数。优先区分首个与后续快照、创建与释放。后续优化可以研究不变拓扑/skin 证明的复用及减少冗余回读；仍需证明输出一致，不能删除皮肤矩阵完成边界或提前释放快照。

### H10 · P0 · 首次依赖索引与依赖查询锁

位置：[GetIndex / ManifestKeys / Augment](../src/eiem_entry_bundle_dependencies.h:63)。新 manifest/依赖图第一次建立索引时，会复制并扫描完整 Bundle 目录、匹配选定名字及做原生反查；该工作可能发生在调用游戏依赖查询的线程。GetIndex 的 mutex 覆盖首次构建，其他查询可能等待。

测点：`dependency-index`、`dependency-all/direct` 的 mainMaxMs 与 allMaxMs、`STARTUP-DEPENDENCY-KEYS`。换图/首次切 UI 要区分首次命中与同一图的重复查询。若成立，再评估利用已具备的生命周期预先建立索引、缩小建表锁范围或改进查找；不得跳过精确来源/依赖校验。

### H11 · P0 · 加载入口的整块回放退路与原生提交

位置：[LoadRuleInput](../src/eiem_resource_entry.cpp:1054)、[StreamRead / StreamView](../src/eiem_native_bundle_loader.h:34)、[Materialize](../src/eiem_compiled_mod_set.h:108)。优先使用编译输入视图；无视图时可能解压/回放完整输入、分配大缓冲，或复制托管 Byte[]。即使异步加载入口，插件在返回异步请求之前完成的准备依然占用当前调用线程。

测点：`file`、`view-read/seek`、`backing-read`、`stream-route`、`ENTRY-RULE-SUBMIT` 中 compiledInputView、replay/upload/nativeSubmitMs、async 标志。大地图/角色首次加载、首次材质/贴图引用和温缓存重复均需测。若成立，先找出具体退路条件；不能把“已预编译”直接等同于运行时没有工作，也不能用总包体积代替单次输入成本。

### H12 · P1 · 所有 Instantiate 的线性路由查找

位置：[Routed 及 clone/Instantiate detour](../src/eiem_entry_full_resource_reload.h:148)。路由存储为列表，每次原生实例化适配都按 source 扫描。入口不仅被人物调用，战斗中大量无关特效/物体也可能经过未命中路径。规模为实例化次数×route 数。

测点：`instantiate-route` 的次数、hit/miss、主线程耗时与 `routes` 数。比较 F10 前/后的 route 集合，以及多 Mod 但同一固定战斗的表现。若成立，可考虑精确对象身份索引；仍保留 nativeId 核验、代际持有与 UI proxy 特例。

### H13 · P1 · F10 迁移、覆盖校验与退休代际

位置：[DiscoverOwners](../src/eiem_entry_full_resource_reload.h:1138)、[Rebuild](../src/eiem_entry_full_resource_reload.h:1222)、[RetireSuperseded](../src/eiem_entry_full_resource_reload.h:932)。owner 发现会核对每个作者 Renderer 属于哪个已捕获 root；迁移阶段同步释放/加载多个 owner、校验资源、重绑动画/武器/音频。外层 2ms 软预算不能抢占单次 Rebuild。退休检查还按旧 assets×当前 routes 比对持有。

无变化 F10 本来就不重建模型，但控制图 revision 仍会变化，动态材质 Frame 因 revision 重注册全部材质绑定，可能产生一次额外尖峰，需要区别于“加载了新模型”。

测点：`reload`、`reload-owners/rebuild/retire/drain`、`material-register`、`FULL-RELOAD-NATIVE-COST/LATENCY`。测试顺序独立于普通 UI/换图，不把主动 F10 算成普通场景切换开销。若成立，分别优化重复校验/注册或事务准备；涉及分帧迁移时必须重新验证可见性和回滚原子性。

### H14 · P1 · 多 Mod 的后台周期检查与启动组合

位置：[ProcessJobs](../src/eiem_resource_entry.cpp:1274)、[PollCompiled](../src/eiem_entry_full_resource_reload.h:725)、[DirectoryStamp / AuthorStamps / Unchanged](../src/eiem_compiled_mod_set.h:53)、[Read](../src/eiem_compiled_mod_set.h:249)。worker 一秒唤醒检查目录和作者文件元数据；启动/变更时读取、校验并组合多个 Mod 的预编译记录。同一轮检查可能多次访问目录/文件信息。

这些工作主要在 worker，不能直接记为 Unity 主线程阻塞，但会竞争 CPU、内存带宽、文件缓存与存储。只增加未出场角色 Mod 的测试最适合区分这项与实例成本。

测点：`resource-prepare changed=0/1`、启动 compiledSetMs/routingMs、进程 CPU/I/O、稳定场景帧轨迹。若成立，可改进未变化快照的复用和批量文件检查；不引入游戏内作者编译器，不靠跳过完整性校验换取数字好看。

### H15 · P2 · 输入轮询、快照发布与 Ins 窗口

位置：[PollControls / PublishPanel / PanelCommands](../src/eiem_entry_controls.h:159)、[panel Tick / Present](../src/eiem_control_panel_win32.h:229)。按键按已安装绑定轮询，重复组合键也各自采样；图状态发布复制数值/动画快照，但已有事件驱动，不是每帧复制完整面板。窗口有独立 D3D11 渲染线程，显示时 Present，隐藏时不绘制但仍低频检查。

测点：`control-input`，对照面板关闭/打开/拖动、形态/材质 hold 连续调节、许多绑定但不按键。面板线程和 GPU 成本不在 plugin-frame 内，需结合进程/帧轨迹。若成立，优先缓存一帧物理按键采样或减少无变化发布，同时保留失焦后的物理边沿观察与多个 Mod 同键分发。

### H16 · P1 · 同步逐行日志与诊断开销

位置：[Log](../src/eiem_resource_entry.cpp:1535)。每行获取共享 mutex、格式化时间并 `fflush`，如果存在 mirror 会写两份。集中模型登记、重复失败、UIWait 等可能生成成批日志；worker 与主线程也可能争用日志锁。

测点：`logging` 与新增 `diagnostic-logging` 分开。后者只记录性能汇总自身写日志的成本，不能当成生产业务日志的成本。若成立，可评估异步批量输出/相同错误限频，仍保留关键失败与事务证据；完成测量后必须以普通生产构建复核收益。

## 4. 当前已存在的有效控制

- Update 主体已按 Unity frame 去重；动态部件显隐按版本更新，空闲 Renderer 的弱引用清理分摊至 64 帧。
- 动画采样按 Mod 活跃组共用，不为每个 NPC 重复计算曲线；非活跃组不逐条插值。
- 动态材质值不变时跳过原生 block 重写，属性 ID 在登记时解析；游戏 block 与作者覆盖分开。
- 同骨架的模型 LOD 共用一个特效实例；层不变不重复写子树，补登记 LOD 不重启粒子。
- 原生 Bundle 内存维护有 `memoryWork` 空闲门控，不是每帧轮询所有长期驻留 Bundle。
- 资源准备在 worker，普通资源替换不在每帧重算 Mesh/Texture；无变化 F10 不重建人物。
- 不扫描全场景寻找人物/特效归属；原生退出未知时保留资源，不能为了降低内存读数破坏生命周期。

这些事实用于避免后续“优化”重复已做过的工作，或把必要正确性机制粗暴去掉。

## 5. 实机场景矩阵

每一条件至少重复三次；稳态建议 60～120 秒；UI/地图往返至少 10 轮；长期观察 15～30 分钟。固定分辨率、帧率上限、VSync、画质、TAA/超分和同一条路线/镜头，保留初次与温缓存结果。性能测试关闭正在进行的抓帧；如使用 RenderDoc/代理，其状态必须在对照间一致并记录。

| 编号 | 场景与控制变量 | 分别观察什么 | 主要候选 |
| --- | --- | --- | --- |
| 基线 A | 无 DLL 的原游戏，同一场景/操作 | 原游戏 CPU/GPU 与真实帧轨迹；需要独立会话 | 外部开销基线 |
| 基线 B | 有 DLL、没有角色/特效 Mod，同样操作 | 全局 hook 未命中成本；注意必须实际空集合，Ins 禁用按键不等于卸载 Mod | H01/H12/H15/H16 |
| S01 | 单角色，先静置，再分别只启用形态、材质、特效 | 稳定维护与单功能增量；关闭的是对应功能，不改变其他测试条件 | H02/H03/H04 |
| S02 | 相同 Mod 1/2/4… 个真实角色实例同时可见；再不同角色混合 | 实例成本是否近似线性、谁在逐帧工作，首批生成尖峰 | H01～H07 |
| S03 | 场上只留同一角色，安装 1/若干/全部 Mod；其他角色不出现 | Mod 元数据/按键/worker 成本，避免和“实例变多”混淆；更改集合后重新建立稳定会话 | H10/H12/H14/H15 |
| S04 | A↔B↔C UI 角色，每个停留几秒，开关详情页共 10 轮；单列首次与重复 | UI 登记/释放、动态材质、特效 clone、层切换；是否有回收延迟累积 | H03/H05/H07/H08 |
| S05 | 大世界地图 A→B→A 往返；加载结束后固定镜头静置 | 首次来源/依赖索引、重复加载退路、稳定状态是否恢复、资源是否平台化 | H08/H10/H11/H14 |
| S06 | 同角色 NPC 逐渐增加；包含已经显示服装 Mod 的 NPC；再离开/返回 | 相同资源共享下的实例 CPU 成本、无特效目标 NPC 的对账、LOD 可见性与回收 | H01～H08 |
| S07 | 固定队伍、敌人数、技能与连招顺序，重复同一场战斗 | Capture 创建/释放尖峰，非人物实例化路由，游戏材质回写；战斗前/结束后各静置 | H02/H03/H09/H12 |
| S08 | 固定世界/UI 场景，先无变化 F9，再单改一个特效参数 F9 | 磁盘准备、共享资源准备、每 owner 创建、失败旧实例保留；避免同时换图 | H05/H07/H08 |
| S09 | 无变化 F10、单一控制变化、实际模型资源变化分别测试 | 控制重登记与完整迁移成本不能混在一起；记录成功/拒绝和资源代际 | H03/H05/H10/H13 |
| S10 | 多次 UI→世界→战斗→换图，最终回到相同场景，15～30 分钟 | owner、临时快照、特效退休、隔离数、进程内存是否形成平台 | H06/H08/H09/H13/H16 |

额外对照：同一场景普通生产 DLL 与测量 DLL 各跑一次，估计计时/原子计数/汇总日志的扰动。不能只以测量版的 FPS 当成最终发布性能。动态材质和特效会改变引擎批次/模拟/透明绘制，这类额外负担即使 plugin-frame 很低也仍然需要检查。

## 6. 本轮测量数据与标记工具

构建命令：`build.bat v35 load-perf`。产物位于 `bin/resource-directory-entry-v35-load-perf/eff.dll`，与普通生产构建采用同一功能代码。测点通过 `EIEM_LOAD_PERF` 编译开关启用，不启动特效调试管道，也不使用 capture 探针。

本轮增加：

- 模型 Update 入口、世界/UI 完成登记、动态材质 Frame/登记/三个 property block hook、特效磁盘准备与 Frame/匹配/骨骼挂载/owner 查询/原生准备/创建/退休/收集、实例化路由、F10 owner/迁移/退休阶段。
- 独立的主线程 P95/P99 和全线程 P95/P99 直方图上界；原有最大值与主线程总量保持。
- 托管调用、GC handle 新建/释放、形态权重读写、材质属性写入、特效 Renderer 检查、特效创建/退休调用计数。创建计数含失败尝试，实际持有实例数另见 effectInstances。
- 已登记 owner/Renderer、动画频道、特效代际/退休/隔离和持有归档字节等低频负载标签。
- 每个约五秒汇总窗口的 Windows uptime 起止时间，支持与游戏外场景标记对应；不依赖 PID 手工输入，也不受跨午夜日志时间影响。

托管调用次数用于观察桥接强度，不等于托管分配字节或 GC 时间；GC handle 释放也不证明 Unity 原生/GPU 对象释放。`diagnostic-logging` 只包含测量日志写出，不含全部原子统计、分桶与报告整理成本，测量扰动仍须用普通版对照。

使用 [Start-EFFPerformanceTest.cmd](../tools/diagnostics/Start-EFFPerformanceTest.cmd) 启动场景记录器：

1. 测试前由已验证的测量版 DLL 替换游戏 `plugin/eff.dll`；**游戏运行期间不覆盖 DLL，不重启/终止用户进程**。普通 DLL 和原日志要先备份。本轮安装状态见文末。
2. 双击记录器，或在 PowerShell 运行 `tools/diagnostics/Start-EFFPerformanceTest.ps1 -Notes '120FPS，分辨率/画质/TAA，Mod数量与本轮配置'`。
3. 输入 1～10 开始场景；回游戏先等待 10 秒，再做操作，结束后原地保持 10 秒，回记录器输入 0。`N 备注` 记录具体卡顿/技能/异常；`Q` 备份日志退出。直接退出时未结束的场景标为未完成。
4. 文件保存在 `bin/performance-runs/<时间>/`，包括 session.json、markers.jsonl、日志副本和 latest-log.txt。记录器只写这些输出，不改 Mod/DLL/游戏配置，不创建常驻采样线程。
5. 每次重启游戏前结束当前场景，让工具保存日志；游戏启动会截断自己的 eff_log.txt。一个进程一份日志分析，不把两次启动拼成同一轮。

分析示例（使用 latest-log.txt 指向的日志）：

```powershell
python tools/diagnostics/analyze_scene_performance.py '<日志副本>' --markers '<记录目录>/markers.jsonl' --output '<记录目录>/report.json' --markdown '<记录目录>/report.md'
```

分析器只将完整落在场景标记内部的窗口归到该场景，跨边界窗口保留在总报告但从单场景统计排除。时间太短或尚无完整窗口会标记 `no-full-windows`，不会以 0ms 宣称“没有开销”。旧日志仍可做总计，缺少 uptime 时不伪造场景归属。

## 7. 解释结果与后续优化顺序

先看真实帧轨迹/用户观察是否恶化，再看对应窗口的主线程最大值、每插件帧摊销、调用数和负载标签。临时建议的调查线是：稳定场景 plugin-frame 连续出现 ≥1ms 的窗口上界、单次 ≥4ms 的插件主线程路径、或规模增加时明显超线性；这是用于筛选重点的阈值，不是未经用户确认的最终性能承诺。60/120 FPS 分别只有约 16.67/8.33ms 的整帧预算。

| 观察 | 下一步 |
| --- | --- |
| 固定 M，实例数增多，effect-frame/Renderer 检查近似线性放大 | 先优化 H04；确认层跟随和所有 LOD 正确后对照 |
| material-frame 平均高，动态值写入少；或 hook 调用量异常高 | 分离临时快照/查表与游戏 block 回写成本，优先 H03 |
| 一秒节奏尖峰，effect-owners/bind 集中 | 核对无目标 owner 的负结果重试，优先 H06 |
| 首次 UI/地图 dependency-index 或 file 单次高，重复很低 | 优化首次建表/回放退路，不能误归因到稳态循环 |
| 战斗 capture-snapshot 高并随 V×S 放大 | 研究 H09 的快照与证明复用，先保持正确性回归 |
| DLL 测点低，但真实帧率/GPU 时间随特效和高模增加而下降 | 继续看 Unity 模拟/批次、透明 overdraw、GPU 上传/Shader；DLL 低耗时不能作为无责任证明 |
| 多轮回到同场景，Q/G/临时快照持续上升且不形成平台 | 追踪实际 owner/原生 Release 与隔离原因，不按时间强制释放 |
| 仅增加未出场 Mod 就恶化 | 优先 H14/H15/H12，排除实例几何与粒子数量因素 |

每轮只改一个已验证原因，保留同集合/同场景的前后报告和负载标签；正确性回归后，以普通生产 DLL 再验证。高模、透明材质和粒子数量的内容成本要与 DLL 管理成本分别报告。

## 8. 发布前验收清单

- [ ] 上述基线、稳态、多实例、UI、地图、NPC、战斗、F9/F10 和长期往返均有明确条件和重复结果。
- [ ] 没有“衣服生效但特效丢失”、材质被覆盖、实例挂错骨架、LOD 切换重播等功能回归。
- [ ] 报告区分主线程/worker、首次/重复、稳态/迁移、原生 API 调用耗时/异步完成等待。
- [ ] P95/P99 标为桶上界；最差窗口上界不冒充整段精确分位；嵌套阶段不相加。
- [ ] 驻留资源与进程内存区别明确；重复往返不出现无法解释的持续增长，所有隔离项都有来源。
- [ ] 测量版与普通版的差异已核对，普通发布 DLL 不包含 `[DEBUG-loadperf]`、抓帧探针或调试管道。
- [ ] 正式 DLL 和代理通过静态 CRT/PE 与真实 DLL 隔离加载检查，测试过程中未替换正在运行的 DLL。
- [ ] 未经实机验证的项目保持待验收，不能用模拟测试/日志 PLAY 代替实际画面或帧时间。

## 9. 历史证据与当前交付

历史数据保留在 [原场景性能记录](performance-scene-test.md)：2026-10-13 的五 Mod 会话 plugin-frame 平均约 0.270ms、最大 14.296ms，Capture 8 次平均约 4.516ms、最大 8.347ms；首次依赖索引约 29.5～35.1ms。2026-10-19 启动集合准备有约 6.26 秒的只读本地测量。这些是旧源码/旧集合/旧缓存状态下的证据，不能当成本轮发布候选的结果，也不能据此声称已经优化。

本轮初始源码只增加可选性能测量与分析/记录工具，没有做行为层性能优化。测量 DLL、普通 DLL 的初始构建/验证记录位于 `bin/dll-performance-audit-20261021/`。后续 S01～S07 部分实测、实际热点和覆盖缺口见 [首轮实测报告](dll-performance-results-20261021.md)；特效配置遗漏、诊断修复和 14:35 新部署见 [特效跟进](effect-recovery-20261021.md)。初始部署记录不代表后续当前安装摘要，NPC/大世界功能仍待实机验收。

### 本地安装与验证记录

2026-10-21 05:36，确认游戏未运行后已备份部署最新测量版 `plugin/eff.dll`，大小 2,678,784 字节，SHA256 `9855504a09c1de5faa0ac2ebaf3b75e32938d1dd97c331fcae981d076271a5ba`。只替换该 DLL，336 个 Mod、配置、能力清单及代理受检文件摘要保持；没有启动或终止游戏。普通版回退文件为 `bin/dll-performance-audit-20261021/backup-20261021-053224-904313/eff.dll`，详细清单见同目录上级 `deployment.json` 的 ordinaryBackup 字段。

普通构建 2,673,152 字节；两种构建均通过 PE 静态 CRT/诊断标记检查及游戏运行库下真实 DLL 隔离加载。统计器检查主线程/worker、分位上界与溢出、窗口复位、操作计数和 uptime；分析器 5 项回归及记录器实际 PowerShell 日志备份/源文件保持/拒绝覆盖检查通过。材质、特效、NPC/UI、LOD、失败保留、资源退休回归在普通及启用测量的模拟路径通过；这些结果不代替实机帧轨迹、画面和长期驻留测试。

测试结束、准备正式发布时，需在游戏退出状态恢复普通构建，再按相同场景复核。不要将当前包含 `[DEBUG-loadperf]` 的测量 DLL 当作正式发布附件。
