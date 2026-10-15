# AnimeStudio 解包来源基线

日期：2026-10-02。范围：作者输入的来源与装配保真，不是游戏内替换实现。

## 2026-10-15：重新解包更新已有资源与 Blender 接入

本节契约已接入，具体实现、旧庄方宜工程和源包验证边界见[本轮实现](update-workflows-20261015.md)。

用户最新确定职责：解包资源的更新由 AnimeStudio 负责；对新游戏资源重新解包时，自动更新已有解包输出。Blender 接受来源文件更新并继续使用旧工程；[EFF 修复工具](eff-repair-tool-design.md)只修 Mod，不处理解包资源或工程迁移。

### AnimeStudio

- 按已有解包记录的类型、完整逻辑路径、资产名及所属 Prefab，在当前实际游戏资源中重新定位，更新原输出位置。无需作者逐个重选已解包的角色；首次更新可从旧 `source/manifest.json` 恢复可确认的记录。不以旧 CAB/PathID/物理范围作为跨版本不变身份，也不凭同名猜测。
- 资源未变时可复用已验证输出；变化时重新提取当前资源、模板、依赖和必要骨架数据。已有文件不能因为存在就跳过，重新解包必须更新对应内容，并保持原包路径和资源映射可供旧工程继续解析。
- 清单、SerializedFile 快照、schema、引用及生成的资源文件按同一轮当前来源生成，暂存验证成功后再提交；失败保留上一份完整输出。只修改版本号、摘要或 CAB/PathID 不能代替真实更新。
- 更新范围依据生成文件归属；不删除无关文件。作者手工改过的解包输出需备份并报告冲突，不能静默丢失修改；已退休或无法唯一匹配的资源明确列出。作者自己的成品 Mod 目录不作为解包输出目录处理。

### Blender

- 导出前核对来源清单及内容身份，接受同一解包目录的新输出；刷新资源定位、原生模板、相关 LOD/材质来源与缓存，不要求重新导入角色或另存新工程后才能生效。原目录搬移时保留明确的来源重绑定入口。
- 保留当前工程中的几何、UV、权重、材质参数、形态键、Action、按键组与新增骨链。读取新游戏来源不意味着用新解包 Mesh 覆盖作者正在编辑的网格，也不自动保存 blend 文件。
- 按类型、完整逻辑路径、资产名解析当前 Mesh/Material；Prefab 内无独立路径的组件通过所属 Prefab、完整层级与组件关系核验。跨角色借用的材质来源也须接受更新。CAB/PathID 和缓存键中的旧内容不能盖过当前来源。
- 来源更新期间的一次导出必须读取一致的完整结果；若文件正在被解包器替换，终止本轮暂存导出并保留上一 Mod，不能混用新清单与旧快照。下一次导出读取已提交的新来源。
- 可确定的骨骼槽重排重建映射；资产删除/改名、骨架或 shader 发生不受支持的变化时指出具体冲突，不能承诺所有语义变化都可自动兼容。导出所用全局索引必须与新来源及当前游戏一致，不能刷新模板后仍使用旧工作区索引。

操作目标：AnimeStudio 对新版本重新解包并自动更新已解包文件 → 原 Blender 工程接受更新后的来源 → 导出适用于当前游戏的 Mod。EFF 修复工具不参与这条作者流程，DLL 不承担自动版本适配。

### 已有证据与验收

已对本机 13 份旧源包（含一份 LZY 历史副本）做只读核对：1527 条带逻辑路径的资源记录全部在当前完整索引唯一命中，其中 Mesh 656 条；12 条无独立路径的 Transform 仍需 Prefab 上下文。庄方宜的 44 Mesh、20 Material、40 Texture2D 全部唯一命中。这只证明本次更新可以定位，不证明来源更新和旧工程导出已经通过。报告为 `bin/game-update-repair-20261015/author-source-index-audit.json`；Python 全索引诊断约 9.28 秒，不是正式产品性能指标。

`EiemPackageWriter` 已改为完整暂存验证和备份提交，记录已导出位置及生成文件归属。旧来源模板与当前全局索引随来源一起更新；Blender 的源包锁、重新读取和发布前核对防止混用更新中的来源。实际范围与回归见本轮实现记录。

