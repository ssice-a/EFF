# 源 Shader 核对记录（旧版完整适配参考）

0.3.0 默认使用轻量 PBR，仅保留基础通道、UV 覆盖和相位/滚动逻辑；下文的钻石、闪片、细节门控及视角计算不再自动执行。源码保留旧版适配供证据/回归检查使用，不是导入选项。

2026-10-15，对 `ch_f_ming_wuzhen_lv_s3_billboard.prefab` 实际引用的 Shader 按 CAB / PathID 读取 TypeTree、程序表、纹理绑定、常量缓冲区，再检查 D3D11 字节码。导出参数不是源码；以下结论来自对应编译程序。诊断转储位于工作区 `diagnostics/naraka-preview-20261015/wuzhen-shader-evidence`，游戏资源不随插件分发。

## AdvancedCloth

已读 `lhmstandardcharacterclothadvanced_{e0de75c0,d47dec16,ea693cd0,bab3fa28,ce653701,ca4bed42}.shader`。这些是专门化程序，ForwardLit fragment blob 4 的关键字表为空，功能已经编译进不同 hash；不能把另一款的浮点开关套过来。材质关键字用于识别对应功能，适配只应用于这六个已读身份。

| 资源 / 通道 | 编译程序中的用途 |
| --- | --- |
| `_AdvIDTex` R | `_VELVET_Str` 绒面；受 `_ShaderFeatureParam1.w` 控制 |
| G | `_AnisoAspect`、`_shiftPower`、区域镜面染色；受 feature.y 控制 |
| B | `_Scale` 透光 / 皮下项；受 feature.z 控制 |
| A | `_IridescenceStr` 虹彩；受 feature.x 控制 |
| `_TillingChannelMask` RGBA | 四个细节/光学层覆盖，既影响表面也影响周期发光 |
| `_DetailTillingArray` | BC7 数组，slice 0/1 为钻石或闪片数据；slice 2/3 为普通细节，RG 法线、B 权重；不是每层的 A 都是透明 |
| `_EmissionMap` R/B | 两层平方后的发光蒙版 |
| G/A | 两层正弦相位；不能作为绿色/Alpha 发光颜色 |
| `_MainTexUV2` A | UV1 第二套底色/MRAV/法线的混合权重；不是表面透明度 |
| `_BumpMapUV2` A | 通过 `_UseUV2DetailMask` 加到四个细节蒙版；RG 为法线 |
| `_SpecGlossMap` RG/B | 金属度、粗糙度、环境遮挡；B 由游戏光照路径消费 |

Vertex blob 0：细节 UV 绕中心按 `DetailMapNProp2.w` 顺时针旋转，然后应用 `DetailTilling_STN`。效果 UV 用 `_EffectUvSelector` 位 0/1/2 选择并相加 UV0/1/2，再执行 `(uv - EffectUVTransform.xy) / zw`。

Fragment blob 4：周期层为 `mask² * pow(saturate((sin(phase*x + time*y)*.5+.5-z)/(1-z)),w)`，乘视角项和原始 HDR 颜色。AdvancedCloth 使用绝对法线点积作流光视角项，几何面方向决定背面强度。细节 effect gate 用原始位掩码值（如 16 / 32），不能先变成布尔值。场景曝光和游戏灯光没有从静态 PFB 中取得。

已实现普通细节 3/4 的法线、底色、镜面色、金属度、粗糙度混合；UV2 叠加；分区绒面、各向异性和透光的 Principled 近似；钻石数组的折射采样、动态法线与发光；闪片数组的相位/法线、HSV 染色、视角发光与 HDR clamp。普通 Detail1/2、特殊环境光/阴影、游戏钻石高光项和虹彩薄膜 BRDF 未完整重建。Diamond 的场景曝光、alpha 和多 pass 合成仍有限制。**读取了通道用途不等于已完整复刻该 BRDF。**

这些 AdvancedCloth 还包含 ForwardLit / DoubleTransparentPass / DepthOnly 的深度分工。当前仍保留其 alpha unresolved 状态，没有仅因 D 图存在 Alpha 或 `_Cutoff` 就启用透明；需要独立还原整套深度/覆盖路径后才能准确复刻多 pass。

## Uber / Skin

`fx_uber_transparent.shadergraph` 按这套材质的四个关键字精确选出 fragment blob 925 / vertex 67：`_MASK_LAYER_ON _SECOND_LAYER_ON _TEXTURE_ROTATION_ON _UV_DISTORT_ON`。恢复 UV 选择、中心旋转、缩放/滚动、RG/A 扭曲、三层染色与亮度 falloff、multiply/add/replace 图层、mask RGB/A 选择、顶点色、gamma、预乘。One/One 用透明闭包与发光闭包相加，保留后方模型可见性。场景深度柔边、雾、海面裁切、粒子 custom-data 和游戏曝光仍未执行。

`lx22standardcharacterskin.shader` fragment blob 8：SpecGloss R 是 smoothness，roughness=`1-pow(R,GlossMapScale)`；G 是 AO，B 是厚度。NormalBentMap RG 重建法线 Z，不能把 B/A 的 bent-normal 数据直接当普通法线。SSS / coat 与全局光照仍采用 Blender 近似。

## 验证

- Blender 5.0.1 完整时装：67 个网格，11 个材质，118 根骨骼，0 空物体；验证完整世界矩阵、静止蒙皮、骨架移动、非均匀缩放、导入回滚和节点无重叠。
- 真实 Graph.build 分支注入固定 packed 像素，用 CPU Cycles 的线性 EXR 对照两帧解析公式，并验证 tiling B / 数组第三层 / effect gate 确实关闭发光。禁用降噪，避免小图降噪改变验证值。
- 前向约定独立断言 Unity +Z → Blender -Y，Unity +Y → Blender +Z；坐标基右侧使用 inverse，同时转换绑定矩阵与形态键。
- 旧 alpha 共享用例和实际透明覆盖渲染回归保持通过。
