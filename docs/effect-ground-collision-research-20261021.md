# 原生地面碰撞特效调查

用户反馈当前花瓣仍穿地，要求找原游戏中带地面碰撞的特效对照。本轮只读调查，没有修改游戏包、DLL 或 Blender 作者参数。

## 当前花瓣

重新解析用户最新导出的特效资源，确认主花瓣 `-2912475041744378721` 的 CollisionModule 已是 enabled=true、World/3D、高质量、全部碰撞层、阻尼1、反弹0、寿命损失0。`collidesWithDynamic=false`。因此不能再把问题解释成碰撞开关没有写入导出文件。当前另有用户新增的 `-1400851969995594593`，碰撞关闭；保留该选择，没有混同为主花瓣。

这些文件参数不证明游戏运行时能检测地面：地面碰撞体、物理场景和粒子模拟运行状态尚未取得实机证据。普通 World 碰撞为何穿地仍未定位，不能由下述 GPU 范例直接推断游戏不支持普通粒子碰撞。

## 普通粒子调查范围

复用现有资源索引筛出的12,424个特效 Prefab 路径，检查12,420个不同原生 Bundle 中的61,704个 ParticleSystem，读取完整 CollisionModule；无读取错误。先查碎石/落叶等531包，再查剩余11,889包。这不是全部游戏资源普查，可能遗漏其他目录中的效果及运行时启用的模块。

只有三个包中的普通粒子碰撞开启：

| Prefab | 节点 | 原生设置 | 范例局限 |
| --- | --- | --- | --- |
| p_common_bomb_01_range | internal/stone01 | World/3D、高质量、动态碰撞体开启、全部层、半径倍率0.59 | stone01默认 inactive |
| p_fxint_s_cannonboom_01 | pos0/stone | Planes/3D | 平面列表只有空PPtr |
| p_interactive_cannonboom_01 | pos0/stone | Planes/3D | 平面列表只有空PPtr |

没有将这些未验证的设置样本称为游戏内已经正常落地的效果，也没有据此盲目复制整套运动参数。

## 更接近需求的原生枫叶

实际资源：

`assets/beyond/arts/effects/gpu/map/prefab/map02/p_fxgpu_map_map02_lv010_leaf_01_fallenleaves_maple_heap_01.prefab`

并检查同目录 heap_02、tile_02、tile_universal_01_s。它们使用 `HGGPUParticleSystem`（classID 1196445779），不是普通 ParticleSystem（198）。每个发射器分别引用 computeMaterial、renderMaterial 和 mesh；碰撞在计算模拟材质/Shader一侧，渲染材质负责外观。

heap_01的计算材质为 `M_fxgpu_map_map02_lv010_leaf_01_fallenleaves_maple_01_tile_compute_heap_fj_01`，Shader为 `V2/FallenLeaves`。Shader原生属性表明确包含：

| 属性 | 原生描述 / 用途 | 范例值 |
| --- | --- | --- |
| _UseSceneDepth | 使用场景深度碰撞 | 材质未覆盖，Shader默认1 |
| _UseHGTerrain | 使用HGTerrain地表 | heap_01材质未覆盖，Shader默认1；tile_02显式0 |
| _UseGroundHeight | 使用固定高度平面碰撞 | 材质0 |
| _CollisionRadius | 碰撞半径，米 | 0.01 |
| _CollisionThickness | 场景深度碰撞厚度，米 | 0.05 |
| _SceneDepthMaxSurfaceDistance | 场景深度表面最大距离，米；原生说明过小易漏碰撞 | 0.1 |
| _Friction / _StaticFriction | 动/静摩擦 | 0.835 / 0.801 |
| _Restitution | 法向反弹 | 0.2 |
| _RestAlignRate | 躺平角速度上限 | 10 rad/s |
| _RestLinearDamp | 静止线速度阻尼 | 10 /s |
| _EnableFloorProbe | 启用墙角地面探测，多一次碰撞采样 | 1 |
| _SpawnAsRest | 生成后直接进入Rest | 1 |

这是叶堆范例，出生即静止、寿命9999秒，另有风唤醒参数；不能整套照搬成裙摆持续飘落。Shader另有生成时地面高度校正、离地偏移、静止坡度阈值、低速去弹性等独立功能。以上来自真实材质和Shader属性表，未将字段名称当作已反编译的完整模拟算法或本次实机验收。

当前编辑器 `EDITABLE={4,21,114,198,199}`、预览及运行时粒子计数均围绕普通ParticleSystem。尚未接通HGGPUParticleSystem/HGMaterial计算模拟的完整编辑、导出和播放验证。因此只更换当前花瓣的渲染材质，不会迁移这些GPU碰撞功能。后续须分别评估普通粒子的实际物理场景，或完整接入这条GPU粒子路线。

## 证据和解析边界

`bin/effect-npc-ui-collision-20261021/`：

- `collision-findings.json`：统计与Shader相关属性。
- `current-author.json`、`effectattachment-before.bin`、`native-resources.json`：用户当前参数和已发布资源快照。
- `native-collision-samples/summary.json`、`all-native-collision-samples/summary.json`：全部扫描统计与三个命中包的完整粒子/层级/脚本数据。
- `gpu-leaf-samples/`：四个枫叶Prefab的原生数据。
- `gpu-leaf-dependencies/`：计算材质、Shader原始字节/TypeTree、只读属性表。
- `CollisionAudit/`、`GpuAudit/`：可复用只读调查程序。

HGShader完整严格解码报告只消费12368/12680字节；诊断另外保留原始字节/完整schema及旧读取器的partial视图，用于查看名称、属性说明及默认值。没有宣称完整字节往返正确，没有改写该Shader，也没有部署GPU效果。材质与HGGPUParticleSystem的严格解码通过。
