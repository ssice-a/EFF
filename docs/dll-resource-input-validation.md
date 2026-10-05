# DLL 资源加载入口验证（2026-10-03）

本轮目标是把格式 2 输入与游戏加载入口接通并取得运行证据。`src/eiem_resource_entry.cpp` 是独立验证构建，仅编译新目录读取器、原生输入检查器、IL2CPP 元数据解析和加载观察 hook。它没有链接旧运行时 Mesh 替换、native 字段复制、克隆、draw fanout 或 Renderer 替换实现。

**当前结果：完整 56 行 Mesh/Renderer/骨骼/材质/贴图输入已实际提交。v11 读取到 60 个已装配 Renderer，240 份样本的顶点数、逐槽骨骼路径和绑定矩阵均与作者数据一致；用户仍观察到刺猬状变形，完整替换验收失败。当前部署 v12 原生顶点/权重/索引读回验证版，尚待新运行，根因未确认。下文保留各版本的历史证据。**

## 已完成和已验证

- `[Mod] format=2` 以及 Mesh/Material/Texture/Part/Replace、共享状态、按键和形态控制声明的 C++ 读取与链接。
- `.mesh/.mat/.tex` 的版本、kind、目录边界、零填充、总长度、body/块 SHA256 校验。
- 源逻辑路径与资产名的精确目标索引；native TypeHash 仅检查格式兼容性，不作资源指纹命中。
- Texture 的 RGBA32、尺寸、mip 字节数、色彩空间和采样声明校验；不在游戏中解 PNG 或生成 mip。
- 原生 UnityFS v6/v7 的无压缩/LZ4 读取以及 v22 SerializedFile TypeTree、对象表、AssetBundle 引用表解析。未知/不兼容结构明确拒绝。
- 不从名称相似度推断目标；CAB/PathID 来自当前输入的引用解析结果，不是发布身份。
- 验证真正部署的 LZY：25 个二进制文件、228 个块、13 个部件、26 个 Mesh/LOD 身份，与 Python 独立读取器一致。
- 88 份原版 SerializedFile 的 225 个本地 Mesh/Material/Texture 引用身份与独立 TypeTree 解码一致；所有 26 个目标各只有一个原生候选。
- 88 个**离线写出的无压缩验证 Bundle**可解析。这不证明所有实际游戏 Bundle 压缩/加密格式均已支持。
- LZ4 重叠匹配与非法偏移、hook 签名的 static/instance/参数/返回类型/歧义拒绝、原调用参数/返回值/字节保持、TLS 恢复、保护性内存读取及失败原因日志通过 fixture 检查。

证据：`diagnostics/resource-directory-entry-20261003/verification.json`、`native-target-match.json`、`smoke.txt`。

## 运行入口与线程边界

依照实际 IL2CPP 元数据，逐个验证完整参数、返回类型与 static 标志后安装 hook。地址缺失、重载歧义和 native 地址共用均明确记录，不猜测 ABI：

- `Beyond.VFS.VirtualFileSystem.LoadBundleFromFile/Async` 的两个 `StringPathHash` 重载；有位置输出参数的重载保持原引用与 CRC。
- `GetAssetStream(string)`、两个 hash 重载、`VFSFileReadStream.Read(byte[],int,int)` 与 Span 重载。
- Unity `AssetBundle` 内存、文件、流加载的原生托管入口。

hash 钩子还检查实际值类型、8 字节大小、已支持的对齐值（1 或 8），以及唯一 64 位实例字段的 boxed offset。Windows x64 的 8 字节聚合参数使用整数寄存器传递，游戏本轮报告 `size=8, alignment=1, Int64 offset=16`。检查失败时不安装，不能把 hash 当成字符串指针。路径只来自游戏自身实际调用的 `get_path` / `GetMappingStrByHash` 返回值；插件不额外调用 getter、不重算 hash。缓存上限 4096，冲突标记为歧义，未知值保留 `<pathhash:...>`，不据此匹配 Mod 资源。

观察路径返回原调用结果；不调整 CRC、loader position、buffer、stream 位置或资源指针。VFS 路径只在嵌套的同步调用栈内关联，异步脱离该上下文的输入明确显示 `<no-vfs-context>`，不把线程相关性当成资源身份。

分段 Read 只取首部样本，**不能拼成完整资源或用分段字节进行替换**。采样仅保留加载调用栈内的数据或识别出的 UnityFS 首部，普通启动配置读取不消耗采样额度。只有完整、识别为 UnityFS 的托管内存数组才复制进有界队列；字节副本与不可变 Mod 图代际由 worker 持有，解析及落盘在后台完成。每个输入上限 64 MiB，队列上限 128 MiB，单次启动完整样本上限 12；超出时不会扩大采集。不扫描场景，不 hook 渲染帧循环。

本修正版保留上轮游戏启动验证过的三秒等待及“校验/发布 Mod，然后安装钩子”的顺序。DLL 句柄可用、domain 非空和程序集可枚举都不等于 VFS/热更初始化完成；没有单独验证明确就绪条件之前，不提前安装。固定等待也不是绝对就绪保证，当前还未实现 VFS 生命周期门禁。

`v3-lifecycle` 的初始化线程只在读取 IL2CPP 元数据及安装钩子期间 attach，安装结束后主动 detach，再进入 Win32/C++ 轮询。异常及提前返回由同一 RAII 附件释放；缺少 detach 导出时拒绝 attach。原生解析 worker 不访问 IL2CPP。此修补消除后台轮询线程永久注册的生命周期缺陷，但不等于已证明用户报告的退出挂住由它造成。

日志允许共享读取、禁止并发写入，输出后立即 flush；回归测试在写句柄保持打开时，通过独立 Win32 读句柄检查内容，并验证第二个写句柄不能截断活动文件。每个进程按 PID 与创建时间保留独立 `plugin/resource-input-diagnostics/run-*/entry.log`，使用 `CREATE_NEW` 防止重开覆盖。`eff_log.txt` 是可用时写入的兼容镜像；镜像被另一进程占用时仍继续记录独立日志。每行包含 PID/TID；完整输入副本也放在对应 run 目录。

新验证版的 `eff_log.txt` 包含：

- `ENTRY-PACKAGE/GENERATION`：新格式校验与发布，或准确拒绝原因。
- `ENTRY-META/HOOK/READY`：实际函数签名、安装结果。
- `ENTRY-LOAD/INPUT/COUNTERS`：真实入口、字节首部和调用数量。
- `ENTRY-CAPTURE/MATCH`：完整输入的独立解析以及路径/资产候选；始终标明 `replacement=0`。

采集副本位于游戏 `plugin/resource-input-diagnostics`，不属于 Mod 发布内容；不会影响源 Mod 文件。

## 当前边界

- 已实现完整输入图重建及提交，v11 实际观察到当前角色 PFB 的 60 个部件 Renderer。材质/贴图完整输入图已部署，视觉效果仍未通过验收。
- 动态 null-Mesh/VFX 消费者，以及世界、UI、NPC 场景覆盖尚未完整验收。
- 未绑定按键显隐和形态控制到运行对象。
- F10 保持**刷新请求**：本验证版重读并校验目录输入；原生资源/实例重建尚未实现。它不是关闭 Mod 按键，不借旧替换路径实现刷新。
- 当前主要失败为完整替换的蒙皮变形。v11 已确认 Mesh 实际装配，不能再把该轮问题解释为未部署；v12 保留完整图，以仅诊断的 CPU 数据保留标记取得逐顶点原生读数。

## 首次重启结果与 hash 修正

`resource-directory-entry-v1` 在 PID 30596 的实际日志确认 LZY 的 13 Mesh、13 Part、5 Material、11 Texture 声明、7 规则和 26 目标均通过校验。最终计数为 5950 次 Read，完整捕获为 0。四个 VFS bundle 钩子因期待 `System.String`、实际为 `Beyond.Resource.StringPathHash` 而被签名检查拒绝；因此这轮不能证明 bundle 输入入口。启动 JSON 还耗尽了通用采样额度。

日志已归档到 `diagnostics/resource-directory-entry-20261003/restart-v1/eff_log.txt`。静态调用分析看到 VFS 转入另一个托管封装，并含继续调用普通 Unity 文件加载的分支及热更分发分支；这是当前磁盘二进制的调用结构，不能当作游戏本轮实际执行的证据，也没有据此安装固定 RVA 钩子。

`resource-directory-entry-v2-hash` 修正 hash 签名及 ABI 门禁，增加被动路径映射、两个 hash stream 重载并收窄 Read 采样。曾构建过提前安装的候选，但用户指出初始化过早的闪退风险后，已恢复原来的等待和发布顺序；提前安装候选未部署。回归 fixture 先复现旧签名拒绝，再验证修正、ABI 拒绝、完整 64 位 hash/位置引用/CRC/MethodInfo 转发、输出保持、嵌套 TLS 恢复、映射冲突以及配置读取不消耗额度。游戏是否实际接受这些钩子及其捕获结果仍须新一轮启动确认。

## 第二次启动与生命周期修补

PID 21300 的 `v2-hash` 日志归档于 `diagnostics/resource-directory-entry-20261003/restart-v2/eff_log.txt`。LZY 继续通过校验，但对齐值 1 被原来要求 8 的门禁拒绝，仍只安装 6 个旧观察钩子；最终 5950 次 Read、0 次 VFS bundle 观察、0 个完整捕获。配置采样不再耗尽额度，但尚未证明 bundle 入口。

用户报告退出时曾需任务管理器结束进程，随后反馈进程已正常退出。没有取得挂住时的线程栈，原因尚未锁定。代码检查确认初始化线程永久 attach、原生循环不 detach；fixture 先复现未释放附件、运行时日志读锁，以及 packed hash 被拒绝，再逐一修补并通过。

`resource-directory-entry-v3-lifecycle` 保留三秒等待和初始化顺序，释放 IL2CPP 附件、允许实时读日志并支持实际 packed hash ABI。测试覆盖仅 attach 一次、主动释放、重复释放保护、启动异常释放、缺少 detach 时拒绝注册，以及日志写期间的真实文件共享；游戏正常退出和新 hash 钩子接受情况仍待实际验收。未实现资源替换、Renderer 展开或原生 F10 重建。

## 第三次启动与公开加载入口修正

`restart-v3/eff_log.txt` 保留了 PID 33836 的 BUILD 行及后段日志，最终计数为 `vfs=38782 reads=5950 memory=0 file=0 stream=0 samples=0 captures=0 generation=1`。这确认 hash VFS bundle 观察已执行，但没有取得完整输入或逻辑路径映射。启动段偏移 130–27712 是 27,582 个 NUL 字节，包校验、钩子安装和 detach 记录缺失，不能声称本轮完整记录了这些步骤。BUILD 的时间晚于后段计数所需的初始化时间，也不能假定整份文件都来自同一进程。

