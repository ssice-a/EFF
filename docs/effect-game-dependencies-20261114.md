# 特效的游戏资源依赖加载

2026-11-15 更新：Blender 资源选择与冷导出不再查询 AnimeStudio `.eidx`。新增逻辑引用编译成原生引用占位，由 DLL 在实际特效需求时通过当前游戏清单定点绑定。来源 Prefab 可以只填 `assets/.../*.prefab`，不要求本地来源目录、导入对象或根对象名称。详细契约及验证见[冷导出修复](effect-export-performance-20261115.md)。下文旧索引/来源版本描述仅对应当时实现。

## 原因与证据

`square_particles_preview` 已命中武器并建立作者 Empty 挂点，但准备失败，未进入播放：

```text
effect game resource missing:
assets/beyond/arts/effects/battle/character/chr_0035_liino/material/m_fx_liino_kv_139.mat
```

同一次游戏 `Player.log` 的 `ResourceManager.LoadAsync` 报 `Can not find Asset In RuntimeManifest`。本机当前 `.eidx` 中确实存在该材质的逻辑路径、资产名和原生引用。因此，解包索引中存在的 Material 路径，不保证是游戏 RuntimeManifest 中可独立请求的入口。

## 游戏链路与 EFF 接入

游戏通过逻辑 Prefab 路径向 ResourceManager 请求资源，由其清单解析物理 Bundle 及依赖。Prefab 的 ParticleSystemRenderer 通过原生 PPtr 引用 Material/Mesh，Material 再引用 Texture/Shader。PPtr 是序列化对象引用，不是每个模块内的文件夹路径。

EFF 继续使用游戏原生 ParticleSystem、Renderer、材质和播放管理器；作者修改后的 Prefab 是 Mod 自己的原生输入。跨来源引用游戏资源时，导出器从来源目录核对资源所属的 Prefab 及依赖闭包，自动记录逻辑来源：

```ini
game.0.path=assets/beyond/arts/effects/battle/character/chr_0035_liino/material/m_fx_liino_kv_139.mat
game.0.type=Material
game.0.asset=M_fx_liino_kv_139
game.0.source_prefab=assets/beyond/dynamicassets/gameplay/effects/prefabs/p_fxbat_liino_attack_05_star_hit_small.prefab
game.0.source_root=P_fxbat_liino_attack_05_star_hit_small
```

这些字段由 Blender/AnimeStudio 生成，用户无需手写。运行时沿现有游戏 LoadAsync 请求来源 Prefab，保留其加载句柄，再加载作者特效。来源 Prefab 不克隆、不激活、不播放，其材质、纹理、Mesh 由游戏依赖管理器提供。一个特效代际内，同一来源 Prefab 的多项引用只请求并释放一次；旧代有实例使用时保留资源。

无来源声明的旧包仍按原有 path/type/asset 读取。任意手填路径是否能独立加载，仍取决于游戏清单；不能把一个未注册的 `.mat` 路径强行当成入口，也不能猜测来源或静默复制整个来源特效。

编译器通过 `EFF_GAME_RESOURCES/2` 将可选来源字段交给导出器，读取端兼容 `/1`。正式 Mod 装配使用 INI。编译缓存同时追踪便携 AnimeStudio 的 `bin/*.dll` 与来源描述，避免只更新实现 DLL 时错误复用旧构建。

## 游戏更新边界

对外声明和 ResourceManager 请求使用逻辑路径，不硬编码物理 `.ab` 文件名。新增逻辑引用的 CAB/PathID 由运行时当前清单与指定容器提供。已导入资源的历史直接 PPtr 仍来自作者快照，尚未全部迁移为运行时引用，不能宣称所有旧包会自动适配游戏更新。

## 2026-11-14 验证与本机部署

