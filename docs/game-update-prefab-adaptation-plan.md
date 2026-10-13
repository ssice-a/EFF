# 游戏更新后自动适配 PFB

2026-10-13。继续采用离线编译输入视图。目标：作者导出时编译已知精确命中 PFB；游戏更新后玩家无需重新导出，新增/变化的命中来源自动补编译。不得切换为 Unity 对象动态装配，也不得等待离线导出提速完成才验证本项。

## 流程与边界

按用户确认的三层边界实施：第一层比较实际游戏资源目录的内容与依赖图身份，不以 Mod 导出次数、目标 Mesh 变化或游戏版本标签判定游戏更新。当前游戏资源基线上的完整离线导出直接信任其全部候选结论；换目标 Mesh 后重新离线导出，仍不触发运行时 PFB 发现。启动/挂载是取得当前身份的事件边界，不能解释为每次重新解析资源。完整离线覆盖证明的产生与游戏身份适配器尚未接到生产。

第二层仅在游戏资源变化后，用相关 Mesh 的反向依赖与变化来源筛选与 Mod 有关的范围；第三层排除已记录且内容/相关依赖仍有效的正、负候选。剩余新增来源精确匹配后补编译；已知来源内容变化属于局部重新编译，不能以“已命中”跳过。旧基线与当前资源必须比较实际内容，不猜当前挂载优先级。比较/准备不在帧循环做磁盘扫描或 PFB 解析。

候选初筛优先使用游戏原生反向依赖查询，再比较 SerializedFile 中 Renderer/MeshFilter 的精确 Mesh 引用及根归属。路径、角色名、Bundle hashName 都不代表 Mesh 内容。目标解析、匹配与编译依赖有效性分别定义，禁止以未知当作未命中。

离线消费者发现保留每个候选的命中和未命中结论；同包多根记录 path/CAB/根 PathID/Renderer 集合。无变化重启时这些结论无需重新解析。来源本体、目标准确定位、相关依赖、规则或算法语义变化时重新处理对应范围。

后台编译受影响输入与必要依赖，多 Mod 修改同一来源仍由完整集合共同合成。每 Mod 一个永久 compiled.bin，以内部块、索引提交和安全整理累积结果；不能每次更新生成一个永久 bin，也不能追加到现有全包 SHA256 后声称旧 decoder 能读取。

增量输出交付必须早于相关原版输入与依赖组的首次 Unity 加载。异步准备未完成时需要延迟相关资源组；不能先显示原版再靠 F10 补齐。同步/异步入口、依赖队列重入和失败策略必须先验证。尚未实现该门控，更新首次准备可能延长加载时间。

## Blender 再导出必须继承运行时新增命中

2026-10-13 用户补充的功能要求：同一 Mod 在游戏更新后补入新的命中 PFB，之后 Blender 全量或部分导出都不得因为离线解包清单没有该来源而丢弃它。上一发布的 compiled.bin 是必须继承的功能数据，不能仅作为可失败后忽略的性能缓存。

导出先读取同一 Mod 的已提交来源目录、正负匹配结论与代际，再与本次离线来源清单合并。单 bin 必须保留逻辑来源身份、内容摘要、准确范围和相关依赖的结构化信息，不能从文件名、位置显示文本或哈希后的 `.vfs` 身份反推逻辑来源。当前有效挂载/游戏内容决定来源版本；旧离线清单不能覆盖较新且已验证的运行时来源。无法验证继承来源时应保留上一发布并报告具体问题，禁止静默降级成仅含离线旧来源的包。

分别处理“这个 PFB 引用了目标 Mesh”的发现结论与“用本次作者内容编译出的 PFB 视图”。只改几何、材质、贴图或控制时，仍有效的来源命中不需要重新全局发现；依赖变化使相应编译数据失效时，对继承的这些来源按新作者输入重新编译，不能保留指向旧作者资源的视图。更换目标 Mesh、游戏来源/相关依赖改变或匹配算法改变时，只重新核验对应结论；不把负结论复制成永远有效。删除目标规则、来源实际退休或新规则核验不再命中时可退休对应记录，不能无条件永久取并集。