用户反馈“退出卡住似乎已经修复”，复核时游戏进程不存在。这记为本轮正常退出及表现改善；未取得此前挂住的线程栈，退出根因和长期稳定性仍未证明。保留 v3 的线程生命周期修补。

日志测试复现 `_SH_DENYNO` 下第二个写句柄能截断活动日志，原句柄继续写时产生空洞。实际重开文件的来源尚未识别；进程重叠、重复初始化或外部截断都尚未被实际记录区分。`v4-load-seam` 的独立日志、排他写和 PID/TID 记录防止这个文件竞争模式并保留后续证据，不把 fixture 当作现场来源证明。

离线 PE/metadata 映射验证下游封装的真实 owner、token 与 CodeGenModule，分别为 `UnityFileLoaderHelper.ReadAssetBundle/Async`、`UnityStreamingFileHelper.ReadStreamAssetBundle/Async` 和 `UnityPersistFileHelper.ReadPersistAssetBundle/Async`。调用图还确认公开 `UnityEngine.AssetBundle.LoadFromFile(string,uint,uint64)` 的 AOT 方法直接跳到解析后的 native icall，不必经过 `_Internal` 的托管方法地址。这个静态结构解释了原观察面可能漏记，不能证明上一轮运行选择了哪条分支。

`resource-directory-entry-v4-load-seam` 通过实时元数据完整签名解析安装上述六个封装和两个 Unity 公开入口的观察；生产钩子不依赖本文件的 RVA。封装中的 `EFileLoaderPosType` 只在实际值类型、大小 1、对齐 1、唯一 Byte 字段 boxed offset 16 的门禁通过后使用 byte ABI。记录原目录/文件名、offset、CRC、pos、调用返回指针及嵌套 VFS 上下文；不组合猜测物理路径，不额外调用游戏 getter，不读取/修改 loader 位置，不把异步 request 返回当作完成。

配置 hash stream 和 bundle 事件改用独立的 160 次额度；六个封装各记录前 32 次，公开文件入口每种记录前 96 次，其余调用只计数。回归检查覆盖日志竞争拒绝及镜像一致性、独立额度、byte enum ABI 拒绝、六个封装/两个公开入口的完整参数和返回保持。保留三秒等待及校验/发布先于安装的顺序；新观察面的实际执行结果须下一轮启动日志确认。依然 `replacement=0`。

证据：`restart-v3/result.json`、`writer-repro.txt`、`seam-smoke.txt`、`downstream-method-owners.json`、`file-provider-owners-extra.json`、`public-unity-file-call.json`。独立只读 owner 映射工具为 `tools/diagnostics/map_il2cpp_method_owners.py`。

## 第四次启动：真实文件范围与 Endfield 格式

主进程 PID 18700 的独立日志完整保留：LZY 包校验成功、22 个 hook 安装、两个实际 ABI 门禁通过、初始化线程 detach；文件没有 NUL。最后主进程计数 `vfs=38392 publicFile=19196 helperSync=6645 helperAsync=12551 streamingSync=6066 streamingAsync=12484 persistSync=579 persistAsync=67`。`_Internal` 文件计数仍为 0，公开文件观察却执行，证实了前轮观察面遗漏。所有返回只作为同步返回/异步 request 指针记录，未检查 request 完成。

独立日志还保留第二个 PID 24520 的 BUILD，`mirror=0`，说明本轮存在重叠进程写日志，排他镜像正确拒绝第二个 writer。它不反向证明 v3 的日志破坏一定由同样来源造成。主进程后续已结束；本轮未取得退出线程栈，仍不把退出根因当成已证明。

公开入口传入真实 `.chk` 路径与偏移，192 个唯一首部均非 UnityFS。当前 release 格式匹配 AnimeStudio 的 `VFSUtils`，包含混排头、编码后的目录/块表、非标准 SBox 的 XOR stream 以及逆序 LZ4 token/offset。`src/eiem_endfield_input.h` 只支持实际验证的 `encFlags=8` revision、目录 mode 0/2/3 与数据 mode 0/5，其它 revision/结构明确拒绝。输入上限 64 MiB、目录 16 MiB、总解码 512 MiB，所有范围/块 suffix 必须一致。

独立文件范围读取从 loader **已经传入的路径和偏移**取得首部，再按解码出的长度读完整范围；使用自己的共享只读句柄，校验文件 identity/size/修改时间不变，不移动游戏句柄。不声称这是 native loader 读取缓冲的所有权，也不据磁盘快照声称游戏接受了重建输入。release 的 64 位 size 必须 `ROL64(14)`；现有 AnimeStudio importer 的 header `ROR64(18)` 给出错误 size，但它通过块表读取成员，因此成员解码仍可用作独立对照。新的 size 公式与 VFS catalog 长度和实际完整块 suffix 一致。

12 份实际调用范围快照以及当前目录中的 8 份 LZY 目标输入，合计 20 份，DLL C++ 解码成员的名字、长度和全部字节与 AnimeStudio 独立解码一致。C++ 原格式 writer 把相同成员放入原格式无压缩块，20 份重写结果也被 AnimeStudio 解出完全相同成员；这只是离线容器重写检查。回归还检查多成员/raw stream、inverse LZ4 overlap、非法首部/截断/未支持 revision/超额 count，以及独立读句柄的越界拒绝。

8 个目标范围从当前 VFS catalog 解析，读取时检查实际 CHK 存在及长度；Persistent catalog 有 stale 缺失 CHK 条目，选择存在的 StreamingAssets 当前候选。然后在解码资源引用后用 `target.path + target.asset` 匹配，26 个目标均唯一、class/type 兼容。这个离线检查不证明主进程加载了这些具体范围。

`v5-endfield-input` 在公开文件入口只复制 path/offset 进队列，worker 执行独立读取和解析。临时 `plugin/resource-input-diagnostics/snapshot-ranges.tsv` 为 8 个已校验的目标范围保留额度，另有 4 份普通样本，总上限 12；重复范围不重复入队，选中范围的当前长度必须仍一致。它仅选择诊断副本，位于 Mod 外，不是资源匹配身份、PFB whitelist 或替换后端。后续实际目标观察需要新启动日志。保留三秒等待、包发布先于 hook、线程 detach、F10 刷新语义及普通代理；依然 `replacement=0`。

证据：`restart-v4/entry-live-080347.log`、`observed-input-ranges.json`、`endfield-reader-verification.json`、`selected-current-native-match.json`、`selected-current-range-verification.json`、`endfield-writer-verification.json`、`endfield-smoke.txt`。原始范围和两套独立成员解码仅存诊断目录，不发布到 Mod。

## 第五次启动：目标输入命中与 Mesh 内容 writer

PID 42588 的 `v5-endfield-input` 启动日志完整记录 LZY 校验、实际 ABI 门禁、22 个 hook 安装和启动线程 detach，没有 NUL。公开文件加载计数为 19,388，`_Internal` 文件计数仍为 0。12 个独立磁盘输入快照全部完成：4 个普通启动样本以及 8 个目标样本；目标样本中按当前解码后的逻辑路径与资产名唯一命中全部 26 个 Mesh/LOD 身份。独立 CLI 再解析这些实际副本，结果为 missing=0、ambiguous=0。它确认目标范围确实经过本轮加载入口，仍不代表插件拥有 native read buffer 或游戏接受了替换。

`src/eiem_serialized_writer.h` 使用当前 TypeTree 严格读取/重写字段，保留原标量位模式和可保留的 alignment padding；变长字段使后续位置变化时重新计算 padding。对象表保留原 PathID、type index 和原有记录顺序，未修改对象逐字节保留，类型表和对象表后的 external/ref-type/user 元数据保持原字节。新增对象只用当前类型，不猜 class/schema。无修改时返回完整原输入，不重编码容器。

`src/eiem_mesh_input_builder.h` 用逻辑路径与资产名匹配当前 Mesh，对已发布的 14 个几何字段按当前 schema 解码；保留当前 native UV channel flag 高位。每个部件独立生成一个单 submesh Mesh，新增 PathID 由 CAB/Mod/目标身份/部件声明确定并检查碰撞；它不是匹配资源的指纹。新增 Mesh 加入当前 AssetBundle 的 preload，并重算 container/hash container/main asset 的 preload 区间。原 input 的 CAB/成员名/flags 不变。`handling=skip` 保留共享 Mesh，消费者的 Renderer 停用仍待 PFB 层完成，不删除共享资源。

对这 12 个实际副本，C++ writer 生成 47 个部件 Mesh，包含 22 个原 Mesh 的内容重建及 25 个新增 Mesh；另有 4 个 AssetBundle 对象更新 preload。4 个 skip 目标不改 Mesh 内容。AnimeStudio 独立解码重建后的 Endfield 容器，既有 Python writer 使用逐字节相同的当前基线重建每个对象；验证全部对象的 type/PathID/payload、原对象顺序、metadata 前后段、对齐/范围和无改动输入，56 个原对象保持字节一致。新增对象的表内排列不同会改变偏移，不作为资源身份；检查器独立解析对象表后比较每一个对象的完整字节。

新增回归覆盖变长字段导致的后续对齐变化、非零 source padding/NaN 位模式保留、字段 suffix/非法 array count、对象 ID 碰撞/不存在的修改目标、Mesh type hash/geometry count/越界 index 拒绝，并继续通过已有调用参数/结果保持、日志文件共享和线程附件释放检查。原始输入与输出只存在诊断目录，不发布原 bundle/PFB。

`v6-mesh-input-writer` 将同一 writer 接入原生诊断 worker，从 job 的不可变图和输入副本重建并写入 session 的 `mesh-input-rebuild/`，输出 `[ENTRY-REBUILD] stage=mesh-content-only nativeSubmission=0 rendererExpansion=0 dependencyClosure=0 replacement=0`。它不调整正在执行的 loader 参数，也不提交这些尚未完成 Renderer/Material/Texture 依赖闭包的输出。该新 worker 分支的实际游戏执行仍需下一轮启动记录，不能用离线 CLI 代替。三秒启动等待、先校验/发布后安装、detach、普通代理和 F10 刷新语义保持。

证据：`restart-v5/result.json`、`runtime-native-match.json`、`cpp-mesh-input-verification.json`、`mesh-writer-smoke-final.txt`。独立对照脚本：`tools/diagnostics/verify_cpp_mesh_input.py`。

2026-10-03 08:58:17，在再次确认 Endfield 进程数为 0 后备份并部署 v6：`eff.dll` 483,328 bytes，SHA256 `9246F4D1FCC75B2BC112DF0B33C2EBDD07476461B0A4004C751D067E9883A72F`。备份位于 `plugin/resource-export-staging/dll-before-mesh-input-writer-20261003-085817/`。部署前后 LZY 的全部 26 个文件及普通代理哈希均一致。部署记录为 `restart-v5/mesh-writer-deployment.json`；v6 游戏运行验收尚未发生。