隔离验收应覆盖重新解包后的原路径复用、内容未变跳过、文件变化与退休、更新失败回退、已修改文件保护、Blender 已打开时更新、缓存失效、跨角色材质、骨骼槽变化，以及旧工程导出与当前来源冷编译对照；核对作者编辑未被替换，不修改或保存用户原工程。

## 当前附加文件用途（2026-10-07）

Mesh、骨架、材质和 PNG 是可编辑的作者资源，`mod.ini` 记录它们的声明和引用。文件名现在优先使用原资源名，只有重名时加八位来源哈希后缀；进一步冲突时追加序号。检查包含大小写和 Windows 保留名称，完整 CAB/PathID 身份仍在来源清单里，旧包不用改名。

| 附加输出 | 当前用途与必要性 |
|---|---|
| `source/manifest.json` | Blender 原生导出器读取资源身份、原始字段模板的位置和作者文件映射，当前必需。 |
| `source/serialized/*.bytes` | `AuthorSource.file/tree` 读取原 Mesh/Material 的序列化字段及类型树，当前不能整体删掉。导出器仍保存整个前向依赖闭包，范围大于实际修改的模板集合，后续可缩减。 |
| `source/streams/*.bytes` | 原始 Mesh/Texture 流载荷副本，供保真核查和诊断。普通 Blender 原生导出不读取；离线编译器从游戏 VFS 取得原始输入，也不依赖这些副本。 |
| `physics/` | 原布料与碰撞组件的字节、结构和字段证据，仅供可选物理导入/检查。普通 Mesh 导出不需要；新增 Physics 的原生导出尚未接通。 |
| `<prefab>.structure.txt` | 层级和组件报告，供人查看，普通导入/编译链不读取。 |
| `export-status.txt` / `export-errors.txt` | 命令行状态或失败诊断，不是运行时 Mod 输入；成功导出不保留旧错误文件。 |

本轮保留这些输出，只调整作者文件命名，没有删除已有来源文件。原始流与物理证据可改为按需导出，序列化模板则需要先缩减保存范围并同步读取协议。这些文件属于解包编辑输入，不要求全量复制到游戏 Mod 目录；序列化与流数据的体积也不能算作 PFB 引用文件本身的大小。

真实 VFS 隔离导出已核对全部清单作者路径及 INI 文件引用，50 份贴图使用短文件名；重名、大小写冲突与短哈希冲突回归通过。没有改动用户原来源包、`.blend` 或游戏 Mod 文件。

## 格式与输出

沿用 EFFMESH v6、EFFSKEL v2、EFFMAT v1 和 PNG 编辑格式。这轮不改 Blender 导入器，也不要求重做已有 `.blend`。旧作者目录不动，重新读取当前游戏 VFS 的源 PFB 后输出：

`E:\EIEM_Workspace\exports\source-baseline-20261002\assets\beyond\dynamicassets\gameplay\actors\postmodels\characters\chr_0032_lizhiyan_postmodel.eff`

新目录扩展名是当前导出器的 `.eff`，旧 `.eiem` 的作者数据仍可供历史核查。工作区索引的旧 EIEM v3 标志可直接读取，数据布局、VFS 指纹与完整记录计数继续校验；不重写旧索引。

## 具体改动

- PFB 根节点从 `AssetBundle.m_Container[].Value.asset` 精确解析，不从同名 GameObject 猜测。层级去重使用 CAB + PathID，避免跨文件 PathID 碰撞。
- 遍历 GameObject 的实际组件表导出 Renderer。几何按 Mesh 去重，但在去重前处理每个 Renderer 的材质，不遗漏另一使用者的不同材质。
- 新增 `source/manifest.json`，格式 `EFFSOURCE` v1。保存当前 VFS 指纹、容器入口证据、逻辑资源路径和精确物理身份、作者文件映射、全部 Renderer 使用记录、层级与组件表。
- Renderer 使用记录包含 Mesh PPtr、Material 槽、bones/rootBone/skinningRoot、包围盒、形态权重、静态批次及 subset 信息。Endfield 解析器原先丢弃的 skinningRoot 现在保留。
- Material 清单记录精确 Shader PPtr 与纹理属性引用，Texture/Mesh 清单记录流数据定位。独立命令行导出不解析 Shader 程序，完整 Shader 原始对象仍保存在序列化副本中。
- `source/serialized/*.bytes` 保存依赖闭包内完整 SerializedFile；对象表、类型树、未知组件、材质关键字/标记等未修改信息直接保留原字节。`source/streams/*.bytes` 保存对应原始流成员，避免仅保存 PNG 后丢失原格式、mip 与流载荷。
- 清单列出文件大小/SHA256、对象范围、类型哈希/schema 数量、external 表及流范围。检查失败不发布成功清单；重新导出开始时移除旧成功清单，避免失败产物被误认为完整包。

