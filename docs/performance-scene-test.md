# 场景性能检查

2026-10-21 首轮 S01～S07 部分实测已分析，见[实际结果与覆盖缺口](dll-performance-results-20261021.md)。NPC/大世界特效异常及后续测量 DLL/LOD 包部署见[特效跟进](effect-recovery-20261021.md)，不能以旧部署摘要辨认当前构建。

2026-10-21 新一轮发布前测试以 [DLL 性能审计与实机场景矩阵](dll-performance-audit-20261021.md) 为准，包含形态、动态材质、特效、UI/NPC、实例化路由、长期回收与 10 类场景。测量版新增主线程 P95/P99 上界、工作量/调用计数、uptime 窗口及场景标记工具。下文安装日期、结果与 DLL 摘要保留为历史记录，不能作为当前候选的实机结论。

2026-10-13。使用专门的 `build.bat v35 load-perf` 测量构建，输出独立到 bin/resource-directory-entry-v35-load-perf。它与普通生产构建使用同一资源逻辑，仅 `EIEM_LOAD_PERF` 启用计时；不包含目录调查、资源抓取或对象快照探针，不改变 Mod 文件、按键和贴图配置。

## 覆盖

- file、dependency-all/direct、prefab：插件在公共加载入口的命中/未命中操作。
- view-read/seek、backing-read、stream-route：游戏后续原生加载调用中的输入视图读取/定位，来源内存映射与作者缓冲读取分别记录，包含字节数。未命中的路由只测插件查找，不把游戏原函数计入插件。
- plugin-frame：每帧插件维护外层，包含 reload、stream-lifetime、controls。controls 已分开 control-input、shape-animation 和 control-bindings；不再把重载全部称作按键成本。shape-animation 只推进播放状态并插值共享频道，实际 Renderer 权重写入仍计入 control-bindings；它是 controls 的子阶段，不能再与外层相加。
- resource-prepare、dependency-index、reload-drain、logging：后台集合准备、首次清单索引、失败重载清理和逐行日志 fflush 成本。
- model-register：世界/展示模型完成与 helper/controller 初始化中的 Renderer 登记。capture-snapshot/retire/matrices：技能或特效的形态 Mesh 快照、释放和矩阵补充；只测插件工作，保留原生 Capture/矩阵函数在测点之外。
- 每 5 秒汇总调用数、平均、最大、粗分桶 p95 上界；主线程单独汇总。阶段有嵌套，不能相加成总 CPU。并发读统计窗口边界近似，不是精确 CPU 调度记录。
- 主线程由 Unity 的 CurrentThreadIsMainThread 验证后登记；尚未识别时的启动样本只进入 all 统计。changed 字段是各阶段的分类位：backing-read=1 为映射文件，resource-prepare=1 为显式 F10，model-register=1 为存在就绪绑定；其余为对应路径命中/实际处理，不能把所有 changed=0 当作 Mod 未命中。
- frameHookIntervals 使用两次 Unity 每帧 hook 到达的真实墙钟间隔，同时保留 Unity delta 桶；它不是 PresentMon，也不是 GPU 帧时间。
- wholeProcess 内存/页错误/CPU/I/O 是整个游戏，不归因于 DLL。nativeLifetime 数量帮助判断请求完成与长期持有，但不证明具体 Mesh/Texture/GPU 的已释放字节。

这些测点用于第一轮定位 DLL 本身是否与卡顿同时繁忙。GPU 上传、Shader 编译、Unity 异步内部解包/实例化/GC，以及游戏原生无关资源加载没有内部计时；若卡顿时插件各主线程测点很低，需要同场景的帧轨迹/CPU 或 GPU profiler 继续确认，不能据此宣称插件毫无影响。

## 操作顺序

1. 首次启动进入世界，保持约 20～30 秒，建立稳定状态；先不重新导出，也不按 F10。
2. 切不同地图、世界/NPC/剧情、角色详情页，再回到同一场景。每次加载结束后的卡顿与稳定状态都留至少 10～20 秒；记录场景名和卡顿开始时间。
3. 随后单独测试按键切换、形态键、技能/特效、Ins 打开/关闭。
4. 最后单独测试 F10，记录按键时刻和成功/失败。不要把换图和改 Mod/F10 同时操作的结果当作单纯地图成本。