## 第六次启动：游戏进程内重建结果

`v6-mesh-input-writer` 主进程 PID 21444 的完整 session 记录：包通过校验、22 个 hook 安装、启动线程 detach、12 个输入快照完成，以及 26 个目标身份唯一命中。09:01:35 的 8 条 `[ENTRY-REBUILD]` 全部成功，共 47 个 Mesh 部件、26 个原对象改写（22 个 Mesh 与 4 个 AssetBundle preload）、25 个新增 Mesh；4 个 skip 目标不改共享 Mesh。所有实际 worker 输出及其原输入，分别与 v5 已独立验证的输出及输入逐文件 SHA256/长度一致。没有 NUL 或重建拒绝记录。因此游戏进程中的后台 Mesh 内容 writer 已执行并得到了独立验证的字节结果，**仍没有提交给 native loader、完成 Renderer/材质/贴图依赖闭包或游戏资源替换验收**。

本轮还存在 PID 35904 的独立 BUILD/LOG 记录，镜像 `mirror=0`。该 session 未记录 hook/READY，不假定它完成了初始化或加载。复核时 Endfield 进程已不存在；没有取得退出方式或线程栈，不能把进程结束单独当成退出根因或长期稳定性证据。

证据：`restart-v6/entry.log`、`run-35904-startup.log`、`result.json` 和 `mesh-input-rebuild/` 的完整输出。

## Material/Texture 内容 writer：当前输入离线验证

v6 运行证据核验后，`src/eiem_texture_input_builder.h` 补齐原生材质字段和 Texture2D/原始 stream 的内容构造。独立 AssetIndex 从当前输入的 AssetBundle 引用、对象名称和元数据产生，使用完整逻辑路径与资产名匹配，检测多候选歧义。当前 LZY 的 5 个 Material 均采用原有贴图引用：14 个声明属性唯一存在、pointer 解析到当前图中对应的 Texture2D；原 pointer、preload、shader 和其它字段保持原字节。更换成未经 preload 注册的新贴图依赖明确拒绝，不能把完成这 14 个原依赖检查当成通用新增材质依赖闭包已实现。

11 个 Texture2D 使用当前 TypeTree 更改 INI 声明中的尺寸、格式、mip、色彩空间和 sampler，向对应当前 CAB 的 `.resS` 追加已导出的 RGBA32/mip 字节，保持原 stream path 和完整原有 prefix。stream 必须本地存在且唯一，当前每个 source range 都对**原始 stream 长度**检查，不能因之前追加作者数据而让错误原范围变得有效。原 platform blob/inline image 必须为空，imageCount=1、dimension=2；未知结构拒绝，不解图片、不在游戏里生成 mip。

从当前 VFS catalog 找到 8 个 Material/Texture 输入，独立句柄取得快照并校验实际 header 长度。它们是本轮后续的离线当前文件读取，**不是新取得的游戏加载观察**。解出的 SerializedFile 和 resS 与现有独立验证基线逐字节一致；C++ 生成的对象表、每个 Texture 的完整 payload 和 stream 的所有字节与既有 Python TypeTree/texture writer 对照一致，另由 AnimeStudio 解码实际 Endfield 输出容器。5 个 Material 输入完全不变，11 个 Texture 重建，13 个原对象和 44,739,536 bytes 原 stream 前缀保持原字节。共享作者像素在各自 native texture 中重建，合计追加 208,317,084 bytes；不把 Mod 的 7 份去重 pixel 文件误写为 7 个 native Texture。

最大当前作者 Texture 输入重建后为 135,623,924 bytes。现场快照、队列、默认 ReadHeader/Decode 的 64 MiB 上限保持；仅经过包校验的 Texture writer 与其输出自检显式使用 256 MiB 输出界限，仍小于既有 512 MiB 总解码界限。默认解码拒绝大于 64 MiB 的输出的回归检查继续通过。此接口不是扩大现场任意资源采集的许可。

回归还检查缺少 stream、原 stream 越界、INI 与 pixel byte count 不同、缺少当前目标 identity/property、新材质依赖未登记的拒绝；既有日志/生命周期/hook 参数结果检查继续通过。`resource_directory_check --texture-rebuild <Mod> <input-directory> <new-output-directory>` 运行同一 C++ writer；全部结果仅写到诊断目录，已部署 v6 DLL 和 Mod 没有因本阶段修改。

证据：`restart-v6/material-texture-current-ranges.json`、`material-texture-snapshots.json`、`cpp-texture-input-verification.json`、`texture-writer-smoke-final.txt`；独立对照脚本 `tools/diagnostics/verify_cpp_texture_input.py`。Renderer/PFB/LOD 展开、骨骼路径重新绑定、动态登记、输入递交所有权与游戏验收、显隐/shape/F10 的运行对象重建仍待完成；不以贴图离线通过替代这些步骤。

## Renderer 与原生加载候选：v7

`src/eiem_renderer_input_builder.h` 已实现当前 SerializedFile 中的 SkinnedMeshRenderer 展开：按实际 Mesh PPtr 与当前逻辑身份匹配，保留原 Renderer/owner/Transform，额外部件增加独立 GameObject、单位局部变换的子 Transform 和 Renderer。各自绑定一个 Mesh/Material、作者顺序的骨骼、初始显隐、shape weights 和包含同时形态变化的 rootBone 空间 bounds，保留原 rootBone/skinningRoot 与其它渲染字段。skip 停用消费者并移除其 LOD 引用，不删除共享 Mesh。新增对象和全部部件 Mesh/Material/骨骼加入原 Renderer 的 preload 区间，更新 container/hash container/main asset 区间与 LODGroup。

骨骼路径通过 Renderer owner 与 rootBone 的共同祖先、完整作者路径和唯一 palette 确定相对锚点，兼容角色嵌入过场场景的情况。不能把场景顶层当成角色根，也不能只匹配骨骼名称。原有 GameObject/component 和 Transform 父子引用在展开前校验；缺少/歧义骨骼、循环、未登记的外部 Material、未重建 Mesh、缺少 schema 或 PathID 碰撞均拒绝。

`prepare_resource_input_graph.py` 从 Mod 的 42 个完整逻辑身份和索引依赖反向可达关系定位 16 个资源输入及 52 个消费者候选，不按 prefab 名称设置白名单。当前 StreamingAssets VFS fingerprint 与 locator index 一致；Persistent 的有效现存覆盖优先。定位结果只是候选，C++ 从实际输入重新解析名称、路径、CAB、PathID 和引用。68 个独立句柄快照均与当前目录中的范围/长度一致。

52 个消费者输入全部由 AnimeStudio 独立解码并导出当前 schema/对象表，再由独立 Python writer 重建所有预期对象。2619 个部件 Renderer、4191 个新增对象、46 个 LODGroup、105093 条追加 preload 引用及 189909 个骨骼槽的完整字节一致，279766 个未修改对象保留原字节。1440 个目标消费者中 218 个由 skip 停用。第一次仅验证角色 postmodel 的 33 个 Renderer/60 个部件是该完整结果的子集，不能叠加计数。空 Mesh 的动态模板仍不在这些静态引用结果中。

同一 C++ writer 将 Mesh、Renderer 和 Material/Texture 组合成 68 个最终输入，其中 56 个变化、12 个完整输入不变。每个最终文件的 SHA256 分别与已独立验证的 Mesh/Texture 输出或本轮消费者输出一致。包含 47 个 Mesh 部件、2619 个 Renderer 部件、5 个 Material 声明和 11 个 Texture。原始 PFB/bundle 不进入 Mod；生成文件仅进入 `plugin/resource-export-staging/v7-prepared-inputs-20261003/` 缓存。

`src/eiem_loading_input_substitution.h` 校验提交表、完整 Mod 代际 digest、当前原输入及缓存 SHA256、文件 stamp、CAB/member identity 和 flags。v7 在已验证的原生 `LoadFromFile`/`LoadFromFileAsync` 公共入口将当前物理范围转交为缓存文件 offset=0，保留原 MethodInfo、CRC=0 及原函数返回值。非零 CRC、缓存/源文件改变或包代际更新时拒绝选择该缓存并保持原调用参数。缓存文件和强 GC handle 的路径字符串保留到进程结束，以覆盖异步文件读取；不修改未拥有的 loader buffer。提交表仅在同步、异步及完成/asset 观察 hook 全部安装后发布。

`[ENTRY-SUBMIT-RETURN]` 仅说明原调用返回，异步 request 非空不证明完成；`[ENTRY-SUBMIT-COMPLETE]` 观察 request 的 assetBundle getter；`[ENTRY-SUBMIT-ASSET]` 观察来自提交 bundle 的 LoadAsset 返回。它们仍不证明新增 Renderer 被游戏登记或视觉正确。三秒等待、先包校验/发布再安装 hook、启动线程 detach、普通代理保持。

回归检查通过同步/异步路径、offset/CRC/MethodInfo 和返回值转交、异步 bundle/asset getter 参数结果保留、失效缓存和代际拒绝，以及上述 Renderer/骨骼/LOD/preload 检查。当前 Mod 26 个文件与普通代理部署前后 SHA256 一致。

已在确认 Endfield 进程数为 0 后部署 `resource-directory-entry-v7-prepared-input-submission`：`eff.dll` 512512 bytes，SHA256 `D3C15D435A1EB8AA389076C98AC1709E022F3C13A8EAEE9775050DF305DBCC3C`。原 v6 DLL 备份位于 `plugin/resource-export-staging/dll-before-v7-prepared-inputs-20261003/`，56 条提交表安装在 `plugin/resource-input-substitutions.tsv`。**v7 尚未取得游戏进程运行、native 提交接受或显示验收。** 运行按键、原生 F10 重建、动态 afterimage/VFX Renderer 登记仍未完成；F10 的包代际刷新会使旧准备表失效，不等于完成原生重建。

本轮证据：`restart-v6/current-input-candidates.json`、`current-input-graph-ranges.json`、`current-input-graph/snapshots.json`、`cpp-current-renderer-input-verification.json`、`prepared-loading-inputs.json`、`v7-deployment.json`、`v7-smoke-result.json`。

## 第七次启动：v7 闪退与原始输入对照

PID 34648 的 session 确认 v7 校验并发布了 56 条准备表，24 个 hook 安装完成。10:21:48 提交了 11 个异步输入；返回值是 request 指针，尚未观察到 bundle 完成或资产加载。`Player.log` 随后报告 `input-029.vfs` 与较新的 Unity runtime 不兼容。Windows Application 1000/1001 和 Unity dump 确认 `unityplayer.dll+0x2CDBC9` 发生 `0xC0000005`；异常上下文中 RCX=0，读取地址为 0x80。不能把请求返回或此前离线解析通过视为 native 接受。