导出到原 Mod 目录默认继承该目录的已提交状态；新 Mod/无上一发布目录不猜另一个 Mod 的命中记录。改名/迁移需要携带完整原包或明确的同一 Mod 来源身份，禁止跨 Mod 无条件合并。

导出准备期间不长时间占用游戏补写锁。最终发布用同一个跨进程锁和代际核对：若游戏在导出期间又提交来源，只补合并该次增量、按本次作者输入生成有效结果后重试发布；不能用较早的暂存副本覆盖较新代际。整个作者资源和编译结果仍按完整事务发布，失败保留上一代。

验收包括：运行时新增一个/同包多个 PFB 后的全量及部分导出；改 Mesh/材质后新 PFB 使用新作者资源；目标删改与正负结论失效；来源变化/退休；来源目录损坏不静默丢数据；导出期间并发补写、发布失败与重启。

### 本轮导出端实现

- `eiem_compiled_mod_store.h` 的同 bin `discovery/source-selection` 块保存游戏相对范围、逻辑名、摘要、资产映射、依赖与候选。`Save`/`SaveAffected` 可提交该结构，提交时核验每个 compiled source 都有精确逻辑来源。已有结构的新增写者未提交新来源路由时拒绝，禁止无声留下无法再次导出的命中。旧本地块样本可用随包 `source-inputs.bin` 补足路由；缺失精确来源时导出拒绝，保留旧发布。
- `eiem_compiled_export_carryover.h` 与 `EiemCompiledModWriter::Export` 读取上一已发布 snapshot，合并候选与来源。previous 数据读取失败不再按缓存 miss 忽略，跨 Mod 身份拒绝；来源重定位按相同内容身份更新范围。重编译使用本次作者资源和合并后的输入，生成新的来源块、正负结论和共享 PFB 描述，继承后下一次 source descriptor 也包含新增来源。
- `eiem_prepared_export_carryover.h` 接入真实 `--export-mod`。游戏在导出期间仅补入新 PFB 时，暂存包作者摘要与本次一致才尝试追赶；读取已离线生成的共享描述，只编译新增候选并 `SaveAffected`，不重写既有来源/证明/帧。核心/已知来源变化需要普通重新编译路径。规则删除或目标变更走重新匹配，而不是合并旧编译视图。
- Blender 完整/部分导出共用 `publish_reload`。编译前记录已提交代际，最终发布取得与 C++ 写者相同的路径 mutex 并再次核对；变化时在锁外追赶编译，最多两次追赶仍变化则不发布。检查块提交索引，不读全部 native payload；未提交尾部不当作新代际。partial staging 的 generated bin 先解除硬链接，避免写穿上一包。原子替换使用 ReplaceFileW，保留运行 reader 的旧 snapshot。

生产 reader 与自动适配尚未迁移，不自动把现有普通包改成块格式，也不安装游戏 DLL。Blender 若发现旧 DLL 不能读正在继承的块包，会明确拒绝发布；初始普通包仍输出当前格式。当前挂载优先级适配器未接通时，同一逻辑来源出现两个不同内容版本会拒绝并保留旧包，不能以旧 eidx 或“磁盘仍可读”猜哪个是当前版本。这一限制需要随原生目录迁移解除。

真实 CLI 隔离回归使用 Typhoea 的 50 输入和 Allure 的 150 输入，故意让随包 source-inputs.bin 缺一个叶 PFB，仅向 compiled.bin 提交该来源和路由，再修改作者 Mesh 的 bounds。再次导出与完整冷编译逐 native view 摘要/大小相同，来源/结论保留，下一次导出继续保留。并发样本先编译新作者但缺 PFB 的暂存包，再给已发布包补入 PFB，真实 `--export-mod` 追赶后与完整冷编译一致，所有旧 source/proof/frame 的块偏移保持。partial staging 硬链接样本未写穿 published bin。

