# 材质 UV 动画与展示页特效生命周期

## 问题与证据

`.011` 主图使用 UV0，动画写入 `_MainTexUVRotateMat`。已安装 `.effanim` 四个分量均有变化，每通道 201 样本，3.333 秒一圈。这里的问题是游戏内不旋转，与缺少 UV 控件无关。

旧日志中，展示页同一武器挂点约每 3 秒记录一次 `EFFECT-ATTACHMENT-RECOVER`，创建不同实例并将粒子时间归零。大世界没有相同重建序列。工程的 EffectSetting 为 `isLoop=0,duration=3`，内部三个发射器则持续发射。游戏 EffectInstance 的 DurationTick 对非循环实例递减剩余时间并结束实例，插件的存活恢复随后重新创建它。

## 修正

- RuntimeMaterials 的匹配 Renderer 持有对应采样的活动引用。加载器 Bundle proxy 与实例模型的生命周期不同，不能只用前者决定是否采样。未匹配时不读取样本；多个 Renderer 独立持有，最终释放后停止持有。
- 不播放时也检查 Renderer 存活和 Mesh 身份，防止已销毁模型遗留采样引用。
- 每个绑定约一秒后只读回一次原生向量，记录 `MATERIAL-ANIMATION-STATE/VECTOR`，区分未采样、未写入和 Shader 行为。读取绕过游戏视图的 baseline 隔离，不更改游戏属性块。
- Blender“设为持续发射”同步开启祖先 EffectSetting 的 `isLoop`，并沿原生 Transform 父链查找。该标志表示持续运行、忽略整体时长，不会定时清空粒子。ParticleSystem 的 looping 负责持续发射，已有粒子按自身寿命结束。
- 面板改为“持续运行（忽略整体时长）”，解释关闭后的时长回收行为。

## 本机验证

- 完整采样播放器 → 四分量向量 → Renderer slot 的回归，修正前失败，修正后通过，覆盖无匹配不读取、多实例和最终释放。
- 原生 Transform 父链回归通过，仅修改祖先，不修改兄弟节点。
- 生产 DLL 校验和特效、材质运行时回归通过。已部署到游戏 `plugin/eff.dll`，Blender 0.50.33 已事务安装并热重载。
- 当前柔光根三个持续发射器已同步设置，导出覆盖 `mods/lizi`，耗时约 2.41 秒。导出作者编辑层的 `effectLogicCfg.isLoop=1`；workspace/original 保留模板原值，符合差异编辑结构。
- 未覆盖保存用户的 .blend。当前场景含修正；原 Mod 备份在 `diagnostics/uv-and-ui-restart-20261116/mod-backups`。

## 首轮实机结果与后续诊断

用户反馈 `.011` 仍未旋转。原生读回已确认世界与 UI 的四级 LOD 都持有采样，四个输出通道持续推进，Renderer slot 中 `_MainTexUVRotateMat` 也随时间变化。这排除了“仅仅没有导出/播放样本”的解释，但没有证明实际 Draw 使用该属性块，不能据此宣称旋转已修复。

该轮日志从 11:13:56 的 UI 实例创建持续到 11:18:26，没有再次出现 `EFFECT-ATTACHMENT-RECOVER`。展示页三秒回收重建的日志症状已消失；粒子视觉仍以用户确认结果为准。

下一轮实现将同一组动画输出同时写入独立的 Material 实例与 Renderer slot，验证绘制是否读取材质常量。每个实际 Renderer 仅在播放时创建私有材质；共享来源不修改，停止/释放时仅在仍持有本插件实例的情况下恢复来源并销毁私有材质，游戏主动换材质时不会被旧来源覆盖。现有 INI、Action、样本和播放时钟不变。这是针对渲染传递链路的待验证修正，不代表已从 GPU 捕获确认 HGRP 的实际消费路径。

回归覆盖真实延迟样本到属性块与私有材质的传递、多个实例隔离、停止时与游戏写入交错、恢复来源和销毁实例。新增原生 API 的本机元数据候选已检查；精确类型由运行时解析器核对。诊断读回包含材质值、实例与来源地址，每个绑定仅记录一次。

11:29 的生产构建与运行时 fixture 均通过，已部署游戏目录，安装 SHA256 为 `D5C2FBE497D537E4629D4867C70C2F558061A3FC369B200BAB746A517113FA66`，回执为 `bin/local-deployments/20261116-112947-216/Dll.json`。本轮未修改或重新导出测试 Mod；下一次实机画面结果仍待确认。

随后 PID 37460 的世界/UI 共八个 Renderer 均成功分配不同的 Material 实例，来源材质仍共享；属性块与材质读回一致（例如 UI 矩阵 `(-0.00587583,0.999774,-0.999774,-0.00587583)`）。原生接口运行时精确解析成功，未出现材质动画 GAP。此证据仍只证明传递到 Material，并不代替画面验收。日志与统计保存在 `diagnostics/uv-and-ui-restart-20261116/eff_log_after_material_instance.txt` 和 `material-instance-live-evidence.json`。

## 旋转验收与 F10 后续问题

用户已确认该版旋转成功，同时反馈更换材质、添加 Alpha 和修改动画倍率后 F10 未正确更新。日志显示新通道已由 4 增为 8，控制 revision 确实重新绑定；私有材质恢复也通过新增的控制图发布、新来源交接和旧实例销毁回归。因此动画并非脱离 F10，但原有状态合并与 UI 生命周期存在缺口。

- 倍率从 1 改为 0.5 时，`MergeLive` 无条件保留旧持久值，连未调整过的默认值也遮住新声明。修正为所有保留变量统一进行三方合并：运行值仍等于旧默认则采用新默认；真正的运行时覆盖仍保留。保持兼容动画相位和显式 `reload=reset` 语义。
- 11:33:33 的 UI 更新记录 `game UI page did not accept the completed model`。按需 F10 先卸载旧 GameObject，再异步交付新实例；Lua 交接闭包却仍检查并读取旧对象。现先显式暂存页面变换、Animator 状态与页面绑定信息，旧模型卸载后使用暂存状态接收新实例；关闭页面或游戏自行更换 item 时仍拒绝过期完成。
- 增加执行真实生产 Lua 字符串的 `ui_page_lifecycle_smoke.lua`。先确认旧实现会在“卸载旧模型后完成”步骤失败，再验证修正、连续重载及结束/更换 owner。此场景不能由原 C++ Lua API mock 单独覆盖。

失败与通过记录分别为 `control-reload-red/green.txt`、`ui-page-red/green.txt`，私有材质 F10 回归为 `material-f10-test.txt`。完整原生热重载回归见 `full-reload-test.txt`，包含卸载前调用页面暂存的生产链路。生产构建与校验通过；11:47 已安装，SHA256 为 `8084E22C1697E9C3689FDFBE0F298CFBFAA1EF25F3A2DC88DF6630C9FC6E19A9`，回执 `bin/local-deployments/20261116-114748-016/Dll.json`。四仓库均在 main 且包含 origin/main。新一轮实机 F10 验收仍待完成。