原始和改写的 `input-029` 成员均为 `2021.3.34f5`，AssetBundle 的 `m_RuntimeCompatibility` 均为 1。因此日志中的兼容性错误不能直接归因为 Unity 版本字符串不同；容器重编码、SerializedFile 改写、路径转交仍需区分。崩溃栈中的空指针也不能单独证明哪个 writer 是根因。

原始提交表已移到工作区 `diagnostics/resource-directory-entry-20261003/v7-crash-34648/resource-input-substitutions-v7.tsv`，两个 session、Player.log、Unity/WER dump 和 v7 DLL 均已保存。56 个模型/贴图/消费者替换不再启用，Mod 的 26 个文件及普通代理未修改。

v8 增加显式的单范围 loading control。表头版本 2 的最后一项只能为 `raw-copy` 或 `reencoded-unchanged`：前者要求完整输出与原输入逐字节一致；后者要求完整容器字节有变化、每个解码成员及身份/flags 均不变。每张 control 表仅能包含一个范围。版本 1 仍要求输出有变化；校验 digest、当前源范围、路径、大小、stamp 和包代际的规则保持。控制日志明确记录 mode，READY 区分 `replacementArmed` 与 `controlArmed`。

`prepare_loading_input_probe.py` 从观察记录中的物理路径/offset 重新取得源快照，校验当前 SHA256，再分别生成两种对照。已安装的只有 `input-029` 的 raw-copy：原始 64,142 bytes 原样放到缓存，从原 offset 282221658 转交为 offset 0。重编码但内容不变的 89,872-byte 对照仅暂存，须先取得 raw-copy 运行结果再测试。该顺序区分路径/hook、容器和内容改写，不能同时启用全部候选。

已确认游戏停止后部署 `resource-directory-entry-v8-loading-input-control`，DLL 为 514,048 bytes，SHA256 `9B3D8404F61C19DA66D7F303B98974D441968277785BAB297F05A846F9E5BB96`。新增 control 校验及既有生命周期、同步/异步参数、CRC/stamp/代际拒绝、Mesh/Texture/Renderer 回归均通过。**这些检查仍未证明闪退修复；下一次启动需要进入此前触发加载的场景，确认 raw-copy 是否实际提交及是否再次失败。**

证据为 `v7-crash-34648/result.json`、`v8-deployment.json`；游戏缓存分别为 `plugin/resource-export-staging/v8-raw-control-20261003/` 和 `v8-reencoded-control-20261003/`。v7 DLL 另备份在 `dll-before-v8-loading-control-20261003/`，v6 备份仍保留在 `dll-before-v7-prepared-inputs-20261003/`。

## 原始字节对照：正常进场、手动退出

v8 raw-copy 的主进程 PID 40972 完成包校验、24 个 hook 安装及启动线程 detach。10:58:08.683 从同一原始范围转交缓存 `original.vfs`，返回非空异步 request；submitted=1。该缓存与当前源范围的 64,142 bytes、SHA256 完全一致。用户确认正常进入场景后手动退出；Player.log 没有上一轮 AssetBundle 兼容性错误，没有新 Unity crash dump 或 Windows Endfield crash 事件。此轮行为对照通过，路径转交本身没有复现闪退。

已有 getter/LoadAsset 观察仍显示 bundlesReturned=0、assetsReturned=0，不能把这两个计数当作原生对象接受的证明，也不能由用户进场推出全部模型替换有效。它仅为这个原始字节范围和当前进场路径的正常行为证据。另一个 PID 39052 的 session 只有 BUILD/LOG，镜像为 0，不假定它完成了初始化。

保存原始对照的两个 session、Player.log、表文件及结果到 `v8-raw-40972/` 后，确认游戏已退出、DLL 和已安装 raw-copy 表 hash 均未改变，再校验并安装第二轮 `reencoded-unchanged` 表。只改变此表，DLL 保持 v8 SHA256，Mod/普通代理 hashes 保持。第二轮使用同一输入/身份/range，但容器由 64,142-byte 原始包重编码为 89,872 bytes，每个解码成员仍完全一致。安装表 SHA256 为 `805AF757E4C4AB8B1541B49F727026B1291333B0269DB05014C21FAA76B0901C`。**尚需下一次启动验证，完整模型/贴图替换继续关闭。**

## 重编码且内容不变的对照：正常进场、手动退出

PID 26376 的 v8 session 确认包校验、24 个 hook 安装和启动线程 detach。11:11:32.694 实际提交同一源范围的 `reencoded.vfs`（89,872 bytes），返回非空异步 request；最终 submitted=1。用户确认正常进入场景、没有闪退并手动退出。Player.log 未出现上一轮 bundle 兼容性错误，没有新的 Unity dump 或 Windows Endfield crash 事件。另一个 PID 23644 只有 BUILD/LOG，不能据此推断其加载行为。

本轮原始成员内容完全保持，容器字节不同；因此当前范围的容器重编码没有复现 v7 闪退。与 raw-copy 对照一起，它缩小了路径转交和容器重编码的嫌疑，但不能证明全部重建输入有效。bundlesReturned=0、assetsReturned=0，仍没有 getter/asset hook 的直接接受证据。结果保存于 `v8-reencoded-26376/result.json`。

下一轮只提交 v7 原样的 `input-029.vfs`：524,008 bytes，SHA256 `21FAB815F9E7A8341B0826AF833A0DEB17A686DA0DACC738BBB5E0B806BDBC4A`。该包改写原 Mesh 与 AssetBundle preload，新增 4 个独立 Mesh；没有 Renderer 或 Texture 改写。使用版本 1 的单行替换表和相同 v8 DLL，其余 55 条保持未启用。依次检验的假设是此包的序列化/预加载结构、作者 Mesh 数据、其它输入或跨包依赖。该探针的准备校验不代替游戏加载结果，也不代表完整替换已完成。

11:18:04（Asia/Shanghai）确认 Endfield 停止、已安装 DLL/重编码表 hash 未变后，备份原表并安装上述单行表。安装前后 `--check-loading-table` 均通过，当前源范围、缓存和包 digest 有效；安装表 SHA256 为 `D096BF9384232C0F048259F7991417B2CF1A29F73D83950B6C9C7CDF4CD48BE9`。DLL、普通代理及 Mod 保持原 hash/digest。部署记录和上一张表保存在 `v8-single-input029/`；单包游戏运行结果尚未取得。

## 单独提交 input-029：未闪退，但 bundle 被拒绝

PID 32728 的 v8 session 在 11:23:01.230 实际提交了精确的 v7 `input-029.vfs`，异步 request 非空，最终 submitted=1；用户确认正常进入场景后手动退出，并观察到部分源 Mesh 消失。两个 session、Player.log 和运行时提交表已保存到 `v8-single-input029/`，没有新 Unity dump 或 Windows Endfield crash 事件。

**该轮没有通过 native 加载验收。** Player.log 第 512 行仍报告此包与较新的 Unity runtime 不兼容，第 513 行报告 `S_actor_lizhiyan_cloth_03_lod0` 的 Mesh/sharedMesh 为空。因此缺失的源几何不能当作 `.bar` 已显示的证据。单独加载没有复现原 v7 native crash，但它已经独立复现 bundle 拒绝，后者现在是更清晰的反馈信号。bundleGetter/asset 观察仍为 0。

此包的原对象为一个 Mesh 和一个 AssetBundle；完整改写保留原 Mesh 身份并用第一个作者部件 `.bar` 的内容替换，新增 `.dou`、`.lian`、`.neiku`、`.xiu` 四个 Mesh，同时展开 AssetBundle preload。当前排序假设优先检查新增对象的表结构/顺序，其次 preload 区间，再检查作者 Mesh payload。原对象表有序，完整新增后的表无序；这只是已确认的结构差异，尚未证明 Unity 拒绝由它引起。

`resource_directory_check --mesh-probe table-roundtrip|first-part-only <Mod> <input> <output>` 仅在离线诊断工具中最小化现有 writer，不改变 DLL 或生产替换路径。table-roundtrip 强制原 payload 经对象表 writer，但最终成员没有任何变化，完整输出 SHA256 与已通过的 reencoded control 完全一致，因此不重复 native 启动。

first-part-only 保留原来的两个对象及完整 AssetBundle payload，只将原 Mesh 改成完整被拒绝包中的 `.bar` payload，不新增 Mesh 或修改 preload。69,352-byte 输出 SHA256 为 `45EE94505FA15106D3FFFC0947F662E4B05C1ADAD25B431E15E4C25C8ECB1A26`。独立 struct 解析以此前 AnimeStudio 解出的原始/完整成员逐对象核对：原身份、类型、顺序、schema、metadata suffix 保持；原 AssetBundle payload 不变；Mesh payload 与完整改写的第一个部件精确相同。证据为 `v8-input029-first-part/verification.json`，native/视觉结果仍待取得。其余包继续关闭。

11:35:51（Asia/Shanghai）确认 Endfield 已停止，并核对已安装表、v8 DLL 和普通代理 hash 后，备份前一张表并安装 first-part-only 单行表。部署前后提交表校验通过；安装表 SHA256 为 `2A13837DFE86C02E8D0CECA8BCA7D9173E55F09DC6C68B9C499E65E5A61DED49`。只改变提交表，DLL/代理/Mod 保持。结果须以新 Player.log 是否仍拒绝 bundle、是否出现 null sharedMesh，以及用户反馈共同判断，不要求用户从残缺外观推断资源接受。记录为 `v8-input029-first-part/deployment.json`。

## 第一部件 Mesh：进入蒙皮，但骨骼表不匹配

PID 34804 在 11:39:01.927 实际提交 first-part-only，最终 submitted=1。用户确认进入场景未闪退，但 Mesh 撕裂并持续乱飞。session/Player.log 保存于 `v8-input029-first-part/`，原 v7 Unity dump 仍为最新记录，没有新 Endfield crash 事件。Player.log 未再报告 bundle 不兼容或 null sharedMesh；第 513 行明确报告 `Bind pose count is greater than bone count, skip skin`，整轮有 1,277 条 `Skinning failed`。这证明新 Mesh 已进入引擎蒙皮检查，但没有通过视觉验收；getter/LoadAsset 观察仍为 0。

独立解析证实该 `.bar` Mesh 有 84 个 bindpose，原消费者 Renderer 只有 80 个骨骼槽。816 个作者顶点中 524 个使用原 80 槽表之外的非零权重索引，最大索引 83。各顶点 UNorm16 权重和均为 65535，作者索引都在 84 槽范围内。Mesh-only 最小化探针没有同时提交对应 Renderer 骨骼表，因此不能用它验收正确的动画显示。该数量不匹配已经确认；原 v7 新增对象/preload 的 bundle 拒绝和 native crash 根因仍未确认。

