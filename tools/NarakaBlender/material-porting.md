# 这套衣服的贴图与流光怎么移植

0.3.0 默认材质已简化为基础色、金属度/粗糙度、法线、UV1 叠加、相位发光和普通 UV 滚动。图片、UV、通道与 PBR 连接继续留在顶层，仅共享小型法线解码和相位公式组；不再生成钻石、闪片、细节数组、视角发光和多 pass。下文保留源 Shader 的真实通道与旧版实现说明，供移植时查阅。已有工程选中模型后 F3 →「简化永劫材质」即可转换，无需重导网格。

以下对应当前 Blender 中的 `ch_f_ming_wuzhen_cloth01_cb_lv_s3`，Shader 身份为 `lhmstandardcharacterclothadvanced_bab3fa28.shader`。参数来自导出的材质，通道用途按这套资源的编译程序核对。其他 Shader 不能只凭贴图后缀照抄。

## 先看顶层与两层流光

0.2.4 将所有实际图片、UV 来源和直接通道分解留在顶层；只有数学运算放入功能组。每张图旁边标明 UV 集与平铺/运动信息，R/G/B/A 插口直接写用途。不要再进入大表面组找图片；N → 永劫材质 → 贴图与通道用途 → 定位，可以跳到任何一张图。最主要的路线是：

```text
第二套 UV → 发光数据贴图 → R/G/B/A 分工 → 流光1 / 流光2 → 发光合成 → Principled 发光
时间（秒）───────────────────────────────┘
表面 / 细节 → 法线、效果区域、闪片发光 ───────┘
```

两个流光组共用同一个公式；各自暴露“区域蒙版、相位、时间、相位频率、速度、阈值、锐度、视角、颜色、强度”。鼠标放在节点组按 Tab 可进入计算链，内部按 ① 到 ⑪ 标注。输入的说明会显示在提示中。其余表面、视角、覆盖和发光合成按功能与采样依赖阶段组织，计算组之间不包含图片节点。

Shader Editor 按 N → **永劫材质 → 通道与流光**，可选择区域蒙版、相位、移动亮区、单层发光和全部发光。播放时间轴即可在模型上观察。选回“完整材质”恢复原输出。这个观察模式保留原正反面与覆盖规则，不改源贴图。

## 贴图与 UV 对照

| 材质属性 / 实际文件 | 使用的 UV | 应接到什么地方 |
| --- | --- | --- |
| `_MainTex` / `ch_f_ming_wuzhen_body02_lv_s3_d` | UV0 | sRGB 基础色；A 是该 Shader 的覆盖/其他计算数据，不能统一当透明 |
| `_BumpMap` / `...body02_lv_s3_n` | UV0 | Non-Color 法线，按 BC5 / DXT5nm 布局解码并重建 Z |
| `_SpecGlossMap` / `...body02_lv_s3_mrav` | UV0 | Non-Color：R 金属度、G 粗糙度、B AO；粗糙度再算 `1+(G-1)*GlossMapScale` |
| `_AdvIDTex` / `...body02_lv_s3_mask` | UV0 | Non-Color：R 绒面，G 各向异性/镜面分区，B 透光，A 虹彩；各项还需材质开关/强度，四通道都不是直接 RGB 颜色 |
| `_TillingChannelMask` / `...body02_lv_s3_pd_mask` | UV0 | Non-Color：RGBA 控制四个细节/光学层的覆盖，也参与决定哪些地方允许周期发光 |
| `_MainTexUV2` / `ch_f_ming_wuzhen_cloth01_lv_s3_2u_d` | UV1 | sRGB 第二套底色；**A 是这层叠加的权重**，同时混合第二套 MRAV 与法线，不直接代表表面透明 |
| `_BumpMapUV2` / `...cloth01_lv_s3_2u_nx` | UV1 | RG 法线；A 通过 `UseUV2DetailMask` 加到细节蒙版。本材质该向量全 0，因此这项追加无效 |
| `_SpecGlossMapUV2` / `...cloth01_lv_s3_2u_mrav` | UV1 | 第二套 R 金属度、G 粗糙度、B AO；按第二套底色 A 混合 |
| `_EmissionMap` / `...cloth01_lv_s3_2u_em` | UV1 | Non-Color：**R 第一层区域，G 第一层相位，B 第二层区域，A 第二层相位** |
| `_DetailTillingArray` / `...cloth01_lv_s3_pd_array_generated` | 各层旋转/平铺后的 UV0 | 纹理数组，不是一张普通贴图。导出为 4 张 slice，语义见下表 |

这里 **UV0 是第一套 UV，UV1 是第二套 UV**。材质属性名 `UV2` 指“第二套”，不是 Blender 的 `UV2` 层。不要把所有贴图都接同一套 UV，否则细节和发光位置会错。

## 发光图不需要滚动 UV

本材质 `_EffectUvSelector=2`：取 UV1。`_EffectUVTransform=(0,0,1,1)`：保持该 UV。Shader 对发光图的采样坐标没有 `UV + 时间 × 速度`。

取出一个像素的区域值 `region` 与相位值 `phase`，每帧计算：

```hlsl
wave   = sin(phase * frequency + timeSeconds * speed) * 0.5 + 0.5;
shape  = pow(saturate((wave - threshold) / (1 - threshold)), sharpness);
region = region * region;
moving = region * shape;
emissive = moving * viewWeight * hdrColor;
```

