# CharacterNPR_VFX 的 PolarUV 与贴图流动

此 Shader 的环绕流动属于原生 UV 采样模式和材质速度参数，不使用 AnimationClip，也不旋转网格。

从游戏 Shader 元数据 `E:\EIEM_Workspace\shader-audit-20261109\jsspsi-material-check\shader-raw\Shader\Shader#2.dat` 读取到：

- `_MainUVSet` 的真实枚举是 `UV0=0`、`UV1=1`、`PolarUV=2`、`ScreenUV=3`。Blend、Mask、Disturb、Normal 各层也有各自的 UVSet 和权重向量，因此 UV 模式是逐贴图选择，不是材质级总开关。
- `_MainTexUVSpeed` 的 Shader 提示为 `XY: By Time, ZW: By Custom1.X`；Blend 等贴图有独立的速度字段。面板只编辑 XY 时保留 ZW，导出写回原生 `*_UVSpeed` 和 UV 权重。
- `_MainTexUVRotate` 的 Shader 提示是 Degree，并写入 `_MainTexUVRotateMat`。它是采样前的固定角度，不是持续旋转速度。
- `M_fx_jsspsi_cloth_06_01.mat` 使用 `CharacterNPR_VFX`，有逐层 UVSet/UVSpeed，未保存 `_UsePolarUV`。另一材质 `M_fx_ui_mifu_toppotential_hulu_901.mat` 有独立 `_UsePolarUV` 字段；这属于不同 Shader 参数形态，不能套用到庄服材质。

这些数据只能证明 `PolarUV` 是一个采样坐标模式，不能证明它会按任意网格的 UV 布局产生预期的旋涡，也不能证明它等同于贴图旋转。切到 `PolarUV` 会改变采样坐标来源；UV 速度为零只会停止时间偏移，不会恢复 UV0，也不会撤销坐标映射。对依赖现有 UV 排布的贴图，应先保留 `UV0`，只有经目标 Shader 和目标网格验证后才切换 `PolarUV`。

`_MainTexUVRotate` 写入的是静态角度，不是持续旋转速度。元数据中的 UV 速度提示了时间与 Custom1 的输入，但没有给出 PolarUV 下的坐标公式或各分量语义，因此当前不能建议用 PolarUV 加速度实现纹理旋转。AnimationClip 也不是修补错误采样坐标的办法。

当前证据确认了枚举、字段和速度参数提示；Shader 元数据没有暴露 PolarUV 转换公式，因此坐标生成方式及角向/径向分别对应速度向量的哪个分量，仍须通过编译程序分析或目标游戏画面验证，面板不预设轴向解释。活动工程中的 `MeshS_wpn_sword_0022_01_lod0_0.011` 只有 UV0，当前主图模式为 PolarUV，权重为 `(0,0,1,0)`；其 Blender 节点树只有 Principled BSDF 和输出，没有连接主图路径的 Image Texture，所以普通 Mesh 视口不会显示该纹理。这不能代替游戏内 Shader 验证。离线特效预览曾把枚举值 2 错当 UV2，现改为有标记的 UV0 预览回退，像素回归通过；游戏 PolarUV 公式仍未复刻。当前特效根级 `effectLogicCfg.isLoop=0`；两个扩散波 ParticleSystem 的 `looping` 已关闭，并另存到 `G:\zmd\机械刀\幻枢归奇.刀_UV0预览回退_停止发射器循环_20261116.blend`。原工程和 Mod 未覆盖。

回归覆盖 PolarUV/ScreenUV 的逐贴图枚举与 one-hot 权重写入、每贴图速度独立写回，以及贴图行只显示资源路径。
