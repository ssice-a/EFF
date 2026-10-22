# 永劫无间时装 Prefab 导出与 Blender 导入

2026-10-17：插件 0.3.0 默认切换轻量 PBR，保留基础色、金属度/粗糙度、法线、UV1 覆盖、发光和相位/UV 运动，停止自动生成钻石/闪片/细节数组/视角多 pass 还原。导出格式与骨架、网格规则无需改动，已有包直接可用。F3「简化永劫材质」可转换现有材质并单独备份旧材质。详见 [插件说明](../tools/NarakaBlender/README.md)。

2026-10-15。本功能与终末地的 EFF 作者/游戏 Mod 流程并列，目标是独立 Blender 场景还原，不向永劫无间安装或生成运行时 Mod。

入口为 AnimeStudio **Export → Format specific → Naraka / 永劫无间**，支持 Asset List 多选与 Logical Asset Paths 树勾选。单个 Prefab 生成一个 `.naraka` 目录；头发、皮肤、饰品不按类别过滤，导出所选 Prefab 层级内的全部网格 Renderer。运行时从其他 Prefab 装配的对象不属于此静态闭包。

导出代码：`tools/AnimeStudio/AnimeStudio.GUI/NarakaPrefabLoader.cs`、`NarakaPackageWriter.cs`、`NarakaPrefabPackageCommand.cs`。按实际 CAB/PathID 解析依赖，保留每个 Renderer 的材质槽和层级，不以名称或共享 Mesh 合并消费者。共享 `EiemPackageWriter.WriteMeshPayload` 的 EFFMESH v6 编码及既有暂存/校验/备份发布，不改变终末地包的语义。

新插件源码及说明在 `tools/NarakaBlender/`，安装包由 `python tools/NarakaBlender/build.py` 构建到 `tools/NarakaBlender/dist/naraka_blender-0.2.0.zip`，不依赖 EFF Blender 插件。导入选择 `naraka.json`，先校验摘要和几何，再建立独立集合；保留源层级元数据、完整网格变换、子网格材质槽、UV/法线/切线/顶点色、四权重和形态帧。失败回滚本次新建数据，重复导入保持原集合。

0.2.0 参考终末地导入器的 Meshes/Skeletons 资源集合组织：只创建 Mesh 和 Armature，Meshes 内按 LOD 分集合，不生成逐个 Transform Empty。源层级及辅助挂点保存为 Blender 文本。网格通过完整父级逆矩阵保留原层级世界变换并挂到共享骨架，避免嵌套非均匀缩放分解丢失剪切；移动骨架时整体跟随。已有包无需重新导出，重新导入即可。骨骼名称及完整主体层级的恢复范围没有因此扩大。

材质节点按功能分框、按有向连接排列列与行，两层流光单独分组；提供 F3 整理当前材质操作。布局不改连接、驱动或着色参数。材质及导入报告提供结构化 materialStatus，明确当前表面为 approximate，并列出验证过的功能、未接入贴图与未适配开关。

材质保留全部原参数与纹理引用，自动建立 Blender 节点。已对本机 `ch_cloth_silk_specular4_periodic2` 的 D3D11 `_MFX_EMISSIVE_PERIODIC_2_ON` 像素变体核对双层 RG/BA 波形、时间/视角项、HDR 颜色及 MRAV 的金属度/粗糙度通道。Shader 反汇编/反编译证据仅保存本机忽略目录 `diagnostics/naraka-preview-20261015/shaders/`；不能把文件名当成未知 Shader 的功能证明。布料 BRDF、虹彩、各向异性、皮肤散射、灯光和未实现 Shader 仍为近似或待适配，具体记录到 Blender 导入报告。

实际衣服 Prefab 的部分 Renderer 不带骨骼 Transform 指针。导入器依据逆绑定矩阵恢复静止骨架和蒙皮，保留原骨骼哈希；有明确指针时保留可恢复的父子关系，不猜造主体角色骨架和动画。

## 验证

