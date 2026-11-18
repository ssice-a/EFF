# 人物普攻残影冷启动缺失：LOD 可见性查询冲突

## 2026-11-18 lx 单独停用对照与跨 Bundle 脚本预加载

原生产 DLL `5c8ee8b8…` 在 PID 37028 再现六次 32/56 字节和一次 32/64 字节布局错误，随后在世界角色 `PrefabInstantiateProxy._PreloadPrefab` 越界。保留同一 DLL，仅停用 lx，其他九个 Mod 正常加载。PID 12500 正常进入同一场景，日志中没有上述序列化错误。对照日志保存在 `diagnostics/afterimage-cold-start-20261117-111310/script-preload-order-20261118/`。

两次成功加载中，三份关联 Bundle 比洛茜世界 PFB 晚 383–481 ms 提交。十三次失败中，三份关联 Bundle 与世界 PFB 的提交相差不超过 2 ms。NPC 的 `data_npc_avatarmesh_wulfa.asset` 直接预加载世界 PFB 中的六个 `BeyondBoneSphereCollider`，其预加载区间内只有 AvatarMesh 自身的 MonoScript。两个触发布局错误的脚本都在世界 PFB 自身的预加载表中。观察结果支持关联资源先访问世界组件时缺少 MonoScript 的解释，仍需实机确认补齐预加载能消除崩溃。

在代际输入组装中读取同组 PFB 的原生脚本表，对跨 Bundle 预加载补齐脚本引用。每个资产区间先包含脚本，再包含源组件。沿用普通容器、哈希容器及主资产的原生加载区间。真实 AvatarMesh 与世界 PFB 的离线回归在修正前失败，修正后通过。原生范围读取回归覆盖重叠区间、靠后的原有脚本引用和重复组装。生产 DLL 构建、二进制检查及两套运行库下的 DLL 加载均通过。

生产候选 `ee51efe2…` 已部署，回执为 `bin/local-deployments/20261118-064120-788/Dll.json`。lx 的 `mod.ini` 已恢复并按原摘要核验。实机需确认同一场景可连续进入，以及后续人物残影表现。

## 2026-11-18 去重版仍闪退，已恢复原生产二进制

PID 24592 的 Player.log 仍出现六次 32/56 字节和一次 32/64 字节布局不符。EFF 的路由记录显示，游戏创建世界角色实例时收到的源对象未被实例化钩子替换，随后 Unity 在 `PrefabInstantiateProxy._PreloadPrefab` 内越界。日志与回滚回执保存在 `diagnostics/afterimage-cold-start-20261117-111310/preload-dedup-20261118/pid-24592/`。

已补跑 NPC PFB 的原生组装和原版数据逐字节核对。世界与 NPC 各保留 30 个命中目标、88 个 Mesh 部件、38 个命中 Renderer 和 9 份材质。世界预加载由 4716 条减至 1586 条，NPC 由 4715 条减至 1585 条。两份 PFB 各自的依赖引用集合和原有对象的物理排列保持一致，44/42 个原版脚本组件的序列化载荷、指针及类型记录均保持一致。这次重放验证了去重范围，实机结果表明去重未解决闪退。

两个 PFB 在游戏 manifest 中有相同的 39 个直接依赖和 86 个全部依赖，脚本包 `D53F0E5983D207B3` 位于两份原始列表首位。此前快照中该包已加载并持有引用。成功的 PID 39336 和失败的 PID 24592 均记录到世界角色 Bundle 依赖就绪。此前 PID 36552 的首次绑定观测显示，依赖就绪约 24 毫秒后，世界 PFB 的七个组件仍拿不到两个 MonoScript 对象。Bundle 代理的就绪状态无法确定这两个对象在首次绑定时的实际状态。

已事务恢复此前实测可进同一场景的原生产二进制 `5c8ee8b8a22c1ef14664868d64fb17626c2c40de76a33881b2b93b033944c6a8`，安装摘要与备份一致。当前场景的冷启动仍待确认，人物普攻残影尚未验收。

## 2026-11-18 角色预加载去重对照已部署

路由观察版在 PID 43332 仍复现角色加载闪退。触发断言的世界角色实例化调用把原对象原样传给 Unity。此前捕获的世界与 NPC 输入中，分别有 44 和 42 个脚本组件的类型、脚本引用和载荷保持原样。首次绑定仍曾出现 7 个组件拿到空 MonoScript 的情况。

一个挂点替换成多个 Mesh 是受支持的用法。原 Renderer 组装会为每个命中 Renderer 重复写入同一容器里已有的依赖。两份失败输入各增加 3511 条预加载引用，其中 3130 条重复。Renderer 现在复用按容器范围补齐预加载的逻辑，保留原有条目和顺序，并补齐每个新增部件需要的引用。新增原生回归先复现旧代码重复追加共享 Shader，再验证一对多替换、共享依赖及生成对象。

使用当前游戏资源和 lx Mod 离线重放世界 PFB，预加载从已捕获输入的 4716 条降至 1586 条，按 CAB 和 PathID 对比的引用集合相同。30 个目标、88 个 Mesh 部件、38 个命中 Renderer 和 9 份材质均完成组装，44 个脚本组件保持字节一致。此前捕获的 NPC 输入用于统计重复项，本轮实际资源重放聚焦世界 PFB。证据位于 `diagnostics/afterimage-cold-start-20261117-111310/preload-dedup-20261118/`。

本机安装摘要为 `46c79039ea9d682284f8cab31ad2c28241689da81c59c53157ee3abf63dd28ea`，回执为上述目录的 `deployment.json`。构建保留同一 PFB 路由观察点，系统和游戏 MSVC 环境下的 DLL 加载检查均通过。下一次验证应在相同 NPC 与世界角色共存场景进行，进入后先观察 30 秒并检查多 Mesh 部件是否齐全。角色加载闪退的因果关系与普攻残影均待实机验证。

用户确认 lx 的人物普攻残影从进入游戏起就缺失，武器残影仍在。本问题独立于 F10 武器 owner 刷新。已用实际捕获日志、游戏原生控制流和调用边界回归定位到可见性查询冲突；生产修正版已于 12:00 部署，但随后在尚未普攻、加载角色时发生实例序列化崩溃，残影画面尚未验收，当前版本不能据此视为可发布。