Typhoea 追赶 CLI 总墙钟约 554 ms，其中新 PFB 编译约 90 ms；Allure 约 2,093 ms，其中新 PFB 约 110 ms。包括读取暂存/发布包、作者与目录、合并/局部提交，不能把整个命令耗时叫做游戏线程等待或新 PFB 匹配成本。此流程只在 Blender 发布时核对发生变化的代际，不在帧循环执行。初始作者重编译本就需要重新生成受作者编辑影响的视图；并发新增的追赶不再完整重编译。

17 项离线提交测试包括旧编译器能力拒绝、并发追加重试、持续更新不发布、未提交尾部忽略，以及真实 C++ 写者在 Python mutex 内阻塞/解锁后提交。26 项导出核心测试包括 Windows pinned reader 保持旧文件快照。材质增量/空原生贴图槽回归、生产 DLL 构建/PE/隔离加载通过。日志为 diagnostics/export-carryover-typhoea-20261013.txt 和 export-carryover-allure-20261013.txt；用户工程、游戏档案和游戏 Mod 未改动。

配套编译器已备份安装到 Blender 的 nativepack，SHA256 `3D96C212C8F15874A46554821950684231FC5F8F6468110E1EBEE58DFF754972`；当前插件目录 Junction 指向仓库 tools/Blender。备份位于 bin/export-carryover-20261013/backups。实时 Blender 已重载两个导出模块并核对 compiler capability，工程/选择/模式/切换组保持。没有发布未完成的 Allure 或替换游戏 DLL。

## 当前代码状态

- `eiem_prefab_consumer_index.h`：精确本地根归属索引，当前消费者发现已使用；同包多根与非法层级回归通过。
- `eiem_consumer_update_records.h`：候选完成记录及 source/dependencies/rules/algorithm 有效性编码，包含负结论。离线 FindConsumers 已产出这些记录，当前 compiled.bin 尚未分发它们。
- `eiem_mod_update_store.h`：单文件追加块、双提交槽、独立摘要、跨进程写锁、旧块复用、安全整理。中断尾部、撕裂提交、重启、过期写者和负结论持久化回归通过。尚未与现有编译索引迁移或生产加载链集成。
- `eiem_source_adaptation_jobs.h`：CPU/磁盘后台队列，同内容请求合并、待完成交付等待、负结论放行、依赖变化与过期 ticket 拒绝、失败保持及显式重试已模拟回归。未调用 Unity；该回归不证明原生依赖组的首次交付门控已经接通。
- `eiem_resource_catalog_delta.h`：当前挂载目录的内容/位置/依赖分别比较。新增、移除、依赖传播、同包新根的内容变化、纯位置迁移、规则变化及未知候选均有模拟回归；需要游戏适配器提供真实当前条目，不自行猜挂载优先级。
- `eiem_source_adaptation_compiler.h`：单来源 CPU 编译直接复用 `EiemCompiledModWriter::Compile`，没有外部 EXE；结果持有完整 Artifact/必要依赖与目标 native view，后台队列可交付其不可变引用。真实 Endminf 命中与 Typhoea 未命中均通过，前者与完整编译 startup view 摘要一致。
- `eiem_source_namespace_keys.h`：集合仍保留事务 key；隔离 CAB/Bundle 名按每个来源的可达内容、依赖拓扑与参与 Mod 契约决定。新增不相关来源/重排不改旧名称；依赖内容和共享参与者变化失效相关名称。集合组合源码已接入，本地模拟与四 Mod 201 输入的 startup/isolated native view 语义审计通过；当前诊断 DLL 包含该改动，正式自动更新未发布。
- `eiem_compiled_mod_store.h` / `eiem_compiled_source_records.h`：已把全局输入视图/证明拆成按来源独立块，作者 frame 单独存放，共用元数据与来源身份目录不包含所有视图/帧。`SaveAffected` 只编码显式变化来源；`Load` 不拼回旧整包，证明通过固定文件快照按需读取。真实 Typhoea 50 输入、Endminf 150 输入可等价读回；负结论不移动 103/303 个运行块，也不改变运行资源摘要。尚未替换生产 reader/离线 exporter。
- `eiem_game_update_filter.h`：三层分流的纯逻辑接口；当前游戏身份等于完整离线覆盖证明时，相关候选 provider 调用为零，新目标 Mesh 的当前基线导出仍直接复用。目录未就绪保持待处理；旧基线下已有效正负结论排除，新来源/旧来源失效/退休分别输出。当前回归使用模拟游戏身份，真实目录身份与完整离线覆盖证明仍需产生并接入，不能把该测试当成游戏已启用快速跳过。
- `eiem_compiled_artifact_append.h`：新增来源的完整输入视图/证明合并，已有内容和输出必须一致；新增根、依赖映射和 namespace 重定位通过 Typhoea 隔离回归。`eiem_compiled_prefab_update.h` 新增叶 PFB 来源的替换/退休，重新映射保留输入的索引、证明读取与根归属；不读取无关旧证明。含 Mesh/Material/Texture/Shader 的核心来源、有保留输入依赖它的来源、退休全部根的情况明确拒绝，仍需要完整批量核心更新路径。
- `eiem_mounted_source_identity.h`：挂载来源的变更判定使用真实 CHK `contentMD5` 与文件 offset/len，而非 CHK 名称或 Bundle hashName。位置独立编码；真实逐文件摘要可用时才使用。内容不变换挂载位置、同名 CHK 内容变化、范围变化、加密 IV 变化和非摘要占位值均有回归。它是目录有效性 seal，不替代编译所需的来源 SHA256。
- `EIEM_CATALOG_SURVEY`：只读诊断构建；对照当前 Mod 离线候选与游戏原生 `TryGetBundleReverseDeps`。只对目录/查询操作，不读 PFB、不改变替换路由。首次内部数组布局假设与 VFS/Manifest 名称直接等同均被拒绝；现已改用原生接口和编译记录中的实际 Bundle 名。
- 当前生产 LoadRuleInput 对未知输入仍走原版。游戏 VFS 当前内容身份/位置已完成只读实机核验；单文件编译索引迁移、生产目录适配器、未知来源与依赖组首次交付门控仍未接入，尚未发布自动更新。

