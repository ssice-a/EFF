# ZFY 原生形态键动画实验

2026-11-13：用户提供的 `plugin/backup/ZFY` 是下一步真实 Mod 验证对象。此前最小混合 AnimationClip 已在临时对象上通过数值验证；本轮要验证真实角色 Renderer、既有 EFF 控制及连续播放，不能继承前一轮的通过结论。

## 来源和转换

- 原始备份：`D:\Hypergryph Launcher\games\Endfield Game\plugin\backup\ZFY`，保持不变。
- 测试副本：`D:\Hypergryph Launcher\games\Endfield Game\plugin\mods\ZFY_animation_test`，保留原 ID `ZFY`、全部资源、INI、state.ini 和按键。安装前拒绝重复 ID 或相同角色目标。
- 来源动画：`fe883141e4054cbf`，小键盘 5，1.92 秒循环，默认不自动播放。
- 两个衣物 Part 各有 `Up Buh`、`Up Puh`，合计四条轨道，每条 117 个采样，原值 0～1。
- 每个独立 Renderer 对应一个空相对路径的原生 Clip 输出，两个输出使用同一 EFF 动画时间。转换将形态值乘 100，匹配 Unity BlendShape 权重；帧率为 `116/1.92≈60.416668`，不四舍五入到 60，也不重新采样。

`prepare_mod_shape_experiment.py` 检查真实 Mesh 的形态名称和 INI 绑定，记录源目录所有文件摘要；`NativeAnimationModChecks` 调用已验证的原生 writer，仅创建 Clip 和 Bundle 两个对象，不带来源资源。独立 AnimationClip 读取器检查全部 468 个标量样本、绑定类型和时长。`verify_mod_shape_experiment.cpp` 通过当前游戏侧 Catalog 打开全部网格、材质、贴图和控制，避免只凭 Python 能读就认定包能用。

## 运行边界

显式构建 `build.bat mod-animation-probe`，安装脚本为 `tools/diagnostics/deploy_mod_animation_probe.ps1`。该构建没有 RenderDoc，不运行上一轮临时对象的固定样本；正式发布验证器拒绝其 `[MOD-ANIMATION-PROBE]` 标记。

运行时仍使用 EFF 原控制图、快捷键、Ins 和累计时间。播放或暂停时，命中的两件衣物由原生 SampleAnimation 写入相应形态，旧逐轨道写入跳过这些形态；旧数值求值保留为面板投影和误差对照。每次原生采样读回实际 Renderer 权重，与原曲线同一时间的预期比较，容差 0.02（Unity 的 0～100 权重单位）。异常记录 FAIL 并恢复既有形态执行器，不能把回退后能动视为原生通过。

非 Legacy 原生采样需要目标 GameObject 的 Animator。本实验只给命中的 Mesh GameObject 添加独立、无 Controller 的 Animator；已有同节点 Animator 时拒绝接管，不修改角色根 AnimatorController。停止、手动控制抢占、禁用、Renderer 退休和 F10 代更新移除实验拥有的 Animator。代更新先退休消费者，再卸载 Clip Bundle；清理不确定时保留依赖。骨骼、材质动画及全套生产链路仍不在本次完成范围。

## 测试和证据

安装到固定游戏目录后，进入庄服角色场景，用小键盘 5 开始原动画。日志需出现两个 Part 各自的 `phase=bind` 和持续的 `phase=sample result=PASS`；`wraps` 增加证明经历循环，`nativeWriterOnly=1` 表示旧形态写入已跳过。初步验收还应检查：

1. 实际画面与旧动画表现一致，跑动时身体骨骼不受影响。
2. Ins 暂停保持时间，恢复继续；修改速度保留当前相位，停止后恢复原手动值。
3. 后续单独验证世界/UI 多实例、LOD 和 F10 代替换。数值采样 PASS 不自动覆盖这些项目。

离线结果位于 `diagnostics/zfy-native-animation-20261113/`；游戏安装回执位于 `plugin/diagnostics/zfy-native-animation/deployment.json`。离线转换、当前 Catalog、实验 DLL 构建和原生 writer 回归通过；修正后的会话 42728 已通过真实角色连续播放验证，详见下文。全套生命周期验收仍未完成。