## 当前状态：六字节对照失败；完整源码撤回已重建并与可用基线核对

用户反馈六字节对照版再次闪退。PID 11036 的 Player.log 仍为六次 32/56 字节、一次 32/64 字节不符，最后越界；禁用新增条件跳转不足以恢复加载。17:11:50 已事务恢复可用生产基线 `5c8ee8b8…`，回执为 `branch-control/rollback.json`。没有把这一结果认定为 TLS 或地址布局的因果证明。

随后从 `RuntimeShapes::GetEnabled` 源码完整移除新增的 `if(scope)` 和相应注释，普通 `build.bat` 构建为 `3515eb5c…`。逐节及全文件校验得到：机器码、全部 TLS、初始化数据、unwind、资源、重定位及其他内容与可用生产基线完全相同；全文件只在 PE/debug 时间戳、日志 `__TIME__` 字符串和 PDB age 中有差异。`source-control/verify_rebuild.py` 仅屏蔽这六个已解析的元数据范围，再断言整个文件相等；证据为 `source-control/full-file-comparison.json`，不再仅比较部分函数。该构建恢复旧行为，因此人物残影缺失仍然存在，不能称为最终修复。

实机下一判据是完整撤回后同一场景的加载结果；若仍崩溃，应重新检查原基线在当前场景/输入条件下的结果和加载顺序，不再凭脚本错误继续修改脚本表或另建加载后端。

生产隔离验证器及系统/游戏两套 MSVC 环境下的实际 DLL attach/启动检查通过。四仓库 fetch 后 main 均与 origin/main 相等；17:17:40 已事务部署 `3515eb5c…` 到固定 `plugin/eff.dll` 并重新读取摘要一致。INI、协议和所有 Mod 声明未变，可用原基线保留于 `plugin/.eff-deployment-backups/afterimage-source-revert-20261117-171740-083/eff.dll`。部署回执为 `source-control/deployment.json`；已请求用户验证同场景加载，尚未取得结果。

### 17:05 六字节对照部署记录（已失败并回滚）

用户授权围绕可用生产版与失败版的差异做对照。本轮不重新编译、不改脚本或加载入口：以首次失败生产修正版 `ba8205e3…` 为输入，只把 `RuntimeShapes::GetEnabled` 中新增的 Capture-scope 条件跳转禁用。准确变更为 RVA `0x129d2a` 的六字节 `0f855b010000` → `909090909090`，保留此前的 TLS 读取、所有函数地址、所有数据和其余文件字节。诊断生成器 `tools/diagnostics/make_afterimage_branch_control.py` 锁定完整源摘要、原指令、跳转目标及函数边界，输出 `a45af63e…`；这不是从源码构建的生产候选，禁止发布。

该对照把三个假设分开：若恢复正常，新增分支行为成为重点；若仍失败，单纯禁用该行为不足以恢复，还需区分 TLS/布局变化与其他加载差异。仅凭一次通过或失败不能直接认定底层原因。本轮禁用了残影修正，不能同时用它验收人物残影恢复。

精确六字节差异、两套系统/游戏 MSVC 环境下实际 DLL attach/启动与控制锁检查通过。未增加观察钩子、游戏调用、堆采集或 RenderDoc。四仓库 fetch 后均在 main，HEAD 与 origin/main 相等。17:05 事务部署固定 `plugin/eff.dll`，再次读取安装摘要一致；Mod 声明、eff.ini、协议均校验未变。可用基线完整备份在 `plugin/.eff-deployment-backups/afterimage-branch-control-20261117-170542-637/eff.dll`。证据、匹配 PDB、部署回执及可执行回滚脚本位于 `diagnostics/afterimage-cold-start-20261117-111310/branch-control/`。

用户已进入同一 NPC/大世界场景并反馈再次闪退；结果与回滚见上方当前状态。这轮没有验收残影。

## 原生文件入口对照失败及回退

用户确认原生文件入口对照版再次闪退。PID 32720 的 `crash-1625/binding-analysis.json` 有 252 个事件、16 次绑定，仍为 7 次缺 MonoScript 参数、8 次完整绑定、1 次正常显式类绑定；未触及记录上限。Player.log 仍为六次 32/56 字节和一次 32/64 字节布局不符及最终越界。更换入口没有消除该机制，不能把流入口作为已确认根因或把临时落盘留作修复。

16:26 的 `crash-1625/stable-rollback.json` 确认已事务恢复用户确认可进入场景的 `5c8ee8b8…`；本轮再次读取固定安装文件摘要一致。游戏已退出，未改 Mod、INI 或协议。源代码内的残影修正仍未实机验收，不能发布。

随后用当前 lx Catalog、当前游戏 VFS 和生产 Material 构建路径重放了九份材质与对应作者纹理。只读工具 `tools/diagnostics/audit_material_preload_scope.cpp` 逐项解析它们的实际 PPtr 与预加载项：所有外部项均能解析为 Shader（48）或 Texture2D（28），本地预加载目标为 Material（21），没有 GameObject、Transform、MonoBehaviour、MonoScript、Animator 或 SkinnedMeshRenderer。证据为 `crash-1625/replayed-resource-build.log` 和 `material-preload-scope.log`，汇总 `materials=9 characterOrScriptReferences=0 unresolved=0`。该重放支持排除“这些材质直接携带来源 PFB 的整段角色预加载对象”，不等于复现了失败会话的原生反序列化时序。原先保存的 22 份提交输入只包含版本化根输入，不含独立材质；不可将其称为已捕获的全部材质输入。

这里的脚本错误不改变架构边界：我们仍按命中 Mesh 改写游戏请求的 PFB。PFB 原有脚本组件由同一次原生读取创建，EFF 必须保持其输入、依赖和读取条件。当前已定位首次绑定的不完整状态及稍后实例化越界的机制，尚未定位使首次 MonoScript 缺失的具体 EFF 行为；保留原脚本字节不等于已证明加载管理正确。

## 15:49 首次绑定证据

PID 36552 再次在同一场景退出。`crash-1549/binding-events.jsonl` 保留 250 个事件、16 次完整绑定，没有触及 512 上限。世界输入中的六个 `BeyondBoneSphereCollider` 和一个 `EmotionShaderSyncMono` 首次绑定时，组件已保存非零脚本 ID，但传入的 MonoScript 对象为空，返回后组件类仍为空。约 1.3 秒后，同一后台线程为 NPC 输入绑定同一脚本 ID 时，对象和类均已存在；主线程随后创建实例时目标有完整类，却从旧空类源读取不足的字段，触发布局错误与最终越界。

