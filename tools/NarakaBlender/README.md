# 永劫无间 Prefab → Blender

配套 AnimeStudio 的 **Naraka / 永劫无间** 特殊导出入口。以所选 Prefab 为边界，导出其全部 MeshRenderer / SkinnedMeshRenderer 实例，包括未启用对象和低精度网格。每个 Renderer 保留自己的层级、材质槽和骨骼绑定，不按 Mesh 名称合并。运行时另行装配的角色部件、粒子和脚本不自动加入。

## 使用

1. 重启更新后的 AnimeStudio，打开永劫无间游戏目录或 `NarakaBladepoint_Data/StreamingAssets/AppRes.info`。
2. 在 Asset List 选择一个或多个时装 `_ui.prefab`；也可在 Logical Asset Paths 树中勾选多个 Prefab。
3. **Export → Format specific → Naraka / 永劫无间 → Selected Prefabs as Blender packages...**，树中勾选则使用 **Checked Prefabs as Blender packages...**。
4. 选择输出目录，每个 Prefab 生成一个 `.naraka` 文件夹。
5. Blender **编辑 → 偏好设置 → 插件 → 从磁盘安装**，选择 `dist/naraka_blender-0.4.1.zip`，启用 **Naraka Prefab Importer / 永劫无间时装**。
6. **文件 → 导入 → Naraka Prefab package / 永劫无间**，选择资源包中的 `naraka.json`。材质预览或渲染模式下查看贴图，播放时间轴查看支持的动态材质。

全部网格都会导入。默认隐藏低精度 LOD，关闭导入选项 **Show highest detail LOD** 可全部显示。原生 LODGroup 不存在时，只按明确的 `_L数字` 后缀暂时隐藏低精度对象，并在报告中说明。所有原始对象与启用状态仍保留。

场景按资源组织：每次导入创建一个时装集合，下面是 **Meshes → LOD0 / LOD1 / …**、**Skeletons** 和独立的 **Effects**。只创建资源网格和骨架，不逐个生成 Transform 空物体。网格保留完整的原层级世界变换，挂在共享骨架下，移动骨架时整体跟随；原层级、辅助节点和挂点存入集合关联的 `source hierarchy.json` 文本，供后续分析。有骨架时，父级矩阵保留嵌套非均匀缩放形成的剪切，无需烘焙顶点或形态键。旧版 `.naraka` 包可直接重新导入，无需重新导出。

`Effects` 按被引用的 FX Prefab 分子集合，导入其中的 MeshRenderer、SkinnedMeshRenderer、独立 Mesh、材质和贴图；若特效网格有蒙皮，会在对应子集合下另建骨架。Billboard、Stretch、水平/垂直 Billboard 使用 Unity 内建四边形而非独立模型，插件会按源挂点放一个带材质的静态检查卡片，并标记 `naraka_preview_only`；它不模拟粒子、朝向相机或播放。Mesh 粒子引用的模型直接作为真实 Mesh 资源导入。特效的面数、子网格、模型资源路径、材质/Shader、贴图属性名与路径、粒子渲染模式和引用关系写在 Blender 文本块 **`effects inventory.json`**；原始组件字段与资源引用另存为每个 Prefab 的 `source components.json`。`fx_pyro_uber.shadergraph` 会把主图 RGB 接到静态检查材质作检视，其透射图和遮罩只保留为未连接的图像节点，不按未知语义猜测 Alpha。运行时脚本创建或选择的资源不在静态 Prefab 引用范围内。

每次导入建立独立集合；重复导入不覆盖现有对象、材质或贴图。失败会清理本次创建的数据。导入器不保存或覆盖现有 `.blend`。