日志位于游戏 plugin/eff_log.txt，按进程 ID 隔离会话。重新启动会截断该文件，分析前保存当前会话副本。汇总命令：`python tools/diagnostics/analyze_scene_performance.py <log> --output <report.json>`，只读取日志。不能由 startup compiledSetMs 推断每次地图成本。

报告保留每个阶段约 5 秒窗口的时间戳，用于和场景/慢帧窗口对齐；p95 超出 1000ms 最大桶时上界输出 null，并标记 p95ExceedsHistogram，不把超范围解释为 0ms。

性能版会有计时/原子计数和汇总日志的诊断开销；生产版这些代码为空函数。计数器回归验证主线程/worker 分离、读取类型与字节、p95 上界及窗口复位；安装前完成真实 DLL PE 和隔离加载检查。游戏进程运行时不覆盖 DLL。

## 2026-10-19 启动成本核对

本机保存的 PID 10836 会话使用 10 月 17 日构建，六个 Mod 的 `ENTRY-STARTUP-COST` 为 `compiledSetMs=10471.408 routingMs=15.696 unityLoads=0`。这是启动线程读取、验证和组合编译资源到路由就绪的墙钟时间，排除游戏运行环境就绪等待；并非主线程阻塞 10.47 秒，也不能推导每次换图都会重复执行。它会与游戏竞争 CPU/磁盘，原生加载仍可能另有成本。该会话形态采样平均约 1.1–1.3 微秒/帧，原生权重写入计在 control-bindings，不能将其解释为全部形态动画成本。

同日对当前安装 Mod 集合运行只读 `compiled_mod_set_smoke --profile-set`，准备总墙钟 6262.19 ms；287 个输入、39 个共享输入。分阶段证据显示编译清单解析及共享对象合并仍有明显成本。workerSumMs 包含并行和嵌套阶段，不能相加为总时间；当前集合和缓存状态与上次游戏会话不同，不能以 10.47 → 6.26 秒宣称本轮优化了启动。未改集合的刷新检查为 30.18 ms，`gameLoads=0 authorBuilders=0 writes=0`，这不是每帧成本，也不是重启持久缓存的命中结果。原始测量为 `bin/startup-profile-20261019.log`。

本轮只优化形态调度，共用时间内核与启动准备没有因果关系。下一步启动优化应分别测清单解析、共享输入合并和游戏线程原生加载，并用同一集合/缓存状态比较；不应取消来源或冲突校验来换取时间。本轮没有修改游戏安装或 Mod。

## 本次安装

2026-10-13 21:08:56 +08:00，确认 Endfield 退出后安装到 `D:\Hypergryph Launcher\games\Endfield Game\plugin\eff.dll`，仅复制 eff.dll。

- 性能 DLL：2,386,432 bytes，SHA256 `430E28EE9FA5AFE011A0AD183A6BFE4F78FE000C4A3689731F4C9204677CAD4E`。
- 前版 DLL 与上一会话日志备份：`bin/performance-scenes-20261013/backup-20261013_210855_899`；前版 SHA256 `4972FA6C49E5559E99D6EC73E2AFDC3249A28E82624BC9E76409F57E5AA7141B`。
- 验证：v35 load-perf 构建、实际 DLL PE 静态运行库/抓取探针排除、游戏运行库隔离加载、完整重载模拟回归、计数器回归、场景分析器多 PID/窗口/直方图超限样本及 CLI、diff check 均通过。隔离加载与模拟回归不能替代游戏场景验证。
- 安装前后核对 231 个 Mod/配置文件的路径、大小、修改时间及 INI/JSON 摘要，清单一致。记录位于 `bin/performance-scenes-20261013/install-receipt.json`。
- 本地热缓存单线程 100000 次 Scope 微基准均值约 0.05µs，仅反映计时/统计基本开销，不代表游戏每帧开销或并发争用；实际每帧与汇总日志影响需要本轮测试。

状态：测量版已安装，首轮游戏基线已收集；后续仍需带场景标记的重复测试和独立 F10 测试。

### 形态动画版更新

