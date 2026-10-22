# 花瓣消失与 NPC 缺失跟进

最新安装是 2026-10-22 的 UI Renderer 激活后重试修复。本文 2026-10-21 的 NPC、世界消失调查作为历史证据保留。

## 2026-10-22 UI 特效渲染恢复

PID 39412 的 `eff_log.txt` 多次记录 UIModelLoader 正常登记 `chr_0028_wulfa_uimodel.prefab`，特效匹配到精确花瓣 Part 与骨骼（`matched=1`），但 `CreateInstance` 在 Renderer 设置阶段抛出 `UI ParticleSystemRenderer rejected renderInUI`。00:01:59 的 UI 根尚未激活；00:02:55 与 00:12:05 的根已激活，仍复现同一拒绝。因此 owner 登记、挂点匹配和 UI 活跃状态均不是拦截点，旧代码的硬性断言让 UI 特效从未到达 `EffectManager.AddUnmanagedEffect`。

修改 `src/eiem_effect_debug.h`：`renderInUI` 设为尽力开启，不再因 getter 仍为 false 而拒绝克隆；在 `EffectManager` 注册并激活根对象后、启动 ParticleSystem 前再次设置。每阶段分别记录 Renderer 数量、accepted/rejected 以及失败阶段；若激活后仍被拒绝，继续播放并留日志，以便识别后续是否为 UI 相机/渲染管线问题。

回归在真实 `CreateInstance`/挂载路径注入“首次 setter 无效、激活后 setter 生效”，旧实现稳定失败，新实现通过；世界、NPC、UI 独立实例、F9 更新、LOD 显隐和回收检查通过。普通 v35 DLL 构建、PE/静态 CRT 检查以及预加载游戏 app-local CRT 后隔离加载真实 DLL通过。

00:28:57 +08:00 游戏进程退出后只部署 `plugin/eff.dll`：旧 SHA256 `ED09E9384EEB1E8106E1B1A41716BEF4DAD1085D732C6A0E63B68B9F2CBFE8B5`，新 SHA256 `5C18D7F78975611101FEB5F1499B3C7480519D676E450D29B171BC9AE7766C61`；337 个 Mod、配置、协议和代理文件保持不变。回退副本、部署前日志与哈希清单在 `bin/effect-ui-render-20261022/`。部署 DLL 的游戏画面尚未验证，必须启动游戏重新打开人物 UI；检查 `[EFFECT-UI-RENDER] phase=after-activation` 及 `[EFFECT-ATTACHMENT-PLAY]`。如果日志显示 `accepted=1` 且 `PLAY` 成功仍无画面，下一步查 UI 相机/Renderer 层提交；若 `accepted=0`，则 setter 在激活后仍被游戏拒绝。

2026-10-21 用户报告 NPC 一直没有花瓣，大世界最初有、后来消失，可能按过 F9。当前修复已证实的配置遗漏和诊断失效；完整实机根因尚未确认，不把 PLAY 或持有一个实例等同画面正常。

## 实际证据

- PID 25776；旧测量 DLL SHA256 `9855504a09c1de5faa0ac2ebaf3b75e32938d1dd97c331fcae981d076271a5ba`。
- 14:04:50.638 世界 BIND，14:05:00.005 READY/PLAY；14:07:28.280 再次 BIND、.690 PLAY。UI 也有独立绑定。没有 EFFECT-ATTACHMENT-GAP。
- 14:11:30.263 请求 F9，.470 准备、.495 提交成功，无 REJECT。同配置不重建实例是原有语义，不能断言 F9 读取失败。之后仍有一个绑定/实例，但不知道粒子实际是否在播放。
- 启动 EFFECT-PARAMETERS-GAP 原因是游戏裁剪了 MinMaxCurve.get_curveMultiplier，导致全部粒子观测停用。metadata 同时证实 Particle 的若干 getter 被裁剪。
- 解开当前安装附件，EffectSetting.isUseDistanceLod=1，内部和花瓣节点的最远距离档 isActive=0。此前更新 DLL/导出器时没有发布新特效包，见 [原部署边界](effect-all-lods-20261021.md)。这是能关闭发射器的真实配置，但尚未证明此次消失完全由此造成。模型 LOD 与特效自身距离 LOD 是两个机制。

## 改动与验证

从游戏当前安装包生成 LOD-only 副本，没有使用含不同贴图 RGB 的旧 Blender 暂存包。只对唯一根对象所属 EffectSetting 应用现有 EiemEffectLodPolicy：禁用 distance/guard 降级、启用原生禁用距离剔除标记、质量掩码 15，各距离档保持 active、速度/更新比例 1。平台与 targetLayer 掩码保持。