退出游戏后可运行 `deploy_mod_animation_probe.ps1 -Restore` 恢复已核验生产 DLL。测试副本仍是完整原包，恢复后使用既有播放器；是否保留该副本由后续测试决定。

## 首轮真实 Mod 失败与修正

会话 32296 的日志保存在 `diagnostics/zfy-native-animation-20261113/session-32296/`。用户观察到详情页周期性卡顿、Ins 显示内部十六进制 ID、动画不动；本轮未通过真实角色验证。

- 原生实验把作者 ID `ZFY` 当作运行时包键，实际安装副本的键为 `ZFY_INSTANCE_<目录摘要>`，因此日志中没有任何原生实验入口记录。修正按作者 ID 与安装目录共同定位当前实例，再用其运行时键访问控制状态；不同安装副本仍隔离。
- Ins 原先直接显示内部包键，现改为目录名与作者 ID，ImGui 仍使用内部 ID 区分同名安装。
- 同两个 Bundle 的目标发现触发 32 MiB 读取预算失败后未缓存失败，重复 201 次，相邻失败间隔中位数约 208 ms。现在将类型明确的预算失败按共享 Manifest 缓存；不会把失败当作成功的“不命中”，新代可重新检查，其他临时错误保留重试能力。这里只证明消除了重复扫描，不能据此宣称实机帧耗时已改善。

完整热更新回归新增两个角色/UI 调用方各重复 100 次的实际预算失败用例：修正前失败，修正后只尝试一次，且新代和临时失败重试通过。控制回归验证安装实例 ID 下原包同键速度绑定与 Num5 播放共存、暂停/继续/循环/停止；真实 Catalog 验证两个副本的定位和可读名称。实验构建同时打开现有加载性能计数，用于下轮验证。

`deploy_mod_animation_probe.ps1 -Update` 校验前次回执、生产备份、当前 DLL、Clip 摘要、配置及现有 Mod 后仅更新诊断 DLL，保存前版 DLL/回执；不重复制备份 Mod，也不覆盖用户 INI 或 state.ini。

## 修正后实机与通道核查

会话 42728：用户确认动画运动且详情页不再周期性卡顿。归档日志和数值摘要位于 `diagnostics/zfy-native-animation-20261113/session-42728/`。两个 Part 在先后两组 Renderer 上均绑定成功，原生采样无 FAIL，最大权重误差约 `0.00002388`，小于 `0.02` 容差；连续循环和停止时移除自有 Animator 已有记录。先前重复 201 次的两个预算失败各只出现一次。长段连续播放内四个完整性能窗口均没有超过 100 ms 的帧钩子间隔，最大间隔约 30.28 ms；这不是 GPU Present 测量。尚不能据此验收同时存在的世界/UI 实例、全部 LOD、F10、全部暂停/速度操作或生产通用接入。

用户随后反馈似乎只有一个通道，重新逐项核对了原始包、测试副本、全部 Mesh 元数据、原始控制和转换清单，结果保存在 `diagnostics/zfy-native-animation-20261113/shape-coverage.json`：

| Mesh | 形态键 | 驱动 |
| --- | --- | --- |
| body_01 | VaginaOpen、AnusOpen | 原包方向键按住控制，不属于采样动画 |
| cloth_01 的形态 Part | Up Buh、Up Puh | 小键盘 5 的同一个动画片段 |
| cloth_02 的形态 Part | Up Buh、Up Puh | 同一动画片段 |

原包只有一个 1.92 秒动画片段，含四条独立变量轨道；四条 117 点曲线逐值完全相同，都是 0→1→0，所以运动同步。转换保留全部四条轨道，未合并成一个通道。游戏中三个 Mesh 的两条形态均 `shapeCompatible=1`，原生实验每次采样分别读回两件衣物的两个形态并比较，不是只检查总数。两个身体形态的绑定已保留，但本轮没有单独覆盖其全部方向键操作，不能将其运行效果一并宣称验收。测试副本相对原备份仅 state.ini 有用户操作变化；资源和 INI 均相同。