下一轮保留已进入蒙皮的 Mesh 输出逐字节不变，将其与当前消费者的 bones-only 重建输入配对。`--renderer-bones-probe` 从当前逻辑 Mesh 身份和实际消费者 PPtr 定位 Renderer；临时限定该目标的第一个部件，以既有完整骨骼路径解析器求作者顺序的现存 Transform，再只改 `m_Bones`。不添加 Mesh/GameObject/Transform/Renderer，不改变骨架、材质、bounds、显隐、shape、LOD 或 AssetBundle payload。84 根骨骼在原消费者 prefab 的预加载区间内全部已登记；新增引用没有引入未加载骨骼。

本轮消费者是此前 v7 实际提交过、当前范围 SHA256 再次确认的 `input-033`，范围 offset=427970008、135,448 bytes。它通过当前 `CAB-b762acb1a73049a0133fa2d49260fbb0` 的外部 Mesh PPtr 匹配到被测 `cloth_03_lod0`，只更改现有 Renderer `-4800652809997959198` 的骨骼数组。该选择是最小化诊断 fixture，不是生产 prefab 白名单。输出 `consumer-bones.vfs` 为 483,004 bytes，SHA256 `D805EBBB89B6616149256B6720C0C5ED1BF96FC0B161203A0B2CE851572104D0`。

`verify_renderer_bones_probe.py` 以此前 AnimeStudio 独立解出的当前成员、schema 和骨骼层级重新计算完整作者路径，验证 84 个 Transform 的唯一性与顺序，逐对象核对 1,293 个对象。只有一个 Renderer 的 `m_Bones` 改变，其余字段、全部其它对象和 metadata suffix、对象顺序、原 preload 保持。独立读取 Mesh 顶点 stream 验证权重/索引以及 bindpose/新 Renderer 数量一致。证据为 `v8-input029-bones-pair/verification.json`；游戏中是否消除蒙皮错误和乱飞仍待下一次运行。

11:50:39（Asia/Shanghai）确认 Endfield 停止，当前 first-part-only 表 hash、v8 DLL 和代理 hash 有效后，备份前一张单行表并安装两行配对表。部署前后均通过原输入/缓存 SHA256 与 Mod digest 校验。安装表 SHA256 为 `B591147DDE2C87FAE68284BF29001C9C6AAACD5BC5BE95E292CE5D4FF46EF427`，DLL、普通代理和 Mod 未改变。记录为 `v8-input029-bones-pair/deployment.json`。下一轮必须确认两条输入都实际提交，并检查 `Bind pose count` / `Skinning failed` 是否消失与用户动画观察；尚未取得这些运行证据，不宣称乱飞修复或完整替换完成。

## 骨骼表配对结果与对象表排序验证

PID 28760 在 11:59:26.620 和 .621 依次提交 consumer-bones 与 first-part-only，最终 submitted=2。用户确认没有再看到撕裂，但不能确定 Mesh 可见装配。Player.log 不再出现 bundle 兼容性、null sharedMesh、bindpose 数量或 Skinning failed 错误，没有新 Windows Endfield crash 事件。此轮通过蒙皮错误回归；完整可见装配仍未验收，getter/asset 观察仍为 0。两个 session、Player.log、运行表与结果已保存于 `v8-input029-bones-pair/`。

下一步的排序验证保留此前被拒绝的五部件 input-029 的全部对象载荷、类型、PathID、引用和 preload，仅将六条 SerializedFile 对象记录按有符号 PathID 排序。诊断 CLI `--object-order-probe` 逐条保留完整 24-byte 记录，并确认表外字节不变；独立 `verify_object_order_probe.py` 再次核对记录集合、header、其它 metadata 和所有 payload。排序输出为 524,008 bytes，SHA256 `59CA75988C691BB5ED2A0B5E9B26D7E93D3433070279EB3E48031A2F1C7EF666`。排序是否消除游戏拒绝尚未确认，生产 writer 尚未改动。

12:16:52（Asia/Shanghai）确认游戏停止且 DLL/代理/当前表 hash 有效后，备份两行配对表并安装排序 Mesh + 原已接受 consumer-bones 的两行表。安装后 `--check-loading-table` 通过，表 SHA256 为 `32EC6EB82B8D8AA05B2ED83332741AEBD4972BBCEF0834260DD9A6D4FC8A449D`。部署记录为 `v8-input029-order/deployment.json`。此轮仅检验新增 Mesh 包能否加载，另外四个部件的 Renderer 装配尚未启用；必须取得新运行结果再决定完整图的启用。

`prepare_ordered_loading_inputs.py` 已验证原完整 56 行表的当前源范围、缓存和 Mod digest，将所有完整候选暂存于 `plugin/resource-export-staging/v8-full-order-candidate-20261003/`，其中 50 个 SerializedFile 成员对象表发生排序，全部对象载荷、身份与引用保持。生成表及报告为 `v8-full-order-candidate/candidate.tsv`、`preparation.json`；候选表 SHA256 为 `FF7266CA41441BD91793F886AC4B4FA8DB65804B86E94CA5EEA01A707F500E7C`，再次通过 loading table 校验。完整候选涵盖此前离线核验的 Renderer/骨骼/LOD/preload、Mesh、Material/Texture 图，但尚未安装，不能以离线通过宣称原生接受或完整替换完成。

## 排序修复与完整静态替换候选

PID 25524 在 12:20:41.521 和 .522 实际提交 consumer-bones 与排序五部件 input-029，最终 submitted=2。用户反馈正常进场，但贴图不对。Player.log 未再报告此前的 bundle 兼容性拒绝、null sharedMesh 或蒙皮错误。此时作者贴图和扩展 Renderer 均未启用，因此尚不是完整装配验收。两个 session、Player.log、运行表和结果保存于 `v8-input029-order/`；getter/asset 观察仍为 0，不宣称已直接观察全部新增 Mesh 对象。

此单变量实验确认对象记录排序可消除 input-029 的 bundle 拒绝。生产 `BuildSerialized` 现按有符号 PathID 排序完整 24-byte 对象记录，保持 payload 写入顺序、偏移、类型、身份与引用；Python SerializedFile 写出器和独立验证同步遵守该规则。新回归在修复前实际失败于五部件 Mesh 对象表无序，修复后覆盖新增 Mesh、扩展 Renderer 及既有生命周期/加载参数/材质贴图/骨骼/LOD/preload 拒绝检查，全部通过。v7 原闪退的完整根因仍未确认。

新生产写出器重新构建全部 68 个当前输入：56 个有变化、12 个不变，包含 47 个 Mesh 部件、2619 个 Renderer 装配部件、5 个材质声明、11 个 Texture2D。所有输出逐文件与此前已验证内容、只排序对象记录的候选核对，字节完全一致；原先的独立 Mesh/Renderer/Texture 载荷证据因此保留。核对结果与回归记录保存于 `v9-full-replacement/verification.json`、`regression.json`，新缓存为 `plugin/resource-export-staging/v9-full-prepared-inputs-20261003/`。

12:34:26（Asia/Shanghai）确认游戏已停止，备份上一版 DLL 与两行表后，安装 `resource-directory-entry-v9-sorted-object-tables` 和完整 56 行提交表。DLL SHA256 为 `AA81C10E85D01D227E2723696489C13A6BE5BECA561F0825AD8FCA5712AFAE2B`，表 SHA256 为 `D65D130E72925FEDB9F028EAD43FA4107F845A286E136AC49D1020396E43F8E5`，安装后 loading table 校验通过，Mod digest 与普通代理保持。部署记录为 `v9-full-replacement/deployment.json`。

该候选启用当前依赖图内匹配 PFB 的独立 Renderer、作者顺序原生骨骼表、材质引用和贴图像素，不设生产 prefab 白名单。完整图尚待下一次原生提交、场景/角色界面外观和动画验收；动态 null-Mesh/VFX 登记、按键/形态控制、F10 原生重建仍未验证。不得把“无撕裂”“正常进场”或离线全图通过等同于这些能力已完成。

## 完整候选的撕裂反馈与实际文件核对

PID 8024 使用 v9 完整候选，实际递交 11 个输入。用户报告几乎全部替换 Mesh 撕裂，并确认静止时也发生。Player.log 没有 bindpose 数量、Skinning failed、bundle 兼容性或 null sharedMesh 错误；getter/asset 完成计数仍为 0。**完整替换的视觉验收失败，撕裂根因尚未确认。** 不把没有数量错误解释为蒙皮正确。

`audit_deployed_skinning.py` 读取已部署缓存的诊断副本，经 AnimeStudio 独立解码和导出实际对象表/schema，再逐对象读取 47 个 Mesh 及 input-033 的 60 个部件 Renderer。Mesh 的全部作者几何字段一致，顶点权重归一化、逐 Mesh 骨骼索引和三角形索引均在范围内，包括 3 个 32 位几何索引对象。Renderer 的实际 Mesh PPtr、逐槽骨骼完整路径及独立层级锚点与各自作者顺序一致。该检查没有读取运行时对象，不证明 native 蒙皮解释或最终装配正确。

以前未观察到撕裂的 `.bar` 配对与完整版使用相同的 Mesh 几何和 Renderer 骨骼数组；对应 Renderer 的字段差异仅为 bounds。input-033 的 19 个骨骼布料组件 sourceRenderers 为空，custom skinning 未启用，目前没有证据证明其烘焙索引直接消费替换 Mesh，不修改或禁用原生骨骼物理。

证据：`v9-full-replacement/tearing-8024/result.json`、`deployed-full-skinning-audit.json`、`deployed-skinning-audit.json`、`skinning-metadata-audit.json`，以及本轮 session、Player.log 和运行表。下一轮使用有界、仅诊断开关启用的装配边界读取，区分序列化输入的正确表与游戏最终实际表；不以运行时 Mesh/Renderer 写入作为修复路径。

## v10：有界的装配状态只读观察

新增 `eiem_entry_skinning_observation.h`，仅在游戏 `plugin/resource-input-diagnostics/skinning-observation.enabled` 存在时安装诊断。解析实际 IL2CPP 方法签名，在材质控制器注册 Renderer 列表及每个 RendererInfo 初始化前后读取；游戏调用 set_bones 时原参数照常转交，仅在返回后观察。每进程最多 256 份样本，按 Renderer/阶段去重，Mesh 名称只用于选择已准备部件的诊断样本，不作为资源替换身份规则。