0.4.1 修复普通 `lx22standardcharactercloth` / `lx22standardcharacterclothsilk` 启用 `_MFX_EMISSIVE_PERIODIC_2` 时被误当成静态 RGB 发光的问题。`_EmissionMap` 以 Non-Color 读取：R/B 为两层区域，G/A 为相位；颜色来自材质 HDR 参数。UV 固定，时间推进相位，不滚动整张发光图。贴图、通道分解及两个轻量流光组都在外层可见。旧场景选中相应材质，F3 搜索 **Repair Naraka phase emission / 修复永劫相位发光**，或在 N → 永劫材质面板点击修复；先备份原材质，仅替换旧版的直接 RGB 发光接线，保留其他 PBR、透明度和渲染设置。已手动改写的发光图和已有流光组不会自动覆盖。无需重新解包。

## 包结构与保真范围

```text
某套时装.naraka/
  naraka.json              # 层级、Renderer、材质、贴图的准确来源和引用
  checksums.json           # 读入前验证资源完整性
  meshes/*.mesh            # 复用 EFFMESH v6 原生格式，不经 FBX/OBJ
  effects/meshes/*.mesh    # FX Prefab 的实际 Renderer Mesh 与粒子 Mesh 引用
  materials/*.json         # 完整 Shader 参数、开关、纹理 ST、属性声明
  textures/*.png
  source/effects/*.components.json # FX 组件字段及其资源引用
  source/manifest.json     # 完整导出凭证；重导出先暂存，再备份旧包
```

保留位置、原始法线/切线、顶点色、UV0–UV7 及额外分量、子网格/材质槽、四权重蒙皮、绑定矩阵和 BlendShape 帧。Unity 与 Blender 的坐标转换同时应用到网格和层级。

部分服装 Prefab 的 Renderer 不带骨骼 Transform 指针。新版 AnimeStudio 会从 Mesh 的精确资源身份及 AssetBundle 的 FBX 容器引用定位原始模型，再以根相对完整 Transform 路径的 CRC32 匹配有序骨骼槽；匹配唯一且完整时，将原始骨名、父子关系与来源凭证写入包。插件直接使用这些引用命名骨骼与顶点组，静止矩阵仍来自绑定矩阵。缺失或有歧义时保留哈希并在 warnings 中说明，不按同名 Mesh 或空间位置猜骨架。游戏待机动画不随此流程导入。静止骨架重建在样例所有网格上的变形误差小于 0.001 mm。

**旧包若只有哈希，需用更新后的 AnimeStudio 重新导出一次。** Blender 0.2.4 已支持包内真实骨骼引用，不需要额外骨架插件；重新导入会建立独立集合。手动改名的现有工程不会因更新导出器而被覆盖。

部分头发由 AnimeStudio 按 RendererAssistant → `avatarMeshAsset` 的精确引用解码为相同网格格式，插件可直接读取，无需额外安装。导出器同时保留 `.mesh.runtime.json` 原始数据；游戏专用头发/捏脸变形暂不执行，相关说明在包的 warnings 中。原生 `m_Mesh` 为空时不能仅凭名称从别的 FBX 随意配网格。

## 材质

**0.4.0 默认生成轻量 PBR。** 自动连接基础色、金属度、粗糙度、法线、第二套 UV 覆盖、普通发光、RG/BA 两层相位流光，以及独立特效的 UV 滚动/旋转、颜色和遮罩。不再生成钻石、闪片、折射、细节数组、虹彩、复杂视角项和多 pass 还原。未使用的衣料贴图保留引用元数据，不再加载或创建空接节点；Shader 语义未识别的特效贴图保留为顶层未连接节点，供资源检查。AO 保留在 MRAV 的 B 通道，不通过乘基础色伪造环境光照。皮肤使用独立的打包通道规则。

实际图片、UV、通道分解和 PBR 连接均在顶层，只将法线重建 Z 和相位波公式放在小型共享组内。按 N → **永劫材质 → 贴图与通道用途** 可查看用途并“定位”。所有节点按依赖从左向右排列。

**已有工程无需重导出或重导网格。** 选中模型，F3 搜索 **Simplify Naraka materials / 简化永劫材质**；默认转换所选物体，操作面板可改为当前场景。原材质先独立备份到工程旁 `NarakaMaterialBackups/*.blend`；未保存的工程备份到临时目录。保留流光面板的时间参数、颜色和强度，任意手工改线不自动迁移，可从备份追加旧材质。转换不保存当前工程，不改网格、骨骼、权重或其他游戏材质。