单消费者补编译隔离对照：复用当前 CPU 编译器，只传入一个候选来源的精确发现结果。Endminf 150 个完整输入缩为 13 个必要输入、1 个 PFB；Typhoea 50 → 29 个输入、1 个 PFB。目标 PFB 的 startup view SHA256/大小与完整编译一致，未编译无关 PFB。仍未实现共享作者资源准备复用，不能把单来源编译时长当作生产加载回调成本。

2026-10-13 早期新增合并保留旧压缩块，但整体编码索引导致 Endminf 约 4.37 秒；这条历史路径只留在差分回归作为对照。当前 `SaveAffected` 不调用整包 Encode，新增一个真实 PFB 时只编码一个来源/一个证明，共用作者帧编码数为零，旧证明读取数为零。49/149 个旧证明和所有旧来源/共享帧块偏移保持，重复提交不增加代际。新格式读回再编码旧格式仅用于证明等价，不发生在新的生产目标路径。

`eiem_prepared_prefab_compiler.h` 为本地验证的 PFB 专用编译片段。共享 Mesh/Material/Texture 的精确资产、schema、骨骼槽与预加载依赖可以离线编码并放进同一 bin；读回后新 PFB 不重新解析这些核心资源、不重新构建作者载荷，只匹配/修改自身并生成视图。早期的作者字节匹配目录已经移除：这个片段只改变 Renderer/层级/引用，未变对象按已验证来源范围保留，变化内容使用 literal，无需搜索作者 Mesh/贴图。核心来源 SHA 变化时拒绝旧描述，需要先适配核心资源。generic Resource/Physics 和拥有核心资源的来源仍需独立路径，不能静默用这个片段冒充覆盖。共享描述尚未由正式 exporter 自动产出；本轮是隔离诊断生成、持久化和回归。