读取实际 vertexCount、逐槽 Transform 完整路径、rootBone、原生 vertex attribute 格式/stream/offset/stride，以及允许读取时的 bindposes；按照序列化 Matrix4x4f 与托管 Matrix4x4 各自布局逐元素比较。不可读取的 bindposes 记录缺口，不改变 Mesh readability。该探针没有 Mesh/Renderer/Transform 写入，不创建替换 Mesh，不修改动画或骨骼布料。

新增回归模拟托管 getter 异常，验证 Renderer 初始化、控制器所有指针/boolean/MethodInfo、游戏骨骼赋值均原样调用且恰好一次；既有 Mesh、材质贴图、Renderer/骨骼/LOD/preload、加载控制和失效表检查通过。已确认 Endfield 停止，备份 v9 DLL 后安装 `resource-directory-entry-v10-bounded-skinning-readback`，完整 56 行替换表和缓存不变，Mod/普通代理不变。DLL SHA256 为 `50E6DBB9CEBA6C493CD41BCF481243EEBA5816B860CD1BE207563343D119C085`。

证据保存在 `v10-skinning-readback/{regression,deployment}.json`、`smoke.txt`、`table-validation.txt`。**v10 尚待重新启动后的运行读数，当前没有新的修复或完整视觉验收结论。** 取得明确结果后关闭临时诊断开关。

## v10 启动失败与 v11 隔离修正

用户报告这轮仍为源模型。目录 hash 确认 DLL/表已部署到 `D:\Hypergryph Launcher\games\Endfield Game\plugin`，PID 37992 的 BUILD 也确认游戏加载 v10。该进程通过包校验和 56 行表校验，但诊断安装在生产 loading hooks 之前，检查所有探针 API 时抛出 `bounded skinning observation API incomplete`，启动因此提前退出；没有提交记录。这轮不能作为完整 Mesh 或撕裂验收。PID 41784 只有启动记录。

现有 Mesh metadata 没有 v10 强制要求的 GetVertexAttributeStream/Offset/Dimension/Format/GetVertexBufferStride 接口。v11 改用该版本声明的 GetVertexAttributeCountImpl/GetVertexAttribute 读取 descriptor，把附加顶点/绑定矩阵读数缺失记录为诊断缺口；所有探针能力逐项打印可用性。生产加载 hooks 和准备表先完成，再有界尝试可选探针；探针安装异常仅停用诊断，不取消替换表，不中断启动及 IL2CPP detach。

新增回归先实际复现 `optional readback failure aborted replacement startup`，修正后验证模拟探针初始化异常不会传出、已发布替换表保持、启动继续。既有完整回归通过。确认游戏停止后已部署 `resource-directory-entry-v11-observation-startup-isolation`，DLL SHA256 `FED5B7995AA918B0D5F913CACE6FB1CF89077E8DF6E5ADCE31E88534D6A274D4`；56 行表/缓存、Mod/代理不变。证据为 `v11-observation-startup-isolation/{regression,deployment}.json`、`smoke-final.txt` 和保存的 v10 两份日志、Player.log、DLL。

**v11 已修正并部署诊断阻断启动的问题，尚待新一轮游戏确认替换提交。此前完整替换撕裂问题仍未修复或验收。**

## v11 实际装配读数与 v12 原生几何读回

PID 22680 使用 v11，在 14:30:51 发布 56 行表并记录 `replacementArmed=1`，实际提交 11 个输入。读回覆盖同一角色 PFB 的 60 个 Renderer，在控制器与 RendererInfo 初始化前后取得 240 份样本：所有实际顶点数与作者相同，逐槽完整骨骼路径匹配且没有空骨骼，绑定矩阵可读并与作者逐元素一致。PID 41192 仅有 BUILD/LOG。用户仍报告模型呈刺猬状；装配已被观察到，视觉替换仍失败。

`audit_deployed_skinning.py` 新增对实际部署数据的静止姿态 CPU 蒙皮检查，保留原生 Transform 层级。47 个 Mesh、60 个 Renderer 的权重/索引/引用检查继续通过；全组最大静止顶点偏差为 0.0005643125 个模型单位，三角形边长与共同刚性绑定结果接近。该数值没有复现尖刺，不证明运行时动画或 GPU 蒙皮正确。证据为 `v12-native-geometry-readback/v11-result.json`、`v11-deployed-rest-skinning-audit.json`、两份 session 和 `Player-v11.log`。

v12 在原只读装配探针中，按实际版本完整签名解析 `GetBoneWeightsImpl`、`GetAllocArrayFromChannelImpl`、`GetIndicesImpl`。每个实际 Mesh 最多读取一次原生权重、位置和三角形索引，对照作者字段；权重按每根骨骼的影响量比较，允许四个影响槽重新排序并忽略零权重填充。读数缺失、异常、数组布局或范围不符均记录能力缺口，不中断原调用或已发布替换表。不创建或写入运行时 Mesh。

为保证 CPU 数据可读，独立诊断 CLI `--geometry-readback-probe` 只在已经重建的序列化 Mesh 上设置 `m_IsReadable/m_KeepVertices/m_KeepIndices`，生产 writer 和 Mod 作者资源不变。临时表保留完整 56 行图，只有 7 个 Mesh 输入改用新缓存。53 个同名候选 Mesh 对象保留 CPU 数据，其中 47 个属于已核对的当前 PFB 消费者；名称只选择诊断样本，不替代生产逻辑路径/资产身份规则。

AnimeStudio 重新独立解码这 7 个输入，`verify_geometry_readback_probe.py` 逐对象检查：只改变上述保留标记，几何、绑定、引用、类型、身份和排序保持。新预期数据/权重比较回归覆盖 13 个真实作者部件、流边界拒绝、影响槽重排、错误骨骼槽与权重范围；既有完整回归通过。记录为 `retention-verification.json`、`preparation.json`、`smoke.txt`。

确认游戏停止后，已部署 `resource-directory-entry-v12-native-geometry-readback` 到 `D:\Hypergryph Launcher\games\Endfield Game\plugin\eff.dll`，SHA256 `FF73FCE40A3CC4838BF1CB969A12C7E003B43B968B843F173153DB66993D3EA2`；临时表 SHA256 `588BFA0BC9392A76861B614137BAF1A4FBDAFB0D537D3C3F8012C0F366BBAD49`。安装后表校验通过，普通代理/Mod 保持，v11 DLL/生产表已备份。**此版尚待游戏运行，只是取得新的故障分辨读数，不能称为尖刺修复。** 取得充分读数后恢复生产提交表并关闭临时探针。

## v12 实际读回与 v13 UV2 存储格式实验

PID 35920 运行 v12，提交 12 个输入；原生位置、权重及三角索引读回各覆盖 47 个 Mesh，所有不匹配计数为 0，位置逐元素误差和每根骨骼影响权重误差为 0，索引全部在范围内。用户仍报告刺猬状变形，故 CPU 几何正确不能视为 GPU 蒙皮或视觉替换正确。完整日志、运行表及 Player.log 已保存，结果为 `v12-native-geometry-readback/v12-result.json`。PID 34212 仅有启动日志。

接下来的三个假设分别是特殊通道标记与格式组合、GPU 蒙皮数据布局、动画更新后的骨骼矩阵异常。本轮只验证第一项：原生 LOD0 UV2 使用 SNorm8 和 dimension `0x24`，作者字段被写成 Float32 而沿用高位标记。`--packed-uv-probe` 是独立离线诊断工具，仅把同名诊断候选的该通道恢复成 SNorm8；名称只选择实验对象，生产资源身份规则与 writer 不变。保留其原标记、调整该流 stride/后续偏移及 16-byte 对齐，其他属性逐字节复制。普通 dimension `4` 的 UV2 保持。

独立解码发现身体 Mesh 的 12 个 UV2 分量是 SNorm8 半步值，转换最大误差为 0.003937007859349251，其余部件只有 Float32 舍入级误差或零。转换明确允许最近 SNorm8 值的半步量化，拒绝非有限值与超出 [-1,1] 的值，不钳制越界数据。此差异只属于 UV2，不允许顶点位置、权重、骨骼索引或三角索引变化。

新回归在 5 个真实作者 Mesh 上检查有符号半步舍入、其他 UV、位置/packed normal/权重流及非顶点 Mesh 字段保持，拒绝截断/越界/NaN，并验证无特殊标记时不变；既有完整回归通过。AnimeStudio 独立解码 7 个输入，`verify_packed_uv_probe.py` 验证 13 个 Mesh 的通道格式变化与量化界限，所有其他顶点通道逐字节一致，非顶点 Mesh 字段、对象身份、类型、排序、引用、外部表与 preload 不变。

确认 Endfield 停止后，已更新游戏 `plugin/resource-input-substitutions.tsv`，完整 56 行图只有 6 个 Mesh 输入改用 `resource-export-staging/v13-uv2-storage-inputs-20261003-r2`。表 SHA256 为 `5B2D493CC2045A07D532F4A110060848E7202E65202B867532DD6F77E26971A4`，安装后表校验通过。DLL 保留 v12 只读探针，SHA256 为 `FF73FCE40A3CC4838BF1CB969A12C7E003B43B968B843F173153DB66993D3EA2`，作者 Mod 与生产 writer 不变；v12 表及 DLL 已备份。证据位于 `v13-uv2-storage-probe/{preparation,independent-verification,deployment}.json` 与 `smoke.txt`。

PID 40264 在 15:24 启动，提交 11 个输入，日志明确记录来自 v13 缓存的 Mesh 输入；原生位置、权重与索引读回各覆盖 47 个 Mesh，全部不匹配计数与误差为 0。用户确认仍是刺猬状，故该 UV2 表示实验没有消除故障；不能把格式差异称为根因。PID 39564 仅有启动日志。日志、Player.log 和结果保存于 `v13-uv2-storage-probe/game-result.json`。确认游戏停止后已恢复实验前 v12 表，SHA256 `588BFA0BC9392A76861B614137BAF1A4FBDAFB0D537D3C3F8012C0F366BBAD49`，完整 56 行输入图与 CPU 保留探针继续。

## v14：原生蒙皮矩阵打包边界取证

当前安装 UnityPlayer SHA256 为 `BEE7BE52370ADDDD67BA61E4937CA51B7F272656841D187E95E505496DA798D1`，与已反汇编二进制相同。`UnityPlayer+0xEE960` 的实际汇编和 `UnityPlayer.extra.ida.txt` 均证明其签名为 `void(nativeRenderer, columnFloat4x4Array, uint64MatrixCount)`，调用 `0xC7BB70` 把列布局 float4x4 的前三行逐字节转换为三行 float4。该函数把根矩阵写入 Renderer `+0x388` 缓冲区开头 48 字节，再写入全部骨骼；`+0x374/+0x370` 是记录的 current/previous 行基址。静态签名、布局与安装 image 证据保存在 `v14-native-skin-upload/static-gate.json`，不代表最终 GPU draw 已读取这些数据。

