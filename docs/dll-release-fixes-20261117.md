# DLL 发布前审查修正（2026-11-17）

对应[修复前审查](dll-release-audit-20261117.md)。本轮处理待退休通知的最终推进、默认诊断开销和一处实测的原生布局重复复制。生产发布仍需实机长期验收，冷请求的整段主线程停顿尚未量化改善。

## 生命周期

`SharedPreparation` 统一包住依赖准备、特效逻辑资源解析、特效资源准备和租约释放；返回、异常及主动解锁前均处理待退休通知。Unity 帧入口增加空闲兜底：没有通知时只检查原子标志，有通知时先复制 scope 快照并释放总锁，再尝试取得各 scope 的 preparation 锁。锁繁忙时留待后续帧，不等待 worker，也不在总锁内执行退休。

这不改变游戏 BundleProxy 的所有权：仍有 owner 的输入继续保留；代理重新激活会取消尚未处理的旧退休通知。周期 Report 也使用同一推进函数。

正式回归先在旧实现得到 `final queued retirement remained stranded without another resource request`，修正后通过：

- preparation 被另一线程占用时，Unity 帧正常返回，保留仍在准备的输入。
- 不发生新资源请求或 F10，最终通知仍释放旧 graph、scope、VFS 路由和 prepared input。
- 旧通知排队后重新激活代理，保留新使用者；其真正退休后再释放。
- 实际依赖负命中、特效准备验证失败和特效租约释放失败的出口也处理通知，没有依赖帧兜底来掩盖提前返回。

证据：`diagnostics/dll-release-fixes-20261117/retirement-red.log`、`retirement-exits.log`、最终 `full-reload-test.log`；回归位于 `tools/diagnostics/retarget_generation_checks.h`，并纳入完整资源重载测试。

## 诊断与日志

全局 `plugin/eff.ini` 新增可选配置，默认关闭；旧配置无需修改，F10 发布设置后生效：

```ini
[Diagnostics]
detailed=false
```

只有 `detailed=true` 时才执行粒子明细、Renderer/材质/纹理枚举及详细请求时序记录。基础错误、加载、退休及统计保留。真实调用入口回归确认关闭时不调用粒子 native getter、不推进采样状态；开启时原有观测、失败隔离及次数上限仍有效。默认值、发行配置、旧配置和显式开关均有解析/发布回归。

日志继续使用现有同步互斥写入与两个输出，不增加后台无界消息队列。每个输出设置 64 KiB stdio 缓冲，正常消息由已有 25 ms 轮询线程按约 250 ms 间隔批量刷新，缓冲写满也会输出。`BUILD` 及 `FATAL/GAP/ERROR/FAIL/FAILED/FAILURE/REJECT/REJECTED/QUARANTINE/CRASH/WATCHDOG/TIMEOUT` 标签立即刷新，严重记录不等待批次。进程突然终止仍可能丢失最后一小段普通消息。

实际文件回归验证普通记录保持缓冲、关键记录立即刷新两份输出、定时刷新、写满刷新、会话/镜像一致和进程/线程标识。修复前同一测试因逐行同步刷新失败。证据为 `logging-red.log`、`entry-contract-test.log`、`effects-test.log`。

## 冷请求性能

`PrepareReplacement` 原来保留局部 `SerializedLayout`，再按值传入事务，事务另复制基线；现在把解析结果直接交给事务，并通过其不可变 `SourceLayout()` 进行匹配，省去一份完整类型树及对象表复制。没有加入游戏索引扫描、预加载或“先给游戏旧资源”的旁路。

对真实 `chr_0036_jsspsi_postmodel.eff` 中的 `CAB-0708fc9259c3e93c7c8a8939444995c4.bytes` 做 `/O2` 对照：原生文件 1,477,092 字节，3 个类型、2,653 个节点、3 个对象。相同进程交替运行两种路径，每种每轮 100 次重新解析并建立事务：

| 轮次 | 原路径每成员 | 省去复制后每成员 |
| --- | --- | --- |
| 1 | 1.36107 ms | 1.05877 ms |
| 2 | 1.12862 ms | 0.680638 ms |

该实验复用文件字节和 reader 的解码缓存，只说明这处 CPU 分配/复制的成本，不代表真实游戏冷 IO、帧时间或整组资源准备耗时。此前日志约 231 ms 的主线程整组准备仍需新游戏会话复测，不能宣布该停顿已消失。实验源码、构建命令和输出保存在 `diagnostics/dll-release-fixes-20261117/cold-layout-*`。

## 验证边界

旧目标 A→B→A 恢复及紫材质问题已有上一轮用户实机通过反馈。本轮增加的退休推进、默认诊断和冷路径优化尚需连续 F10、世界/UI 切换及最后 owner 退出的游戏复测。

相同 Mod 的不同安装目录仍隔离资源身份，不自动跨目录合并相同贴图。AMD + RTSS、Wine 实机、长时间提交内存/显存和 20–30 Mod 压力本轮没有新增测量；这些边界保留，不因自动测试通过而标为已验收。

## 最终回归与部署

当前源码重新构建并通过六组回归：完整资源重载、特效/动画（原生特效生命周期包含 1,401 次检查）、入口/配置/日志契约、共享资源/材质删除、代际差异、原生随机读取。构建和运行输出保存在 `diagnostics/dll-release-fixes-20261117/` 各 `*-build.log`、`*-test.log`。生产二进制排除探针检查，以及使用系统和游戏目录 C++ runtime 的真实 LoadLibrary/导出控制循环均通过。

四仓库均已 fetch 并核对：本地均处于 `main`，`HEAD` 包含 `origin/main`。本轮未提交、推送、创建标签或发布 Release，保留 Manager/Blender 既有工作区修改。实际构建基于 EFF `9b89656ae0f4ccc337bbcbfbfc53b6c269f58144` 加当前未提交修正；有关源码摘要、差异和新增回归头副本分别在证据目录的 `source-files.json`、`source-changes.patch`、`retarget_generation_checks.h`。正式发布仍须按发布规程固定源码提交。

通过固定部署器安装，游戏未运行，安装结果：

- 目标：`D:\Hypergryph Launcher\games\Endfield Game\plugin\eff.dll`
- 文件大小：3,731,968 字节
- SHA256：`84C1FDFC9A116B3890CC8FAE9AF1498F07C22AD7FA509313D3C8AE6CDB403B7E`
- 回执：`bin/local-deployments/20261117-090243-076/Dll.json`
- 用户 `eff.ini` 摘要前后一致，Mods 未改动。

安装摘要独立复核见 `installed-verification.json`；回滚副本由部署器保留在游戏 plugin 的 `.eff-deployment-backups/local-20261117-090243-076/`。
