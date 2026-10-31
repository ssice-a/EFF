# Format 3 架构审查与性能测试准备

日期：2026-11-01

本文记录当前 DLL 与 Blender Format 3 路径，并界定首次实机性能测试能回答的问题。它不把旧 Format 2 编译耗时当成新架构基线。

## 当前资源路径

1. DLL 启动时读取 Mod 的 Format 3 manifest、控制/特效元数据及形态控制所需的 Mesh 元数据，不遍历游戏内全部 PFB，也不提前打开所有作者资源载荷。
2. 游戏请求 native 资源后，DLL 沿游戏的 VFS/Bundle 依赖关系检查该次请求涉及的 native 输入。它读取所需输入目录并按 CAB 建立本次依赖关系；不展开全游戏资源。
3. 只有目标规则命中时，Catalog 才打开对应 Mod 声明的 `.effres` 与 DDS。Mesh、Material、Texture、Physics 的作者数据按实际命中访问，并在当前共享资源图中复用。
4. DLL 在游戏请求的 native 输入上组装替换后的序列化数据，再交给游戏原有加载/反序列化链路创建 Unity 对象。EFF 不另建一套运行时 Mesh/材质对象来替代游戏创建流程。
5. 共享代理退休时，DLL 从共享图移除不可达输入、目标依赖和作者 Mod；对应 Catalog 的 Mesh/Texture/Material/Physics 缓存会清空。`[DEMAND-LIFETIME-RECLAIM]` 可核对回收事件和条目数。`evictedViewBytes` 统计共享图输入视图，不是 GPU 显存或全部 Catalog 内存字节。

Blender 直接写 Format 3 作者目录；`.effres` 保存类型化资源，DDS 是独立可替换文件。Format 3 冷导出入口为 `tools/Blender/benchmark_format3_export.py`，要求隔离且不存在的目标目录，以及有效的 EFFSOURCE 来源基线。旧文档中的 `compiled.bin` 编译时间不代表该导出路径。

## 性能证据边界

| 指标 | 当前证据 | 限制 |
| --- | --- | --- |
| DLL 路径耗时 | `build.bat load-perf` 的分阶段调用数、总时长、平均值、最大值、P95/P99 上界及主线程子集 | 计时阶段可能嵌套，不能求和；分桶上界不是精确分位数 |
| 按需准备 | `[DEMAND-SHARED-GROUP] prepareMs`、`[DEMAND-NATIVE-PREPARED]` 和 `[DEMAND-TIMING]` | `DependencyGroup::Stage` 当前只是 watchdog/错误阶段标签，不记录每个阶段的独立耗时和字节数 |
| CPU、内存、I/O | 每五秒记录整个 Endfield 进程的 CPU、工作集、Private Bytes、页错误和 I/O | 数据属于全游戏，不能直接归因给 EFF；没有 GPU 显存计数 |
| 帧间隔 | Unity delta 桶及两个 Unity 帧 hook 的墙钟间隔 | 不是 PresentMon、GPU 帧时间或 Unity 异步资源加载耗时 |
| 生命周期 | `[DEMAND-LIFETIME-RECLAIM]` 记录回收输入视图、依赖节点和 Catalog 缓存条目数 | Catalog 只给条目数；不能据此声称回收了确定数量的 CPU/GPU 字节 |
| Blender 导出 | Format 3 基准脚本记录墙钟、输出文件数和总字节数 | 需要固定 Blend、EFFSOURCE、Blender 版本和目标；一次冷导出不能代表覆盖/增量导出 |

目前没有 Format 3 游戏内的受控性能基线。首轮端到端测量可以直接使用 `load-perf` 开始；历史 Format 2 的冷编译、启动和导出数据只用于说明旧路径，不能用来推算 Format 3 的命中延迟或内存占用。

## 首轮测试矩阵

固定游戏版本、分辨率、画质、Mod 集合和角色/场景；每轮保存 DLL 摘要、Mod 文件摘要、游戏日志、场景标记和起止时间。不要在同一轮里同时改 Mod、按 F10、换地图并切编队。

1. 冷启动：记录进程启动、Format 3 manifest/catalog 就绪及首个命中资源的时间；区分 DLL 准备时间与游戏自身加载等待。
2. 首次命中和重复命中：分别进入一个含替换角色的世界场景和人物预览页，再重复进入；记录命中数量、`prepareMs`、原生异步提交/完成、主线程阶段耗时与帧间隔。
3. 场景切换：在地图、编队、人物预览页之间往返，观察 `[DEMAND-LIFETIME-RECLAIM]`、活动代理/缓存条目、工作集和 Private Bytes 是否随资源退休回落。
4. 操作成本：单独测试形态键、显隐键、技能/特效以及 DDS 替换后的 F10，记录操作时段的主线程 P95/P99 和异常日志。
5. 稳态与规模：先以当前 Mod 集合重复三轮，再逐步启用更多 Mod；用真实命中资源数、输出字节、请求数和活动依赖图规模解释增长，不按 Mod 数量做简单线性外推。
6. 导出：用固定工程与来源基线至少测三次冷 Format 3 全量导出；分别记录 Blender 作者处理、资源写出、贴图处理与总墙钟。覆盖/增量另开一组目标已存在的测试，不与冷导出混合。

每轮优先报告中位数与范围；插件计时报告 P95/P99 上界并注明超桶情况。工作集、Private Bytes、帧间隔和游戏总加载时间都标为整进程/端到端数据，不标成 DLL 独占成本。

## 首轮测量的归因限制

- 当前缺少需求准备阶段的独立耗时和读取字节计数，无法从 `prepareMs` 直接分辨 VFS 解析、目标筛选、作者资源读取、资源组装和提交前工作的成本。首轮总耗时和整进程内存测量不受此限制；若数据确认准备阶段是瓶颈，再添加仅 `EIEM_LOAD_PERF` 开启、单调时钟计时并汇总输出的分段计数。
- 需要通过实机切换验证代理退休回调确实覆盖地图、编队和 UI 生命周期；检查缓存条目回落，不把日志中的单次回收事件当作完整释放证明。
- GPU 显存和 Unity 内部异步反序列化/上传要由外部 GPU/帧分析工具补测。DLL 的 CPU 计时不能替代这些数据。
- 性能版只用于测量，和普通开发 DLL 使用相同 Format 3 资源路径；它不属于生产 DLL。当前工作未构建或部署 DLL。