- 普通 Unity / 永劫材质与流式 Mesh、永劫分轴压缩 XYZ、八面体法线/切线、光照 UV 和常量压缩回归通过。
- 既有 BLC v3/v4/v5、便携/增量索引及源包备份/失败回退回归通过。
- Blender 5.2.0 LTS 独立后台验证，不连接或修改用户已打开的工程。
- 0.2.0 使用当前 Blender 5.0.1 经 MCP 导入查看，并使用同一 5.0.1 可执行程序后台检查；网格与骨架之外对象数为零，LOD 集合、源世界变换、整体移动、嵌套非均匀缩放、节点无重叠及连接不变检查通过。
- 玉玲珑 `lv_s16_ui`：135 节点，59 网格实例，12 材质，13 贴图，11 个周期发光材质；静止蒙皮最大误差约 `2.4e-7 m`。两帧驱动/实际渲染检查通过。
- 玉玲珑 `lv_s0_ui`：52 网格实例，15 材质，24 贴图，静止蒙皮最大误差约 `2.6e-7 m`。该套的其他 Shader 使用通用预览适配，结构/渲染成功不等于游戏画面匹配。
- 插件 ZIP 经临时脚本目录安装、启用和禁用验证；导入前损坏拒绝、导入中失败回滚、重复导入不覆盖与材质槽一致性通过。

样例包、验证 JSON、独立 `.blend` 和 PNG 位于本机忽略目录 `diagnostics/naraka-preview-20261015/`。尚未在游戏 UI 同机对照逐项验收其他时装效果，不能宣称所有 Shader 或全部皮肤已完整还原。

## 逻辑目录与模型预览修复

2026-10-15 根据用户截图复现两项缺陷：AppRes 紧凑索引把 ContainerOnly 记录存放在目录上，界面却将没有子目录的节点一律当文件，导致 `model` 和角色目录无展开按钮、仅能选到第一条记录；此外 BuildAssetData 的 Container 展示标签会被共享 preload 的后续条目覆盖，FBX 的旧预览回退可能错误选择贴图。

目录构建现在区分目录上的 catalog 记录与文件上的 object 记录，恢复完整文件列表，保持终末地对象索引的 Prefab 叶节点语义。模型预览直接解析精确 AssetBundle.m_Container 条目的 main asset / preload 引用和 GameObject 网格，不用有损展示标签猜测；逻辑 FBX 可展开为具体 Mesh 子项，选中后预览几何。UI Prefab 仍位于 `assets/res/prefab/actor_visual_part/<角色目录>/*_ui.prefab`。

`AnimeStudio.PreviewTests` 在修复前确认两个失败，修复后合成目录/旧对象索引/贴图错误回退回归通过。当前实际目录有 3,091 个 actor_visual_part Prefab，其中包括头发、变体和 UI/游戏版本，不能当作独立时装数；玉玲珑目录完整显示 124 个 Prefab。用户截图中的 christmassnowman/model 显示两个 FBX，各解析出 12/16 个非空 Mesh。当前发布目录已备份替换 GUI.dll，通过实际窗口验证角色目录展开、FBX 展开、单 Mesh 预览和再次选择模型；截图在 `diagnostics/naraka-preview-20261015/tree-preview-fix/`。BLC/便携增量索引与材质/Mesh 格式回归仍通过。

## Prefab 组合基础色预览

点击永劫无间逻辑 Prefab（树或 Asset List）直接解析同一导出器的精确依赖图，将所有 Renderer 实例的世界变换、子网格和材质槽组合为 OpenGL 基础色预览，不先导出、不使用整包同名 Mesh 猜测。不运行皮肤动画、物理或游戏 Shader。基础色使用材质主贴图、颜色、UV 通道和 ST，解析贴图色彩空间、简单 alpha coverage 和面剔除；纹理解码只保留基础色，预览缩至最长边 2048，原导出精度保持。GPU 的 VAO/VBO/EBO/贴图及程序在切换资源、Reset、关闭时释放。