首个 script ID 为零、显式传入 class 且成功绑定的事件是正常路径，分析器现将其标为 `explicit_class_bound_without_monoscript`；不能与上述七次缺对象绑定混为一谈。真实记录分类为正常显式类绑定 1 次、缺 MonoScript 参数 7 次、完整组件类绑定 8 次，已用保存记录断言验证。

`SubmitSharedInput` 的完成日志来自 `NativeBundle::CollectMemory` 轮询观察到请求完成的时刻，不是 Unity 实际建好 Bundle 的精确时刻。因此“空绑定早于完成日志”不能证明脚本绑定发生在 Bundle 创建阶段；当前证据定位的是首次绑定缺脚本对象，仍不能独立判定原因为 Stream 入口、原生依赖就绪时序或输入引用。

### 原生文件入口单变量对照

`build.bat script-binding-file-control` 是独立诊断输出，不是新的生产加载后端。只将源 key `0627FAA2D20B8F97` 的世界根输入，通过已有原生 `LoadFromFile*_Internal` trampoline 提交；字节来自同一份已经改写并隔离的 `input.view`，其他输入、脚本元数据、依赖、游戏 BundleProxy 请求归属及残影代码保持一致。文件由请求持有到 Bundle 退休后删除，写入失败不回退到另一套输入。

构建摘要为 `e834a75c3ea6ea9db12e0787c7b8a45610101fc765214cad20332d9da734c0e6`。真实捕获世界输入逐字节一致、原生只读共享、拒绝覆盖、请求到 Bundle 持有、退休删除和失败清理回归通过；现有捕获桥接回归及系统/游戏两套 MSVC 环境下实际 DLL 加载通过。生产验证器按预期拒绝此诊断构建，不能把它作为生产发布验证通过。未更改 Mod、INI 或协议。

16:21 在游戏退出、四仓库 fetch/main/远程祖先检查通过后事务部署到固定 `plugin/eff.dll`，安装摘要一致。旧观察版与已验证生产基线分别保留；匹配 DLL/PDB 在 `crash-1549/file-control-binary/`，回执为 `script-binding-file-control-deployment.json`。已请求同场景测试，尚未取得本轮结果。

对照判据是相同场景中首次七个绑定是否仍缺 MonoScript，并同时检查角色加载是否成功。若原生文件入口仍失败，须削弱流入口假设并恢复已验证生产基线，继续检查依赖与原生读取控制流；不得把临时落盘直接保留为生产修复。残影恢复和隐藏衣物仍须在解决加载崩溃后验收。

## 15:31 首次绑定观察版部署记录

当前固定安装为首次绑定诊断构建 `08d119bcbd2c8490a4a45460cc7538269ddb4cd555d55c8219077f33ec00716c`，不是已修复的生产 DLL。原先可进入场景的基线 `5c8ee8b8…` 备份在 `plugin/.eff-deployment-backups/script-binding-20261117-153117-181/eff.dll`，部署回执为 `diagnostics/afterimage-cold-start-20261117-111310/script-binding-probe-deployment.json`。只替换 `eff.dll`，INI、协议和 Mods 未改。

本轮静态确认了 UnityPlayer 的五参数脚本绑定入口及两种调用点；入口按 void 原样转发一次。诊断构建通过完整 SHA256、PE 架构/时间戳/映像大小和入口指令校验锁定本机版本，地址只存在于隔离的诊断代码中。记录两个出错类型 `BeyondBoneSphereCollider` / `EmotionShaderSyncMono` 的 MonoScript cache/class、组件 cache/class、脚本 ID、原生参数、线程和栈，在原生调用前后各保存一次。首次根输入提交后，另保留最多 128 次 MonoScript 参数为空的调用，避免按名称过滤漏掉缺依赖的关键事件。总事件上限 512；已有提交/完成/依赖就绪边界使用同一 QPC 时钟，后台线程写入 `plugin/script-binding-diagnostics/pid-<PID>.jsonl`。

该观察不改字段、不增加游戏方法调用、不强制脚本就绪、不增加实例化或预载；不安装异常处理器、不保存大块堆、不启用 RenderDoc。新判据是：首次 MonoScript class 本身为空、MonoScript 已就绪但组件得到空类，或者 MonoScript 对象根本未取得。任何一种结果仍须与原生栈和提交时间关联，不能直接将空类归为某一个已确认的加载原因。

已验证真实 MinHook 的五参数 ABI、原生仅执行一次、嵌套调用、原生异常透传、空脚本记录、非目标/禁用直通、四线程事件发布、记录限额、后台排空及 guard page 不被读取；用实际 UnityPlayer 映像验证入口匹配。记录读回为有效 JSONL，分析器识别完整/未返回的绑定对。两套 MSVC 运行环境下实际 DLL attach/启动锁检查通过，生产校验器明确拒绝该诊断构建。四仓库 fetch 后均在 main 且包含 origin/main。证据位于 `script-binding-selftest/`、`script-binding-probe-build.log` 及上述部署回执。

已请求用户在同一 NPC/世界角色共存场景复现，先不按 F10。尚未取得本轮首次绑定数据，加载崩溃与残影恢复仍未实机验收，不能发布此构建。取证后应修复根因、移除临时观察并恢复生产构建。

## 14:34 候选失败，14:41 恢复已验证基线

外部脚本延后绑定候选 `debd5170…` 在 PID 44004 实测失败。原来的 6 次 56 字节、1 次 64 字节脚本布局错误及 Instantiate 越界仍然存在，还新增 14 次 `Recursive Serialization is not supported. You can't dereference a PPtr while loading.`；旧 PID 38276 日志中该错误为 0。离线字节校验没有覆盖 Unity 的真实绑定时序，不能据此认定该策略受引擎支持。