新增可选 `eiem_entry_native_skin_upload_observation.h`，通过独立 marker 启用单个矩阵打包入口只读 hook。安装前核对完整 UnityPlayer 磁盘 hash 与映射函数前 24 字节；通过当前 IL2CPP `UnityEngine.Object.m_CachedPtr` 字段元数据，在原有注册边界关联原生 Renderer/Mesh，并检查原生 Mesh vtable 和 Renderer 当前 Mesh。最多 128 个目标，每个在游戏自己的调用中记录首次、至少 1 秒及至少 5 秒三份样本。此间隔仅用于证据采样，不延迟或修改替换。

输入矩阵在原调用前复制，原调用使用原三个参数且恰好一次，返回后读取 packed palette、根矩阵、当前/前一基址、原生 bindpose 数和 effective influences。输入列布局与输出三行逐位比较，记录非有限值及范围，并保存小型 binary 快照供离线按作者骨骼槽/权重重算三角形。探针不调用 native job 线程上的托管 getter，不扫描场景，不增加 render-loop hook，不修改 Mesh/Renderer/Transform/物理，不使用运行时写入作为替换路线。缺口按 `[DEBUG-skin-v14-GAP]` 有界记录，初始化失败不阻断已发布生产输入。

新回归验证 Before/After 异常时原参数与调用数保持、禁用时直通、真实打包布局、错误元素/NaN 检测、快照、采样节奏及预算、输入矩阵未改。既有完整回归通过。`analyze_native_skin_upload.py` 可读取运行快照，独立比较 packing，并按作者权重计算范围及最长三角边；13 个真实作者部件的 identity palette 投影检查通过。它明确标记实际 GPU draw 尚未观察，不能仅由有限矩阵推断最终着色器正确。

确认游戏停止后已部署 `resource-directory-entry-v14-native-skin-upload-readback` 到游戏 `plugin/eff.dll`，SHA256 `06EF5A651E69555D9A884E1757D5D86980DF2946F26C91ACD0DA632684D18DB9`；完整 v12 输入表、作者 Mod 及普通代理不变。只读 markers 启用，安装表校验通过，v12 DLL 已备份。证据位于 `v14-native-skin-upload/{deployment,regression,static-gate,analyzer-fixtures}.json` 与 `smoke.txt`。

PID 40584 实际提交 11 个输入，登记 60 个原生 Renderer；矩阵入口取得 72 份样本，覆盖 24 个 Renderer、12 个作者部件，每个 Renderer 三个时刻。输入到 packed 输出逐位一致，没有非有限矩阵，没有探针缺口；用这些实际矩阵重算作者三角形，没有复现严重尖刺。最大最长边比约 2.200908，发生在 cloth04 `.lian`，对应最长边约 0.039608 模型单位。完整样本与重算结果保存在 `v14-native-skin-upload/native-upload-result.json`。

用户确认本轮画面**仍是刺猬状**。身体部件没有命中此入口，不能由其他 12 个部件的结果推出身体矩阵正确；最终 GPU draw 的骨骼资源、基址和顶点解释也尚未观察。**尖刺未修复、完整视觉替换未验收。** 后续取证针对实际绘制的骨骼缓冲区与顶点布局，并保留身体/形变路径这一覆盖缺口。

## v15：Renderer 记录和设备顶点格式取证

实际安装 image 的 `0xC7B750` 单参数函数在既有 Renderer 记录 `+0x50/+0x54` 写入 current/previous 骨骼行基址，`+0x4C` 写入影响数/路线标记并返回索引。新只读 hook 保留原调用参数、次数和 64 位返回寄存器；返回后按 `0x16D520` 的实际地址计算读取现有记录，不调用会改变状态的 native getter。再按 `0xEEBF0/0xEEC40` 的段起点和容量核对 `palette == ringBase + (currentBase-rowOrigin)*16`，范围包含根矩阵三行。最多 128 个已登记 Renderer，每个三个时刻；它仍是 CPU 原生记录，**不是最终 GPU draw**。

`0xD7340` 的四参数函数把 14 个顶点属性的格式/维数调整为设备支持的格式，输出 28 字节格式表和变化 mask。另一只读 hook 在原函数前后复制这些小型描述，最多缓存 8192 个 VertexData；在既有 Mesh 注册点按原生指针关联，记录转换前后格式、当前通道、四个流的 start/stride/mask 和原生 GPU buffer 指针。它不修改作者或 native 顶点字节；未命中转换、缓存上限或后续原生布局变化均作为覆盖缺口保留。

构建与完整回归通过，新增检查覆盖取证 Before/After 失败时原调用/64 位返回保持、独立 handle 的记录偏移、缺失 feature/越界拒绝、环形段 origin/根矩阵预留/容量关联，以及准确 prologue 校验。分析器另测合成基址/指针不匹配和空运行不得误判通过。实际 image 的汇编、签名及偏移依据见 `v15-native-render-record/static-gate.json`，检查见 `regression.json` 与 `smoke.txt`。

确认 Endfield 进程已结束后，已部署 `resource-directory-entry-v15-native-render-record-readback` 到游戏 `plugin/eff.dll`，SHA256 `FAC79E497DBD0E02ACA856E8DDE837028B8FCFD3FA798B3A083F6B5073C0C328`；v14 DLL 已备份在本轮证据目录。部署时完整 56 行 v12 替换图、作者 Mod 和普通代理保持，表校验通过。新增 `native-render-record.enabled` marker。本版后续已取得 PID 25276 的运行记录和 GPU 抓帧；仍未完成尖刺修复验收。

用户进一步描述严重爆炸像顶点索引错误。针对这一反馈，独立读取 v12 已解码输入中与实际 47 Mesh 装配记录关联的全部 13 个作者部件：原始三角索引与作者逐字节一致，16/32 位格式、三角拓扑/count、firstByte/baseVertex/firstVertex/vertexCount 均正确；各流 start 按 16 字节对齐、stride 按当前通道计算、所有范围在顶点字节内。补查只证明序列化绘制输入，没有取得最终 GPU 的 index type、index-buffer offset、vertexOffset、顶点 binding 和实际 buffer 内容。记录为 `v15-native-render-record/mesh-draw-input-47-audit.json`。真实输入的 firstByte/baseVertex/firstVertex/vertexCount 和 indexFormat 扰动拒绝检查通过，见 `draw-input-fixtures.json`。

v15 分析器已补充按当前原生通道计算四个流 start/stride/mask，并与只读流记录比较；packed frame 的维数高位不增加元素数，设备格式变化按实际宽度计算，缺少活跃流记录保留为缺口。静态检查另发现 `0xD63D0` 可在 provider 的 VertexData 副本上调用 `0xD7340`，因此未按 MeshData 原指针关联到转换日志不能解释为没有发生设备转换。PID 25276 后续取得 47 个 Mesh 布局和 72 个原生记录样本，覆盖 24 个 Renderer、12 个衣服部件；记录基址和环形 palette 地址均匹配，设备格式转换 changedMask 为零。分析器报告的 47 个 stream-layout mismatch 集中于 channelMask，其读取偏移可能错位；start/stride 匹配，不把该探针缺陷当成真实渲染错误。原生日志仍不直接证明最终 draw；后续 GPU 证据见下节。

## v15 GfxTrace 抓帧准备

2026-10-03 22:29，用户指定使用既有 GfxTrace，沿用 EIEM Vulkan 代理在 Vulkan 初始化前启用注册捕获层的方式。捕获版仅在 `EIEM_ENABLE_GFXTRACE_CAPTURE` 编译分支中清除 GfxTrace 的 disable 变量、启用 GfxTrace 并禁用另一个 RenderDoc 层；普通分支继续关闭两者。诊断 wrapper 把捕获前缀固定为 `v15-renderdoc-capture/lzy-v15`，不改生产资源提交方式。

独立真实 Vulkan 实例测试成功，模块路径确认是 `E:\renderdoc\x64\Release\gfxtrace.dll`。对照普通代理没有加载两种捕获层；两版的 265 个转发导出逐项相同。PATH 中的已有 `renderdoc-cli` 成功回放 9 月 30 日的 Endfield 帧，确认分析入口可用；通用 vkcube fixture 映射内存失败已记录，旧帧不用于判断这次替换。

确认游戏停止且 v15/表 hash 与基线相同后，已备份并只覆盖游戏根目录 `vulkan-1.dll`，捕获代理 SHA256 为 `D75EC0E0E6D0AC6162FD2CA6227C320CF04A801D559BDF28F7D9D2A9D06F4D6D`。当时插件、完整输入表与 Mod INI hash 前后相同。部署与恢复记录见 `v15-renderdoc-capture/deployment.json`、`README.md` 和 `restore_normal_proxy.ps1`。取证结束且游戏停止后恢复普通代理。

## v15 实际 GPU 帧与 v16 身体通道对照

用户 F12 捕获的 `v15-renderdoc-capture/lzy-v15_frame2044.rdc` 已成功回放，关联 PID 25276、22:30:03 的 v15 BUILD、session `run-25276-01DD5343AA1E5CAF` 和 11 次替换输入提交。帧为 Vulkan，1,331 个 draw、44 个 pass，没有 HIGH 级验证消息；导出画面重现巨大三角形。包内 CLI 的 draw 输出上限为 1,000，Vulkan snapshot 缺字段；只读本地 CLI 扩展以匹配 replay DLL 的 v1.45 公开头文件导出完整实际绑定和 buffer，未修改游戏。

完整 13 个作者部件核对见 `gpu-input-audit-13-parts.json`：三角索引、权重及各 Mesh 骨骼槽逐值匹配；12 个衣服部件的实际 GPU 静态位置匹配，post-VS 有限。身体 event 4140 的静态源位置也匹配，但绑定给绘制的形变／CPU 蒙皮输出含 148 个非有限位置分量，最大有限分量约 `3.355e38`，post-VS 含 NaN/Infinity。身体路线 flags=16，衣服 flags=52。**本帧严重爆炸定位到身体的形变／CPU 蒙皮输出，三角连接及衣服 palette 不是该帧的故障来源；具体内部原因尚未确认。** 旧输出按 48 字节重新解释只能改善部分样本，不能视为 stride 修复。

源模板条件已回读核对：`v9-full-replacement/tearing-8024/original-schema.json` 中的原始身体 `S_actor_lizhiyan_body_01_lod0`（CAB `CAB-287860d091d5e377828534e90dba63b5`，PathID `-5968019704218085388`）没有形变通道，而作者身体有两个。原始 normal descriptor 是 stream0/offset12/format0/dimension0x31，tangent dimension0，`m_MeshCompression=0`；这仍是由特殊通道标志定义的 packed-frame 存储。新增形变改变处理路线，复制原始字段布局不能证明这一新功能组合兼容。写回规则见 [顶点数据契约](vertex-data-contract.md)。

