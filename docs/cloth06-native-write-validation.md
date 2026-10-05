# cloth_06 原生资源写出与回读验证

日期：2026-10-03（本地测试批次）。输入为已捕获的 LZY 作者快照，保留用户确认的 14 个对象选择规则。本轮只对其中 cloth_06 LOD0 的两个独立部件实现原生写出与 PFB 装配。

**结果：独立离线回读通过。游戏原生加载、运行时 native 状态、冲刺/残影捕获和 F10 重建尚未验证。** 产物显式标记 `gameReady=false`、`nativeLoadVerified=false`。本轮没有部署 DLL、修改已部署 Mod、载入 RenderDoc，或保存 Blender 工程。

## 产物与核验入口

- 原生写出产物：`E:\EIEM_Workspace\exports\LZY-cloth06-native-20261003-attempt04`。
- 写出清单：产物根目录的 `build-manifest.json`。
- 独立原生回读：`diagnostics/prefab-resource-validation-20261002/cloth06-native-verification-final.json`。
- PNG 像素核验：`diagnostics/prefab-resource-validation-20261002/cloth06-native-texture-pixels-final.json`。
- 作者快照：`E:\EIEM_Workspace\exports\LZY-author-validation-20261002\capture.json`。

写出器位于 `tools/nativepack/`，包括 TypeTree 编解码、SerializedFile/UnityFS 写出、Mesh 几何编码与本次装配实验。回读器位于 `tools/EndfieldVfsProbe/NativePackVerification.cs`，实际使用 AnimeStudio 的 Bundle、Mesh、Renderer 和 Texture2D 解析器，而非仅让写出器自读自证。

## 已实现的装配

目标身份以完整逻辑路径与资产名确定：

```text
assets/beyond/arts/entity/actor/girl/lizhiyan/models/s_actor_lizhiyan_cloth_06_lod0.asset
S_actor_lizhiyan_cloth_06_lod0
```

当前 CAB/PathID 用于本轮源版本的引用核验，不作为永久跨版本匹配规则。

第一个作者部件沿用原 Mesh 与原 SkinnedMeshRenderer 资源身份。第二个作者部件新增一个 Mesh，并在原 Transform 下添加单位局部变换的子 GameObject、Transform 和 SkinnedMeshRenderer。每个 Mesh 只有一个 submesh，每个 Renderer 只有一个材质槽；这两个实际作者部件均使用原生 cloth_02 Material。

| 作者部件 | 顶点 | 索引 | 骨骼槽 |
| --- | ---: | ---: | ---: |
| cloth_06.neiku | 242 | 1,044 | 43 |
| cloth_06.baohsi | 11,844 | 59,238 | 43 |

43 个骨骼槽均引用当前 PFB 的已存在 Transform。bindpose 顺序与骨骼数组按作者调色板写入，保留源 rootBone 与 skinningRoot；没有新增骨架或物理结构。Mesh 局部包围盒、每骨骼空间包围盒及 Renderer 根骨空间包围盒按新几何重建，不能复用只有 116 个顶点的源包围盒。

已更新原生 LODGroup 的 LOD0 Renderer 数组和 Mesh/PFB AssetBundle 的 preload 引用及范围。原有逻辑容器键、资源类型表和外部依赖不改，新增资源通过已有依赖关系及新增 PathID 引用。

cloth_02 的四个已有 Texture2D 资源使用作者 PNG 的 RGBA32 载荷替换。保留原 `.resS` 内容，将新像素与 mip 链追加到尾部并更新序列化 offset/size。基础 Material 原生载荷没有改动。该材质/贴图是共享资源，因此贴图修改会影响其他引用它们的消费者；本轮“cloth_06 LOD0”限定几何和装配范围，并不表示纹理影响被限定在这个 Renderer。

## 独立验证结果

- 88 个 UnityFS Bundle 和 SerializedFile 全部解析成功，2,653 个对象全部加载为离线解析对象。
- 源版本有 2,649 个对象；本轮修改其中 10 个、新增 4 个。其余 2,639 个源对象的原生载荷逐字节一致。
- 88 份类型、脚本、外部依赖及引用类型元数据，除重建对象表之外，原始字节保持一致。
- 39 份原始 `.resS` 前缀共 81,151,856 字节全部保留，已有其他资源没有因追加纹理而被破坏。
- 顶点位置、三角索引、UV、颜色、骨骼索引及 bindpose 完全一致。
- Endfield 打包法线/切线符合当前源顶点布局。法线分量最大误差 0.003442，切线分量最大误差 0.005119。
- 四权重 UNorm16 最大误差 0.000011139；每顶点量化权重总和严格为 65,535。原生序列化 `m_BonesPerVertex=4`。
- 86 个每骨骼包围盒、两个 Renderer 根骨空间包围盒与独立按作者几何计算的结果一致；本样本 float32 误差为 0。
- 子节点父子/组件引用、单位局部变换、两个 Renderer 的 Mesh/Material/43 骨骼 PPtr、rootBone/skinningRoot、LOD 和 preload 范围均通过。
- 四张 2048×2048 PNG 经原生载荷及 AnimeStudio 回读后，RGBA 基础像素与作者 PNG 逐字节一致。完整 mip 载荷大小通过校验。

