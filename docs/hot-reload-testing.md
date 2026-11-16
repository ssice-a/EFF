# 游戏内 F10 热重载测试入口

本页指游戏 DLL 重新装配已安装 Mod；Mod Manager/Blender 下载并安装新软件是另一链路，见 [软件更新诊断](software-update-audit-20261112.md)。

旧逐次日志、DLL 摘要、已失败尝试与历史修复证据完整保存在 [截至 2026-11-05 的测试记录](archive/testing/hot-reload-testing-through-20261105.md)。它们只证明各自日期/源码/场景，不能当作当前 DLL 的全部验收。

## 当前回归入口

2026-11-14 的首次添加/移除再添加修复、外部贡献采纳边界和自动验证证据见 [Issue 6 修复记录](hot-reload-issue6-20261114.md)。

2026-11-15 的“F10 后当前特效不变，切换角色才更新”涉及嵌套武器的准确 owner 被顶层枚举折叠，见[活动特效刷新修正](effect-f10-live-owner-20261115.md)。

2026-11-16 的另一条“切换后才更新”路径已确认等待 raw Prefab 引用约 20 秒；补齐角色 UI 原生武器列表的依赖捕获及重载顺序，见[共享武器 Prefab 交接](f10-shared-weapon-20261116.md)。这与仅刷新挂点的旧问题不同，实机仍需分别验证。

- `tools/diagnostics/build_full_resource_reload_smoke.cmd`：游戏 owner/资源代际的模拟原生 API 回归。
- `tools/diagnostics/build_generation_native_identity_smoke.cmd`：内部 CAB、外部引用和资源流的代际隔离；生成的 `bin/generation-native-identity/smoke.exe --game "D:/Hypergryph Launcher/games/Endfield Game"` 可额外检查本次故障对应的当前原生输入。
- `tools/diagnostics/build_native_random_access_smoke.cmd`：声明与按需资源/输入读取回归。
- `tools/diagnostics/build_demand_resource_regressions.cmd`：按需资源相关回归集合。
- 当前控制/动画租约及目标变更覆盖见 [架构修复](architecture-repairs-20261112.md) 和 [武器重定向](weapon-retargeting.md)。旧 `build_entry_reload_smoke.cmd` 不是当前完整 Format 3 验收入口。

## 实机验证矩阵

固定 DLL、游戏版本、Mod 内容与起始场景，保存日志和起止标记，分别测试：

1. 世界、UI 展示同时存在的实例，修改 Mesh/DDS 后 F10，确认两侧实际显示新内容。
2. 增加、删除、更改逻辑目标，及空 Mod 集合恢复原版；未命中资源不应全量预载。
3. 仅控制修改时保留兼容变量、选择与动画相位；资源改变时使用正确新代际。
4. 同 Mod 多目录安装、同作者资源多目标，确认状态与绑定相互隔离。
5. 连续 F10、切图/编队、UI 开关与 owner 退休，记录耗时、未完成加载和内存变化。
6. 无效 INI、缺失资源与原生加载失败分别验证拒绝/恢复边界，不只看“root complete”日志。

现有自动验证通过不等于上述全矩阵实机通过。不要用旧 DLL 的症状、旧 namespace 策略或单次成功推断当前所有场景。