已撤回 `DeferExternalScriptBinding` 及调用，重新要求 SerializedType（含 scriptTypeIndex）、脚本表、组件 payload 原样保留。下面 14:30 一节仅记录失败实验，不代表当前代码或已安装版本。14:41 已恢复此前用户确认可进入该场景的生产 DLL，SHA256 `5c8ee8b8a22c1ef14664868d64fb17626c2c40de76a33881b2b93b033944c6a8`；只更换 DLL，Mod 与 INI 未改。回执为 `diagnostics/afterimage-cold-start-20261117-111310/crash-1434/stable-rollback.json`。

崩溃根因仍未修复，源代码还包含待实机验收的残影和其他修改，与安装基线不同。继续使用已保存的堆、22 份实际输入和新旧日志检查首次脚本初始化、依赖时序、加载边界与重入，避免再修改脚本元数据猜修。正确链路仍是命中 Mesh → 改写引用它的 PFB 加载输入 → 游戏原生加载和创建世界/NPC/UI 对象；不引入另一套主动克隆实例流程。

### 用户确认的架构边界与本轮复核

按 Mesh 命中定位游戏实际请求的 PFB 消费者，改写其加载输入，保留未声明修改的组件和原生依赖。世界、NPC、UI 各自通过游戏正常的 Instantiate 创建对象。`FullResourceReload::Clone/Instantiate*` 包装器选择代际来源并调用对应原生入口；`CompleteControlledClone` 记录结果和注册控制，不主动再创建另一份角色。这里的实例只是正常 PFB 消费者。NPC 与世界同时命中不能作为禁用替换或另造实例后端的理由。

### 作者新增形态键与既有残影兼容层复核

用户补充：原版 Mesh 可以没有形态键，而作者 Mesh 可以新增形态键；残影应反映作者网格捕获当时的变形。现有实现已经覆盖这个边界，不需要恢复原版网格或另造角色实例：

- `RuntimeControls::Prepare` 从作者导出资源的 `mesh.vertices` / `mesh.shapes` 建立规格，不读取原版 Mesh 的形态键数量作为捕获资格。
- `RuntimeShapes::Bind` 核验替换后 Renderer 实际引用的 Mesh，并为全部导出通道建立索引；`SnapshotFor` 读取该 Renderer 当前各通道的真实权重，`Read` / `Evaluate` 在作者 Mesh 的基准顶点上叠加形态键。这里的网格是局部空间且尚未蒙皮，由游戏继续完成骨骼变换，避免重复蒙皮。
- 仅给本次原生 Capture 的 Mesh getter 返回冻结的 Mesh 副本：清理副本的形态键、写入计算后的位置/法线/切线，保留拓扑、蒙皮与其他既有数据。本体和后续动画继续使用带形态键的作者 Mesh；旧残影不随新权重变化。此处复制的是 Mesh 数据，不是 PFB、角色或脚本组件。
- 生命周期沿用原生 Capture 返回实体及 Release/Reset 的管理；矩阵桥接使用当前 Renderer 的真实骨骼槽和捕获父空间。

本轮重新构建并运行 `shape_snapshot_smoke`，多通道叠加、多帧插值、负权重/外推、法线/切线和快照不可变回归通过；现有 `runtime_shape_bridge_smoke` 的调用边界、显隐、矩阵和生命周期回归通过。这些测试没有重放真实 Unity 的首次脚本初始化，也未验证游戏画面的残影恢复。已有实机证据仍是 Mesh getter 之前的 LOD 可见性拒绝；原版是否含形态键不能解释或成为绕过该问题的限制。

同时继续离线核对首次修正版与已验证基线：相同的 9002 条 unwind 函数记录中，只有 `GetEnabled` 的代码长度增长，其余地址按统一 32 字节迁移；反汇编比较还识别到形态键捕获 TLS 槽位重排，以及原生库内嵌跳表的 RVA 变化。该比较尚未覆盖全部叶函数和所有数据，不能把函数长度变化直接当作完整行为差异证明，更不能据此宣布加载崩溃根因已解决。没有部署新 DLL；安装摘要仍为 `5c8ee8b8…`。

撤回后已重建并运行 `generation_native_identity_smoke`，外部/本地/未设置脚本索引的完整元数据和组件字节保护通过；`--game` 对当前七组 chen/wulfa 世界、UI、NPC 输入的 3290 个对象通过。完整 `full_resource_reload_smoke` 也已重建并运行通过，覆盖同步/异步原生创建入口、参数保持、多 owner、连续 F10、代际卸载和失败恢复。该回归使用模拟原生 API，不能重放 Unity 的首次 MonoScript 初始化，仍不能证明实机崩溃已修复。

补查独立材质附带脚本类型：重放的九份材质中，body 材质保留一条通用 `MonoBehaviour` 类型，`scriptTypeIndex=-1`、scriptID 和类型 hash 均为零，脚本表为空；不是引用缺失脚本槽位的具体 SphereCollider/Emotion 类型。已捕获的 22 份 root 输入也未发现越界脚本索引。这个结果不支持“该材质携带具体脚本的残缺元数据”假设，未据此裁剪生产类型表。

诊断脚本已允许原游戏会话的临时 manifest 文件不存在时继续读取保存的私有堆；纯静态反汇编不再创建堆快照读取器。没有重采游戏堆或部署新 DLL；安装摘要再次核对为 `5c8ee8b8…`。

## 定位与修正

第二版观察 DLL 的 PID 2364 成功进入原生 Capture。人物控制器 `0x101BF0CAF0` 的七次捕获均返回对象，骨骼就绪查询分别为 16/16/16/16/12/16/16，全部真实返回 true；但网格读取、快照和快照 owner 均为零。身体 Renderer `0x1047666480` 已绑定 14644 顶点和两个形态键。另有多件衣物记录为 `authorVisible=0 nativeEnabled=1 enabled=0`。

只读检查本机 GameAssembly 的 `Capture`（入口 `0x55a2f40`）发现，其 LOD 选择循环（`0x55a331e..0x55a339e`）要求整组内每个启用且活动的 Renderer 都可见、骨骼矩阵就绪；任何一个不可见 Renderer 都会拒绝该组，再检查其他 LOD。只有选出整组后才读取 Mesh。这里的 RVA 仅为本次静态证据，不作为运行时定位方式。