差分调查修复了两项重复/遗漏工作：材质与贴图预加载扩展必须和完整编译一致，不能仅改 Renderer 指针；一次 Mesh 匹配的结果直接用于 Renderer 规则，不重复为每个 Renderer 扫描同一来源。追加回放复用已准备的作者 frame，不再扩展全部 frame。最新 Endminf 记录：旧单源完整编译约 1,317 ms；PFB 专用片段约 703 ms，其中读取约 5.6 ms、精确匹配约 22.8 ms，主要剩余为 Renderer/层级、装配证明、namespace 和视图构建；局部合并与提交约 177 ms，旧整体编码约 3,622 ms。Typhoea 本轮 PFB 片段约 145～356 ms、局部提交约 61～99 ms。机器负载未控制，部分构建/验证同时进行，数字只描述本机样本；不宣称匹配/首次交付已达性能目标。

初始共享描述准备单独计时，约 0.53～1.00 秒，是读取核心来源/建立描述的诊断成本；目标为离线产生，不能算进每个新 PFB，也不能在首次加载回调重复做。本轮仍有初始作者读取/视图加载成本，未测试几十个 Mod 的总体启动时间。完整差分墙钟含强制旧整包重编码、字节对照和往返，不能称为增量更新延迟。日志：`diagnostics/source-record-typhoea-20261013.txt`、`diagnostics/source-record-allure-20261013.txt`。

`PollBeforeNativeGroup` 已通过本地队列回归：未完成返回 Pending，不等待 future、不交付原版；全部就绪且 stamp 一致后才选修改结果，负候选可放行原版，混合依赖组不丢正命中。此接口尚未连接游戏原生队列，不能证明同步入口不会阻塞、整个依赖组已被正确保留或第一帧模型已替换。生产接入前需要核验原生队列待处理语义和重入，当前 `BeforeNativeLoad` 的阻塞 get 不能直接放到游戏线程。

Windows 的文件整理采用 `ReplaceFileW`：旧代际 reader 持有 delete-shared 文件句柄时，`MoveFileEx(REPLACE_EXISTING)` 返回 ERROR_ACCESS_DENIED；ReplaceFile 可保留旧快照并原子发布新文件。固定旧快照整理后仍能按需读证明，整理/撕裂提交/过期写者与并发回归通过。

`tools/diagnostics/compile_consumer_delta.cpp` 为可复现入口，只有临时编译输出，不写游戏 Mod。

### 2026-10-13 后续局部编译与退休回归

本轮消除同一输入中的重复分析：原始 `GraphData` 供装配和 pristine 证明共同使用；修改后的成员只解码变化对象，未变对象须同时满足完整 schema 等价、PathID/type/size 一致和对象字节相同才复用不可变字段。每成员的多个 PFB 根共用分析，根地址和组件序号仍独立。namespace 准备只读取流引用和外部路径，不再为纯名称修改分配每个 PPtr 的清单；原始字段范围、未支持字符串位置和 opaque 对象仍验证。

串行样本：Endminf PFB 专用片段约 209～215 ms、精确匹配约 11 ms、局部合并提交约 108～120 ms；Typhoea 片段约 96～99 ms、精确匹配约 4 ms、提交约 36 ms。此前本机记录 Endminf 为约 703 ms，机器负载未控制，不能把两个样本相除作为正式加速承诺。这些任务仍只适合后台；首次请求时不能在游戏线程直接等上述时间。Endminf 保留 5,981 个未变装配对象，仅重新解码 346 个修改/新增对象；Typhoea 保留 2,094 个、解码 180 个。修改后的原生视图摘要/大小及成员回放与完整离线结果一致；局部 source/proof 编码各一个、共享 frame 编码和旧证明读取为零。

叶 PFB 退休读取该来源的一份证明，其余来源/证明块保留偏移。补回和原子替换、懒加载索引重映射、Renderer 部件计数、重启读取通过两个真实 Mod 回归。临时修改真实来源的 root 名称后，旧请求 stamp 被拒绝，新内容产生新的输入身份/视图，旧来源退休且其他输入保持；移除全部精确 Mesh 引用得到负结论、无替换输出。测试未改游戏档案，未安装 DLL，也未发布 Allure。范围限制仍包括核心资源更新、所有根消失、多 Mod 同来源联合更新和原生依赖组门控。

