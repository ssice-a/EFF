# 花瓣 TAA 抓帧分析与响应式透明对照

**最新状态（03:51）**：已利用新帧 55162 定位第二次材质对照的全零输出，配置真实遮罩采样，并连同用户要求的地面碰撞部署到 lx。当前 Blender 作者状态已同步；以下两次对照段落为历史。完整证据、部署与实机边界见[材质与碰撞配置](petal-material-collision-configuration-20261021.md)。

2026-10-21，实际帧：`diagnostics/petal-taa-capture-20261021/petals_frame2527.rdc`。Vulkan 回放正常，未报告 high 级验证消息。所有离线分析、临时程序、回放替换 Shader 与部署证据在 `bin/petal-taa-capture-20261021/`。未保存或修改 Blender 工程，未启动、停止游戏，未替换生产 DLL。

## 03:14 响应式对照失败，已回退

用户实机反馈响应式透明对照后花瓣消失。已运行带版本哈希保护的 `restore_responsive.py`，恢复此前两个文件，并再次核对均等于部署前 SHA256，见 `responsive-rollback.json`。生产 `eff.dll` 仍为 `9f4129c113a8483fe69e4fb0e87cececd263ee971cad63c20f0cc195651ffbd4`，抓帧代理保持。游戏需 F9 重新载入旧包；磁盘恢复不代表已经确认画面恢复，原 TAA 残影也未解决。

失败会话日志保存在 `responsive-failed-eff_log.txt`。03:12:58 的 READY / PLAY 说明完成资源准备与挂载播放登记，不能证明材质成功绘制；游戏 `Player.log` 为空。目前没有失败对照的新抓帧。以下部署与方案段落保留为失败实验记录，不可再当作待验收修复使用。离线对象解析与编译通过不足以验收 Shader 渲染，下一步须核对真实引用和 Pass，并以渲染证据验证。

## 03:20 用户要求更换原生材质，已部署新对照

改用当前游戏真实光点材质 `M_fxmap_map02_lv008_lightspot_2501` 的完整配方，Shader 仍为 `HGRP/Effect/VFXBaseV2`，不再迁移 Dither 材质的同名参数。保留原材质混合、光照等设置和 `_Responsive=0`；仅把主图、遮罩绑定到作者资源，开启遮罩与双面。材质实际绑定 `flower_D.png` / `flower_M.png`，来源 Material 绑定仍保留其来源贴图。没有调整粒子参数、Mesh、挂载、DLL 或 Blender 工程。

`ShaderAudit verify-native` 已解析两个 Material 的真实外部引用，均可在候选四个 Bundle 中找到预期 Shader 和两张 Texture2D。19 个既有原生对象只有两个 Material 变化；ColorModule / TrailModule 仍关闭。角色包 `--update-effects` 编译通过。03:20:28 已备份部署到 lx，只更新特效资源与 compiled.bin，其余 333 个受检文件保持。证据为 `donor-native/proof.json`、`donor-stage.json`、`donor-compile.log`、`donor-deployment.json`，备份为 `donor-backup-20261021-032028/`。

游戏按 F9 读取新材质，显示和残影尚待实机观察。Blender 作者材质保持原样，重新导出会覆盖临时对照。回退使用 `restore_donor.py`，带版本哈希保护。旧响应式对照的 `restore_responsive.py` 不能用于回退本次材质。

03:21 用户反馈 F9 后仍无效果。日志确认 03:21:10.708 与 03:21:17.697 均收到 F9（vk=120），随后 prepared / committed；03:21:11.326 READY 的原生 key 为 `1497703EF8454758D78E7F834B027BA8796EDEA15D9AA7EFF10F1E3B9E51E276`，与本次 donor 候选精确一致，随后记录挂载播放。磁盘两个文件也与候选 SHA256 一致。记录见 `donor-f9-verification.json`、`donor-f9-eff_log.txt`。已证实快捷键与新包准备/挂载/播放调用成功，显示对照仍失败；日志没有 GPU 绘制结果，不能由 PLAY 推断可见。后续需当前不显示状态的帧，核对材质实际 Draw 或缺失提交。本次没有继续修改材质或 DLL。

## 已确认的绘制与运动数据

