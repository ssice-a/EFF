# 花瓣遮罩与地面碰撞配置

后续实机反馈：**花瓣仍穿透地面，碰撞配置未达到用户目标**。已重新解析最新导出，确认主发射器的World/3D碰撞开关和参数确实写入；不得将下文文件验证通过解释为游戏地面碰撞已生效。原生CPU/GPU对照及当前支持边界见[地面碰撞调查](effect-ground-collision-research-20261021.md)。

2026-10-21 03:51，按用户要求配置当前 `G:/zmd/lx/洛茜新.blend` 会话中的花瓣，发布到 `D:/Hypergryph Launcher/games/Endfield Game/plugin/mods/lx`。没有保存 blend、重启游戏/Blender 或更换生产 DLL。

## 消失原因的实际证据

新帧 `diagnostics/petal-taa-capture-20261021/petals_frame55162.rdc` 中，花瓣真实绘制为 event 10882（312 索引、208 顶点）。主图为作者原图，顶点 Alpha=1、主图 Alpha=1；PixelHistory 的两个覆盖三角形通过深度/模板，未 discard，但 RGBA 输出全零。

实际 PS 126175 与原游戏 VFXBaseV2 program 2436/2498 逐字节相同，关键字只有 `HG_ENABLE_MV, SRP_INSTANCING_ON, _USE_FOG`。其 material constant offset 236 为 1，导致初始遮罩乘数 `1-1=0`；没有任何作者遮罩采样。离线回放只绕过该乘数后花瓣可见，证据在 `bin/petal-taa-capture-20261021/donor-mask-probe/`。该回放只用于定位，并未部署修改后的 SPIR-V。

VFXBaseV2 的真实 keyword 表没有旧 Dither 的 `_USE_MASK`，而是 `_SAMPLE_TEX0` 等采样槽。单独打开 `_UseMask` 及无效旧关键字不足以启用该槽。这解释此次不显示，不能据此宣布历史 TAA 拖尾也已解决。

## 实际材质配置

- Shader：原游戏 `HGRP/Effect/VFXBaseV2`，完整原生 Shader 快照来自 `Bundles/Windows/initial/e9dd0a379806dc341a764f33.ab`，未改游戏公共 Shader。
- 主图：`G:/zmd/lx/flower_D.png`。
- 实际遮罩：`G:/zmd/lx/flower_M.png`，绑定 `_SampleTex0`，线性数据，R 作 Alpha。保留旧 `_MaskTex` 绑定供来源预览/兼容；**今后游戏实际使用的槽是 `_SampleTex0`**。
- `_UseSampleTex0=1`、`_UseSampleTex0AsAlpha=1`、`_SampleTex0Use=3`（Mask）；Weight3=1，其余采样用途权重为0；关键字 `_SAMPLE_TEX0, _USE_FOG`。
- `_UseMask=1`；UV0、单位旋转、零滚动，不进行噪声/溶解采样。
- `_BlendMode=0`（Alpha），材质原生混合为 One / OneMinusSrcAlpha；双面。关闭 `_UseNearCameraFade`，不再采用 donor 的2～5米近镜头渐隐。`_Responsive=0` 保持。
- 保留当前 Blender 作者的白色 Tint 与强度25；没有把当前作者值恢复成对照包的强度2。

## 实际碰撞配置

目标 ParticleSystem：`cab-27f0888b327e2472e676202b71550605:-2912475041744378721`。只修改其 CollisionModule：

| 字段 | 写入值与含义 |
| --- | --- |
| enabled | true |
| type / collisionMode | 1 / 0，World / 3D |
| quality | 0，高质量 |
| m_Dampen | 常量1，碰撞后消除速度 |
| m_Bounce | 常量0，不反弹 |
| m_EnergyLossOnCollision | 常量0，不扣除剩余寿命 |
| minKillSpeed | 0，落地速度归零不提前消亡 |
| collidesWithDynamic | false，只考虑静态碰撞体 |
| collisionMessages | false，不发送碰撞回调 |

碰撞层遮罩仍为全部层，未猜测游戏的“地面层”编号，因此静态墙面等也可能碰撞。半径倍率1和碰撞体上限256保持。既有模拟空间为 World（moveWithTransform=1），已发射花瓣留在世界。寿命3～5秒、初速度0.15、重力倍率0.03、数量、Noise、形状、网格及挂载保持用户当前值；ColorModule / TrailModule仍关闭，已删除的第二发射器未恢复。碰撞只对游戏实际存在的场景碰撞体生效；当前 Blender 离线预览未模拟 CollisionModule。

## 作者状态与再次导出

原始解包 `.efffx` 保留。新来源副本为 `E:/EIEM_Workspace/effects/petals-configured-20261021.efffx`，保存完整原生 Shader、材质底稿与生成文件校验。当前根已重绑到该副本，同时保留用户的发射器/曲线/网格及图片路径。材质使用同一 EFF Material 作者属性，eiem_shader 也已实际同步为 VFXBaseV2，并非只改显示文字。

`EiemEffectRuntimeWriter` 增加可选 `runtimeEmbeddedBundles`：只允许包含完整快照、并从根可达的依赖，令新 Shader 随特效归档，不依赖原 Prefab 的旧 Shader lease。普通来源没有此项时行为保持。本机当前根使用包含这些生产构建源文件的 net9 专用编译入口：`bin/petal-taa-capture-20261021/ShaderAudit/bin/Release/net9.0-windows/ShaderAudit.exe --build-eff-effects`；未发布通用的 Shader 切换 UI，也未替换现有 AnimeStudio GUI 程序。该编译器路径随用户保存 blend 后保留；不要手动换回不支持该来源字段的旧编译入口。

## 验证与部署

- 使用当前 Blender 作者值隔离导出到 `bin/petal-taa-capture-20261021/configured-stage/lx`，首次约3.11秒；再次原生重建/导出约1.69秒，生成文件逐字节相同。选择、活动对象与时间轴帧未改变。
- `ShaderAudit verify-native` 已解析两个 Material 的真实 Shader、主图、旧遮罩及 SampleTex0 引用。实际 Renderer 使用的材质引用作者主图和线性遮罩克隆。完整候选含3个原生 Bundle。
- `verify_configured_native.py` 比较当前 Blender 作者和重新解析结果：碰撞字段准确；除碰撞外的粒子数据按原生float宽度一致；其他共用对象保持，移除两个不再使用的法线资源，新增一个线性 SampleTex0 遮罩克隆。
- `test_configured_dependency.py` 通过：不显式保留时构建仅有2包，证明新 Shader 不能依赖默认终端裁剪；未知/非快照依赖明确失败且不发布结果。正常配置明确包含Shader包。
- 共享来源锁、发布锁、版本哈希保护下备份并替换两个文件：`compiled.bin` 与 `resources/effectattachment_428fc265c91642bab14c33719e961d7f.bin`。其余333个受检文件保持，包括生产DLL、代理、协议、其他Mod与角色资源。
- 本次原生 key：`71FB1C2958F67ED64418EB4D2A3F423D8A561CD10FC585A433B3E1E0AB6E9F3F`。
- 部署回执：`bin/petal-taa-capture-20261021/configured-deployment.json`；备份：`configured-backup-20261021-035131/`。作者修改前状态：`configured-author-before.json`；原始解包来源仍在。

游戏需 F9 读取本次包，再观察花瓣显示与静态地面碰撞。尚无本次部署后的游戏画面证据，不能宣称落地效果或原 TAA 残影已验收。