Prefab 可展开为其全部网格 Renderer 引用，名称重复仍保留独立来源身份；点击子项显示单独网格，回到 Prefab 显示组合。默认显示源启用状态及高精度 LOD（原生 LODGroup 优先，无该组件才用明确 `_L数字` 后缀），列表保留所有低精度及未启用项，底部“显示全部网格”可强制显示全部实例。该显示筛选不影响导出范围。Dump 页保存网格/Renderer/材质身份和未识别基础色贴图说明。

独立 OpenGL 3.3 实测玉玲珑 s16 的 59 个实例（默认 18）、12 材质、3 张基础色贴图，以及 s0 的 52 个实例（默认 15）、15 材质、4 张基础色贴图；组合/全部/单项/线框均实际渲染检查，坐标保持直立、索引与材质槽校验通过，GL 无错误。证据在 `diagnostics/naraka-preview-20261015/prefab-basecolor-test/`；完整 Shader 和最终游戏效果不在此预览验收范围内。

已在用户当前发布程序中实际点击玉玲珑 s16 UI Prefab，自动显示组合及基础色；展开显示 59 个网格引用，单击 body02 切换到该 Mesh，显示全部切到 59/59，再恢复默认 18/59。现场截图为同目录 `live-prefab.png` 和 `live-all-meshes.png`。发布前 GUI.dll 已备份到 `tools/AnimeStudio/dist/backups/naraka-basecolor-preview-20261015-181135/`。

### 正反面与遮挡复核

随后复核发现原基础色预览漏读 `_Culling`，将所有带 `_AlphaClipThreshold` 的材质都当作硬裁切，且没有半透明混合／透明排序／ZWrite 状态。修正为同时读取 `_Cull` 与 `_Culling`，源变换镜像奇偶修正绕序；明确区分不透明、关键字/开关启用的 cutout、已识别 Periodic2/旧 silk 半透明和已识别专用深度材质。不因一个阈值字段就裁切不透明 ShaderGraph。透明 pass 读取源混合因子、ZWrite/ZTest/自定义队列，按队列及当前相机空间的子网格中心从远到近排序，处理颜色遮罩并在绘制后复原状态。两层相交或自交透明几何仍可能需要三角形级排序或游戏专用多 pass；不包含游戏 Shader 动态裁切与物理，不能承诺所有视角完全复刻。

验收边界：修正前的真实 GL 像素测试确认正面剔除、背面剔除、双面、不透明绘制顺序无关的深度遮挡和 alpha 孔洞正常，并复现“半透明遮住背景”的失败。修正后编译、纯材质策略及 s16/s0 真实 Prefab 的所有材质状态检查通过；s16 的 9 个半透明材质已识别，s0 保留 silk 深度写入及两个专用深度材质。修正后的 GPU 重验在 GLFW 创建测试窗口阶段出现 native 0xc0000005，尚未进入绘制；独立 WGL 上下文亦在像素格式初始化失败，已移除临时替代上下文代码，未修改系统或用户游戏进程。新增半透明/镜像渲染与正反面实际时装画面验收仍待通过，不能将状态测试当成 GPU 渲染通过。证据和新增可重跑像素测试在 `AnimeStudio.PreviewTests/PreviewVisibilityTests.cs` 及 `diagnostics/naraka-preview-20261015/prefab-visibility-test/material-state-verification.json`。

GUI.dll 已备份更新，备份为 `tools/AnimeStudio/dist/backups/naraka-face-depth-20261015-185147/AnimeStudio.GUI.dll`。构建与发布 SHA256 一致；当前没有启动查看器或中断用户游戏。BLC、便携增量索引、Material/Mesh 格式回归通过；新的 GPU 画面验收仍保留为待完成。

### D 图 Alpha 的材质语义修正（0.2.1）