轻量流光保留 R/B 区域、G/A 相位、源 HDR 颜色、时间/频率/速度/阈值/锐度，不计算视角和细节门控。独立特效保留图层、UV 运动、颜色、遮罩和加法混合，省略扭曲、亮度 falloff、gamma 等特殊还原。D 图 A 仍按材质策略处理，不统一接成透明度；已知丝绸改用基础阈值覆盖，完整多 pass 不执行。`body02_lv_s0_em` 所属 billboard 未启用的周期分支不会误接为 RGB 发光。

Blender 5.0.1 实际样例：cloth01 顶层 45、展开 103 个节点，7 次贴图采样；当前旧材质顶层 74、展开 713 个节点、15 次采样。11 个材质共加载 14 张贴图，独立进程材质构建约 0.7 秒；该数字不是整个工程打开时间。两帧相位发光像素、UV 滚动、透明策略、保存重开和旧材质迁移已验证。转换范围为「当前场景」时，还会单独备份并清理没有实际引用、且未标记资产的旧永劫材质与其闲置图片，避免历史版本继续占用工程。

以下为 0.2.x 历史记录，其中的复杂光学/细节还原不再默认执行：

0.2.4 将所有实际图片节点、UV 来源和直接通道分解留在顶层，图片旁的框标明 UV 集与运动/平铺信息，R/G/B/A 插口标明对应用途。纹理数组的每个切片和钻石的折射再取样都单独可见；只有数学运算放进功能组，不再把纹理藏入表面组。Shader Editor 按 N → **永劫材质 → 贴图与通道用途** 可查看用途并用“定位”跳到该图和通道节点。

两层流光分别暴露区域、相位、时间、频率、速度、阈值、锐度、视角、颜色和强度；**通道与流光** 面板可在模型上观察每一阶段，再恢复完整材质。区域/相位来自原贴图，当前周期流光的 UV 固定，亮区沿相位场推进。独立 Uber 特效的 UV 滚动是另一条路径。

完整贴图通道、当前数值、UV0/UV1 对应、两种运动的区别和移植顺序见 [material-porting.md](material-porting.md)；不依赖 Blender 的核心代码见 [naraka_flow.hlsl](shaders/naraka_flow.hlsl)。分组前后在两帧完整表面渲染中的线性像素一致；分组用于阅读，不代表减少了 GPU 着色指令。仅关闭所导入材质的 NodePreview 逐节点缩略图，其他材质/插件全局设置不变。此版复用现有完整包，无需重新导出。

- 自动连接基本色/颜色、透明度、法线、金属度/粗糙度、发光和区域镜面颜色。
- PNG 保留原数据；普通颜色与数据贴图分别设置色彩空间，BC5 / Unity DXT5nm 法线在节点中重建。
- D 图存在 A 通道不代表透明。按精确 Shader 适配、材质关键字和参数区分不透明、裁切和混合，`_Cutoff` / `_AlphaClipThreshold`、队列、ZWrite 或 `hair/uber/transparent` 名称不能单独启用 D 图透明度。未知 Shader 即使带混合因子，也先将 A 保留为数据，在报告 `alpha.resolved: false` 中说明来源待确认；不自动接到表面 Alpha。
- 已核对 Periodic2 与旧 cloth silk 的 `DoubleTransparentPass` 基础变体：D 图 A × 颜色 A 进入阈值/视角覆盖公式。其他同名属性不跨 Shader 套用，完整多 pass 合成仍为预览近似。图像使用独立 RGBA 通道读取，数据 A=0 不会抹掉不透明基础色。
- 已根据本机 `ch_cloth_silk_specular4_periodic2` 的 D3D11 变体验证 RG/BA 两层周期发光公式、时间/视角项与 MRAV 的 R 金属度、G 粗糙度。插件以帧率换算秒，支持时间轴播放。
- 特效材质的显式 Scroll / Scale / Offset 参数生成 UV 节点和驱动。
- 全部原参数、Shader 身份、纹理引用保存在材质自定义属性；未接入的贴图仍创建为可编辑图像节点。
- 节点按基础色、法线解码、金属度/粗糙度、布料分区、发光、两层流光、透明度分组并自动排列。选中材质所在网格后，F3 搜索 **Arrange Naraka shader nodes / 整理永劫无间节点** 可重新整理当前材质，只改变布局，不改连接与参数。
- `naraka_conversion_status_json` 与导入报告的 `materialStatus` 分别列出已验证功能、待适配开关和未接入贴图。`surface: approximate` 明确标识当前表面着色仍为近似。