- 新回归先复现独立 `.mat` 加载错误，再验证逻辑来源 Prefab、共享句柄、单次释放和不实例化来源。
- 真实测试 Mod 经生产 Catalog 读取通过；验证来源字段完整性、来源改变触发代际变化，以及旧无来源字段的兼容性。入口 `tools/diagnostics/build_effect_game_dependencies_smoke.cmd`，执行 `bin/effect-game-dependencies/smoke.exe <Mod目录>`。
- 当前真实两系统请求经已安装 AnimeStudio 构建，Python 读取 `/2` 并写入 INI 后往返通过；来源特效的外部材质未加入作者输出 Bundle。
- 原生 `effect_debug_smoke` 的完整默认回归已通过：1401 项生命周期检查、只读粒子观察、材质动画隔离、按需骨骼挂载和多 owner/LOD 挂载。早期夹具的 `invalid map<K, T> key` 已归因为测试 Renderer 缺少父级且声明路径与实际路径不一致；补全真实层级并保持精确 Renderer 匹配后，独立 `--attachment-only` 和完整默认回归均通过。
- 正式 DLL 已部署到游戏 `plugin/eff.dll`，生产检查通过；回执 `bin/local-deployments/20261114-021127-701/Dll.json`。
- AnimeStudio 已部署到固定本机位置；回执 `bin/local-deployments/20261114-021127-685/AnimeStudio.json`。
- 测试 Mod 已通过正式 Format 3 writer 和事务发布覆盖 `plugin/mods/square_particles_preview`，仍为柔光、星芒两个系统；作者快照、武器目标与 Empty 挂点保持一致。备份和回执在 `diagnostics/basic-particles-preview-20261114/`。
- 用户退出 Blender 后已完成上一包本机事务安装并核对 63 个文件；回执 `bin/local-deployments/20261114-021254-752/Blender.json`。随后新增的跨 LOD 修复按下面单独验收、部署，不以此前回执代替。

## 特效挂点覆盖目标网格的所有已知 LOD

用户所说的 LOD 是**目标网格各级 LOD 上的特效挂点**，不只是作者模型的 Replace 目标。旧开关只把 `lodCoverage=all` 交给特效编译器，取消来源特效自身的距离/质量降级；运行时仍只匹配 LOD0 Renderer。另一种仅扩大匹配但继续挂到 LOD0 的方案也不成立：游戏关闭 LOD0 GameObject 时，子特效同样会被隐藏。

修复后的导出器从目标 Prefab 来源清单枚举同一 Mesh/Renderer 家族的确切 LOD1～N，生成 `lod.N.renderer/path/asset/bones`，不对整个场景做同名或通配匹配，也不把相邻 VFX 网格纳入。骨骼挂点继续验证各 LOD 的骨骼调色板并共享实际骨骼。

不指定骨骼时，导出 `lod_anchor` 公共祖先及其到所选 Renderer 的完整局部 TRS 链。对这把武器，公共祖先是 `Mesh_all`；必须保留 Renderer 的约 -90° X 旋转，不能简单移动父级而丢掉基准。链式保存也避免带旋转的非均匀缩放被压成一个 TRS 后丢失信息。公共祖先为 Prefab 根时，INI 写 `lod_anchor=.`。

运行时在公共祖先下创建由 EFF 管理的稳定基准链，作者 Empty 层级位于其下，AnimationClip 的采样根与节点路径保持不变。同一 owner/挂点的所有 LOD 共用一个实例，只合并可见性和图层来源；世界、UI、独立骨架实例仍分别持有状态。只要任一目标 LOD 可见就继续播放；全部隐藏时停止，重新可见时恢复。第一次只加载远 LOD、后补 LOD0 也不依赖 LOD0 存在。旧实例退休期间继续固定基准链与作者层级，最后一个消费者退出才释放。

Blender 的“覆盖全部 LOD”同时负责目标网格挂点覆盖和取消来源特效降级；关闭后只跟随所选 Renderer。旧独立特效作者包若没有跨 LOD 来源信息，重新勾选时会提示选择来源 Mesh 后导出，避免把未知范围默认为“已覆盖”。上述 INI 由导出器生成，不要求作者手写。

验证：