`RuntimeShapes::GetEnabled` 为避免作者隐藏状态污染游戏模型的显隐保存/恢复，向游戏返回保存的 `nativeEnabled` 意图。此前残影捕获也读到了这个意图：实际已经被作者关闭的衣物被报告为启用，而引擎的 `isVisible` 正确返回 false，于是整组被拒绝。这解释了就绪查询成功却没有后续网格读取的组合。

修正仅在现有主线程 Capture 的 TLS scope 内让 `GetEnabled` 返回原生 getter 的实际渲染状态。普通游戏显隐保存/恢复仍返回原生意图；不强制可见、不更改 culling/分类、不伪造矩阵就绪，也不改用户 Mod。未增加钩子或每帧扫描。

调用边界回归通过实际 `RuntimeShapes::Capture` 和 `GetEnabled` 重放原生 LOD 筛选顺序，包含同组隐藏衣物与可见身体、世界/UI 两实例，以及真正被剔除的可见部件。修正前失败为 `author-hidden garment rejected the whole visible afterimage LOD`；修正后必须同时保持原生捕获调用次数/返回值、普通显隐保存/恢复和 culling 行为。

证据：`diagnostics/afterimage-cold-start-20261117-111310/combat-1143/`、`lod-visibility-evidence.json` 和 `lod-visibility-red.log`。临时观察源码已移出生产源码，存入该证据目录的 `observation-source-archive/`；独立探针构建选项也已移除。

## 验证与生产部署

- 原生 LOD 调用边界 RED→GREEN、原生 readiness/矩阵/捕获生命周期、世界/UI 三态显隐保存/恢复与模型复用均通过。
- 实际 lx Format 3 Catalog 回归通过：17 个受控部件、34 个初始 Renderer 实例，所有声明的切换键、按住不重复、原生禁用、晚到实例和持久状态恢复均通过。测试读取入口改为复用生产 Catalog；隐藏状态取自作者已声明的切换值，仅在隔离副本内设置，不修改用户 Mod。旧测试对容器及默认隐藏状态的假设已消除。
- 生产二进制隔离检查、系统/游戏两套 MSVC 运行库下的实际 DLL 加载通过。临时观察宏、源码与构建选项已移除，没有新增游戏钩子。
- 四仓库均为 main，部署前 fetch 与远程祖先检查通过。固定目标 `D:\Hypergryph Launcher\games\Endfield Game\plugin\eff.dll` 已核对 SHA256：`ba8205e32c171c36151c035f3b608d1a2e70e9479231d1664ddc9085c2b8000e`。用户 INI 和协议摘要未变。
- 部署回执为 `bin/local-deployments/20261117-120035-552/Dll.json`，完整核验为上述诊断目录的 `production-fix-verification.json`。已验证的旧生产 DLL `5c8ee8...` 仍保留在 11:41 的独立回滚目录，当前安装不含观察版代码。

待实机：重新启动游戏，用 lx 普攻，先不按 F10，确认人物残影恢复且隐藏衣物不进入残影。当前没有把离线通过写成游戏画面已验收。

## 12:08 生产版加载角色崩溃（未修复）

用户确认尚未普攻，加载角色时即退出。PID 11680 使用上述不含观察代码的生产 DLL，再次出现 `The file 'none' is corrupted` / `Position out of bounds`。这削弱了此前把第一版额外观察钩子列为主要嫌疑的推断；目前不能将崩溃归为该钩子、残影 Capture 或已确认的内存耗尽。

已保留安装 DLL 与匹配 PDB，摘要与安装文件相同，证据位于 `diagnostics/afterimage-cold-start-20261117-111310/crash-1208/`。转储主线程 Capture scope 为零。Prefab 代理 TLS 的路径哈希 `0x0d1205ac233f06db` 与日志路径离线交叉核验，唯一匹配 `assets/beyond/dynamicassets/gameplay/actors/postmodels/characters/chr_0028_wulfa_postmodel.prefab`，长度也与 TLS 中的 97 字节一致。路由入口保存的原始和已选源对象相同；这不排除其更早已来自修改后的加载输入。

x64 unwind 的前 29 帧与 Player.log 一致。Unity 克隆循环写入后再读取的临时组件缓冲区总长 20 字节，游标已经到末尾，后续读取 Vector3 时触发断言。实际组件堆对象没有保存在本次转储中，尚不能直接认定是哪一个组件。

更早的 Player.log 已连续出现 6 次 `Read 32 bytes but expected 56 bytes`，以及 1 次 `expected 64 bytes` 的脚本布局错误，随后出现 wulfa PostModel 网格查找失败，最后才是实例化越界。成功进入的生产对照和第二版普攻观察日志中均没有这些布局错误。

只读离线检查已从当前安装的有效 VFS 读取该原版 PFB 及它的 MonoScript 包，没有读取作者索引或修改游戏文件。PFB SerializedFile 为 420824 字节，与作者快照逐字节相同；其 44 个 MonoBehaviour 的类型表脚本索引均与各自 `m_Script` PPtr 一致，12 个被引用的 MonoScript 均能解析到原生脚本对象，且这些脚本对象的字节与作者快照一致。整个 MonoScript 文件与旧快照不同，不能据此认定游戏资源过期或损坏。

当前原版 PFB 恰有 6 个 56 字节的 `BeyondBoneSphereCollider` 和 1 个 64 字节的 `EmotionShaderSyncMono`，与报错数量、大小相符。这是具体组件候选，尚不是崩溃组件身份的直接证明。证据为 `current-scripts.tsv`、`current-script-summary.json`、`current-source-validation.json`、`monoscript-snapshot-differences.json` 及同目录只读诊断工具。原版输入通过检查不代表失败会话实际提交的修改输入或原生依赖生命周期正确；下一步须在这一边界比较实际提交数据与依赖加载状态。

这里的 Clone/Instantiate 是游戏 `PrefabInstantiateProxy._PreloadPrefab` 触发的正常模板实例化：生成带独立变换、骨骼及组件状态的运行实例。它不是复制磁盘 PFB，也不意味着所有 Mesh、贴图和材质各复制一份。EFF 在该入口选择模板，再调用原生实例化；仍须对我们更早的输入改写与依赖管理负责，不能用“游戏原生调用”排除插件原因。

### 实际作者材质重放与现场缺口（12:49）

用户补充，同一场景包含命中 Mod 的 NPC 和大世界角色，旧生产版能够正确处理。这是必须保持的多实例场景，不能将多个消费者或正常模板实例化当作错误原因，也不能以禁用 NPC 替换规避。