0.2.2 统一前向坐标为 Blender -Y，网格、骨架、绑定矩阵和形态键一起转换。修复 AdvancedCloth 关键字启用的 packed RG/BA 流光被当作 RGB 发光的问题。接入已核对的六个 AdvancedCloth Shader 的 ID 分区、细节层 3/4、UV2 覆盖、钻石/闪片，以及 Uber 贴图图层、扭曲和加法特效。Skin 的 SpecGloss 与 NormalBentMap 使用独立于布料的通道规则。具体已实现项和近似范围见 [shader-contracts.md](shader-contracts.md)。

**使用细节数组的旧包需要用新版 AnimeStudio 重新导出。** 旧版只保存了数组引用，没有像素；新版按精确资源身份导出 BC7 `Texture2DArray` 的各 slice，并校验全部切片。只有朝向/packed emission 分支修复时旧包也可读取，缺失细节会写入报告。更新插件后重新导入，已有场景中的旧材质不会自动改写。

0.2.1 的透明度规则保持：读取 `_Cull` 与 `_Culling`；没有按 D 图 A 自动开启透明。专用深度 pass 网格保留但表面隐藏，其游戏深度缓冲贡献暂不在 Blender 复现。AdvancedCloth 多 pass 的透明路径仍单独标注 unresolved。

**这是 Blender 材质重建，不是执行 Unity Shader。** 原游戏的各向异性/虹彩/皮肤散射与全局灯光、曝光、专用效果仍可能不同。未知 Shader 和未转换功能写入集合关联的 `import report.json` 文本与材质的 `naraka_conversion_notes`，不能据导入成功宣称画面与游戏逐像素一致。其他款式需要根据其 Shader 补充适配与对照。

## 命令行与验证

```powershell
AnimeStudio.GUI.exe --export-naraka-prefab "D:\steam\steamapps\common\NARAKA BLADEPOINT" "assets/res/prefab/actor_visual_part/ch_f_ming_yulinglong/ch_f_ming_yulinglong_lv_s16_ui.prefab" "E:\Exports\yulinglong_s16_ui.naraka"
```

独立 Blender 后台验证（不会操作已打开的工程）：

```powershell
blender --background --factory-startup --python-exit-code 1 --python tests/blender_import_smoke.py -- "E:\Exports\yulinglong_s16_ui.naraka\naraka.json" "E:\Exports\validation"
```

验证插件注册/重复注册、资源校验、网格/三角形/材质槽、贴图加载、静止蒙皮、特效资源归属、Billboard 代理和重复导入隔离，并保存独立样例与渲染。旧版衣料样例在 Blender 5.2 验证通过；0.2.0 在 Blender 5.0.1 验证零空物体、LOD 集合、网格世界变换、骨架整体移动、嵌套非均匀缩放、节点无重叠和整理前后连接不变。0.4.0 的真实特效包使用 Blender 5.0.1 验证；该玲珑样例有 3 个 Billboard 粒子系统、2 个特效材质、4 张贴图，没有独立模型 Mesh，因而导入 3 张标记为代理的 Unity 内建粒子面片。

第二套 `lv_s0_ui` 的 52 个网格、15 个材质、24 张贴图也已通过结构、蒙皮和渲染检查。其其他 Shader 采用通用预览适配，不等于与游戏视觉一致。