此前 Blender 通过 `hair/uber/transparent/periodic2` 子串推断透明，查看器还根据透明队列、未知 Shader 的 `_Mode`、单个 `_DstBlend` 推断，范围过宽。现已去除这些判断；基础色贴图包含 A、保存了裁切阈值、ZWrite=0 都不单独启用透明。使用精确 Shader 适配与材质关键字/参数，并把“混合状态”与“透明度来源”分开记录：未知 Shader 即使有 SrcAlpha 混合状态，也不会自动将 D 图 A 接到表面透明度，报告标为 `alpha.resolved: false`。LX22 普通角色/布料的 `_USE_CUTOUT` 才启用已识别的裁切路径，Opaque ShaderGraph 保持不透明。兼容 `_Cull` 与 `_Culling`；贴图用独立 RGBA 通道读取，A=0 的数据不会预乘掉 RGB。

已从本机 Shader TypeTree 与 D3D11 字节码确认两个丝绸 Shader 的 `ForwardLit` 是不透明 pass，`DoubleTransparentPass` 才使用 D 图 A。前者中的阈值重映射用于表面计算，不能当成后者的透明公式。透明 pass 的已检查基础变体为 `u=saturate(D.a*Color.a/max(0.01,threshold))`，`alpha=(1-(1-u)^(1/max(epsilon,abs(N·V))))*min(1,255*u)`。Periodic2 使用 `_AlphaClipThreshold`，旧 silk 使用 `_Cutoff`；已替换此前错误的统一减阈值公式。Blender 接入对应法线和视线，基础色 GL 预览使用几何法线与正交视线近似；这不等于实现游戏完整多 pass。

C# 查看器和 Python 插件共用 24 组策略样例；Blender 5.0.1 使用同一张 RGBA 贴图，节点连接及 CPU Cycles 实际覆盖率验证不透明、裁切、丝绸、未知透明来源五种配置，并单独验证 A=0 的 PNG 仍保留 RGB。玉玲珑 s16/s0 的真实材质参数和独立导入结构检查通过。查看器的新 GL GPU 重验限制仍如上节，不能以这些测试代替当前 GL 画面验收。新增证据保存在本机 `diagnostics/naraka-preview-20261015/alpha-evidence/`、`alpha-policy-test/` 与 `blender50-v021*`。

插件更新为 `tools/NarakaBlender/dist/naraka_blender-0.2.1.zip`。旧包已有完整参数，不必重新导出；需用新插件重新导入来建立新材质，现有 Blender 场景不会被自动改写。

本次发布已更新 `dist/net9.0-windows/bin/AnimeStudio.GUI.dll` 和本机 Blender 5.0 插件。备份为 `tools/AnimeStudio/dist/backups/naraka-alpha-20261015-192923/`（含原 GUI.dll 和已安装插件）。GUI DLL SHA256 为 `130872605937BB67C811009890C82B9A105DF1A8CB0660458444702FFD25348A`。0.2.1 ZIP 已在独立脚本目录中安装、启用和禁用检查通过。

### 预览内外穿透修正（2026-10-15 22:07）

用户实际预览飞刀与时装时发现外表面被穿透。复核真实预览数据：飞刀 2,046 个有效三角形全部与顶点法线同向；玉玲珑 s16 启用网格有 42,159 个同向、10 个反向的小三角形，整体绕序没有翻转。根因是组合预览沿用普通 Mesh 预览的正 Z 缩放投影，但其转换后的几何使用 GL CCW 正面、相机朝 -Z。正 Z 投影令朝向观察者的 +Z 表面反而深度更远，双面材质的对侧表面会盖住外表面，单面剔除和深度判断也互相矛盾。

新增独立 `NarakaPreviewCamera.Projection`，正交投影将 Z 取反，让朝向观察者的表面获得更近的 GL 深度；相机空间透明层随之按 Z 从小到大（远到近）排序。仅用于永劫 Prefab 的组合/子网格预览，保持原材质的 `_Cull/_Culling`、透明度语义、源 Mesh、导出包及 Blender 插件。此前合成平面测试自行使用单位投影及任意“近/远”标签，未验证朝向和深度的一致性；现改用生产相机，并加入封闭外壳对侧颜色遮挡回归。