旧 INI 对 `_MetallicGlossMap` 声明 `linear=false`，与源原生 Texture2D 的线性颜色空间不同。本实验保留未改源材质的原有纹理解释，将它保持为 `m_ColorSpace=0`，并在构建证据中记录声明冲突。Base/Emission 保持 sRGB，Normal/MetallicGloss 保持线性。该策略用于本实验的已有源材质，并不等于所有新材质都应忽略作者颜色空间声明。

写出阶段约 1.96 秒，包含已有快照读取、几何转换、四张纹理处理和 88 个 Bundle 写出。它不包含 Blender 捕获、独立验证或游戏加载耗时。完整依赖副本、RGBA32 和未压缩 UnityFS 合计 239,354,202 字节，这是验证产物，不是最终发布体积或完整导出性能承诺。

## 已排除的工具误报

首次回读缺少 .NET runtime 依赖登记，导致 ZstdSharp 装载失败。已在 Probe 工程补上 AnimeStudio 二进制引用所需的显式包依赖；最终编译为 0 警告、0 错误。

游戏自定义 ClassID 不在 AnimeStudio 枚举中时，公开对象 `type` 会变成 UnknownType。校验已改为核对真实序列化对象表中的 ClassID/typeIndex，没有改变资源类型。

原生 bindpose 载荷是行序，EFFMESH 是 Unity 托管矩阵列序。校验器最初直接比较二者；现已按解包器 `EiemPackageWriter.WriteMatrix` 的相同转序规则进行核对。写出矩阵值无需修改。

失败证据保留在先前 attempt 报告中。校验器会记录失败阶段、具体引用/资源、异常堆栈，以及 AnimeStudio 内部捕获的解析异常。失败不会切换其他写出或替换路线。

## 尚未证明的实施边界

此次离线成功证明的是原生序列化内容可正确写出/回读和这一处 PFB 的原生结构引用成立。游戏资源系统是否接受该 Bundle、是否执行期望的原生 Mesh 初始化、是否登记新增 Renderer，需要接入真实加载入口后验收。离线解析器没有运行游戏，也不能证明私有 native 内存与源初始化结果相同。

本样本只支持 cloth_06 无形态键作者部件；body 的原生 BlendShape 写出、其他作者部件和 32 位索引部件仍需扩展验证。新建 Renderer 的按键显隐仅保留在构建清单里，还没有运行时实例绑定；该部件的 NUMPAD5 控制尚不可在这份离线包里执行。

只更新已证明的原生 LODGroup 和 AssetBundle preload 结构。尚有含 `ReferencedObjectData` 的自定义序列化对象保持原载荷，未知游戏 Renderer 登记列表未改。游戏冲刺 VFX 和通用残影模板能否自动包含新增子 Renderer，不能由层级关系有效推定。UI、NPC、其他 PFB 及其他 LOD 暂未应用本实验变换。

生产路线仍只有一条：在资源加载序列化输入处应用同一 Mesh/装配变换，再交给游戏反序列化、资源登记与初始化。当前离线物化旧 PFB 是写出验证，不作为最终固定消费者白名单发布。F10 的原生资源重新装载/实例重建入口待验证；没有增加运行时 Mesh setter、native 内存复制或失败 fallback。

## 复现

从仓库根目录执行。构建输出目录和报告路径必须为新的路径，工具拒绝覆盖已存在证据。

```powershell
dotnet build tools\EndfieldVfsProbe\EndfieldVfsProbe.csproj -c Release

python tools\nativepack\cloth06_experiment.py `
  --capture E:\EIEM_Workspace\exports\LZY-author-validation-20261002\capture.json `
  --preflight diagnostics\prefab-resource-validation-20261002\LZY-selected-author-preflight-final.json `
  --schema diagnostics\prefab-resource-validation-20261002\native-schema.json `
  --output E:\EIEM_Workspace\exports\LZY-cloth06-native-new

dotnet tools\EndfieldVfsProbe\bin\Release\net9.0\EndfieldVfsProbe.dll `
  --verify-native-pack E:\EIEM_Workspace\exports\LZY-cloth06-native-new `
  diagnostics\prefab-resource-validation-20261002\cloth06-native-new.json

python tools\nativepack\verify_texture_pixels.py `
  --native-report diagnostics\prefab-resource-validation-20261002\cloth06-native-new.json `
  --output diagnostics\prefab-resource-validation-20261002\cloth06-native-pixels-new.json
```
