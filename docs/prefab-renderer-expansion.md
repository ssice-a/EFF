# 资源级 Mesh 替换与 PFB Renderer 展开

日期：2026-10-02。状态：用户同意的实现方向，离线引用预检完成；二进制写回、加载接入、新资源注册和 F10 原生重建尚未验证。本文件更新旧文档中“合并为一个多 submesh Mesh、禁止新增 Renderer”的当前设计决策，旧版本代码与实机记录仍作为历史证据。

## 输入和唯一执行位置

一个 Mod 规则将源 Mesh 的逻辑资源/子资产身份映射为若干作者部分。各部分拥有自己的 Mesh、Material、Texture 引用、骨骼绑定和形态通道声明；每个目标 Mesh 只有一个 submesh。不同部分的基础材质可以各不相同，不强制使用原 Renderer 的 cloth1 材质。

目标身份从当前游戏资源索引解析为序列化对象身份。不能按 Mesh 名称单独匹配，也不能把特定版本 CAB + PathID 写死作为跨版本身份。已有 INI 的 source/target 字段到真实容器和子资产的映射须核对；实际资源不一定独立存在为该 INI 描述的 .asset 文件。

唯一生产路线是资源加载输入覆盖：Mod Mesh/Material/Texture 载荷与命中的当前 PFB 装配数据均交给游戏自己的反序列化、注册、创建和初始化。没有 runtime Mesh setter、native 字节复制或克隆 Mesh 的第二条路径。

离线批量预检与生产规则分开：离线按实际引用测试所有能解析的消费者；生产按加载中的当前对象记录匹配 Mesh 身份。UI、NPC、世界、过场均在范围内，不要求本轮已经打开过相应场景，也不以人工 PFB 名单控制覆盖。

## 结构变换

1. 读取源 serialized file 的对象表、类型、external 依赖和原始对象字节。解析 GameObject、Transform、Renderer/MeshFilter、LOD 等已支持的结构；未知未改组件保留原字节。
2. 解析 Renderer 的 Mesh 引用。匹配 Mod 源身份后，生成该 Renderer 的展开计划；同一对象在多个 PFB 根中出现时按文件+对象+Mod 代际去重，避免重复增加部分。
3. 原 GameObject/Transform 和原 Renderer 对象身份保留，原 Renderer 改为第一部分的 Mesh/Material。为其余部分增加必要的 GameObject、Transform、Renderer 序列化记录；静态 MeshRenderer 还需自己的 MeshFilter。这是增加序列化记录，由游戏实例化，不调用运行时 AddComponent 或克隆成品 native Mesh。
4. 新部分的默认层级安排是在原节点下面增加单位局部变换子节点，以继续继承原节点动画/移动，而不复制 Animator、脚本或物理。新名称应保持原 Renderer 类型匹配语义；若当前匹配规则有锚点，则调整序列化分类配置以覆盖派生名称，不能靠含 cloth/body 的随意命名保证成功。具体节点布局还须验证游戏的层级过滤和父引用逻辑。
5. 每部分按自己的 Mesh 骨骼槽构建 `bones[]`，绑定当前 PFB 内对应骨骼与 `rootBone`、游戏的 skinningRoot。不能直接复制原数组而忽略作者 Mesh 索引顺序；现有完整角色骨架可用，不新增骨架/物理功能。
6. 写入各部分 bounds、初始形态权重、有效启用状态和适用渲染标志。更新所属 LOD Renderer 列表、自定义 Renderer 列表或其他已确认的组注册输入；实际效果材质依消费者上下文处理，不能覆盖为普通材质。单值脚本引用不具备自动展开语义，必须识别其控制职责；未知相关引用是明确待验证项。
7. 分配无冲突的新 PathID，建立各组件与 GameObject、Transform 的双向引用、父子关系和资源 PPtr。重新写出变长对象的大小、偏移、对齐、对象表及必需的依赖/加载信息。把增加的 Mesh/Material/Texture 放入同一 Mod 资源包中，由合法依赖引用和游戏资源系统加载。尚未证明新增资源的注册机制，不能把修改路径字符串当作注册成功。

PFB 逻辑资产与物理 serialized file / bundle 不一定一一对应。写出器必须以实际容器边界处理共享对象，不能假定一个 .prefab 文本就是一个独立二进制文件。

## 各端和动态装配