只读诊断 `crash-1208/replay_script_input.cpp` 先用实际 lx Catalog、当前有效 VFS、生产 `Author::Compose`、Mesh carrier/consumer 和代际身份改写重放基础组装：30 个逻辑目标、88 个 Mesh part、38 个 Renderer，44 个原有 MonoBehaviour 的 payload、类型记录、`m_Script` 和脚本表均保持一致。此轮保留原生材质，不能单独代表实际生产输入。

随后 `replay_material_input.cpp` 补入真实的 `CollectUses`、`PrepareShared`、`EiemDemandMaterialBuild::Build`、作者贴图和材质 preload closure；来源通过当前 VFS 的精确容器身份核对。作者快照仅提供三个相关来源的原生 bundle 名单，不替代当前游戏字节。实际 9 个 Material 变体全部构建，输出 PFB 长度 643288 字节，与失败会话的提交长度相同；44 个原有脚本组件及其脚本表仍逐字节一致。原运行输入未保存，因此这里只能比较长度，不能宣称与现场输入逐字节相同。

这 9 个独立 Material 的脚本表数量均为 0；其中一个保留未使用的 MonoBehaviour 类型记录，但并未携带脚本表。因此，本次实际材质未支持“独立 Material 继承 donor 脚本表导致失败”的假设。未据此改写或裁剪生产元数据。证据为 `replay-script-input.log`、`replay-material-input.log`、`material-native-bundles.txt` 和 `replayed-material-input/`。

离线重放没有复现 Unity 的原生异步反序列化、运行时脚本对象状态及 NPC/世界消费者时序；它降低基础写入损坏嫌疑，不能证明依赖生命周期正确。当前转储没有源/目标组件堆内存，无法再从现有转储直接确认具体出错组件和 MonoScript 状态。

为填补这一缺口，增加隔离在诊断目录内的外部工具 `capture_crash_heap.py`：只观察指定游戏进程及 **新会话** 的 Player.log；遇到新的脚本布局错误或相同损坏断言时，短暂停住该进程、调用系统 MiniDumpWriteDump 写入 E 盘完整内存，并在 finally 中恢复进程。它不注入代码、不加游戏钩子、不替换 DLL、不修改 Mods/INI/注册表，也不强关游戏。不能单以 UnityCrashHandler64 启动作为崩溃信号。保存日志 EOF 并处理新会话截断，避免误用旧日志触发。

工具已对独立测试进程完成真实 full-memory 转储：197 个内存区间、堆 marker 存在、恢复后测试进程正常退出，证据为 `heap-capture-selftest/capture.jsonl`。完整内存用于补回缺失的堆；若随后发生相同崩溃，Unity 自带的小转储提供原异常上下文，两者需按同一 PID 配对。

截至此阶段，本轮未修改、构建或部署生产 DLL，安装文件仍为 `ba8205e3…`，不是已修复版本；后续需要在用户描述的同一 NPC/大世界场景采集一次新现场。原残影可见性修正仍未完成生产画面验收。

### 12:53 再现及采集器权限纠正

PID 31916 再次在同一场景退出，仍是 6 次 56 字节和 1 次 64 字节脚本布局错误，随后 InstantiateParent 中断言。日志与 Unity 小转储已保存在 `crash-1253/`；没有取得完整堆转储。旧采集器对游戏进程的 OpenProcess 返回 WinError 5，却静默忽略，先前的 armed 回执不能代表它具备读取游戏内存的权限。独立自检只验证同权限进程，未覆盖这一权限边界。

管理员补抓工具曾启动，但当时目标已经退出。Windows 仍可枚举 PID 31916；GetExitCodeProcess 的实际退出码为 `0x80000003`，不能把残留进程对象当作存活现场。用户未看到管理员确认框，不等于管理员 helper 没有启动；必须检查 helper 自己的 token 和日志。

采集器已增加有限查询权限下的退出码预检、明确上报访问失败、启动 PID/token/debug privilege 回执，以及 arming 前的管理员权限要求。真实 full-memory 自检再次通过，另以保留句柄的已退出子进程验证不会误选终止 PID。自检证据为 `heap-capture-selftest-v2/`。

12:58 已启动新的管理员采集器 PID 40644，`heap-capture-admin/capture.jsonl` 实际记录 `administrator=true`、`debugPrivilegeEnabled=true` 和随后 armed；请求用户再现同一场景。输出在 E 盘，尚未取得本轮完整转储。未改变游戏 DLL、INI 或 Mod；崩溃根因与残影画面仍未验收。

### 13:04 再现：DbgHelp 拒绝访问，未取得堆

PID 41668 再现相同的七次脚本布局错误和 Instantiate 越界。管理员采集器确认具有 SeDebugPrivilege，13:04:12 捕捉到新错误，NtSuspendProcess/NtResumeProcess 均成功，但 MiniDumpWriteDump 返回 `0x80070005`（拒绝访问），文件长度为零。这次不能称为完整采集成功，也不能把成功提权等同于所有转储权限已满足。13:04:40 补抓时目标已退出。日志、Unity 转储和 WER 转储保存在 `crash-1304/`。

已移除非必要的 HandleData/ThreadInfo 等辅助转储选项，并添加不依赖 DbgHelp 的直接读取方式：VirtualQueryEx 枚举可读、已提交的 MEM_PRIVATE，ReadProcessMemory 保存字节及虚拟地址映射到 E 盘。该备用数据是原生/托管堆快照，不是覆盖映射文件和模块镜像的完整进程转储；缺页明确记录。它不读取 guard 页，不复制可能很大的游戏资源文件映射，finally 仍负责恢复目标。

普通权限和管理员权限的真实子进程直接堆读取自检均通过：约 3.5 MB 堆、marker 存在、无读取缺页、恢复后正常退出；管理员自检证据在 `heap-capture-admin-raw-selftest/`。最小选项 MiniDump 自检也通过，但尚未证明游戏目标允许该接口，后续实机选择直接堆读取。

13:06 起管理员直接采集器 PID 28044 等待首次布局错误；另有最终损坏断言采集器 PID 39292，分别写入 `heap-capture-direct/` 与 `heap-capture-corruption/`。对实际新游戏 PID 先记录真实堆读取是否通过，不能只靠 token 回执认定可读。用户已被告知本轮采集失败并请求再现；尚未取得游戏堆或确认根因，生产代码、DLL、INI 和 Mod 均未改变。