这个基线复制本次依赖闭包，用于验证写回所需信息完整。后续可以由公共工作区源缓存提供相同输入，避免多个作者包重复复制；它不是游戏运行内存缓存。

## 本次实测

| 数据 | 数量 |
|---|---:|
| 作者 Mesh | 66 |
| 作者 Material / Texture | 27 / 50 |
| 作者 Skeleton | 1 |
| 完整 Renderer 使用 | 75 |
| 共享 Mesh 的额外使用 | 9 |
| 层级节点 | 569 |
| SerializedFile / 原始流 | 88 / 39 |
| 原始字节总量 | 149,008,140，约 142.1 MiB |
| 重新解析的源对象 | 2,649 |

此前目标集合的 18 个 Mesh、23 个 Renderer 引用全部对应到新清单，包含 body/cloth 的 LOD1 与 shadow proxy。75 个 Renderer 是当前选择 PFB 的完整用法，不表示游戏全体消费者只有这些，也不建立跨版本 PFB 白名单。

当前读取器完整消费全部 66 份 Mesh 和 1 份 Skeleton。与旧导出相比，顶点、法线、索引、submesh、权重、bindpose、骨骼来源和形态载荷一致，骨架数据和 50 份 PNG 也一致；60 份 Mesh 的压缩切线转换存在最后几位浮点差异，最大绝对差 `1.7881393432617188e-7`。没有修改切线解码算法，原始序列化载荷独立保存并校验。

离线重新解析原始副本，核对文件头、完整对象/类型/external 表、实际容器入口、Renderer Mesh/Material/bones/root/skinningRoot PPtr、层级组件和 Material Shader PPtr。全部 external 文件在本次副本中存在。它证明来源数据完整，不证明游戏能加载新的原生替换包、F10 原生重建或新增 Renderer 的 VFX 行为。

## 复现

```powershell
dotnet build tools/AnimeStudio/AnimeStudio.GUI/AnimeStudio.GUI.csproj -c Release -p:TargetFrameworks=net9.0-windows
dotnet tools/AnimeStudio/AnimeStudio.GUI/bin/Release/net9.0-windows/AnimeStudio.GUI.dll --export-eff-prefab `
  'D:\Hypergryph Launcher\games\Endfield Game\Endfield_Data\StreamingAssets\VFS' `
  'E:\EIEM_Workspace' `
  'assets/beyond/dynamicassets/gameplay/actors/postmodels/characters/chr_0032_lizhiyan_postmodel.prefab' `
  'E:\EIEM_Workspace\exports\source-baseline-20261002'
```

验证入口：`tools/diagnostics/audit_author_package.py`、`tools/diagnostics/audit_source_baseline.py` 和 `tools/EndfieldVfsProbe --verify-source-baseline`。证据位于 `diagnostics/prefab-resource-validation-20261002/`；其中 v1 的旧包审计只因旧骨架标志识别错误失败，修正实际 `EIESKEL` 标志后的 v2 审计通过，原文件未迁移。

## 后续边界

来源基线没有写出修改后的游戏兼容 SerializedFile/Bundle，没有注册新资源或扩大 Renderer 装配，也没有改变插件替换路线。后续打包器可以基于保留的原生数据只修改已明确的字段；未知字段保持原字节，仍需证明对象表/依赖写出、游戏资源登记与原生加载成功。运行时形态、显隐、热重载以及冲刺材质传播继续按统一资源路线验收。
