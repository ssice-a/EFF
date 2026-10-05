# INI 与独立资源目录格式 v2

## 状态与用途

发布格式现为 `mod.ini + meshes/ + materials/ + textures/`。打包器、离线读取/重建验证器及静态资源加载输入提交已接通此格式。形变帧存储修复已有故障场景 GPU 对照；撤销临时诊断后的正式构建仍待启动验收。正式 Blender 内嵌入口、动态控制、动态 VFX 登记和 F10 实例重建尚未完成，详见 [DLL 接入边界](dll-resource-input-validation.md)。

发布内容是作者资源增量与声明，不是可直接传给 Unity 的独立 AssetBundle/PFB。最终生产路线仍在原资源加载输入处重建合法序列化输入，交给游戏反序列化、注册、创建和初始化。没有运行时 Mesh setter、native 内存复制、克隆或另一种格式 fallback。

## 文件与职责

```text
LZY/
  mod.ini
  meshes/*.mesh
  materials/*.mat
  textures/*.tex
```

- INI 是声明的唯一来源：源 Mesh 目标、部件装配、材质/贴图引用、状态、按键、显隐表达式和形态控制。
- `.mesh` 保存预编码的原生几何字段、按槽排列的骨骼路径、形态名及几何计数。它是作者增量载荷，最终原生顶点布局在重建时按当前源契约与 Mesh 功能规范化，不保证载荷字段原字节直接成为游戏输入。骨骼来源候选和校验精度不发布。
- `.mat` 保存原生材质字段增量与类型兼容信息。LZY 当前只修改贴图引用，字段增量为空；贴图引用全部由 INI 声明，不夹带未改材质原字节。
- Texture 的尺寸、格式、mip 数、色彩空间和采样设置直接写进 INI。`.tex` 保存离线解码和生成好的 RGBA32 图像数据。同内容像素只存一份，不同 Texture 声明可以引用同一文件。
- 不再发布 `.texture` 描述文件或编码后的 Texture 字段增量。当前原源字段与 INI 声明通过 `texture_fields.py` 重建原生输入；此步骤不解码 PNG、不生成 mip、不做图像压缩。
- 插件代码负责资源身份解析、原生字段保留、对象表/流重建、Renderer 展开、注册及控制实例绑定。执行策略不写成 INI 参数。
- 来源快照、CAB/PathID、VFS 指纹、精度报告、诊断状态和完整 PFB 验证副本留在工作区。

资源 ID 为 Mod 内局部声明名。当前验证输入使用作者对象名生成可读部件 ID；对象名不变时资源和控制引用不变。正式 Blender 持久部件 ID 尚未接通，不承诺改名后自动迁移手工 INI 修改。

## 最小声明示例

```ini
[Mod]
format=2
id=Example

[Constants]
persist $outfit=0

[KeyForward]
key=NUMPAD1
type=cycle
$outfit=0,1,2

[KeyBackward]
key=NUMPAD2
type=cycle
$outfit=2,1,0

[MeshDetail]
path=meshes/detail.mesh

[MaterialDetail]
path=materials/detail.mat
source.path=assets/example/material.mat
source.asset=MaterialDetail
texture._BaseMap=TextureDetail

[TextureDetail]
path=textures/detail.tex

[PartDetail]
mesh=MeshDetail
material=MaterialDetail
visible=$outfit == 1 || $outfit == 2

[ReplaceCloth]
target.path=assets/example/cloth.asset
target.asset=Cloth_LOD0
parts=PartDetail
lod.1.path=assets/example/model.fbx
lod.1.asset=Cloth_LOD1
```

一个 Replace 可引用多个 Part，各 Part 只有一个 Mesh 和一个基础材质。Mesh/Material/Texture 声明不重复保存 Mesh 替换目标。Material 的 `source.*` 表示原生模板来源；Texture 是材质明确引用的新增资源，不声明 `source.*`，不覆盖原贴图。

显式 LOD 身份来自当前源引用验证，不通过改名称猜测；它们不是 PFB 消费者名单。隐藏的选中源组用 `handling=skip`，不带 `parts` 或原几何。

```ini
[ReplaceHidden]
target.path=assets/example/hidden.asset
target.asset=Hidden_LOD0
handling=skip
```

## 状态与控制

每个 Constants 声明一个状态；`persist ` 为持久化声明。多个 Key 可写同一个变量，数量增加不要求增加变量。`cycle` 的状态集合必须一致，但顺序可以不同；`press` 赋予一个目标值；`hold` 按 `speed` 接近一个目标值。同轮多键按配置声明顺序处理。

显隐条件为无副作用表达式，支持数字、布尔量、变量、括号、`!`、`&&`、`||` 和比较运算；不允许赋值、函数调用或任意代码。读取器一次解析表达式并保存变量依赖；游戏事件绑定和按依赖更新尚未实现。

ShapeControl 声明变量、label、min/max；Part 用 `shape.<通道名>=$变量` 绑定。当前作者形态值以 0–1 为单位，对应原生百分比权重；通道数据及 100% 帧保存在 Mesh 文件中。

```ini
[ShapeControl1]
variable=$shape
label=ExampleShape
min=0
max=1
```

该变量也必须在 Constants 中声明，且至少一个 Part 的实际形态通道引用它。

## 二进制资源契约

资源共同使用 `EFFRES2\0` 头部，固定小端布局 `<8sIIII16sQ32s`，80 字节：magic、版本 2、kind、块数、目录字节数、16 字节原生 TypeHash、总文件大小、body SHA256。

kind：1 Mesh、2 Material、4 Pixels。kind 3 保留且当前编解码器拒绝使用；原 Texture metadata 侧文件输出已移除。Pixels 的 TypeHash 为零。