新内容使用隔离 CAB/Bundle 身份，保留依赖边、挂载/控制/TRS。重新解包按 class ID + PathID 比较 18 个审计对象，仅一个 class 114 设置存在策略变化，粒子、Renderer、Mesh、Material、Texture 保持；归档逐成员往返检查通过。更新编译器同步 compiled.bin 作者封印，没有重建人物原生输入。证据在 bin/effect-recovery-20261021/stage-verification.json、before-native、after-native、LodPatch 和 verify_and_stage.py。

src/eiem_effect_observation.h 的 multiplier/Particle getter 改为可选，缺失明确报告 unavailable，不伪造倍率，也不猜字段偏移。保留粒子总数、playing/time、寿命/速度/发射配置、simulationSpeed，新增可用时 emitting/stopped/paused/nodeActive。F9 对已有实例读一次，并重启有限的 after-1s/after-3s 快照；不强制 Play 或改粒子值。

F9 新增 EFFECT-ATTACHMENT-MATCH，输出已登记 owner 的路径、Renderer 总数、目标 Mod Renderer 数、精确 Part 匹配数及样例；STATE 记录绑定/实例，提交记录 owners/bindings/diskChanged。可区别 NPC 未登记、挂载 Part 缺失、骨骼证明失败和实例已在但停播。未放宽来源/骨骼校验，不扫描场景。NPC 的具体失败点仍待这批新日志。

先加 optional getter 回归，旧代码确实失败 runtime shape method missing；修复后普通/测量测试通过。生产入口隔离回归覆盖世界/NPC/UI 独立实例、未变 F9 保留、变更事务、准备失败保留、LOD 晚登记/隐藏/恢复、骨架替换与池回收。普通/测量 DLL 都通过 x64 PE、静态 CRT、诊断标记和游戏 app-local 运行库下真实 DLL 隔离加载。离线回归不代替游戏画面。

## 部署

2026-10-21 14:35:20 +08:00，确认游戏已退出，备份部署三份文件：

1. plugin/eff.dll：测量版 2,681,344 字节，SHA256 `15ae0c5a7e4d1fe2c36eefa169c0c41d75520eefc2ba498e354181fd452f5417`。
2. plugin/mods/lx/compiled.bin。
3. plugin/mods/lx/resources/effectattachment_428fc265c91642bab14c33719e961d7f.bin。

其余 334 个受检 Mod/配置/协议/代理文件摘要保持。发布使用来源与 compiled 锁，核对前后摘要并保留回滚；未启动/终止游戏，未操作或保存 Blender。

部署清单 bin/effect-recovery-20261021/deployment.json，备份目录 bin/effect-recovery-20261021/backup-20261021-143520-440291。原日志保存为 eff-log-before-deployment.txt。需回退时只能在游戏退出后按清单恢复，若用户已经重新导出，不覆盖后来的包。

普通候选 bin/resource-directory-entry-v35/eff.dll SHA256 `e968016b396e7dfdc660fbbbba47eaf7637b9af8f14a2c95f5512c8687a34ecb`，尚未部署，因为性能测试继续。

## 实机下一步

正常启动后观察玩家近远距离；让已显示角色 Mod 的 NPC 与玩家同屏，按一次 F9 等至少 5 秒；再测人物 UI 关闭重开与切场景。若消失，保持现场按一次 F9，日志即可记录匹配与实际粒子状态，无需重导人物或改作者参数。

NPC 恢复、大世界不再消失、TAA 和碰撞均未因此通过实机验收。性能 [首轮报告](dll-performance-results-20261021.md) 的旧 DLL/包基线保留，下一轮单独记录。

## 新会话启动证据（PID 10208）

用户随后启动游戏。2026-10-21 14:42:52 的新会话已确认加载部署 DLL；14:43:11 粒子观测成功就绪，无此前 optional getter 安装失败。14:43:57 世界模型绑定，14:44:04.661 新全 LOD 内容键 C68AA7F5... 准备完成并播放。有限观测在 after-1s 读到 8 个、after-3s 读到 34 个粒子，playing/emitting=1、stopped/paused=0、节点 active=1，寿命配置 3～5 秒、发射配置 10～18/秒。证明本次实例实际在发射，不能代替像素可见性或 NPC 验收。证据保存为 bin/effect-recovery-20261021/after-start-20261021-144445-961697.log 与 startup-observation-pid10208.json；尚需 NPC 与玩家同屏 F9 的匹配审计。

## 15:20 NPC helper owner 与 UI 粒子剔除跟进

