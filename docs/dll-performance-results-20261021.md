# DLL 第一轮实测：2026-10-21

本轮是部分生产前测试，尚未通过发布验收。用户同时报告 NPC 一直没有花瓣、大世界花瓣先出现后消失，可能曾按 F9。功能异常优先处理，不能用不可见特效的低耗时证明性能达标。部署跟进见 [特效消失调查](effect-recovery-20261021.md)。

## 数据与边界

- 会话 `bin/performance-runs/20261021-140519-404/`，PID 25776，6 个 Mod。session.json 未填写分辨率、画质、帧率上限和硬件等条件。
- 原测量 DLL SHA256：`9855504a09c1de5faa0ac2ebaf3b75e32938d1dd97c331fcae981d076271a5ba`。
- markers.jsonl 中 S01～S07 正常结束，没有 S08/F9、S09/F10、S10/长期回收标记。
- 分析快照 `analysis-20261021-141051-0671/eff-current.log`；同目录保存 report.json、report.md、scene-summary.json。没有拼接累积日志快照，避免重复计数。
- 功能调查快照 `eff-effect-missing-20261021-141348-1684.log`；退出前完整日志保存到 `bin/effect-recovery-20261021/eff-log-before-deployment.txt`，SHA256 `802ab4a195a5402cede05fe4f5daa770b61b59f62c15ff3b9d1d875b01eafd98`。
- 只归属完全包含在标记内的约 5 秒窗口，排除边界窗口。不能据此断言边界上的操作廉价。
- 阶段为 inclusive 墙钟耗时，嵌套不能相加。plugin-frame 不覆盖全部加载、登记和 Capture 回调；hook 间隔不是 Present/GPU 帧时间。分位是最差窗口的桶上界，不是整段精确分位。测量版有统计扰动。

## 场景结果

| 场景 | 标记 / 有效覆盖秒 | 完整窗口 / 插件帧 | plugin-frame 平均 ms | 单次最大 ms | 实际覆盖 |
| --- | ---: | ---: | ---: | ---: | --- |
| S01 单角色静置 | 23.125 / 20.047 | 4 / 1833 | 0.256 | 20.488 | 特效实例 1，仍有部件登记变化 |
| S02 多角色同时可见 | 9.172 / 5.000 | 1 / 460 | 0.316 | 21.040 | 形态绑定仍 156、特效仍 1，未证实多特效同时运行 |
| S03 多 Mod、角色不变 | 16.907 / 10.000 | 2 / 632 | 0.507 | 26.167 | Mod 始终 6，但发生 UI/加载/绑定变化 |
| S04 UI 角色切换 | 11.578 / 10.016 | 2 / 705 | 0.522 | 9.294 | UI 完成事件存在，负载采样特效为 0 |
| S05 大地图往返 | 19.766 / 15.016 | 3 / 866 | 0.616 | 8.314 | 负载采样特效为 0，仍反复匹配 |
| S06 NPC 多实例 | 27.156 / 20.031 | 4 / 1219 | 1.479 | 50.819 | 匹配/骨架核验重；采样最多 1 个特效 |
| S07 战斗技能 | 9.672 / 5.016 | 1 / 394 | 0.600 | 20.157 | 仅一个完整窗口，Capture 另有独立成本 |

这些最大值不是画面帧时间，不同阶段的最大值不保证同帧发生。不能以场景名称代替日志实际负载。

## 热点与优化顺序

### 首要：特效对账与骨架重复核验（H05/H06）

S06 的 effect-bind 调用 13,679 次，主线程累计 1,561.361ms，单次最大 36.607ms。effect-mount 调用 420 次，累计 1,289.926ms，平均约 3.071ms/次、最大 14.775ms；同期仅 2 次特效创建尝试，effect-create 合计约 0.571ms。本轮主要管理成本在匹配/核验而非 clone。

同场景 effect-frame 有绑定类别累计 1,552.435ms、无绑定类别 37.165ms，最大 50.720ms。bind/mount 与 frame 嵌套，不能相加。托管调用计数 2,223,741，GC handle 新建/释放 645,182/645,190；这是调用强度，不是托管分配字节或 GC 暂停。

源码 src/eiem_entry_effect_attachments.h 中 ownerRevision 变化会对所有 owner 重新 ModelReady，无绑定 owner 每秒重试；Parent 重读 bones、证明完整 palette、遍历骨架并查挂点。2026-10-21 16:47 已实施按 owner 增量对账与仅对失败挂点重试，实测 DLL 候选尚未部署；离线证据见下文。

### F10 后特效 owner 重扫跟进（16:47）