按“压缩法线／切线不兼容 CPU 形变路径、输出与绑定步长不一致、形变数据错误”排序，先验证第一项。`v16-shaped-frame-float-input` 仅修改两份原生输入中的四个身体 LOD Mesh 的 `m_VertexData`：stream0 从 16 字节 position3＋packed frame1 展开为 40 字节 position3＋Float32 normal3＋tangent4。保持两个形变通道及全部非顶点字段，位置、UV、权重、骨骼槽字节相同，Renderer、材质、贴图和资源身份相同。独立 AnimeStudio 解码、全对象图与原生重建校验通过，展开最大 float32 舍入误差约 `2.98e-8`；其余 54 行输入表不变。

确认 Endfield 停止后已备份安装表并部署此完整 56 行对照表，SHA256 `01F65AC6F36084FAD67FA9B166550E8389626CE4F553CF7C98F04BB069F614BF`，安装表校验通过。DLL、GfxTrace 代理及作者包未改；记录见 `v15-renderdoc-capture/frame-{preparation,independent-verification,deployment}.json`，回滚表为 `frame-before-deployment.tsv`。

PID 28072 重启已提交 11 份实际替换输入，其中两份为身体对照输入；四个身体 Mesh 实例均观察到 Float32 normal3/tangent4、stream0 stride40。用户确认本次进入场景外观正常。后续 PID 35340 的日志保存为 `eff-log-v16-normal-user.txt`，用户正常画面抓帧是 `lzy-v15_frame2157.rdc`。单变量对照确认“身体的 packed-frame 存储不兼容新增形变／CPU 蒙皮路径”；原生内部具体错误写入／读取指令仍未直接观察。

正常帧回放有 2,335 个 draw、39 个 pass，无 HIGH 验证消息。13 个作者部件的三角索引、权重及骨骼槽仍匹配，12 个衣服部件静态位置不变，全部 post-VS 有限。身体 event 9954 实际 skin 输出绑定为 stride40；148 个非有限分量降到 0，最大绝对位置约 1.627988，最长三角边约 0.044037，较作者最长边比 1.037704，静态源位置仍一致。导出画面与用户报告均无尖刺。证据见 `gpu-normal-audit-13-parts.json`、`frame-gpu-comparison.json` 和 `normal-frame-image/rt_10567_0.png`。原始场景的爆炸复现已修复。

通用正式规则已加入 `eiem_mesh_frame_storage.h`：按实际非空形变通道处理 packed frame，写成原生 Float32 normal3/tangent4；无形变 Mesh 保留压缩格式，已展开格式保持幂等。它只改反序列化前的 native bytes，不使用运行时 Mesh setter、私有字段、替代后端或角色名白名单。Python 原生 writer／delta 验证同步；Renderer bounds 从实际 position stride 读位置。真实原始身体输入回归先失败再通过；四个身体 LOD 的 C++ 展开与独立 Python 解码逐字节一致，并验证无形变 no-op、幂等、损坏长度／marker 拒绝和 bounds 保持。

完整 68 输入图重新构建，56 行替换、47 个 Mesh 部件、2,619 个 Renderer 部件、5 个材质、11 个贴图均覆盖；仅两份身体输入与已验证 v9 图不同。独立 AnimeStudio 解码、整对象图比较和既有完整回归通过；正式顶点字节与正常 GPU 帧对应输入逐字节相同，其余字段仅撤销临时 CPU 保留。证据为 `production-frame-verification.json`、`production-smoke.txt`，正式 writer 没有保留本轮临时探针路径作为替换实现。

2026-10-04 00:19，确认 Endfield 停止后已部署 `resource-directory-entry-v16-shaped-frame-storage` 到游戏 `plugin/eff.dll`，SHA256 `4A370CC2C4576924E28E14A71843DDD83E78E3074698818A7715408FCF3C387B`；完整表 SHA256 `08A13E08FD528D65885E4D17CA61B9B90BD6ECF604F6C42D940024FB0B6B22E4`，安装校验通过。已恢复原普通 Vulkan 代理，备份并关闭三个取证 marker，作者包／Mod INI 保持。部署和回滚证据是 `production-frame-deployment.json`。**故障场景的 GPU 验证版已通过；撤销临时诊断后的正式构建尚待下一次启动验收，跨场景／LOD、动态控制与 F10 不由这帧替代验收。**

## 构建与复现

```powershell
cmd /c tools\diagnostics\build_resource_directory_entry.cmd
cmd /c tools\diagnostics\build_resource_directory_check.cmd
cmd /c tools\diagnostics\build_resource_entry_smoke.cmd
```

DLL 产物 `bin/resource-directory-entry/eff.dll`；同目录 `vulkan-1.dll` 是未开启 RenderDoc/GfxTrace opt-in 的普通代理。部署 `eff.dll` 到游戏 `plugin`，代理到游戏根目录。验证构建不替换整个工作区的旧代码，也不把旧实验作为另一条运行路径编译进去。


## body 残影：非空形变通道被捕获代码排除（2026-10-04）

`resource-directory-entry-v17-capture-eligibility` 已部署，仅增加 `SkinMatricesRequestFinished` 的只读观察。实际 PID 40184 平 A 触发时 body 的结果为 true，shapeCount=2、readable=false；衣服也为 true。caller RVA `0x35dfa67` 对应实际 Capture 检查。没有观察到 body 因就绪结果 false 被跳过。

当前 GameAssembly 的 Capture 后续路径直接解析 `UnityEngine.Mesh::get_blendShapeCount()`：`0x35e02aa` 调用 getter，`0x35e02ac` 检查结果，`0x35e02ae` 在 count>0 时跳到 `0x35e09ba`；目标分支 Destroy／DestroyImmediate 当前残影对象并继续。因此，原 body 的零形变通道改为两个后，会触发原生残影捕获的显式限制。此处与来源字段可反序列化、静态形变／蒙皮布局可用是不同验收项。

证据在 `diagnostics/resource-directory-entry-20261003/v17-afterimage/`：`CaptureController-disassembly.txt`、`CaptureController-icalls.json`、`capture-eligibility-pid40184.json` 和 `eff-log-v17-pid40184.txt`。独立 DLL 不编译退休的运行时 Mesh setter／clone 替换路径；不是旧替换代码被这一轮重新启用。

已逐对象验证并部署临时 no-shape 对照：仅四个作者 body LOD Mesh 的 `m_Shapes` 和 218 个关联 Renderer 的 `m_BlendShapeWeights` 改变，其余所有对象字段、原生眉毛形变、float40 顶点字节、索引、palette、材质与纹理保持。完整表仍有 56 行。作者目录和 Blender 形态键保留。本次通过清空通道验证捕获分支，不能作为保留形态键的正式残影方案。

对照文件 `no-shape-authored-control`；验证 `no-shape-authored-verification.json`；安装记录 `no-shape-deployment.json`；回滚表 `table-before-no-shape.tsv`（SHA256 `08A13E08FD528D65885E4D17CA61B9B90BD6ECF604F6C42D940024FB0B6B22E4`）。当前临时表 SHA256 `B7974872A26CC8C0663DD3D14BB2D4DC6E4AD8ACD22396F26C61F7DDEEB070BE`。替换表只在确认 Endfield 停止后安装。

探针的 true/false、原调用次数、self／MethodInfo、getter 异常、null、重复状态及预算耗尽回归通过，见 `callback-smoke.txt`。PID 35328 的 no-shape 对照中，body 通道数为 0、蒙皮矩阵请求完成，用户确认身体残影恢复；日志保存在 `eff-log-no-shape-pid35328.txt`，验收记录写入 `no-shape-deployment.json`。正式修复应适配原生捕获消费者，保留作者形变功能；不能全局伪造形变数量或永久去掉作者形态键。

### v18：主 Mesh 形态键保留，残影固定在捕获时的形状

候选已部署到实际游戏 `plugin/eff.dll` 和完整 56 行输入表，备份与校验见
`diagnostics/resource-directory-entry-20261003/v18-shape-capture/deployment.json`。
安装时游戏已退出；普通 Vulkan 代理、作者包与 INI 保持。
DLL SHA256 为 `0F582D182617BD5FD30584599CB11D3ACD92D81C6514A2D462982A6ABB92ABE0`。
输入表 SHA256 为 `448FA914902AF23FEC427A7349574E7F4FC1DE07B0EFE041AB30020CB1337B65`。

四个身体 LOD 的原生形态键全部恢复。插件使用原生权重 setter，捕获调用作用域
仅向已登记的作者 Mesh 提供触发瞬间的局部空间几何副本；副本应用形态键，保留
索引、bind pose 和权重，交给游戏原有的残影骨骼矩阵流程。主 Mesh 不清空形态键，
不覆盖形态键数量 getter，也不对蒙皮后的顶点再套原生蒙皮。
源码、独立字段验证、插值及回调测试记录见该目录 README；控件与残影快照尚未
取得 v18 实机/GPU 验收，不能作为已修复结论。服装显隐、持久化与原生 F10 不在本轮完成范围。

### v19：当前 Blender 选择与原版／自定义纹理隔离

实际选择 14 个对象，按当前相机隐藏设置导出 12 个 Mesh 与两个 skip 规则。
已安装到游戏 `plugin/eff.dll`、`plugin/mods/LZY` 与 60 行输入表。
DLL SHA256 `64E6BE54AC79FA51087C1BC415EA44B9EDDD44E07D593C7518AFE5FD28E74AAB`；
表 SHA256 `DD510A3FE08C57749B24ED77D8880242B381A916AE6B14073B4B653F7FC19DB1`。
旧 DLL、表和完整 Mod 已备份到 `plugin/resource-input-diagnostics/rollback/v19-texture-isolation`。
安装前再次确认游戏已停止，普通 Vulkan 代理保持，v18 形态键捕获功能与 marker 保留。

旧版虽然没有覆写头饰材质，却直接改写了头饰与 cloth_04 共用的原 Texture2D，
导致 toushi 读取自定义衣服贴图。新版在原 CAB 创建独立纹理，只有显式贴图绑定改指针；
原对象、原流前缀与未修改的材质槽保留。相应 preload 引用与容器区间同步扩展。
独立验证覆盖 68 输入、11 个原纹理、11 个作者纹理、14 个显式绑定、403 个 preload 区间；
14 处自定义图片的所有 mip 与所选 PNG 一致，15 处继承图片与来源基线一致。
toushi 的七个贴图槽全部继承原版。几何、回调、共享模板两份作者纹理的回归也通过。
证据与安装 hash 位于 `diagnostics/texture-selection-audit-20261004/`。
目前只确认构建、导出、部署和静态数据正确，v19 游戏外观及 v18 残影快照实机验收仍待记录。