14:46:27 的 F9 已成功提交：玩家粒子数52→53→56，playing/emitting=1、paused/stopped=0，原实例保留；没有新加载错误。日志有两套各78个lx Mod Renderer，第二套在14:44:02.896由另一个EntityRenderHelper完成登记，但F9的136个part/UI owner中只有玩家包含目标Part。源码确认 HelperInitialized 只 RegisterRendererList，没有给特效登记 owner；既有NPC回归以前用BasePart回调模拟，没覆盖真实helper-only路径。现测试直接进入生产 HelperInitialized，先复现失败，再验证Frame通过共用owner集合发现NPC。

新增 FullResourceReload.liveHelperModels：仅在已知游戏Helper.InitAll事件取得其GetTransform和GameObject，弱引用登记；OnRelease在原回调清理前解除helper-only特效和网格登记，part/UI卸载也清理其helper记录。Frame将helper root与已完成part/UI根合并，嵌套helper及同root多个helper去重；重复Init/F9保留模拟，不新增全场景扫描、不弱化Part/palette来源校验。回归覆盖helper-only NPC、F9发现、独立world/NPC/UI克隆、重复Init、同root多helper与单独释放、LOD、骨架重建及最终handle清零。

136个part/UI审计里出现的六类敌人matched=0，不能拿来证明用户眼前NPC不是目标实例，因为helper-only NPC根本未进入这份列表。仍需新构建实机确认其helper根范围和精确骨骼证明；本轮不把模糊的NPC名称当来源证明。

UI日志14:51有两类样本：活跃UI克隆after-1s能产生9或11粒子；另一些page实例在登记时hierarchyActive=0，稍后被激活后playing/emitting仍为1，但after-1s/3s的time=0、count=0、simulationSpeed/timeScale=1。旧资源的ParticleSystem.cullingMode=Pause(2)，符合相机不可见时暂停模拟的路径。该证据不能证明此前F9直接造成，也不能排除UI页自己的预览/采样更新；没有用强制每帧Play掩盖。

对“覆盖全部LOD”的隔离运行副本，离线编译器现在也将本特效层级的ParticleSystem.cullingMode设AlwaysSimulate(3)，并改变内容命名空间；关闭该导出策略仍保留source模式。源作者数据保持。实际source/all构建重新解析两个粒子，比较全部字段，仅cullingMode差异；56项检查、作者源保持和确定性构建通过。游戏安装包直接从上一安装副本生成单项对照，18个审计对象只有两个ParticleSystem的cullingMode改变，寿命/发射率/运动、材质/贴图/网格、挂点/TRS、EffectSetting和依赖保持。

15:20:19 +08:00确认游戏已退出后备份部署测量DLL与lx两个生成文件；334个其他受检文件摘要保持。DLL为2,688,000字节，SHA256 fcb513e52d3d745cd4b51d2e364f99c597c730c6b6c6c8b4fb2e47181a9687b9；特效内容键05BF84281BEBA7E70107220B7CC696FE2CB5619627D3BB502F0C6C3B166A78F7。明细/回退：bin/effect-recovery-20261021/npc-ui-followup/deployment.json及backup-20261021-152019-840042。配套AnimeStudio.GUI.dll也备份更新，核心依赖摘要一致；后续导出不会恢复旧的all-LOD暂停策略。

普通/测量构建、相应真实生产入口材质/特效回归、形态桥接回归、PE/静态CRT、app-local运行库下真实DLL加载通过；性能分析器5项检查通过。未启动/终止游戏，未操作或保存Blender。当前仍为性能测量版，NPC与UI的实际画面均未验收。重新启动后需在目标NPC与玩家同屏、UI关闭重开和F9前后分别观察；不应承诺此次单项culling修正一定解决UI不可见。

## UI F9 仍不可见（待新会话日志）

用户报告在 UI 人物页内按 F9 后仍没有特效。已核对游戏目录中的 `plugin/eff.dll`、`plugin/mods/lx/compiled.bin` 和花瓣资源 SHA256 均与 15:20 部署清单一致；部署没有被后续文件覆盖。当前游戏目录 `plugin/eff_log.txt` 最后修改时间仍为 14:58:53，早于部署，且检查时游戏进程未运行，因此现有日志无法对应这次 UI F9，也不能证明新 DLL 已在用户所见会话加载。

旧会话中，UI 特效绑定和粒子发射曾成功；部分 UI 克隆在 3 秒内 count/time 持续为 0，另一活跃克隆在 1 秒时 count=11。新部署已将粒子剔除模式改为 AlwaysSimulate，但这只解决不可见时暂停模拟的路径，不能证明 UI 摄像机实际绘制或材质可见。下一轮需在部署后重新启动游戏，让日志更新，再在 UI 页面按 F9；根据新 `EFFECT-ATTACHMENT-MATCH`、`EFFECT-ATTACHMENT-STATE` 和粒子快照区分 owner/挂载、模拟与仅渲染不可见问题。