2026-10-13 22:59:42 +08:00，确认 Endfield 未运行后安装含形态动画的性能 DLL，2,421,248 bytes，SHA256 `CB539AB139331CAB0FF935F96DAB69DC97CED4D460E2B1BC6E9AFDE57F53C912`。编译器 SHA256 `971F504F5D79DF8D0304C4E5E277037C61CBF08AF9BE1ED5CF127F7C306DF70F`，协议声明增加 shapeAnimation；上一 DLL/编译器/协议保存在 `bin/shape-animation/backup-final-20261013_225942_785`，初次功能安装之前的版本也有独立备份。Mod 文件清单未变，用户工程未保存。

构建、PE 静态 CRT 检查、真实 DLL 预加载游戏运行库隔离测试、Python/Blender 动画导出和原生播放/偏好回归通过。真实 LZY 隔离副本动画资源完整编译与 compiled roundtrip 通过，总计 5865.21ms；这是作者端完整编译耗时，不是游戏动画帧成本。安装证据见 `bin/shape-animation/install-receipt.json`。

下轮在同一场景分别记录未播放、播放一个组、多个不同频道组、Ins 调速/暂停、技能 Capture 与 F10。同 Mod 多实例只共享一次曲线计算，实例写入按当前绑定执行；重点比较 shape-animation 与 control-bindings 两项。首轮基线来自更新前的 DLL，不能作为新增播放功能的实机性能证据。

## 首轮基线：PID 31988

2026-10-13 21:10:05～21:14:38，5 个 Mod，53 个统计窗口，17258 次每帧维护调用。用户反馈「没有观察到明显卡顿」；未提供具体地图/剧情顺序，不能把某个窗口直接归属到某张地图。21:15:31 检查时进程已经结束，日志副本保存为 `bin/performance-scenes-20261013/session-31988-211531.log`，JSON 为同目录 `session-31988-first-report.json`。

| 路径 | 本轮结果 | 含义 |
|---|---|---|
| 每帧维护外层 plugin-frame | 平均 0.270ms，最大 14.296ms | 包含 controls/reload，不能再与它们相加；这是维护入口，并非 DLL 所有主线程入口的总耗时 |
| control-input / control-bindings | 平均 0.127 / 0.137ms | bindings 在截至 21:13:17.983 的窗口有单次 14.186ms 墙钟尖峰；需复现后区分真实工作、GC/调度等待 |
| 命中资源入口 | 103 次，其中 102 次 compiledInputView | 一个来源仍有 201339764 字节整块回放，21:10:32.006 replayMs=32.428，外层 file=32.519ms；尚未确认该来源退回缓冲路径的具体条件，不能断言是旧包 |
| 依赖索引 | 2 次，约 35.1 / 29.5ms | 日志都在后续已确认的主线程 tid 8428；第一份统计发生于主线程 ID 尚未登记，因此 mainCalls=0。扫描清单 255842 条，目标名称 271 个，属于首次索引工作而非每帧 PFB 解析 |
| 形态 Capture 快照 | 8 次，均值 4.516ms，最大 8.347ms | 集中在截至 21:14:13.082 的窗口，包含插件发起的 Unity Mesh 复制/回读操作，不包含原生 Capture 函数整体 |
| view-read | 1.35GB，累计 663.9ms，主线程累计 0.061ms | 主要在后台读取；不能和其内层 backing-read 相加 |
| 资源状态轮询 | 后台 258 次，平均 6.27ms | resource-prepare scope 包含无变化检查，不能把这 258 次视作重新编译或全量重载 |
| 启动集合准备 | compiledSetMs=4243.436，routingMs=26.189 | 后台启动一次；随后启动提交标记 ALREADY-PUBLISHED，nativeLoads=0、modelsRebuilt=0 |
| F10/资源重载 | 本轮 0 请求、0提交 | 本轮未覆盖实际 F10，未见地图操作触发再次全量准备 |

存在约 0.6～2.5 秒的 frameHookIntervals 间隔；它记录 Unity hook 两次到达的墙钟距离，包含加载/暂停期间不调用该 hook 的情况，不能直接解释为用户看到的画面卡住。用户未观察到明显卡顿，所以保留为待标注的窗口，不作为已复现性能回归。

后续优先核查：整块来源回放进入主线程的条件、首次依赖索引及重复建表；再复现 bindings 尖峰和技能快照。所有数值均为本轮诊断墙钟测量，不能据此宣称所有地图、所有 Mod 数量或 GPU/Unity 内部工作已经验证。