材质参数增量编辑现在在修改前退休旧 lazy proof reader，避免新存储写回旧证明。50 个 Typhoea 视图与完整编译一致，序列化暂存目录名由诊断直接提取。四 Mod 集合读取回归覆盖 201 个输入、90 个根，157 个隔离输入逐对象/流载荷或完整字节核对通过。旧 `--namespace-replay-audit` 只认整块缓存的断言已修正为同时检查输入视图，视图路径实际核验 member 回放和流载荷，不靠同一绑定与自身比较。

日志：`diagnostics/source-record-*-20261013-range-literal.txt`、`diagnostics/source-record-*-20261013-revised-source.txt`、`diagnostics/source-record-*-20261013-final-local.txt`、`diagnostics/compiled-set-namespace-audit-20261013.txt`。最终串行样本 Typhoea 约 93～97 ms / 35 ms，Endminf 约 211 ms / 108 ms（编译 / 合并提交）。独立保留的 proof 优先于旧 backing reader，避免混合来源代际读取错误索引；对应 reader 明确抛错的回归通过。生产 v35 本地构建、PE 导入/探针排除、宿主旧运行库预加载下的真实 DLL 隔离 attach 均通过；游戏安装 DLL 仍为之前的目录只读调查版。

### 原生首次交付接口调查

只读检查当前 metadata/PE，已定位 `BundleProxy.LoadAsync/LoadSync/UpdateLoading/IsAllDependenciesLoaded`、Manager `_LoadBundleProxy/_LoadBundleProxySync/_UpdateOperationLoadBundle` 和 group/operation 身份字段。同步路径 `_LoadBundleProxySync` 在调用本包 `LoadSync` 前递归处理依赖；仅在 PFB 文件回调处等待会晚于部分核心依赖的加载。后续接入必须在整组首次加载前准备/保留组，不能仅挂 PFB 回调或把 `LoadSync` 的失败返回当成 Pending。当前 PE 含 hotfix 分支和分散函数区间，只读反汇编不证明异步调度的重试/失败语义，需要运行中观察；本轮没有新增 native gate hook。

报告：`diagnostics/game-bundle-proxy-rvas-20261013.json`、`diagnostics/game-managers-rvas-20261013.json`、`diagnostics/game-bundle-queue-functions-20261013.json`、`diagnostics/game-bundle-group-functions-20261013.json`。RVA 只标识被检查二进制，后续绑定仍须核验运行时签名，不以这些地址直接发布 hook。

## 当前游戏目录调查结果

2026-10-13 14:03，游戏进程 37284 加载诊断 DLL `E5C7C84660489E7073B1A90B15ECE650FBD6BC30F48B5CED5919BA54655F1A84`。两份实际挂载 Manifest 都有 255,842 个 Bundle。原生反向依赖候选与现有 Mod 离线候选完全一致：

| Mod | 目标 Mesh 所在包 | 离线来源候选 | 原生查询候选 | 遗漏 |
| --- | ---: | ---: | ---: | ---: |
| LZY | 8 | 60 | 60 | 0 |
| Typhoea | 5 | 33 | 33 | 0 |
| ZFY | 4 | 30 | 30 | 0 |
| lino | 11 | 20 | 20 | 0 |

这里的来源候选包括目标 Mesh 包和其他资源包，不是此前只读报告中的 96 个 PFB 候选。139 个不同候选的原生直接依赖查询均可解析到当前 Manifest 里的键；完整调查分别约 171.962 / 192.783 ms，包含读取 Mod 来源描述、遍历目录名称、反向依赖筛选、辅助查询、报告整理。没有读取 PFB。该诊断没有逐个核验新挂载条目的 MD5/物理位置，也没有新增未知输入首次交付证明。

