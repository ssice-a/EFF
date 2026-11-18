# DLL 生产发布前审查（2026-11-17）

本页保留修复前的审查结论和证据。随后已修正退休通知推进、默认诊断及日志写盘，并完成本机部署；当前结果和剩余边界见[发布前修正](dll-release-fixes-20261117.md)。

当前不建议直接发布稳定版：已复现一个待退休通知无法保证最终处理的生命周期缺陷。另有冷请求主线程停顿和生产诊断采样开销，应在发布说明及性能改进中明确。现有结果不支持“插件没有性能成本”或“已经证实长期内存稳定”的结论。

本轮只审查、运行回归并保存诊断复现，没有改动 DLL 源码、重新部署 DLL、推送或发布。

## 审查对象与现场结果

固定安装目录 `D:\Hypergryph Launcher\games\Endfield Game\plugin\eff.dll` 与 `bin/resource-directory-entry-format3/eff.dll` 一致，3,730,432 字节，SHA256：

`4A628C393499DDD203322EB969728D9C1476D7D32BC5B01C77EB63D42037EA39`

部署回执为 `bin/local-deployments/20261117-074709-360/Dll.json`。生产二进制检查和真实 LoadLibrary/导出接口循环检查通过，未包含 RenderDoc 注入或临时截帧探针。二进制包含尚未提交的本机修正，正式发布仍需固定对应源码及子仓库提交。

现场日志副本：`diagnostics/dll-release-audit-20261117/eff-current.log`，PID 23424，07:56:42–08:03:05，3,442 行。解析摘要在同目录 `log-summary.json`。

- 启动读取 4 个 Mod 的声明、32 个目标，209.113 ms，在启动线程执行；网格、纹理、特效 payload 读取为零。
- 后续两次 F10 的声明读取分别为 38.896、34.954 ms，在 worker 执行；payload 读取为零，没有枚举所有消费者 PFB。
- 截至日志结束，99 次共享输入提交全部完成，加载失败 0、等待 0、GAP 0。
- 用户已回复“已测试，恢复正常”，确认武器重定向后旧模型恢复及紫材质问题的本轮画面结果。
- 这两次 F10 的提交记录均为 `paths=0 owners=0`，所以这段日志不能单独证明“原地保留活动世界/UI owner 时的即时重建”已经实机覆盖。该行为目前还有完整 fixture 回归作为代码证据。

## 发现

### P1：待退休队列依赖后续特定准备路径，可能永久保留旧代资源

位置：[`ObserveSharedRetire`](../src/eiem_entry_demand_shared_inputs.h:790)、[`DrainPendingSharedRetiresLocked`](../src/eiem_entry_demand_shared_inputs.h:781)、[`PrepareDependencyGroup`](../src/eiem_entry_demand_shared_inputs.h:1257)、[`PruneSharedLocked`](../src/eiem_entry_demand_shared_inputs.h:89)。

`ObserveSharedRetire` 无法取得 preparation 锁时，把退休通知放入 `pendingRetires` 后返回。正常的准备退出会处理队列，但处理守卫仅在确认依赖组相关之后才建立。命中已准备结果、负命中、发现不相关资源或在此之前抛异常的路径均可能绕过它。逻辑资源解析及特效资源准备也使用同一把锁，没有统一的退休队列退出处理。

之后每 300 帧的 `Report → PruneLocked` 不处理队列；旧 scope 的 `activeAuthorProxies` 又会阻止 scope 被回收。因此不能保证最后一个原生消费者退休后，输入视图、文件持有及有关作者缓存最终释放。它们可随重复重载积累。这里不等同于 Unity/GPU 对象已经泄漏，也不能归因于之前的系统内存耗尽事故。

审查复现直接调用生产 `ObserveSharedRetire`、`Report` 与退休函数，通过现有 native API fixture 模拟 preparation 占用期间的真实退休事件。解锁后执行 101 轮周期检查，再移除当前 manifest，结果仍为：

```text
stranded=1 pending=1 active=1 graphInputs=1 scopeRetained=1
```

