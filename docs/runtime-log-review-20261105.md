# 普通版恢复后的武器复测与剩余问题

核对时间：2026-11-05 22:42。用户反馈“目前看起来成功了”，对应恢复普通 DLL 后的会话 PID **30544**。

原始快照及机器可读结果：`bin/runtime-log-audit-20261105-224250/eff.log`、`player.log`、`audit.json`。会话 BUILD 为 22:40:37.066，普通构建时间 22:01:08；日志末尾 22:41:48.661。核对时游戏已不在运行。

## 武器链路：本次场景基本通过

- `sw` 的四个 LOD 均登记了精确来源路径、asset 和 Bundle 路由（eff.log 95～98、370～373 行）。
- 22:41:42.321 命中 `mods=sw carrierParts=4`，准备耗时 11.397 ms（3601 行）。
- 22:41:42.331 成功解析 `wpn_sword_0022_refined.prefab` 的内嵌材质 `M_wpn_sword_0022_refined_01`，`containerPreload=1`（3602 行）；此前阻塞详情页的材质解析链路已经通过。
- 之后存在四个 Renderer 的武器网格绑定，helper `ready=4`（3617～3622 行）。
- 全会话最终 `loadSubmits=170 loadCompletions=170 loadFailures=0 pendingLoads=0`。没有资源准备 GAP、共享键冲突、依赖环或超时。
- 用户画面反馈与上述链路一致，记录为本次武器详情页检查基本通过；不扩展成所有武器场景和距离 LOD 均已验收。

## 明确仍存在：lx 的 UI 粒子渲染标志

22:41:42.210，`lx` 的特效 `428fc265c91642bab14c33719e961d7f` 挂到 `chr_0028_wulfa_uimodel.prefab`，scope=ui（3597 行）。

22:41:42.737，同一个 Renderer 在激活前和激活后各出现一次：

```text
[EFFECT-UI-RENDER-SET-GAP] setterCalled=1 renderInUI=0 playbackContinues=1
```

这是 **同一实例的两个检查阶段**，不是两个不同特效故障。`src/eiem_effect_debug.h` 的 `EnableUIRenderers` 调用了 `set_renderInUI(true)`，随后 getter 仍返回 false，于是记录 rejected=1；不能把它当作设置成功。

另一方面，播放没有停止：after-1s 为 11 个粒子，after-3s 为 39 个，playing/emitting 均为 1（3634、3636 行）。这证明粒子模拟在推进，**不证明 UI 相机实际绘制成功**。本轮没有对它实施代码修改，也没有由日志直接断言画面一定不可见。

## 游戏 / Unity 日志中的剩余记录

Player.log 有以下四类 Error/Critical 头：

1. 第 1 行：`Could not load symbol HGSetupCustomVulkan`。
2. 第 75 行：`Lua ifix`，调用栈经过 IFix 和 LuaMultithreadingGC。
3. 第 103 行：`No NumId 19 in 16140901992250720264 19`，经过 BitsetSystem 与网络同步。
4. 第 392 行，退出内存统计之后：`There are remaining Allocations on the JobTempAlloc`。

停用 EFF 的对照快照 `no-eff-baseline/after-player.log` 也有前两条；这是日志对比，不代表已确认整个图形代理链无关。第三条的栈属于游戏同步链路，尚未定位源头。第四条不能忽略，但没有分配调用栈，尚不能归因于 EFF，也不能仅凭退出告警反推 20 秒加载的原因。

此前的 `Can not find bundle hashName`、女管理员 `meshRenderer`、资源 `Position out of bounds` 没有在本会话出现。依赖释放已记录 `direct=32 released=32`（3635 行）；单次成功不是长期资源回收全部验收。

## 尚未闭环的历史事项

- **剧情完整播放与转场、剧情/世界按键状态同步：** 修复已在当前普通版，本轮没有 `CONTROL-RUNTIME-CLONE` 事件，也没有 F10 请求，不能用武器页通过替代原剧情/热重载场景的验收。旧女管理员 NPC Renderer 错误需要回到原场景确认是否消失。
- **长期切图和多代回收：** 共享释放入口已有实机成功记录，但仍缺反复切图后的引用/内存趋势验证。
- **约 20 秒加载的责任归因：** 已按用户要求停止排查并恢复普通 DLL；未取得完整、可比较的无 EFF 场景秒数，不能将其登记成已证明与插件无关。
- **Tangtang 特效预览：** 已有指定 Softbody VAT、Clip 采样和 Trail 预览；完整技能 Animator/Timeline、其它 VAT 类型、自定义顶点流、蒙皮重绑定及完整游戏像素渲染仍未全部实现，水衣服也尚未完成实机验证。以 `tangtang-water-effect-research-20261105.md` 的当前边界为准。
- **发布同步：** 晚间武器、剧情及共享生命周期修复有本机部署和未提交工作树记录，没有在本轮执行推送/Release；本机效果不能直接当作现有 Release 已覆盖。

本轮只保存日志并更新状态文档，没有更换 DLL、改 Mod 或启动新诊断。