目录项为 UTF-8 名称（UInt16 长度）与 `<QQ32s>`：文件绝对偏移、块长度、SHA256。数据块按 16 字节对齐。读取器检查整个 body 与每块校验和、重复名称、偏移、零对齐、未引用尾部和类型；不接受任意多余文件。

Mesh 必须包含 `delta_apply.GEOMETRY_FIELDS` 中全部 14 个关联原生字段，以及：

- `@geometry`：UInt32 顶点数与索引数。
- `@bones`：UInt32 字符串数，随后每项为 UInt16 UTF-8 长度和数据；顺序即作者骨骼槽顺序。
- `@shapes`：同样的紧凑字符串表。

原生 UV descriptor 高位标志由重建代码继承当前目标；不发布可切换的策略字段。

对当前已支持的 packed-frame 源布局，无形变通道的 Mesh 保留 stream0 stride16；含非空形变通道的 Mesh 在反序列化前转换成 Float32 position3/normal3/tangent4、stride40。描述符、顶点载荷、流偏移和对齐同步重建，其他流的有效数据保持；已展开输入不再次转换。不增加手工布局开关，当前 `EFFRES2`/INI v2 及已有作者工程继续使用。来源、作者语义与原生存储的具体边界见 [顶点数据契约](vertex-data-contract.md)。

新 TEX 包含 `@texture` 与 `pixels` 两个块。`@texture` 为小端 `<5I>`：元数据版本 1、width、height、编码 ID 4（RGBA32）、mip 数。读取器从 TEX 计算各层字节数并校验载荷大小；INI 不重复保存尺寸、格式或 mip。换用 1K/2K 等不同尺寸 TEX 时，文件内的规格始终生效。旧的仅 `pixels` TEX 可用原 INI 规格读取；重新导出会迁移到新格式。当前格式只支持 RGBA32，DLL 不在线转换图片。

`linear=true` 对应线性解释，`false` 对应 sRGB；`filter`、`wrap`、`aniso`、`mip_bias` 是采样设置。默认值依次为 false、1、0、1、0；导出只写非默认值。这些解释/采样参数可以直接改 INI，无需重算图像数据。尺寸或 mip 改变由 Blender 重新导出 TEX，INI 保持不变。

离线编译器根据材质槽解析游戏的序列化模板，为新增 Texture2D 写入 TEX 尺寸/mip/编码与 INI 色彩空间/采样设置，更新图像数据大小与新流 offset/size。模板来源由编译器内部解析，不是作者配置。

自定义 Texture 声明在原 CAB 中增加独立 Texture2D；PathID 按 CAB、Mod 和声明名确定，不依赖贴图源身份。
原 Texture2D 对象与原流前缀保留，只有 Material 中明确列出的 `texture.*` 指向新对象。
未覆写的槽仍指向原对象，因此同一源纹理可以同时供原版材质与自定义材质使用。
新增对象沿用已有 external FileID，AssetBundle 的原 preload 表插入新引用，并同步所有容器区间。
不同 Texture 声明可以使用同一模板；不能把作者图片写回模板本身而使其他材质一起改变。
本轮真实包的对象、全部 mip 像素、14 个显式绑定与 403 个 preload 区间验证见
`diagnostics/texture-selection-audit-20261004/texture-verification.json`。静态验证不代替游戏画面验收。

这些载荷依赖兼容的原生 schema 和已验证的存储/功能组合。TypeHash 与字段布局兼容只证明结构层条件；新增形变等改变原生处理路径时仍需运行验证。游戏序列化结构或通道编码变化需要调整转换器/加载器；未知布局明确拒绝，当前格式不承诺任意游戏更新兼容。

## 批量装配与后续入口

发布包不枚举所有 PFB。加载器解析当前正在加载的装配引用，以逻辑路径和资产名解析出的实际 Mesh 身份匹配 Replace，展开该使用位置并更新必要的骨骼、LOD、preload 与游戏登记输入。每个对象/Mod 代际只展开一次。

直接 Renderer、NPC 专用装配描述等需要对应的序列化类型适配。初始 Mesh 为空的残影模板无法在加载时直接命中角色 Mesh；新增部件必须进入游戏原有特效登记和捕获。仅接受单 Mesh 的动态消费者仍待验证，不能用另一条 Mesh 替换路径补齐。

截至 2026-10-04，独立 DLL 已接通格式 2 的读取、完整性校验、图链接、静态序列化重建提交及 Renderer 展开；控制绑定与原生刷新仍未完成。导出落盘和读取成功均不是游戏替换验收，抓帧修复版与正式构建验收分别记录。新构建不编译旧运行时资源创建作为兜底，详见 [DLL 接入边界](dll-resource-input-validation.md)。

## 工具与验证

- `ini_package.py`：目录写出及发布前链接检查。
- `ini_reader.py`：严格读取、引用、状态、表达式和资源完整性检查。
- `resource_file.py`：二进制资源编解码。
- `texture_fields.py`：读写 TEX 规格并校验像素，从原生模板重建描述字段，不执行图像解码。
- `state_rules.py`：表达式及共享状态输入语义的离线实现。
- `selected_experiment.py`：沿用实际 14 对象输入，生成原生验证副本及新发布目录。
- `verify_delta.py`：只从发布资源和原源模板重建，逐目标与独立 AnimeStudio 回读的参考结果核对。

```powershell
$env:PYTHONPATH = Join-Path (Get-Location) tools
python -m unittest nativepack.test_directory_package -v
```

完整真实输入的复现命令见 `selected-mod-incremental-export.md`。构建失败写 `failure.json` 到独立验证目录，验证失败写指定诊断报告并返回非零；失败输入不进入部署。