修复前纯相机断言和 OpenGL 实际像素测试均稳定失败，明确复现外壳透出对侧颜色；修复后在 NVIDIA 591.74 / GL 3.3.0 通过外壳、正/反面剔除、双面、绘制顺序无关的深度遮挡、材质数据 A、裁切孔洞、半透明与多透明层排序，以及 24 组材质策略测试。玉玲珑 s16/s0 的组合/全部网格/单网格/线框和当前飞刀的四个相机角度均已真正渲染，GL 无错误。此前 GL 上下文初始化失败的验收限制在本轮已解除；完整游戏 Shader 和多 pass 视觉匹配仍不属于本基础色预览的验收范围。

本机程序已更新、正常重启并重新载入游戏目录。证据在 `diagnostics/naraka-preview-20261015/orientation-fixed/`；旧 GUI.dll 备份到 `tools/AnimeStudio/dist/backups/naraka-orientation-20261015-220748/`。发布 GUI.dll SHA256 为 `A18DD82370691F76A0414D16488AE62E9E83D6B480E565334223BF765705A28E`。这是查看器的相机错误，已有导出包无需重新导出。

### 导出后预览失效的复核与 runtime mesh 支持（2026-10-15 22:42）

现场选择 `ch_f_ming_haikou_hair_lv_s7.prefab` 时显示 `Unresolved Mesh: CAB-63d0ddbff98dd7cc63e9fcbf82abf366:0:0`，但信息标签仍残留此前 `ch_f_luotihuaban_lv_c0.prefab`。隔离复现确认该头发在导出前已同样失败；衣服 c0 与飞刀在导出前后能正常加载，保留 GPU 预览像素也相同，未证明导出破坏共享依赖状态。

真正漏项是此 Prefab 的 12 个 SkinnedMeshRenderer 原生 `m_Mesh` 全部为空（TypeTree 与现有解析结果一致），其同物体 RendererAssistant 的 `_renderer` 精确指向该 Renderer，`avatarMeshAsset` 再指向保存几何的 MonoBehaviour。新增 `NarakaPrefabLoader.RendererMesh` 按这条精确链解析，不按名称搜索或借用其他 Prefab；有原生 Mesh 时仍优先使用原生引用。读取已验证的 v2、类型 0/1 自定义数据：顶点/法线/切线、三角形、普通 UV 或 hair UV0/UV1、Color32、绑定矩阵和四权重蒙皮。未知版本、混合 UV 布局、无效索引/权重拒绝，保留原错误语义，不静默漏掉网格。原生 Mesh 类支持有明确源对象身份的解码视图，不替换源对象或修改源包。

预览和导出共用同一 resolver；原始 runtime 数据完整保存在对应 `.mesh.runtime.json` 并纳入 checksums，特殊 avatar/hair 变形数据暂不执行。骨骼没有 Transform 指针/名称时继续从绑定矩阵恢复静止骨架，不扩张为游戏主体骨架。失败页清理旧模型标签和 Dump，避免上一套模型信息覆盖当前错误。此次扩展没有改变 D 图 A 的材质语义或之前的深度修复。

回归覆盖普通及 UI 头发（各 12 网格）、衣服（8 网格）、飞刀（3 网格）的真实“预览→导出→保持 GPU 预览→再次加载”循环：导出前后 GPU 像素逐字节一致，Renderer 身份一致；导出产物按原始 runtime 数组逐项核对位置/法线/切线、索引、UV0/UV1、蒙皮与绑定矩阵列序。Blender 5.0.1 独立导入头发得到 12 网格、3 材质、13 贴图、97 个恢复的静止骨骼、0 Empty，最大静止蒙皮误差约 `1.33e-7 m`，这不表示已复刻专用头发 Shader。24 组材质策略、Material/Mesh 格式和 BLC/便携/增量索引回归通过。

