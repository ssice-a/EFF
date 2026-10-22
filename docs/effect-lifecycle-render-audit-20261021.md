# 花瓣实例、导出与大世界残影检查

**最新材质状态（2026-10-21 03:51）**：作者材质与 lx 原生包已配置为 VFXBaseV2 的真实 SampleTex0 Mask 采样、正常 Alpha 混合及关闭近镜头渐隐，并开启场景静态碰撞。当前情况见[材质与碰撞配置](petal-material-collision-configuration-20261021.md)；下文 Dither 材质数据保留为历史原始审计，不代表当前包。实机显示和原 TAA 残影尚待确认。

当前工程 `G:/zmd/lx/洛茜新.blend`，特效 `428fc265c91642bab14c33719e961d7f`。
证据保存在 `bin/effect-runtime-export-20261021/`。未保存用户工程，未启动或停止游戏。

## 已修复并验证

- `MeshS_actor_wulfa_cloth_01_lod0_1.002` 有完整权重及原骨架标记，缺失 Armature 修改器。原导出器写出空 skin 和非空骨骼表，触发 `author skin vertex count`。现在直接报出对象名和缺失关联；当前会话按相同 import ID 和骨架资源段唯一匹配原骨架并恢复修改器。14 个网格的原始选择在隔离目录完整导出成功，选择、active、帧保持。
- 游戏独立销毁或禁用特效 clone 后，附件现在重新创建该实例。资源 Generation 仍可共享；实例和骨架绑定独立。
- 同一 root 下按实际挂载 Transform 分组，多个骨架各有实例；同骨架的 LOD renderer 合并。F9 未改动保持播放，内容变更分别更新世界/NPC/UI。
- 骨架回收导致绑定丢失后，每秒仅对已记录且仍由游戏拥有的无绑定模型重试。不搜索整个场景。
- 未修改的角色包特效先做完整内容校验；确认作者文件无变化时跳过协议进程及编译器。不依据 mtime 信任资源。当前现场隔离测量从约 1.77 秒降至约 0.88 秒。
- 特效导出增加整理、构建、发布的实际阶段计时。独立包回归：改粒子参数约 1.27 秒，改网格约 1.62 秒；角色包回归：改挂载约 1.71 秒。这些是当前机器的离线测量，不是所有工程的速度保证。
- 当前 14 网格首次全量导出实测 14.14 秒，其中整理 6.76 秒、编译 7.06 秒、发布 0.32 秒。此次已修复导出失败，但首次全量耗时仍有优化空间，不能将纯特效的加速表述成角色全量已提速。

## 实际发布资源检查

直接读取 `mods/lx/resources/effectattachment_428fc265c91642bab14c33719e961d7f.bin` 并解开原生归档，没有以 Blender 视口近似代替检查。

| 项目 | 实际值 |
| --- | --- |
| Mesh | 原始外部 `Plane001`，PathID `1892389911223263920`，未被重写 |
| 几何 | 4 顶点、2 三角形；坐标有限，法线/切线长度 1，切线符号 -1，UV0 和 AABB 正常 |
| 材质 | `M_fxmap_common_camphor_leaves_04` 的独立克隆，花瓣贴图 |
| Shader | `HGRP/Effect/VFXDitherAlpha` |
| Queue / RenderType | `2450` / `Opaque` |
| Keywords | `_USE_MASK` |
| Pass 禁用 | `ForwardOnly`；该 Shader 本身只有 HGBuffer 和 DepthOnly |
| TrailModule | 关闭 |
| ColorModule | 关闭，保留用户要求 |
| Renderer | Mesh 模式；部署前 `m_MotionVectors=2`（强制无运动），本次单项对照改为 `1`（物体运动） |
| 顶点流 | 非自定义，序列 `[Position, Normal, Color, UV]` |

实际 Shader Pass 状态来自反编译 Shader 的 `m_State`，而非猜测材质浮点字段：

- **DepthOnly**：Blend One/Zero、ZWrite On、ZTest LEqual、Cull 取 `_CullMode`；进行遮罩/抖动 discard，Stencil 取材质的 Ref/Mask。
- **HGBuffer**：Blend One/Zero、ZWrite Off、ZTest Equal、Cull 取 `_CullMode`；依赖前面的深度预通道。
- 因此材质里 `_SrcBlend=1`、`_DstBlend=10`、`_ZWrite=0` 并不代表这个 Shader 走普通透明 Alpha 混合；这两个 Pass 没有引用这些混合/深度属性。