复核游戏 `plugin/eff_log.txt`：15:55:40 一次候选因 `Mod changed during parallel index read` 被拒绝，符合导出文件仍在写入时读取的竞态；15:56:07 后的下一次 F10 成功，加载完整跨度约 609 ms，单个原生主线程阶段最大 6.324 ms。随后 15:56:33 的约 5 秒窗口中 `effect-owners` 27 次、累计 923.848 ms、单次最大 39.167 ms；`effect-bind` 5,622 次、累计 973.967 ms；`effect-mount` 220 次、累计 811.419 ms。这些阶段互相嵌套，不能相加。同期 frame hook 最大 153.195 ms，22 帧超过 100 ms。15:56:38 窗口仍有 4 次 owner 扫描、累计 135.523 ms；插件帧最大 40.420 ms。游戏日志与源码共同指向：helper/Part 注册和释放递增 ownerRevision，Frame 把局部生命周期变化扩大成全量 Native::Owners + ModelReady；每秒兜底又枚举所有未绑定根以找迟到骨架。

修改后，EntityRenderHelper 只把自身 GameObject 根放入去重队列；普通 helper 事件仅枚举对应根。F9 或控制图变化合并成一次全量对账并清空待办，避免同帧重复 sweep。周期性骨架恢复改为只重试曾命中目标 Renderer、但骨骼/挂点证明失败的根；有效挂点失效时入队，成功绑定或 owner 释放时清理。没有匹配特效的普通角色不再参加每秒枚举，也不扩大场景扫描范围。

回归先按旧实现复现：helper-only NPC 注册会重扫已登记角色；新增周期重试断言也确认无待修挂点时旧实现仍枚举 owner。修复后通过 helper 单根扫描、重复 helper 去重、F9/控制图与 owner 同时变化只扫一次、无候选时周期零 owner 遍历、骨架销毁后仅对应根迟到恢复，以及 world/NPC/UI/LOD 生命周期测试。`effects-debug` 共 1401 项生命周期检查通过；普通/测量 v35 DLL 隔离校验通过。测量版 DLL 是工作区候选，尚未部署；这些离线回归不能代替游戏 F10 A/B。

### 无实例仍有匹配成本

S04 采样特效绑定/实例均 0，但 effect-bind 3,346 次、101.755ms。S05 为 9,909 次、253.958ms，effect-frame 269.481ms。无实例不等于没有特效管理开销，值得定位非目标 owner 的重复负匹配。

### 控制网格轮询（H02）

S01 shapeBindings=156，control-bindings 约 0.052ms/插件帧；S04 为 569，约 0.242ms/帧；S05 为 550～569，约 0.219ms/帧。S04 权重读 73,990 次、写 152 次。支持研究读回调度，但登记表包含 inactive LOD/待清理对象，不代表可见人物数，也不能删除防止游戏原生覆盖的读回。

### 战斗 Capture（H09）

S07 有 17 次有效 capture-snapshot，累计 61.477ms，平均约 3.616ms/次、最大 4.721ms；17 次有效 capture-matrices 合计 0.245ms。Capture 不包含在每帧维护全部成本里，不能把它任意加到 plugin-frame 最大值。需更长固定技能序列，区分首次缓存和重复快照。

### 启动与 worker（H14/H10/H11）

启动 compiledSetMs=10736.173、routingMs=12.851；属于启动准备的墙钟成本，不能称为稳定场景单帧阻塞。后台 resource-prepare 在 S06 20 次共 479.007ms，主线程计数为 0；需拆目录/元数据、完整校验和冷缓存。

## 未覆盖与不能下的结论

- 全部采样 materialBindings=0、activeAnimations=0、animationChannels=0，动态材质和动画播放未测。
- Mod 始终 6，没有 1/3/6 等受控对照；S03 有 55 次输入提交和 7 次 UI 完成，不能孤立 Mod 数影响。
- S02/S06 未确认多份独立特效同时播放，且用户确认 NPC 没有特效，因此应先修功能再测实例扩展。
- 采样 effectRetired/effectQuarantined/retainedGenerations 均 0；effectArchiveBytes 在 0 与 23,447,416 间变化，结束时一个代际。短测不能证明无泄漏。
- 最后原生样本 requests=0、bundles=64、viewStreams=112、consumersRetained=1。需固定场景长期往返验证平台化；整个游戏 private bytes 不能归因 DLL。
- F9 发生于标记结束后 14:11:30.263，准备 .470、提交 .495。约 232ms 是请求到提交延迟，不是主线程阻塞；相同内容没有新 PLAY 符合保留播放语义。
- 无 Present/GPU 轨迹及重复轮次，不能签发帧率、GPU 性能或发布通过结论。

## 23:18 UI 打开崩溃与计时

CrashSight 对应的 `Player.log` 堆栈为 `Component::GetComponent` → `RuntimeShapes::Call` → `EffectDebug::EnableUIRenderers` → `CreateInstance` → `RuntimeAttachments::Frame`。PDB 路径和部署哈希确认崩溃版是当时的 v35 测量 DLL，SHA256 `7AA4352A4A4001CD4F8FE0456D767401BB9946E307BC03755A2FA26EBED1ECA8`。PDB 将调用定位到 `eiem_effect_debug.h:167`：方法按 `UnityEngine.Component.GetComponent(Type)` 解析，却把 `get_gameObject` 返回的 `GameObject` 当作 receiver。现已改为直接从 `ParticleSystem` 组件查询 `ParticleSystemRenderer`；离线 world/NPC/UI 生命周期回归覆盖该调用。

