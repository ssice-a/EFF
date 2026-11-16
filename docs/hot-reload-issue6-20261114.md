# Issue 6：增删 Mod 的 F10 资源代际修复

本次审查针对 [EFF issue 6](https://github.com/ssice-a/EFF/issues/6) 的空 Mod 集合进入游戏后添加 Mod（0→1），以及移除后再次添加（1→0→1）。用户已授权按现有架构适配；本次没有推送或发布 Release。

## 20:41 启动闪退：本轮未通过实机验收

20:37 部署版的用户复测在尚未按 F10 时闪退，不能把本轮写成已修复。已保存 `diagnostics/issue6-disappearance-20261114/Player.new-crash.log` 与 `eff_log.new-crash.txt`。20:41:42 武器请求自动命中已安装的 `langzhifei`，29 个输入合计 15,955,712 字节，全部完成且没有重复提交；`lizi` 挂载也已到达播放。用户没有主动换模型，不等于已安装 Mod 尚未被游戏请求命中。

Player 日志最后是两次 `Commit memory failed, error code is: 0x1000000 (:870)`，本轮没有此前 13:15 那次的 Vulkan 分配失败记录。已从当前 `UnityPlayer.dll` 核对：该日志对应原生提交内存路径，底层 `VirtualAlloc(..., MEM_COMMIT, ...)` 失败，Windows 错误 8、14、1455 归一为 `0x1000000`。二进制摘要与相关反汇编保存在 `unity-commit-failure-disassembly.txt`。这能确认失败的分配类型，不能单独证明 EFF 泄漏或排除它的资源驻留成本。

同一会话另有 35 次合成 Bundle 标识的原生目录缺失。已用生产入口回归复现：存在未解析目标时，`SharedGetOrCreate` 把已发布的合成依赖重新交给 `PrepareDependencyGroup`，绕过查询钩子调用原生目录，并通过仅供查询的 VFS 别名读到原来源 CAB，继而产生身份冲突。当前修正让代理选择和准备入口均先识别已发布输入，包括旧代仍存活的输入；它们继续使用自己的图，不再重走原版发现。回归先失败再通过，完整热重载、依赖图及代际原生身份回归通过。这个缺陷与最终内存失败的因果关系尚未确认。

新增只读 `tools/diagnostics/watch_runtime_memory.py`，每半秒采样真实系统提交量/上限、游戏与 Blender 的私有提交量及 NVML 全局显存，在游戏退出时保留日志。它不注入游戏、不更换 Mod、不修改分页文件。单次采样已与当前进程和显存读数核对；后续实机需以这个时间序列定位峰值，不能用进程退出后的空闲量代替。

发现入口修正版已于 20:53:26 安装到游戏固定目录，3,536,384 字节，SHA256 `6cad5a57f8d6c489f9b9e820bed72a1c1daecee295d0f3bf9721b206b41ade7f`，构建与安装文件独立核对一致。回执：`bin/local-deployments/20261114-205249-412/Dll.json`。生产校验排除截帧探针。独立采样输出为 `diagnostics/issue6-disappearance-20261114/live-discovery-fix/`；本轮先验证原场景启动稳定性，尚未进行 F10 验收。

### 20:57 再次崩溃：系统提交量实测

用户确认再次闪退。修正后的 Player 日志中原先 35 次合成目录缺失已降为 0，CAB 重入冲突为 0，但仍出现原生提交失败，故不能宣称本轮已解决闪退。29 个输入仍各提交一次；特效只到达挂载绑定日志，不能把先前会话的成功播放写成本轮结果。

半秒采样记录了以下时间相关性（GiB/MiB 为二进制单位）：

| 时点 | 系统已提交 / 上限 GiB | 可提交 MiB | 游戏私有 GiB | 显存空闲 GiB |
| --- | ---: | ---: | ---: | ---: |
| 20:57:16.374，组发布前 | 35.486 / 36.448 | 985.262 | 5.095 | 3.716 |
| 20:57:16.870，组发布后 | 37.261 / 37.467 | 211.230 | 6.871 | 2.054 |
| 20:57:28.884，最后有效采样 | 37.433 / 37.467 | 34.828 | 7.186 | 1.973 |

独立采样器随后在 `CreateToolhelp32Snapshot` 收到 Windows 错误 **1455：页面文件太小，无法完成操作**，并退出。因此这轮系统提交不足已有独立证据；组加载附近约 1.8 GiB 的进程增量尚未通过分配栈归属或原版对照归因，不可用输入仅 15 MiB 来排除插件成本。采样器现已容忍这类进程快照失败，保留系统采样并在异常退出时备份日志；该错误注入回归已通过。

WMI 配置声明 F 盘分页文件为 17,217–32,228 MiB，但实时 `Win32_PageFileUsage` 与原生 `NtQuerySystemInformation(SystemPageFileInformation)` 均只返回 C 盘实际分页文件；F 盘未生效的原因未判定。C 盘当时约剩 3 GiB。没有修改分页文件、注册表或重启系统。

### 21:11 无 EFF 对照仍崩溃，已恢复生产 DLL

21:02:45–21:11:11 的对照仅暂把 `plugin/eff.dll` 重命名为 `eff.disabled-memory-control`，Mod/Blender 保持原样，代理 DLL 和其他插件未变，因此不能称为完全原版对照。用户确认同样闪退；保留的 `Player.no-eff-user-confirmed-crash.log` 也包含同一条 `Commit memory failed ... 0x1000000`。期间 EFF 日志未产生新会话；模块枚举没有返回模块，不能当作成功的模块级核验。

无 EFF 采样捕获的游戏私有提交峰值约 8.526 GiB，系统可提交余量最低约 0.360 GiB。采样器在一个进程间隙停止，不能把这些数值当作用户最后一次崩溃的精确峰值，也不能由不同进程/时段直接计算 Mod 额外成本。现有证据确认 EFF 不在场时也能触发同类提交失败；它不排除 EFF 对资源峰值或驻留量的影响。

21:14 只读复核：C 盘约剩 10.81 GiB；实际仍只有 C 盘 3,452 MiB 分页文件，F 盘配置仍未实际启用。当前游戏已退出，系统提交量约 24.12 / 35.11 GiB；退出后的余量不能代替崩溃前测量。未删除文件、修改分页配置或重启系统。清理 C 盘能提供增长空间，但必须另外核验分页文件是否真正生效。

同一生产修正版已于 21:11:11 恢复到固定 `plugin/eff.dll`，再次核对 SHA256 为 `6cad5a57f8d6c489f9b9e820bed72a1c1daecee295d0f3bf9721b206b41ade7f`；21:14 复核一致，临时禁用文件已不存在。恢复记录在 `no-eff-control-journal.json`。先解决系统提交余量，再继续 F10 的 0→1→0→1 实机验收；轴向仍未验收。

### 21:34 用户授权清理与分页文件实际启用

用户随后明确授权清理 C 盘崩溃报告、EFF 缓存和历史遗留，并使其他盘分页文件生效。维护记录保存在 `diagnostics/system-maintenance-20261114/`，与此前崩溃取证分开。未修改 Mod、DLL 或 Blender 插件代码，未重启系统或强制退出程序。

- 已删除 21,151 个文件，逻辑大小 5,466,409,422 字节（约 5.09 GiB），覆盖已发现的用户/Windows 崩溃报告、Endfield 崩溃记录、EFF/EIEM 临时测试与旧发布目录、纹理/索引缓存及旧插件更新备份。管理员复核包括系统报告目录和 Windows Temp 转储；维护结果没有删除失败项。21:34 C 盘约剩 16.76 GiB；磁盘空闲量还受其他程序影响，不能把前后差值全归为本次删除。
- 可能被 `.blend` 引用的特效源与当前构建数据共 132,493,394 字节迁至 `E:/EIEM_Workspace/.cache/blender-preserved-sources/`，逐文件哈希验证后才移除 C 盘原文件，并用目录联接保持原路径。未删除 Blender 自动恢复文件或已安装插件。
- 纹理缓存、AnimeStudio 索引缓存和原生按需快照目录现在通过目录联接写入 `E:/EIEM_Workspace/.cache/`，避免这些固定目录的有效载荷继续占用 C 盘；软件设置、资源根配置和安装元数据保留。
- F 盘配置为初始 16,384 MiB、最大 32,768 MiB，C 盘系统管理设置保留。管理员维护程序成功调用原生分页文件创建入口，并以实时 `Win32_PageFileUsage` 观察到 F 盘分配 16,384 MiB，系统提交上限从约 35.11 GiB 升至 51.11 GiB。这次已经实际生效，无需重启，不是只有注册表配置变化。

`final-verification.json` 记录上述结果和原生产 DLL 摘要一致。当前只完成系统内存条件修正；启动稳定性、F10 的 0→1→0→1 和特效轴向仍未重新实机验收。

## 19:51 实机回归：人物消失与当前修正

19:32 部署的第一次修复未通过实机验收。用户在原版会话加入 Mod 后按 F10，世界与 UI 人物消失。日志快照位于 `diagnostics/issue6-disappearance-20261114/eff_log.snapshot.txt`。

19:51:40 的 generation 2 事务先释放了 `chr_0005_chen_postmodel.prefab` 的旧 owner，再等待原版 Bundle 退休。四个 Mesh 来源 Bundle 仍有合法引用（例如 `D44BBE0EC853337D` 的 refcount=1）；延迟重试不会使这些引用自动消失。随后 UI 的四个合成依赖也在提交时被同一门禁拒绝，依赖就绪轮询超过七万次。根因不是 Mod 未被发现，而是新资源仍沿用内部 CAB 身份，试图靠退休原版资源来避免身份冲突。

当前源码改用原生输入的完整代际身份隔离：

- `eiem_native_generation_identity.h` 在真实请求的依赖组内，为 SerializedFile 和资源流成员生成代际名称，同时更新已隔离依赖的外部 archive 路径、流式描述符和 AssetBundle 名称。保留 FileID 顺序、PathID、GUID/type 记录；未修改的游戏依赖继续使用原身份。不会搜索替换任意 payload 字符串。
- `eiem_entry_demand_shared_inputs.h` 在共享图发布前完成隔离，保留原准备视图用于同代缓存校验；实际提交的是隔离后的惰性视图。未变像素/压缩块继续引用源范围，不全量读入。
- `ReleasePath` 不再以原版 Bundle 归零作为 F10 的前置条件；已隔离输入提交也不退休原版或其他代际的 Bundle。原有 Asset/Prefab 缓存失效与 owner 生命周期流程继续生效，旧资源由真实持有者释放。
- 原生异步提交返回空请求时，调用游戏自己的失败完成入口，避免代理永久处于 loading 状态。已有请求仍交给游戏泵送，不伪造状态字段。

这次修正不等于所有加载失败场景都有事务回滚。现有原生 Asset/Prefab 缓存门仍可能等待真实引用，不能把代际身份隔离写成通用的旧画面保留保证。

### 本轮自动验证

1. `build_generation_native_identity_smoke.cmd`：先观察保留原版 CAB 的失败，再验证跨代不同身份、同代稳定、Prefab→Mesh 引用、原版依赖保留、资源流匹配和普通资产名不变。8 MiB 像素 fixture 准备只读取 16,629 字节。
2. `bin/generation-native-identity/smoke.exe --game "D:/Hypergryph Launcher/games/Endfield Game"`：读取当前 VFS 中本次故障的五个来源包，检查 961 个真实对象、4 个跨 CAB 重映射、39 个流字段及原生 schema 保留；不创建游戏对象，不覆盖游戏文件。
3. `full_resource_reload_smoke.exe`：旧 Bundle 合法引用保持为 2 时，新隔离输入和 F10 缓存门不再等待或卸载它；异步空请求回归先失败，修正后通过。整个 owner/世界/UI/动画/连续 F10 回归通过。原生引用 fixture 验证实际门禁；不代表已经实机完成世界/UI 全流程。
4. `build_native_random_access_smoke.cmd`、`build_demand_resource_regressions.cmd`：范围读取、压缩块、按需装配、共享图及材质移除回归通过。

本轮输出位于 `diagnostics/issue6-disappearance-20261114/`。修正版生产 DLL 已于 20:37:32 部署到固定游戏目录，大小 3,535,872 字节，SHA256 为 `dda6bd7a5b7c21b52de3dcc02c9d1cd4a55737bd9cc8a578cc7c2a6ee7719140`。独立重读构建和安装文件确认摘要一致；生产校验确认无截帧探针。回执为 `bin/local-deployments/20261114-203652-244/Dll.json`，保留用户 INI 和 Mods。源码仍为 main 工作区未提交修改。用户实机复测待反馈；上述离线验证不能代替游戏内 0→1→0→1 的验收。

## 第一次修复的外部参考（下文为已被修正的历史）

参考 BakaMai 的 [EFF-HotFix 修复提交](https://github.com/BakaMai/EFF-HotFix/commit/55b2210d15b07eb2b13aac268fa9fb37e3ede660)，对照基线为 `f117a4e768c0462dd8c40d20c2424b3e23f60218`。比较记录保存在 `diagnostics/issue6-hot-reload-20261114/fork-comparison.json`。这不是对整个分支的合并。

采纳原版实例候选补全、原生源 Bundle 退休、移除 Mod 后返回原生 VFS 的容器清理，以及 `.asset` 命名子资源查询顺序。没有采纳在 BundleProxy 仍有引用时直接 `AssetBundle.Unload(false)` 的兜底，也没有合入整个文件读取和快照性能改造。

## 可复现缺口

1. `DemandGenerationReload::Affected` 原先只识别作者 Mesh/Renderer 标记。首次加入 Mod 时，世界和 UI 中仍是原版 Mesh，因此没有进入 owner 刷新事务。
2. 原生源 Bundle 不在 EFF 的内存流记录中。仅调用 `NativeBundle::UnloadIdentity` 无法退休游戏原来的 BundleProxy。
3. 删除 Mod 后回到普通原生 VFS 路径时，没有对应的旧合成 CAB 退休入口。
4. `.asset` 先查询整个路径，可能取得容器信息而没有优先取得声明的命名子资源。

## 第一次修复的实现与职责

- `eiem_entry_demand_generation_reload.h`：原版 Mesh 名称只提供刷新候选，剥离 Unity 的 Instance/Clone 后缀；再以游戏的逻辑 Prefab 路径或已观测路径哈希，查询原生 manifest 的 Bundle 依赖范围。实际资源替换仍由原来的精确逻辑身份检查执行。同名但不在该 owner 依赖范围内的目标不会触发刷新。结果仅在本次候选收集内缓存，避免世界/UI 共用路径时重复查询。
- `eiem_entry_bundle_dependencies.h`：只允许零引用、非加载中、非卸载中、非锁定的 BundleProxy 调用游戏自己的 `UnloadSync`。临时保留已加载对象，并在成功或异常后恢复原卸载策略。
- `eiem_entry_demand_bundle_handoff.h`：协调已观测原生/合成容器的退休、有限的反向依赖遍历、普通 VFS 返回路径和未完成加载屏障。只查现有缓存，不创建额外游戏代理；不引入第二套加载器。
- `eiem_entry_demand_shared_inputs.h`：匹配的合成 VFS 输入加载失败时返回已处理的失败，不能继续落到原版字节。未命中且没有历史 CAB 身份的请求继续走原逻辑。
- `eiem_demand_logical_asset_lookup.h`：`.fbx` 和 `.asset` 优先查询路径加资源名；失败后保留路径回退。

退休屏障接入原有 `ReleasePath` 缓存门：释放 owner、丢弃池和退休原 Asset/Prefab 缓存之后，才允许新 owner 加载。尚有原生 holder 时沿用现有节流、延迟重试及最新 F10 优先机制，不强制破坏 holder。按需资源准备仍发生在真实游戏加载树入口；候选收集和缓存门不读取资源 payload，也不扫描整个场景。

## 自动回归证据

先观察了三个失败，再修复：

- `0-to-1 missed the vanilla owner or selected an unrelated same-name Mesh outside its logical dependencies`。
- `synthetic first load did not retire the idle original game BundleProxy and restore its unload policy`。
- `multi-object .asset resolved the container before the declared named subasset`。

`tools/diagnostics/build_full_resource_reload_smoke.cmd` 构建、`bin/resource-directory-entry-v35/full_resource_reload_smoke.exe` 运行通过。新增覆盖世界/UI 同时存在、同名不同依赖隔离、移除目标、仅控制修改、只有缓存没有活跃 owner 的原版 Prefab、空闲反向依赖退休、引用/加载/卸载/锁定保护、合成 VFS 失败不落回原版、F10 缓存门等待和节流恢复、原生 VFS 返回、同 CAB 请求未完成及再次添加。

`tools/diagnostics/build_demand_resource_regressions.cmd` 通过，包含共享依赖图、命名子资源查询和原生材质属性移除回归。完整资源事务回归保留原有连续 F10、异步/UI 延迟、owner 退休、动画消费者重绑和失败隔离覆盖。日志保存在 `diagnostics/issue6-hot-reload-20261114/`。

## 第一次部署记录与失败边界

本次生产 DLL 已于 2026-11-14 19:32 部署到 `D:\Hypergryph Launcher\games\Endfield Game\plugin\eff.dll`，大小 3,518,464 字节，SHA256 为 `88484c4cbcaacb700fffeaf879cf8d084bf3df8e91d6e1439042917ca8270011`。已独立重读构建文件和安装文件确认摘要一致；生产校验确认不含截帧探针。回执为 `bin/local-deployments/20261114-193205-800/Dll.json`，保留原用户 INI。源码修改仍在 main 工作区，回执中的 HEAD 是构建基线，不代表这些修改已提交。

上述验证使用模拟原生 API，证明调用顺序、保护条件与回归行为；不能视为当前游戏版本下的完整实机验收。新生产 DLL 部署后，仍需在同一次游戏进程内验证 0→1→0→1，分别观察世界和 UI 的实际模型、动画及日志。旧 DLL 的日志不能证明本次修复成功。

这次部署已由 19:51 用户实机结果证伪。当前修正见本文顶部；不能再把等待原版 Bundle 归零当成隔离代际的正常必经步骤，也不能强制卸载仍有引用的游戏资源。