GUI 与解析库已更新并重启，用户已重新载入目录。备份为 `tools/AnimeStudio/dist/backups/naraka-runtime-mesh-20261015-224222/`，GUI.dll SHA256 `1060757200D01DDF1E919C72CB7D47DBAA380962B846BAAF2A76415653257D6D`，AnimeStudio.dll SHA256 `11AAA024E6B7F6F88EC37C5439DC0C27C983860CDA5AD2BA15CEAC7D0707B9E3`。现场和隔离证据位于 `diagnostics/naraka-preview-20261015/export-preview-lifecycle-before/`、`export-preview-lifecycle-fixed/`、`runtime-hair-blender50/`。已有成功导出的原生包无需重导；此前因空 m_Mesh 导出失败的资源可用更新程序重新导出，现有 Blender 插件已兼容该网格格式。
# 2026-10-15：0.2.2 材质与前向修复

前向默认转换为 Blender -Y（Z 向上），完整网格/骨架/绑定矩阵/形态键一致转换。已核对武真的这套 billboard 时装实际编译 Shader，修复 AdvancedCloth packed emission 分支，补 BC7 Texture2DArray 切片导出、ID 分区、细节 3/4、UV2 与光学/独立 Uber 特效适配。旧导出包若缺细节数组需重新导出。详细证据、通道语义、支持的六个 Shader 身份和保真限制见 [shader-contracts.md](../tools/NarakaBlender/shader-contracts.md)。Blender 5.0.1 上以实际时装和线性像素测试验证。不会按 D 图 A 一律开启半透明；完整游戏 BRDF / 多 pass / 场景灯光仍有未还原部分。

## 2026-10-16：从原始 FBX 恢复骨名与层级

服装 PFB 缺少 `m_Bones` 并不证明其原始 FBX 没有骨架。实际 `ch_f_ming_wuzhen_lv_s3_billboard.fbx` 的 GameObject/Transform 图有 183 个节点，当前 118 个骨骼哈希全部对应到唯一的根相对路径 CRC32。此前导出器只遍历所选 PFB，漏读了同一来源 FBX 的骨架。

新增 `NarakaModelBones`：优先保留 Renderer 原生骨骼指针；指针为空时，通过 Mesh 的 CAB/PathID 与 AssetBundle 的精确 FBX main/preload 引用定位模型，遍历其原始 Transform 图。仅在所有有序骨骼哈希都唯一匹配、且原始模型唯一时恢复骨骼 palette，不用模型名、短骨名或骨骼位置猜测。恢复的骨骼及祖先追加到清单 nodes，role 为 `source-skeleton`；Renderer 的 `boneSource` 保存匹配方法、FBX 容器、源根及 Mesh 身份。仅补骨架元数据，不增加原 FBX 的 Renderer 或改动所选 PFB 的网格闭包。哈希缺失、路径冲突或多来源歧义时继续保留绑定矩阵和哈希，并明确报告 unresolved。

配套 Blender 导入器原本就支持非空 bones 引用，因此无需新增骨架格式或插件；静止矩阵仍用原 Mesh bindposes 重建，名称与可恢复的父子关系来自源节点。旧包需重新导出以获得这些信息。当前作者工程只经 MCP 同步改名：118 根骨骼、51 个绑定网格的 1,183 个顶点组；静止矩阵、权重及求值顶点位置均不变，未保存 blend，也未重导入作者网格或改变父子层级。

验证：新增实际导出回归在修复前因 body 的空 palette 失败，修复后 67 个 Renderer 全部恢复、骨名共 118 个。匹配策略测试覆盖缺失拒绝、哈希冲突拒绝、重复槽及顺序保持。Blender 5.0.1 独立重新导入验证 118 个骨名、117 条骨骼父子关系、槽位顺序、顶点组对应与静止蒙皮；最大误差 `2.481166393361496e-7 m`。新旧导出的几何、UV、法线、切线、颜色、权重、bindposes、形态帧、材质及贴图描述逐项一致。证据位于本机忽略目录 `diagnostics/naraka-bone-names-20261016/`。