显式执行队列处理后：

```text
graphInputs=0 scopeRetained=0
```

`/Od` 与 `/O2` 复现一致。源码及构建命令在 `diagnostics/dll-release-audit-20261117/deferred-retire-review.cpp`、`build-retire-review.cmd`，结果为 `retire-review-optimized.log`。

建议修正：把待退休处理纳入统一生命周期推进，使其不依赖下一个相关资源请求；覆盖全部 preparation 返回/异常路径，并有空闲帧兜底。不能在持有全局 `mutex` 的 `PruneLocked` 中直接调用退休函数，因为退休函数还会取得该锁。应先取得 scope 快照并释放总锁，再用每个 scope 的 preparation 锁串行处理，保留真实所有者及原生请求的租约规则。需要补“最后通知发生于提前返回/失败/特效准备期间，之后没有新请求”的回归。

现场日志 `graphInputs=97`、退休观察为零，无法证明这个缺陷在该会话中已经触发。游戏仍持有资源时保留旧代是正确行为；本项结论来自可重复的队列状态复现。

### P2：冷请求仍在游戏主线程同步准备，存在可见停顿

位置：[`SharedGetOrCreate`](../src/eiem_entry_demand_shared_inputs.h:466)、[`PrepareDependencyGroup`](../src/eiem_entry_demand_shared_inputs.h:1249)。

主线程 tid 5344 的两次作者依赖组准备为 230.843 ms 和 156.386 ms。前者包括 22 个已打开输入头、14 个完整材质；其嵌套的消费者输入准备为 167.351 ms。原版恢复组为 8.891 ms 和 11.769 ms。嵌套耗时不能相加。

这是当前二进制真实日志中的同步开销，能够造成首遇、换目标或新代际首次请求时的停顿。它不是启动全量预载，也不能据此断言每帧都损失约 231 ms。当前调用栈必须及时给游戏提供正确的代际输入，不能简单改成异步后让游戏先拿旧资源。

改进应继续使用请求范围内的准备：定位消费者重写、布局解析及重复文件工作的占比，复用同一请求事务中的不可变结果，并将不依赖 Unity 的工作放到已有准备阶段。不能恢复全游戏索引扫描、批量预加载或旧代旁路来掩盖停顿。

### P2：生产包默认执行详细粒子采样及同步日志，瞬时开销没有预算限制

位置：[`Observation::LiveParticles`](../src/eiem_effect_observation.h:51)、[`Capture`](../src/eiem_effect_observation.h:75)、[`Tick`](../src/eiem_effect_observation.h:95)、[`CreateInstance`](../src/eiem_effect_debug.h:394)、[`Install`](../src/eiem_effect_debug.h:505)、[`Log`](../src/eiem_resource_entry.cpp:1719)。

生产启动会安装 Observation。每个新特效实例在播放后、1 秒和 3 秒采集详细状态，包含 Renderer、材质、纹理、模块及最多 256 个活动粒子。粒子明细为每粒子 3 次 managed getter，还会分配数组及临时 GC 句柄；多个发射器的采样可以集中在同一帧。这是有次数上限的诊断，不能称为每帧无限扫描，但并非播放效果所必需。

每条日志持有同一个 log mutex，并对每个启用输出执行 `fprintf + fflush`。现场最密集的两个自然秒有 685、671 行，周期统计每 300 帧还会写 4 行。本轮没有测量磁盘写入对该游戏帧的实际耗时，不能将这些行数换算成已经证实的帧率下降。

建议保留基础错误/加载/退休记录，将粒子明细、纹理枚举和详细时序做成按需诊断，或分帧限制预算；降低正常路径的同步写入频率，仍保证严重错误及时落盘。

### 多份相同 Mod 的内存边界：独立实例不代表纹理内容跨目录去重

[`DDS 原生身份`](../src/eiem_demand_texture_build.h:48) 包含 `ownerRoot`、资源 ID、DDS 文件路径及文件指纹。同一 Mod 复制到不同目录，即使贴图像素相同，也会得到不同的原生纹理身份；当前共享图不会将它们自动识别为同一 Texture 输入。