### 13:08 外部读取全部被拒，13:16 部署进程内诊断

PID 38640 启动初期实际读取 64 字节成功；场景加载后，外部工具枚举的 5,601,136,640 字节私有已提交内存全部返回 WinError 5，没有任何字节被保存。13:08:44 NtResumeProcess 成功，随后采集器正常报告失败并退出；另一采集器也因实际读取验证失败退出。零字节文件不能用于分析，也不再继续让用户重复外部采集。相同布局错误再次出现；未据这些失败认定游戏内存损坏的具体机制。

隔离工具 `tools/diagnostics/serialization_crash_snapshot.h` 改在进程内读取自身堆：只观察选定 UnityPlayer 模块的 breakpoint 异常，在独立工作线程保存异常寄存器、可读 MEM_PRIVATE 和虚拟地址映射。主线程短暂等待保存完成，随后返回 EXCEPTION_CONTINUE_SEARCH，保留游戏自己的异常处理；不吞异常、不强制关闭、不改游戏对象、不增加游戏方法钩子。跳过 guard 页、映射文件和采集工作线程自身栈；写入前核对空间，缺页和写入失败在元数据中标明。正常运行只保留等待事件的工作线程。

另在现有 SubmitSharedInput 边界保存最多 32 份、每份不超过 16 MiB 的 versioned-root 实际输入及依赖 key，便于与离线重放核对；保存源为最终隔离后的 input.view，不是再读原版 PFB。数据路径仅供此隔离诊断构建使用，未加入 Mod 协议或用户 INI。

真实 Win32 VEH 自检通过：无关异常被忽略，目标断点触发工作线程，保存后原异常继续传递，工作线程退出；根据捕获的虚拟地址映射读回 128 KiB 标记逐字节相同，缺页为零。`inprocess-selftest/pid-30392/verification.json` 为回执。诊断 DLL 两套 MSVC 环境下真实加载通过，四仓库 main 包含 fetch 后 origin/main；新增的生产二进制检查明确拒绝此快照构建，避免误发布。

13:16 已在游戏退出后事务部署 `build.bat serialization-probe` 的独立输出，仅覆盖 plugin/eff.dll，SHA256 为 `e053687729865a191f359fa8d4776b0a6cd55592805e09ac0fea089224a5bc09`。INI、协议和 Mods 未变。原安装版本 `ba8205e3…` 备份在 `.eff-deployment-backups/serialization-snapshot-20261117-131613-617/`；匹配诊断 DLL/PDB 保存在 `serialization-probe-binary/`。部署回执为 `serialization-probe-deployment.json`，本轮待游戏验证的数据目录是 `inprocess-crash-next/pid-<PID>/`。

当前是取证版，尚未确认或修复崩溃根因，尚未取得游戏进程内快照，也未验收残影。必须完成诊断并恢复生产构建，不能发布此 DLL。

## 13:20 现场堆取证成功（根因仍未修复）

PID 38276 的进程内快照已完成：私有可读内存约 5.73 GB，只有 20 KiB 读取缺页，关键组件完整；最终提交的 22 个输入也已保留。证据位于 `diagnostics/afterimage-cold-start-20261117-111310/inprocess-crash-next/pid-38276/` 与 `crash-1320/`。当前安装仍为 `e0536877…` 取证构建，不可发布。

真实出错组件已从寄存器、克隆 ID 映射和类继承链交叉确认是 `BeyondDynamicBone.BeyondBoneSphereCollider`。源组件 `0x28A27A5B0` 缓存的脚本类为空，克隆目标 `0xAF17BF10` 却已识别正确类，双方引用同一个 MonoScript instance ID `0x2CAB4`。源组件缺少脚本字段，克隆缓冲区只有基础字段，目标按完整脚本读取时越界。该机制解释了最终断言，尚未解释源组件为何首次无法识别脚本。

已排除或削弱的分支：

- 对实际提交的世界输入 `input-10.ab` 与 NPC 输入 `input-20.ab`，逐字节核对原有 44/42 个 MonoBehaviour 的 payload、m_Script、完整类型记录和脚本表，全部保持当前原版。两种 PFB 的普通与 hash 加载入口均保留正确脚本预加载区间，MonoScript 引用仍位于表首，顺序未改。
- 原生 MonoScript `0x289154C70` 的名称、程序集及 properties hash 与原版脚本和 PFB SerializedType 相同，现场已能解析正确类。未发现把作者脚本快照替换进游戏的证据。
- 从真实 manifest 映射复原世界 PFB 的 86 个 all / 39 个 direct 依赖，并按现场原生 `bundlePools` 字典逐个核对。脚本包 `initial/2dbc0ac4e142db9292c8949b.ab` 的 proxy 已成功、request 为空、引用数 1959，未发现脚本包已释放的证据。其他被代际别名替换的 native key 不存在本身不代表依赖遗漏，需按别名核对。

继续检查原生脚本首次初始化与修改输入的加载边界。不能把“依赖太晚”、流读取错误或引擎缓存冲突当作已确认根因；截至此次现场分析结束，尚未修改生产代码或部署新 DLL，也不需要重复获取同类数据。

## 14:30 外部脚本延后绑定实验（14:34 实机失败，已撤回）

进一步检查本机 UnityPlayer 的脚本传输控制流：组件保存的 MonoScript instance ID 与传入 ID 相等时，绑定函数可直接退出；首次绑定若未取得脚本类，随后读到相同 `m_Script` 不能保证重新建立类状态。它与现场“同一脚本 ID、源组件空类、克隆目标有完整类”的状态吻合。为何初次解析得不到类仍缺少直接的时序证据，不把这一输入修正称为已实机确认的最终根因。

`EiemNativeGenerationIdentity::Isolate` 现在仅对重建输入中指向外部 MonoScript 的 MonoBehaviour 类型，将可选的提前绑定索引 `scriptTypeIndex` 设为原生支持的 `-1`；组件仍通过自身 `m_Script` 字段在反序列化阶段交给引擎绑定。本地脚本和原本未设置索引的类型保持原样。不删除组件、替换脚本、清空脚本身份表或裁剪依赖，不增加游戏方法钩子、帧扫描或主动实例化。