- Python 导出/导回覆盖精确四级 LOD、排除同名 VFX 旁支、公共根挂点、骨骼挂点保留骨架、TRS 和无效四元数拒绝。
- `effect_debug_smoke.exe --lod-attachment-check` 先在旧分支挂点上复现失败，再验证仅 LOD3 初次命中、补齐四 LOD、关掉 LOD0 GameObject、单实例连续播放、作者 Empty/原生 Clip 根与统一时钟、全部隐藏/恢复、基准变换热重载及 owner 释放。
- `build_author_transform_runtime_smoke.cmd` 的共享层级、原生采样、暂停/停止、资源代际与最终退休通过。
- 真实两粒子测试 Mod 已通过正式 writer/事务发布更新 `plugin/mods/square_particles_preview`，生产 Catalog 校验 LOD0～3 和 -90° 来源旋转通过。原生特效载荷、来源 ZIP、粒子参数、作者编辑和 Empty 位置保持原值，只增加 LOD 装配。备份 `diagnostics/basic-particles-preview-20261114/before-lod-mount/`，回执同目录 `lod-mod-deployment.json`。

本机新部署已完成：

- 正式生产 DLL 和协议已覆盖固定游戏 `plugin` 目录，回执 `bin/local-deployments/20261114-025013-080/Dll.json`。`eff.dll` SHA256 为 `3df3ffa247840986b9e30d2f1e15324f14f678f7d6e08561b45b333029756f4f`，与构建及安装文件一致；生产检查确认不含 RenderDoc/临时探针。
- Blender 已安装到 5.2 固定插件目录，回执 `bin/local-deployments/20261114-025113-947/Blender.json`。64 个清单文件摘要全部通过，新增挂点模块、编辑器、导回模块和 nativepack 与本机源码逐项一致。
- 共享作者工具已部署为当前选择的 `E:/EIEM_Workspace/tools/ccc7aa22801abbd3345c733370d6dc276e88c9ea34dc41113e8b6213522ac8ca`，回执同部署目录 `shared-author/SharedAuthor.json`。
- 正式 Blender 作者/更新回归共运行 197 项，1 项跳过，其余通过。四仓库均为 `main` 且本地不落后于 `origin/main`；本轮未提交、推送或发布。

以上是本机构建、安装及离线验证，不替代下述实机验收。

## 实机反馈：粒子在模拟，仍不可见

2026-11-14 的 PID 37124 已运行上述 DLL：02:57:24.298 建立武器绑定，精确匹配四级目标 Renderer；02:58:40.334 游戏来源 Prefab 租约完成，随后 READY/PLAY。播放后一秒柔光/星芒计数分别为 16/3，三秒后为 29/3，节点激活、缩放为 1。用户明确反馈画面仍没有粒子。因此已验证来源加载、实例挂载和模拟，**视觉问题尚未解决**，不能将 READY/PLAY 或非零计数写成显示成功。

`prepareMs=76113` 取自创建绑定代际至 READY 的墙钟时间。运行时仅在目标 Renderer 可用且激活时推进准备，计时可能包含武器隐藏、异步请求等待等；用户也说明并非进游戏后立即使用武器。该数值不能直接解释为 76 秒连续加载、CPU 阻塞或实际资源耗时。需区分绑定时间、首次请求时间及请求完成时间后才能下性能结论。

本次日志已保存在 `diagnostics/basic-particles-preview-20261114/visual-failure-pid37124/`。只读解析实际编译载荷确认两个新增 ParticleSystemRenderer 已启用，EffectSetting 中存在对应的发射器/Renderer 登记，材质 PPtr 已写入；这些静态信息仍不能证明游戏已正确解析 Shader/Texture 或相机已绘制。下一次有界 `[EFFECT-RENDER]` 快照补充实际 Renderer.enabled/forceRenderingOff、世界位置、包围盒、图层、主相机掩码及真实材质/Shader/Texture 状态，不修改任何渲染参数。主相机掩码仅用于定位，不能代表游戏所有相机；`isVisible` 也不等于目标游戏画面可见。

只读渲染快照已通过完整 `effect_debug_smoke`、独立 LOD 挂点与游戏来源租约回归：能报告模拟仍运行时的 Renderer 隐藏/相机图层不匹配、实际材质/Shader，并验证观察失败不会阻止播放，定时边界外没有持续查询。新生产 DLL 已经固定本机部署，回执 `bin/local-deployments/20261114-031234-634/Dll.json`；安装 SHA256 为 `3a647fefb9a686d697635867618b1556c1684b85b8ac90905b6fca517e275120`。这次仅补充可见性证据能力，尚未声称粒子不可见的根因已修复；待新进程复测。