同 ID 副本分别改目标、控制状态独立的回归通过。这种安装隔离是有效的，但多份副本同时命中时，仍有重复原生纹理对象和额外 CPU/GPU 驻留的风险，本轮没有实测其峰值。不能承诺复制相同武器 Mod 到多个目标几乎不增加显存。后续若做内容共享，应仅共享不可变纹理数据，继续隔离目录可修改资源、材质实例、控制变量和退休租约。

## 性能结构与回归结果

没有发现启动/F10 全量加载所有 Mod payload、每帧读取全游戏索引或扫描全部场景 Renderer 的常态路径。原生 BundleProxy 继续管理实际请求、依赖引用和卸载；输入使用有持有期的文件 span/view。按需加载仍会产生真实 Unity/CPU/GPU 分配，不能因作者输入是流式视图就承诺没有内存峰值。

`CollectMemory` 在没有异步加载/卸载工作时由原子标志直接返回，不会逐帧遍历全部常驻 Bundle。静态控制清理分散到 64 帧，材质动画仅处理注册对象并跳过未变化的输出；可见特效仍逐帧检查所有者/Renderer 状态，开销随真实实例和通道数增加。

本轮从当前源码构建并通过：

- 完整资源重载：原版恢复、连续 F10、世界/UI 共享武器、异步完成、失败隔离、材质/纹理身份、最终持有者释放。
- 共享依赖图/材质删除、代际差异、原生随机读取与请求外零加载。
- RuntimeShapeBridge、动画/材质恢复、特效事务切换、独立实例、全部 LOD 挂点。
- 游戏模型池、作者 manifest、静态 Mesh 装配、控制程序及性能采集器。
- 多 Mod 资源组合与同 ID 多安装目录的独立目标/变量/动画/特效、F10 增删身份及持久化。
- 真实 D3D11 面板：composition 失败回退 HWND、设备失败回退 WARP、Present 设备丢失后的 worker 重建。
- 游戏 owner 窗口停止处理消息时，独立面板仍能关闭；本机测得关闭约 9.04 ms。
- 已安装生产 DLL 的隔离检查和真实加载检查。

另以 `/O2` 审查 harness 运行生产辅助函数的模拟 API 微基准：

| 路径 | 次数/条件 | 合计耗时 |
| --- | --- | --- |
| 未命中 Prefab 通过原生路径 | 10,000 次 | 4.0323 ms |
| 没有异步任务的 `CollectMemory` | 10,000 次，常驻 0 / 64 / 256 Bundle | 0.0223 / 0.0198 / 0.0234 ms |
| 未命中依赖查询 | 1,000 次，原依赖 0 / 64 / 512 | 0.4706 / 5.4703 / 33.9508 ms |

这些结果只验证路径复杂度和回归趋势。fixture 的 managed invoke 是模拟函数，不能当成真实游戏每帧 CPU 时间、FPS 或 GPU 数据。原有 `--profile-load-misses` 单独入口在 fixture 进程退出时异常终止，本轮复用其函数并在审查子进程中结束，取得完整 `/O2` 输出；未将该辅助入口失败算作生产 DLL 崩溃或完整通过。

## 发布判断与尚未覆盖的范围

优先处理 P1 后再决定稳定版。之后应在真实游戏中反复增删/换目标、F10、世界与 UI 来回切换，确认最后 owner 释放后退休队列、旧 scope 和资源驻留能够回落。现有原版恢复实机反馈有效，不能替代这项长期验收。

AMD + MSI Afterburner/RTSS、Linux/Wine 的真实驱动组合本轮没有复测。面板已有独立线程、HWND/WARP 回退及故障恢复，相关自动回归通过，但无法证明第三方 hook 内部卡死或不同驱动问题已经消失。长期提交内存、显存峰值、GPU 帧时间及 20–30 Mod 压力也未取得新的实机测量，不能标为已通过。
