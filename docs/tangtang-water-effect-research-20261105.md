# Tangtang 水特效研究记录

日期：2026-11-05

研究输入是 `E:/EIEM_Workspace/assets/beyond/dynamicassets/gameplay/actors/postmodels/characters/chr_0027_tangtang_postmodel.eff` 关联的五份 `.efffx` 来源。原始来源保存在 `E:/EIEM_Workspace/effects/tangtang-water-20261105/sources`，由 `source-integrity.json` 记录摘要。研究过程没有修改游戏安装、Mod 或原始来源。

## 可打开工程

- `tangtang-water-preview.blend`：`E:/EIEM_Workspace/effects/tangtang-water-20261105/`
  - 只显示 start_1 的原始 MeshRenderer，保留 3 个材质绘制层。
  - 场景为 60 fps、1-122 帧；第 61 帧显示原始 AnimationClip 和 VAT 帧 31。
  - `water-preview-report.json` 包含材质、贴图路径、原生格式、动画绑定和限制。
- `tangtang-water-trails-preview.blend`：同一目录。
  - 使用 live_loop 的真实 TrailModule，30 fps；第 31 帧可见拖尾几何和独立材质槽。
  - `trail-preview-report.json` 保留 `cab-5e357...:4755763333884177792$.m_Mesh` 未解析引用告警。

## 已确认

- `M_fx_tangtang_zrx_066` 使用 `HGRP/Effect/VFXWaterRefract`，启用 `_VAT_SOFTBODY`、法线、溶解和遮罩；`_UseRefract=0`，所以不能仅凭 Shader 名称断言可见折射。
- VAT 是 `RGBAHalf` 浮点位置/旋转纹理，网格 UV1 是查找坐标；位置纹理采样结果是相对原顶点的偏移，不能用 PNG8 位预览代替数值输入。
- start_1 的 Clip 轨道实际存放在 StreamedClip/ConstantClip；`_displayFrame` 和溶解 UV 的轨道已能采样。正向 61→101→61 跳帧可重复，作者导出基线不变。
- Unity 的额外材质槽已按独立预览对象保留；循环样本的 Trail 使用槽 1 材质。

## 当前边界

- 预览只对已核对契约的 `VFXWaterRefract` / `VFXBaseV2` Softbody VAT 生效。粒子自定义顶点流 VAT、刚体/流体 VAT、SkinnedMeshRenderer 的骨骼重绑定尚未实现。
- 独立 AnimationClip 不等于完整 Animator 技能状态机；根 Transform、Euler/PPtr、未解码 ACL、游戏脚本和事件没有被猜测回放。
- Trail/Ribbon 是有限时间采样重建，不能替代游戏逐帧缓存；屏幕 UV、深度、折射、完整 billboard/custom stream 和像素 Shader 仍是 Blender 近似。
- `tangtang-water-materials.blend` 仍是材质对照库，不是已经验证的水衣服。将水材质套到衣服时必须保留衣服原有蒙皮、骨骼、bindpose 和权重；不要把水花 VAT 直接当成衣服骨骼动画。

## 回归证据

- Python 离线测试：31 项通过。
- Blender 真实 start_1：5040 顶点、8272 面、UV1、3 材质层、VAT 反向跳帧、保存重开通过。
- Blender 真实 live_loop：Trail 几何、槽 1 材质、反向跳帧、无作者数据变化通过。
- AnimeStudio EffectsTests：43 项断言通过，确认旧 `uv` 别名与 UV1、顶点色、切线、子网格元数据同时导出。
- 后台尝试渲染时本机 `nvoglv64.dll` 在 Blender 呈现阶段发生访问冲突；`.blend` 保存和数据级验证成功，不能把该驱动崩溃当作特效资源失败。