## 新进程复测与柔光尺寸对照

PID 33388 的实测记录显示，两系统均已解析真实 Material、Shader、Texture；Renderer 启用、非强制隐藏，世界场景图层 11 被主相机包含，挂点、包围盒及单位缩放正常。柔光使用 `M_fxmap_map02_lv008_lightspot_2501` / `HGRP/Effect/VFXBaseV2` / `T_fx_star_401_D`；星芒使用 `M_fx_liino_kv_139` / `HGRP/Effect/VFXDitherAlpha` / `T_fx_star_105_2_D`。这排除了上述直接缺失情况，不等于证明所有透明度/顶点数据或渲染通路正常。用户随后明确反馈“星芒有，粒子好像没有”，因此星芒已经取得视觉确认，柔光仍待确认。

柔光发射率 16/秒、寿命 1.8 秒，运行第 3 秒实际计数 29；星芒实际仅约 3～4 个。数量本身不能解释柔光完全难以看见。原柔光初始尺寸范围为 0.008～0.016，按当前世界单位对应约 0.8～1.6 厘米，生命期尺寸曲线还会缩小；星芒为 0.06～0.12。当前优先验证尺寸，其次再检查分布/遮挡及材质透明度，不把尺寸猜测写成已定位根因。

已通过正式 AnimeStudio 编译器、Format 3 writer 和事务发布覆盖 `plugin/mods/square_particles_preview`，仅将柔光 `InitialModule.startSize.minScalar/scalar` 放大四倍为 0.032～0.064；数量、寿命、位置、材质、星芒及四级 LOD 挂载均保留。编译后逐对象解码对比确认只有这两个尺寸值变化；作者快照的编辑增量同步更新，来源归档及干净工作区基线保持一致。回执与备份位于 `diagnostics/basic-particles-preview-20261114/soft-size-x4/deployment.json` 和同目录 `before-mod/`；本轮旧日志保存为 `before-size-eff-log.txt`。该目录在 mods 之外，不会导致两个测试 Mod 同时加载。尚待 F10 热重载及柔光视觉反馈，未宣称修复完成。

### 截图反馈：柔光已可见，测试挂点坐标轴不一致

尺寸对照后用户截图显示柔光沿腿部延伸而非沿刀身，因此已经取得柔光可见的视觉证据，但装配方向错误。读取实际 `langzhifei` 中 `.012` Mesh 作者快照确认其坐标标记为 `unity-y-up-left-handed`，原生顶点的长轴为局部 Z，范围约 0.254534～1.020144；本次柔光却沿特效局部 Y 发射和移动。不能将原生 Mesh 顶点的模型轴约定与特效 Transform 的 Y-up 约定当成同一编辑空间。

本次测试挂点制作时遗漏了这两种空间的基转换。Mesh 编辑沿用 X 镜像 `C=diag(-1,1,1)`，特效编辑使用 `B: (x,y,z) -> (-x,-z,y)`，故在该测试 Mod 的作者挂点根应用一次 `C^-1 B: (x,y,z) -> (x,-z,y)`；同步转换根节点的位置与旋转，不修改粒子内部的发射形状、速度、贴图或 Shader，也不叠加修改 LOD 来源旋转。作者根节点的位置由 `(0,0.366,0)` 改为 `(0,0,0.366)`，旋转为 X 正 90°。这既将柔光轨迹转到原生 Mesh Z 轴，也将星芒发射范围转到相同刀身方向。这里修复的是已生成测试挂点的装配数据，未以该个案推断或改写全部导出器/运行时的既有坐标语义。

修正后数学检查：发射中心 Z 为 1.010144，速度为 `(0,0,-0.414227)`，寿命 1.8 秒后的终点 Z 为 0.264534，分别位于 `.012` 刀身两端内约 1 厘米。通过正式 writer 与事务发布覆盖同一个测试 Mod，回执 `diagnostics/basic-particles-preview-20261114/mesh-axis-alignment/deployment.json`，备份在同目录 `before-mod/`。作者快照与 INI 一致，原生 Bundle 逐字节不变，LOD0～3 装配不变。当前仍保留四倍尺寸对照；方向修正尚待 F10 和游戏画面确认。