解出对应 `HG_ENABLE_MV` 程序，HGBuffer 顶点程序读取 `TEXCOORD4.xyz` 作为上一帧位置输入之一；片元程序由当前/上一帧非抖动投影位置差输出运动数据。Renderer 强制无运动与此时间重投影流程存在明显风险。普通静态 Mesh 的 UV4 不能充当每个粒子实时的上一帧位置，不能伪造一套静态 UV 来修复。

用户确认使用**原生 TAA / TAAU**，镜头静止时仍有粒子拖尾。运动矢量字段现在有中文下拉；本次只将 `leaves_01` 的 `m_MotionVectors` 从 2 改为 1，作者和游戏包保持一致，没有保存 `.blend`。

直接重新解包对照产物，按 class ID 与 PathID 对齐、归一化归档 CAB 名后，19 个原生对象仅此字段发生语义变化。材质、粒子模块、原网格引用、挂载与控制元数据保持，ColorModule 和 TrailModule 仍关闭。证据为 `motion-verification.json`。

目前**没有实机 GPU 帧证据**证明这个粒子的有效运动 RT、引擎最终常量和 TAA 历史权重，因此这次是单项实机对照，不能断言改为 PerObject 就一定修复。没有替换 Shader 或重写 UV。若仍拖尾，下一步应对同一静止镜头捕获深度、HGBuffer 运动输出和 TAA 输入，确认引擎是否提供每粒子历史位置；普通静态 UV 无法补上这个输入。

## 部署与当前会话

2026-10-21 02:04，确认游戏未运行，生产 DLL 检查通过后，已备份并替换：

- `plugin/eff.dll`：包含外部销毁/禁用恢复、同 root 多骨架实例分组和晚骨架重建恢复。
- `plugin/mods/lx/compiled.bin`。
- `plugin/mods/lx/resources/effectattachment_428fc265c91642bab14c33719e961d7f.bin`：仅上述运动矢量对照。

其余 332 个检查文件内容保持，包括角色资源、其他 Mod、eff.ini、代理 DLL 和 package-protocol.json。备份为 `bin/effect-runtime-export-20261021/backup-20261021-020418-540446/`，部署清单与 SHA256 见同目录上级的 `deployment.json`。没有启动、停止游戏或保存 Blender 工程。

本次真实花瓣参数变更导出耗时 2.26 秒：整理 0.27、构建 0.63、发布 1.36 秒。当前角色包纯特效未改导出约 0.88 秒。Blender 当前会话已恢复缺失的 Armature，并保留上述运动矢量对照值；需由用户保存工程才能保留到下一次打开。

下次正常启动游戏加载新 DLL 后，分别检查大世界静止镜头移动粒子、UI 预览、F9、切场景以及同角色多实例。之后只更新特效使用 F9；F9 不会替换已加载 DLL。离线回归不能代替这些实机检查。

## 验证入口

- `tools/diagnostics/build_effect_debug_smoke.cmd` 与 `bin/effects-debug-tests/effect_debug_smoke.exe`：外部销毁/禁用、F9、多 owner、多骨架、LOD、显隐及骨架晚重建。
- `tools/Blender/test_eiem_skin_export.py`：缺失 Armature 不生成错误蒙皮及原有骨骼/权重回归。
- `tools/Blender/test_eiem_native_publish.py`、`test_eiem_native_export_core.py`：发布与缓存回归。
- `tools/Blender/test_eiem_effect_fast_export_blender.py`：改参数/挂载/网格、损坏缓存及保留时间戳的来源损坏。
- `tools/Blender/test_eiem_effect_character_update_blender.py`：追加角色包、无变化不启动编译、多个特效、其他资源保持、发布中途失败回滚。

离线测试与原生数据审计不能替代大世界、UI、切场景的实机画面验收。

2026-10-21 02:13：用户实机反馈运动矢量单项对照后仍有残影。随后已获得实际 Vulkan 帧，确认花瓣 212 个顶点的当前/上一帧投影基本相同，缺少粒子自身运动。03:06 部署原生 VFXBaseV2 响应式透明材质对照后，用户反馈特效消失；对照失败，已恢复部署前的两个 Mod 文件并复核 SHA256，游戏需 F9 载入后确认显示。粒子参数、Blender 作者材质、生产 DLL 和抓帧代理保持，原残影未解决。完整证据及回退清单见 [花瓣 TAA 帧分析](petal-taa-frame-analysis-20261021.md)。抓帧部署仍见 [抓帧部署](petal-taa-capture-20261021.md)。