- 从 GPU 导出并查看资源 46126 / 46129，分别为作者紫色花瓣主图和花瓣轮廓遮罩。真实 DepthOnly 绘制为 event 3394，HGBuffer 绘制为 event 6077，318 个索引、212 个顶点。
- 抓帧 VS 46368 / PS 46369 与原游戏 `VFXDitherAlpha` Vulkan blob 的 program 276 两个 SPIR-V 逐字节相同，keywords 为 `HG_ENABLE_MV, SRP_INSTANCING_ON`。Depth VS 46364 / PS 46365 与 program 724 相同，另含 `_USE_MASK`。因此这些结论来自实际绘制，不是只看来源材质名称。
- 修正诊断工具的 Vulkan 动态 uniform offset 后，实际 per-draw `unity_MotionVectorsParamsInternal` 为 `(0, 1, 0, 0)`；shader 的 x 分支使用当前位置代替上一帧顶点输入。当前与上一帧对象矩阵均为单位矩阵。
- 导出实际 VS 输出，212 个顶点的当前/上一帧投影 xyz 最大差为 `2.9802322387695312e-08`。移动粒子的自身位移没有进入本次提交的运动数据。这解释了为什么仅把 Renderer `m_MotionVectors` 改成 1 仍不能修复拖尾；它没有让引擎生成每粒子历史位置。
- HGBuffer 的运动输出是资源 34245；event 7791 读取它整理时间重投影输入，event 7809 执行包含 history 采样、邻域裁剪与历史混合的 resolve。实际 RT 与 Shader 均已导出。不能用静态 Mesh UV4 伪造运行中的每粒子上一帧位置。

可重复核验入口：`verify_capture_evidence.py`，结果为 `capture-evidence.json`。`scan/`、`petal-draw/event6077/`、`post/event7791/` 和 `post/event7809/` 保存原始证据。原 CLI 的 draws 限制前 1000 条，不能据此判断抓帧缺失；完整动作由扩展的离线 `renderdoc-cli` 扫描。

## 对照方案与验证边界

没有改动原游戏公共 Shader，也没有生成假运动流。`VFXDitherAlpha` 的 `_AntiFlicker=0` 本来就是 Open；回放修改这一标记不能补齐粒子历史位置，未将它作为修复部署。

从当前安装游戏读取原生 `HGRP/Effect/VFXBaseV2`，确认其真实声明支持 `_Responsive = ResponsiveTransparent (1)`，走 ForwardOnly 透明路径。以原游戏 lightspot 材质的真实字段布局为模板，将同名的作者材质数值、颜色及贴图引用迁移到该 Shader；选择 Alpha 混合、关闭深度写入，并使用响应式透明标记。这个方案避免花瓣继续走缺少粒子历史位移的抖动不透明路径，仍需实机比较画面。

该对照是隔离材质包：私有 CAB 命名、独立原生 Shader 依赖，没有替换游戏共享 Shader。保留花瓣两个 Material 对象（来源及作者克隆），原贴图、网格、Renderer、粒子模块、数量、寿命、速度、骨骼挂载和控制元数据保持。ColorModule / TrailModule 仍关闭，删除的第二发射器没有恢复。

离线重新解析 19 个原生资源，仅两个 Material 对象发生语义变化；另外加入原生 BaseV2 Shader 依赖包。角色编译器 `--update-effects` 验证通过，角色原生输入未重新构建。证据为 `responsive-native/proof.json`、`responsive-stage.json` 和 `responsive-compile.log`。

## 部署与操作

03:06:58 已备份并部署到 `D:/Hypergryph Launcher/games/Endfield Game/plugin/mods/lx/`，仅更新：

- `resources/effectattachment_428fc265c91642bab14c33719e961d7f.bin`
- `compiled.bin`

其余 333 个受检查文件保持，包括生产 DLL、抓帧代理、协议、角色资源与其它 Mod。使用共享来源锁和 Mod 发布锁；部署后再次核对 SHA256。详见 `responsive-deployment.json`，备份在 `responsive-backup-20261021-030658/`。

启动游戏后检查大世界静止镜头下的花瓣；已经运行则按 F9 读取这次材质对照，再检查 UI 预览。若仍有拖尾，F12 再抓同场景，可对照确认花瓣是否进入透明 pass、Responsive 标记是否到达 TAA，以及剩余拖尾的位置。

**此对照实机失败，已回退，未修复残影。** Blender 中仍为原作者材质；没有把失败对照接入长期导出功能。

回退本次材质对照可运行 `bin/petal-taa-capture-20261021/restore_responsive.py`；脚本先核对当前文件仍是本次测试版本，拒绝覆盖后续作者修改。抓帧代理的恢复是另一操作，仍见 [抓帧部署](petal-taa-capture-20261021.md)，本次没有恢复或再次替换它。