同一 DLL 在 23:18:25 窗口有 341 帧，`plugin-frame` 均值 0.294 ms、最大 3.048 ms，`effect-frame` 均值 0.030 ms、最大 0.263 ms。23:18:30 窗口有 146 帧，`plugin-frame` 均值 0.247 ms、最大 3.748 ms；`effect-frame` 均值 0.053 ms、最大 3.589 ms；一次 `ui-register` 为 4.739 ms。该窗口 `effect-bind` 5 次合计 8.232 ms、最大 4.607 ms，`effect-mount` 2 次合计 7.744 ms、最大 4.410 ms；这些是包含子阶段的计时，不可相加。

同窗 `frameHookIntervals` 最大 512.550 ms、超过 100 ms 共 3 次，而插件 `plugin-frame` 最大 3.748 ms。这个样本不能把长间隔归给插件；也不能据此推断 UI 稳态，因为负载快照延迟 2.922 秒且记录 `uiModels=0`。`privateBytes` 约 13.2 GB 是整款游戏的进程数据，不是 DLL 内存归因。现有 INS 面板没有独立的 Draw/Present 计时，需单独采样后再判断面板绘制是否有热点。

23:54 已确认游戏与启动器未运行，将普通 v35 构建部署到 `D:/Hypergryph Launcher/games/Endfield Game/plugin/eff.dll`。安装 SHA256 为 `ED09E9384EEB1E8106E1B1A41716BEF4DAD1085D732C6A0E63B68B9F2CBFE8B5`；旧 DLL SHA256 `7AA4352A4A4001CD4F8FE0456D767401BB9946E307BC03755A2FA26EBED1ECA8` 已备份至 `bin/ui-open-crash-fix-20261021/backup-20261021-235450-709/plugin/eff.dll`。代理与 `package-protocol.json` 未更改。生产 PE 检查和使用游戏宿主目录的绝对路径隔离加载检查通过，包含 16 轮导出启停与启动日志检查；它不加载 GameAssembly。用户仍需重新启动游戏验证 UI 角色页；这次部署本身不代表实机已不再崩溃。

## 下一轮

先验证全 LOD 包的大世界近远距离、NPC、UI 和 F9。每个性能场景建议至少 60 秒、3 轮，操作前后各留 10 秒稳定期；S02/S06 必须确认独立 Mod/特效实例实际增加。补动态材质播放、F9/F10 和长期往返。功能恢复后先优化 H05/H06，再评估 H02/H09/启动 worker；相同场景 A/B 后切普通构建复核。详见 [原审计矩阵](dll-performance-audit-20261021.md)。

## 2026-10-22 复测与资源持有

完整会话摘要保存在 `bin/performance-runs/20261022-load-perf-20261022-011624-629897/summary.md`。S03 稳定世界持续 110.1 秒，6 个 Mod、137 个部件和 2 个特效实例稳定：`plugin-frame` 平均 0.305ms、最大 6.348ms，`effect-frame` 平均 0.027ms、最大 0.492ms。S01 部件数从 148 变到 137，只能作参考。S08 混有 3 次 F10，不能作为干净 F9 对照；唯一 F9 在约 201ms 后提交，随后完整窗口的 `plugin-frame`/`effect-frame` 最大值分别为 51.845/51.724ms，`effect-bind` 最大 22.447ms。该窗口游戏帧间隔峰值约 271.6ms，但没有 Present/GPU 轨迹，不能把它归因给插件某个阶段。UI 切换、NPC、地图和固定战斗尚无有效专项样本；本轮未执行 F10 专项。

特效归档现在以 `Resource` 字节区间视图交给原生 Bundle 流，不再为每个 Bundle 复制一份归档；异步请求及已加载 Bundle 保留不可变 Resource 所有者，直到 Bundle 卸载完成。普通 v35 资源重载回归新增了该生命周期检查，并覆盖创建失败和异步卸载。真实 `lx` 包 3 个 Bundle 的 C++ 往返已验证为 0 字节归档复制。

普通 v35 构建新增 `[FULL-RELOAD-MEMORY]`，统计完整候选常规 Mod 图中唯一 `Resource` 的 CPU 字节数，并区分旧图、候选图、共享和新增资源；附着特效归档长度作为视图字节另列，不代表第二份分配。`[FULL-RELOAD-REBUILD-COST]` 把提交阶段分为 owner 释放、资源池清理、owner 加载/验证和发布。该构建当前仅在工作区，尚未部署，因此 10/22 实测日志没有这些新字段。

这些字节数只代表 DLL 作者资源载荷，未计容器、索引、分配器开销、Unity 解压后的原生对象或 GPU 资源。Standalone 特效包由 F9 管线独立维护；它们不属于 F10 的角色 Mod 图。Windows 进程内存仍是整个游戏的读数，DLL 没有可独立读取的显存计量。当前只能确认稳定场景的 DLL 阶段耗时较低、存在一次提交尖峰待拆分；不能据此判断 DLL/显存无瓶颈或签发生产性能通过。