相位图是一张“经过顺序图”：不同位置的灰度决定各处的波起始相位。时间推进后，亮区会沿这些灰度变化的方向出现，形成沿衣服纹路流动的效果。它不是通常所说的二维流向向量图，也不是把绿色通道当绿色光。

| 参数 | 流光1（R/G） | 流光2（B/A） | 修改后的表现 |
| --- | --- | --- | --- |
| frequency | 10 | 10 | 相位分布中的波段密度改变；不是 UV 平铺倍率 |
| speed，rad/s | 2 | 2 | 绝对值越大推进越快；改负数反向，0 停止推进 |
| threshold | -1 | -1 | 改变亮区的基础宽度；需要与锐度一起观察 |
| sharpness | 7.32 | 8.05 | 越大亮区通常越窄、明暗越集中 |
| 视角参数 | 16.32 | 16.32 | 16 无视角抑制，低于 16 偏正视，高于 16 偏边缘；不控制运动速度 |
| 原始 HDR 颜色 RGB | (23.968628, 11.6705885, 4.894118) | (5.2915072, 3.7079175, 7.3772106) | 本材质 MatEffectParam2.y=0，所以第二层直接使用 MatEffectColor0.rgb |

两层都是 `2π/2≈3.14159` 秒完成一个正弦周期。这个数不是衣服上一个光点走完全程所需的秒数，后者取决于相位图空间分布。保持同一亮区相位，`phase=(常数-speed*t)/frequency`，所以正速度沿**相位值减小**的方向推进；不存在对所有像素都相同的“向左 / 向上”移动方向。

最后两层相加，再乘细节效果区域和背面强度。当前 cb 材质背面系数是 0.1，cf 配对材质是 0.2；前后表面还有各自剔除规则。游戏曝光补偿来自场景，静态 Prefab 没提供其完整数值。

## 细节数组里的闪片与钻石

| slice / 本材质功能 | 数据与主要计算 |
| --- | --- |
| 0：Glitter1 闪片 | RG 构造细小法线，B 提供闪烁相位，A 调制效果；时间旋转微法线并计算视角闪烁，再用原材质 HDR 颜色染色，不是将数组 RGB 直接发光 |
| 1：Diamonds2 钻石 | RGB 构造光学/法线数据，折射后重新取样 A；用光学采样影响颜色、法线、粗糙度和发光；不能把 A 当表面透明 |
| 2：Detail3 | RG 细节法线，B 细节权重；覆盖来自 TillingChannelMask.B |
| 3：Detail4 | RG 细节法线，B 细节权重；覆盖来自 TillingChannelMask.A |

每层有独立 `_DetailTilling_STN` 与 `_DetailMapNProp2.w`：围绕 UV 中心旋转后，再执行 `uv * ST.xy + ST.zw`。本材质四层 ST 分别为 `(75,150,5,15)`、`(37,25,0,1)`、`(40,50,5,1)`、`(60,60,5,1)`，不是共享平铺值。游戏特殊高光/虹彩 BRDF 和全部多 pass 还原程度见 `shader-contracts.md`。

## 独立特效材质才有真正的 UV 滚动

`ch_f_ming_wuzhen_fx_body01_lv_s3` 使用 `fx_uber_transparent`，是另一组 Renderer 的材质。当前 UV 选择向量全 0，使用第一套 UV；旋转速度为 0。

```hlsl
sampleUV = (uv - anchor) * scale + anchor + offset + velocity * timeSeconds;
```

| 图层 | 当前参数 | 用途 |
| --- | --- | --- |
| `_MainTexture` / `fx_noise_4_11` | scale=(5,2)，velocity=(0,+0.4) | 第一层滚动噪声 |
| `_SecondTexture` / `fx_noise_4_0015_01` | scale=(1,1)，velocity=(0,+0.07) | 第二层较慢噪声；按图层参数与第一层相加 |
| `_MaskTexture` / `...fx_body01_lv_s3_d` | velocity=(0,0) | 固定效果轮廓，限制滚动噪声出现的区域 |
| `_DistortTexture` / `fx_normal_1013_nrm_mob` | scale=(1,3)，velocity=(0,-0.2) | RG/A 解码为扭曲偏移；本材质主/第二层扭曲强度是 0，蒙版扭曲强度是 (0.01,0.01) |

这里 velocity 的单位是**采样 UV 每秒**，与衣服周期流光的 rad/s 不同。采样坐标增加时，图案视觉上通常向相反方向移动。将自己的目标游戏 UV 上下方向与源图的导出翻转约定对齐后，再判断视觉方向。该特效用 One/One 加法混合、预乘和 gamma，不宜直接套普通不透明材质。

## 在目标游戏中实现

优先做底色 / UV0、第二套底色 / UV1 的 A 权重覆盖、法线、R 金属/G 粗糙度，然后单独实现 `_EmissionMap` 的两层相位公式。先关闭视角项（令 viewWeight=1）、用固定曝光观察区域和速度，确认后加入原视角项、背面强度、细节 gate 及闪片/钻石。独立 fx Renderer 的滚动材质最后接入。

可移植的核心函数见 `shaders/naraka_flow.hlsl`。它不依赖 Blender 节点或 Unity include，输入是纹理取样、秒数、原参数和法线点积；接到目标引擎的线性 HDR 发光路径。完整效果还需要目标引擎支持双 UV、对应剔除/混合与细节着色。Blender 的节点组用于验证和编辑，不能直接作为其他游戏的 Shader 编译产物。
