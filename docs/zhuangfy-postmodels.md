# 庄方宜两个 postmodel 的范围

2026-10-07：使用当前游戏 VFS 和已有 AnimeStudio 索引，读取两个逻辑 Prefab 的实际容器根、层级及 Renderer 引用。只读核对，未导入当前 Blender 场景、未发布庄方宜 Mod；临时组件证据已清理。

逻辑目录为 `assets/beyond/dynamicassets/gameplay/actors/postmodels/characters/`。

| Prefab | Renderer | 不同 Mesh | 层级 Transform | 主要内容 |
|---|---:|---:|---:|---|
| `chr_0030_zhuangfy_postmodel.prefab` | 52 | 44 | 565 | body/face/hair/iris 等 `_01`，cloth_01/02，vfxpart_01/02/03，LOD0–3、阴影代理；布料、袖部 solver、IK、视线相关组件。 |
| `chr_0030_zhuangfy_ult_postmodel.prefab` | 51 | 43 | 443 | body_02/03、face/hair/iris 等 `_02`，tail_02、vfxbody_01/02/03，LOD0–3、阴影代理及较少布料/碰撞组件。 |

精确 Mesh 的 CAB + PathID 交集为 **0**。`ult` 名称和尾部/特效内容对应另一套形态，具体游戏触发时机未做实机验证。普通版有 13 个 BeyondBoneCloth、25 个胶囊碰撞组件；ult 版为 6/5。

当前 Blender 导入器按包创建独立集合、Armature 与材质，图片保留包归属；同名显示项可自动加 `.001`，不依靠全局名称覆盖另一包。可以分别编辑这两个模型。

当前每次原生导出使用一个来源目录。分别选中各自包的对象、指定各自 `.eff` 来源目录并使用不同 Mod 文件夹/ID；不能将两套来源混在一次导出中。两者的 Mesh 目标不重叠。材质作为克隆模板，即使引用同一个游戏材质，不同作者副本也独立绑定；这一规则已在本轮修正并回归。真正重复声明同一个精确 Mesh 目标仍会拒绝，避免含糊的覆盖顺序。

以上确认资源与声明层面的范围，不代表这两个形态在游戏中加载、切换及 F10 生命周期已经实机验收。