报告：`diagnostics/game-catalog-survey-20261013.json`，游戏侧为 `plugin/catalog-survey.json`；日志为该会话 `[CATALOG-SURVEY-*]`。诊断 DLL 已安装，生产自动更新尚未开启。这个结果支持继续使用原生反向依赖作为当前游戏样本的粗筛入口，不代表任意未来目录 API 都已兼容。

### 当前挂载内容身份与范围已核验

2026-10-13 15:36:05，进程 17200，诊断 DLL SHA256 `4972FA6C49E5559E99D6EC73E2AFDC3249A28E82624BC9E76409F57E5AA7141B`。139 个候选、17 个 CHK 的原生内容 MD5、物理路径和 offset/len 全部与相应 BLC 一致。9 个来源实际由 Persistent 提供，其余由 StreamingAssets 提供；未猜测优先级。独立只读工具随后读取这些精确范围，139 个逐文件 MD5 均与 BLC 一致（59,299,257 字节）。原生目录调查约 224.750 ms，调查本身 PFB 读取为零；后续字节核验是离线诊断工作。

`GetFileDataMD5Str()` 在此构建没有可读逐文件摘要，日志中的 19 字节 `<unreadable-string>` 是插件 `String` 的转换失败标记，不能称为游戏返回的 MD5、空字符串或原始占位文本。实际 `FVFBlockFileInfo` 不保存该摘要；可靠来源改用 `FVFBlockChunkInfo.contentMD5`。CHK 名称 `md5Name` 与内容 MD5 是两个独立字段，已实际核对。没有真实逐文件摘要时，同一 CHK 的其他范围变化会保守地重新检查该 CHK 内被筛选的来源；不扫描其他 CHK/PFB。内容相同但位置移动可以跳过；重新分块且无法逐文件证明时必须读取候选范围确认。

首次 Manifest 已可查询时，VFS 仍可能没有挂载完成：15:35:52 的第一份诊断 139 次查询失败，15:36:05 的第二个 Manifest 事件全部成功。生产实现必须显式等待目录就绪或挂载事件，查询失败保持未知状态，不能写成负命中或假定游戏内容未变。诊断只在事件边界运行，不每帧重试。

证据：`diagnostics/game-catalog-mounted-20261013.json`、`diagnostics/game-mounted-ranges-verified-20261013.json`、`diagnostics/game-mounted-identity-verification-20261013.json`。可复现核验入口为 `inspect_vfs_lookup_names --mounted` 和 `tools/diagnostics/verify_mounted_catalog.py`。报告包含进程 ID、每来源错误与失败总数，失败报告不得当作有效目录提交。当前安装仍为只读诊断版；Endminf Allure 作者 Mod 保持隔离、未发布。

## 分阶段验收

1. **候选完整性**：四个现有 Mod 的全部离线候选包含在当前原生反向依赖结果中，扩大样本并记录目标包与查询耗时；包括负候选。游戏清单里 255,842 个 Bundle 不等于需要解析同数量 PFB。
2. **来源变化**：无变化、新增包、同包新增根、依赖变化、内容不变位置移动、挂载覆盖、移除与规则变化分别验证。
3. **增量编译**：同一来源片段编译与完整冷编译结果一致；新增记录不改变不受影响的 CAB/Bundle 隔离身份；共享 Mod 输入保持组合语义。
4. **单文件迁移**：离线工具生成初始块文件，DLL 读取并补写同文件；不永久保留两套生产后端。迁移与完整性验证后再部署现有 Mod。
5. **首次交付**：同 Mod 不重新导出，新命中 PFB 第一次创建使用补编译资源；依赖组未提前加载原版；同步和异步不重入死锁。重启时跳过有效内容判断。
6. **性能**：单独记录目录检查、候选冷判断、单来源编译、更新准备墙钟、等待时间与稳定加载成本。完成记录是增量功能；额外缓存最后优化。

允许继续独立本地实现；缺少游戏候选/首次交付证据时不宣称自动适配已经生效。不能保证任意未来 schema、骨架或 Shader 改动自动兼容。