## Helper 签名导致特效模块启动失败

15:39:13 新游戏日志明确记录 `FULL-RELOAD-REJECT`：安装阶段要求 `EntityRenderHelper.GetTransform()->UnityEngine.Transform`，当前 IL2CPP 精确签名查询失败；随后 `EFFECT-NATIVE-REJECT` 因资源重载/owner 生命周期依赖未就绪而拒绝启动。该错误会关闭整套特效重载，解释这次游戏内特效消失，也解释 F9 无法恢复。Blender 场景本身没有丢失特效：实时工程 `G:/zmd/lx/洛茜新.blend` 的花瓣根与发射器均启用，挂载 Mesh/骨骼有效，frame 88 有 38 个预览粒子；没有修改或保存工程。

Helper 根对象现在直接通过已验证的继承 API `Component.get_gameObject()` 取得，不再绑定这个不兼容的 `GetTransform` 返回签名；helper-only NPC 回归仍经过生产 `HelperReady` 入口。effects-debug 测试通过 1401 项生命周期检查，包含 world/NPC/UI 分离、重复 helper、LOD 与回收。v35 测量 DLL 构建、隔离加载和生产 DLL 校验通过。

15:48:31 确认游戏退出后部署 DLL-only 修复并备份旧 DLL；安装前哈希必须为上次已知版本才允许覆盖。安装 DLL SHA256 `3838d2207fe61d38e1a340e9f9ef86fa44ef0381f4e89b5a721d089c8d7a6aff`；336 个受检 Mod、配置、协议和代理文件保持。部署记录与回退文件在 `bin/effect-recovery-20261021/helper-signature-fix/deployment.json` 与相邻 `backup-20261021-154829-630698/plugin/eff.dll`。部署脚本为 `bin/effect-recovery-20261021/helper-signature-fix/deploy.py`。画面尚未验收；需启动游戏让新 DLL 加载，再观察 UI 与世界特效并按 F9 生成新日志。

## 16:47 编译器与 F10 owner 扫描跟进

Blender 当前打开 `G:/zmd/lx/洛茜新.blend`，场景为 dirty 状态，根对象 `P_fxmap_common_map02_pink_flower_2501` 的“覆盖全部 LOD”已开启；工程指向 `bin/petal-taa-capture-20261021/ShaderAudit/bin/Release/net9.0-windows/ShaderAudit.exe`。该专用 ShaderAudit 项目的 csproj 漏编译现有 `EiemEffectLodPolicy.cs`，所以 Blender 调用的旧程序集并不具备源码中已有的全 LOD 粒子策略。现已把该源码加入项目、在隔离目录编译并先备份旧程序集，再更新 Blender 实际指向的 `ShaderAudit.dll/.pdb`。新程序集与隔离编译结果 SHA256 一致；旧程序集备份位于 `bin/petal-taa-capture-20261021/backup-compiler-20261021-1639`。

从当前 Blender 内存场景导出真实作者编辑请求进行离线构建，未保存 `.blend`。构建 receipt 为 `lodCoverage=all`；AnimeStudio 验证成功。重新解析 runtime Bundle 后，保留的 `leaves_01` 粒子 `cullingMode=3`。第二个仍出现在原始 SerializedFile 的粒子是 `leaves_02`，本次编辑请求已删除该发射器，因此它的 `cullingMode=2` 是未引用残留对象，不是活动发射器。新构建没有写入 `mods/lx`，也没有执行 F9；用户需用 Blender 再导出并按 F9，才能让此修正进入游戏。

同一轮将 15:56 实测的 F10 卡顿归因于 ownerRevision 触发全体重扫和周期性枚举所有未绑定 owner。改为 helper 单根增量对账、全量 F9/控制图变化同帧合并、只对挂点证明失败的 owner 周期重试。诊断回归先失败后通过，覆盖重复 helper、同帧图/owner 变化、无候选 owner 的零周期遍历和骨架晚重建。测量 v35 DLL 已在 `bin/resource-directory-entry-v35-load-perf/eff.dll` 重建并通过生产 DLL 隔离校验；这是候选构建，未部署。游戏安装的 `plugin/eff.dll` 哈希仍为已部署的 `3838d220...`，安装 Mod 未改写；需在游戏退出后部署候选并重启，实机 F10 A/B 尚未完成。