验证范围：

- 代际输入调用边界回归先失败、修改后通过；本地/外部/未设置索引类型、脚本身份表和组件字节保留。当前游戏七组 chen/wulfa 世界/UI/NPC 输入的 3290 个对象通过原生代际检查。
- 现场全部 22 份最终提交输入经过同一生产适配，再由独立 AnimeStudio 解析器读回：37 个外部脚本类型延后绑定，113 个原有 MonoBehaviour payload（含 `m_Script`）、脚本 hash、身份表、预加载顺序和容器区间均不变。明细为 `crash-1320/all-inputs-binding-verification.json`。
- 完整资源热重载回归通过；生产二进制检查通过，包含实例生命周期和残影修正，排除快照、RenderDoc 与临时观察代码。系统和游戏两套 MSVC 环境下实际 DLL 加载通过。
- 四仓库 `main` fetch 后均包含远程 `origin/main`。游戏已退出，使用固定事务部署脚本安装；SHA256 为 `debd5170faf59ed63d7af005941c221a8f3ba1d1f4ee00e21c2b0aa77b2d6717`，回执为 `bin/local-deployments/20261117-143038-228/Dll.json`。保留原取证构建备份及此前稳定生产回滚副本；未发布或推送。

当前安装已恢复为不含取证模块的生产构建，带上述待验证修正。尚需在同一 NPC 与世界角色场景验证加载不再崩溃，再验证 lx 普攻人物残影；离线与加载检查不能代替这两项实机验收。若仍失败，先分析新日志与现有快照，不直接重复完整堆采集。

## 初始现场（修正前）

- 现场 PID 27588 使用 11:03 部署的生产 DLL；没有本轮 F10 记录。
- 形态键捕获桥接安装成功。lx 身体绑定为 14644 顶点、2 个形态键，布局匹配。
- 该会话没有快照、捕获 owner、矩阵或捕获异常日志。这只说明没有进入记录这些事件的分支，不能证明原生 Capture 没有调用。
- 本机游戏的 `EntityRenderCaptureMeshController.Capture` 会检查可见性、Renderer 分类过滤、骨骼矩阵就绪和 Mesh 形态键数量；任一阶段都需要实际返回值来定位。

## 诊断时区分的路径

1. 人物残影没有进入原生捕获：观察 Capture 入口和返回值。
2. Renderer 在捕获中被过滤或骨骼矩阵未就绪：观察真实可见性、`_PassRendererMaskFilter` 与 Finished 结果。
3. 已进入 Mesh/快照生成但最终不可见：关联源 Mesh、快照和捕获返回对象，随后检查生成物。

这些是诊断分支，不是已确认原因；不得强行启用 Renderer、放行分类或伪造矩阵就绪作为修复。

## 临时观察版记录（构建入口已移除）

`build.bat afterimage-probe` 单独输出到 `bin/resource-directory-entry-format3-afterimage-probe/`。仅定义 `EIEM_AFTERIMAGE_PROBE`，不启用通用诊断或 RenderDoc。原有生产输出保留。

观察仅在主线程 Capture 范围内运行，最多 16 次 Capture、128 个去重 Renderer 结果；保留原生调用参数、布尔值和捕获返回对象。前后测试覆盖原生调用一次、真实 readiness 返回与达到上限后停止观察，以及原有矩阵/形态键/生命周期回归。

11:26 已在游戏退出后事务部署观察 DLL：

- SHA256：`95681c3222c975c46ee7a9cb06ee184c771e28a63fa799c8367fdc36fd3021a3`
- 位置：`D:\Hypergryph Launcher\games\Endfield Game\plugin\eff.dll`
- 生产回滚 SHA256：`5c8ee8b8a22c1ef14664868d64fb17626c2c40de76a33881b2b93b033944c6a8`
- 生产备份：`plugin/.eff-deployment-backups/afterimage-probe-20261117-112652-139/eff.dll`
- Mod、全局 INI 与协议声明未修改。

独立输出构建、原生捕获桥接测试、二进制诊断边界检查以及系统/游戏 MSVC 运行库下的实际 DLL 加载检查均通过。四仓库 main 等于 origin/main。证据及部署回执位于 `diagnostics/afterimage-cold-start-20261117-111310/`。

## 11:32 观察版闪退与回退

PID 43832 在残影 Capture 入口出现前退出。Player.log 原文为 `The file 'none' is corrupted` / `Position out of bounds`，调用栈位于 Unity 的实例序列化读取、`Object.Instantiate`、`PrefabInstantiateProxy._PreloadPrefab`。异常为 Unity 主动触发的 `0x80000003`；转储时系统已提交 36.063 GiB、上限 41.853 GiB，余约 5.79 GiB，不能归因为已确认的系统提交内存耗尽。

11:33 已恢复并核验上述生产 DLL。用户随后确认同一场景正常进入；PID 39336 的日志正常退出，没有本轮损坏断言。此对照把观察版额外拦截点列为重点嫌疑，不能由一次对照认定具体哪个钩子为最终根因。

第二版撤掉所有额外拦截点和额外游戏方法调用，仅在原有 Capture、Finished 与 Mesh 回调内记录已有参数和返回值。它不查询更多 Renderer 属性。回归直接通过真实 Capture 适配入口确认原生函数只调用一次、参数/结果/ready 状态不变、Mesh getter 次数不增加，以及采样去重和上限。

11:41 已在游戏退出后部署第二版观察 DLL，SHA256 为 `b8b1a9572e8e05f4b02d5d0fb0a797e43b4f7519bbd366910d39bf07098501a8`，回执为 `diagnostic-deployment-v2.json`。生产 DLL 再次备份到 `plugin/.eff-deployment-backups/afterimage-probe-20261117-114137-924/eff.dll`。构建、零额外钩子/调用检查、捕获桥接回归及两套运行库下实际加载检查通过；用户随后完成普攻并取得上述捕获证据。本轮没有更新 Mods、INI 或协议。

证据保存在同目录的 `crash-1132/`，包含两轮完整日志、转储、内存事实和生产回退回执。结束诊断后必须恢复已验证的生产 DLL，并清理临时观察代码，不能把观察版当成生产修复发布。