当前直接证据为 `diagnostics/prefab-resource-validation-20261002/prefab-expansion-preflight.json`：

| 样本 | 精确目标 Mesh 引用 | 不同目标资源 |
|---|---:|---:|
| character postmodel | 23 | 18 |
| UI model | 5 | 5 |
| NPC postmodel | 23 | 18 |

这三个样本的引用是 SkinnedMeshRenderer.m_Mesh，已有组件级对象身份和各自材质/骨骼计数。2026-10-02 后续通过 AssetBundle.m_Container 的 AssetInfo.asset 入口指针补查，46 个候选 PFB 全部找到唯一 GameObject 根，1004 个独立 Renderer 的精确目标引用全部归属，层级未解析项为 0。完整清单与证据修正见 `diagnostics/prefab-resource-validation-20261002/prefab-correspondence-verification.md` 和 `mesh-container-roots-v2.json`。这是候选范围内静态装配的完整核对，不证明所有动态消费者覆盖。

历史 NPC 代码还存在 `NPCAvatarMeshAssetsSO` / `SubMeshInfo` 等专用装配描述。对于确实通过这类记录引用目标 Mesh 的消费者，要在同一资源加载输入路线中展开其序列化部件描述，让原生工厂建立多个 Renderer；这属于不同序列化类型的适配，不是第二条替换路线。现有直接 SMR 样本不证明已覆盖所有描述类型或运行时动态引用。

通用 afterimage 模板初始 Mesh 为空，不能给它写死角色 Mesh。新部分需正确进入游戏 Renderer 登记与原生捕获；直接仅消费单个 Mesh 而不经过装配/Renderer 列表的动态消费者，不能被一个多部分规则自动展开。后续需检查这类边界，不能以 fallback Mesh hook 弥补。

## VFX 与 F10

各部分以自己的实际基础材质进入 RendererInfo 登记。冲刺消融的普通代码路径从 `sourceMaterials` 创建 Material 实例，施加效果参数；新增部分沿用原来源 Renderer 的类型/影响范围。部分材质使用游戏已有 cloth2/cloth3 材质是正常输入；各 Shader 的效果兼容性仍需检查。

F10 由插件读取、校验新代际并发起刷新，游戏资源加载/初始化与相应显示装配负责创建。位置、动作和战斗状态是保留目标，具体重建入口仍待验证。短暂隐藏/停顿是资源交接可能的现象，并非已决定的强制步骤。按键显隐和形态权重属于运行时实例控制，不能继续通过改索引的旧实现完成。

## 首次实现步骤和失败诊断

现有 AnimeStudio 能读对象图、原始字节和部分 TypeTree，但未发现已验证的 serialized file 完整二进制写回器。Renderer/SMR 解析器有 Endfield 私有扩展；部分值只读进局部变量，不能通过重建公开 C# 对象来保留全部原字段。写出器应基于已验证的 schema/字段字节范围和原字节保留策略，不按标准 Unity 格式猜这些扩展。

先在离线副本验证：无改动往返完整保留所有对象载荷；单 Mesh 展开样本能重新解析且引用、层级、LOD、骨骼和新增资源关系有效；未修改对象字节不变。世界、UI、NPC 同一规则分别验收后再接唯一游戏加载入口。

日志应包含输入版本/资源身份、匹配对象、Mod 代际、改动字段、新对象 ID、资源依赖，以及具体失败阶段。解析/schema 不兼容、引用歧义、骨骼映射缺失、注册失败和原生读取失败要有可定位原因；不得声称失败诊断已能保证不会闪退。未知相关字段不猜写，不增加另一套执行路径。


## 2026-10-03 离线写出进展

已实现基于当前源完整 TypeTree 的字节保留写出器，用实际 LZY cloth_06 两个作者部件验证两个单材质 Mesh/SkinnedMeshRenderer、子节点、43 槽骨骼调色板、根骨空间 bounds、LODGroup 与 preload 变长重建。88 个 Bundle 已由 AnimeStudio 独立回读，原始未改对象、元数据与数据流前缀全部保持一致，四张贴图的回读 PNG 与作者逐像素一致。详见 [验证结果与边界](cloth06-native-write-validation.md)。

这一实验没有覆盖自定义 Renderer 登记消费者或进入真实游戏加载；原生形态键、UI/NPC/其他 LOD、VFX 捕获新增 Renderer 与 F10 重建仍待验收。
