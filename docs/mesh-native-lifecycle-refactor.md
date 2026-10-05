# Mesh 原生生命周期替换重构方案

状态：设计阶段（2026-10-02）

2026-10-05 补充：本文的“唯一入口/禁止运行时 setter”是当时的设计选择。用户现已明确允许研究任何能实现热重载的手段，F10 的当前证据与研究边界见 [热重载替代手段](f10-reload-alternatives.md)。不能将本文的历史限制当成新增用户授权障碍；其中尚未证明的事项也不能当成当前已经失败或已经成功的结论。

本文只讨论 Mesh 资源替换和原生 Mesh 生命周期。平 A 残影属于游戏创建的额外 VFX 实例；冲刺消融属于材质实例在绘制阶段的变化。两者都必须消费已经正确初始化的 Mesh，但不能和材质替换逻辑混成一条链。

## 1. 要解决的根问题

当前生产路径在 Renderer 已经创建以后调用 `sharedMesh` setter，把 DLL 自己创建的 Mesh 接到游戏对象上。这个时机太晚：游戏在更早的资源解析、Mesh 创建、反序列化和原生收尾阶段，已经为源 Mesh 写入了一组私有原生状态。这个状态不是单纯的顶点、索引或材质槽，也不是可以安全复制的普通结构体。

因此会出现以下现象：

- 普通绘制可能看起来正常，但残影、蒙皮、LOD 或其他原生消费者拿到的状态不完整；
- body 和 cloth 命中规则的结果不同，即使它们看起来属于同一角色；
- 运行时再次改 Mesh，可能造成 GPU 上传、异步任务、资源所有权或私有指针不一致；
- 冲刺期间材质实例变化时，后置替换还要额外处理多 submesh，容易卡顿或闪退；
- F10 只能重放覆盖，不能把已经错过的游戏创建阶段补回来。

核心原则是：**替换的是资源输入，原生 Mesh 对象由游戏自己的创建和收尾流程产生；DLL 不复制未知私有字段，也不在成品 Mesh 上补字段。**

## 2. 对三种方案的判断

| 方案 | 判断 | 原因 |
|---|---|---|
| 运行时持续追踪源 Mesh，并把 native 数据同步给 Mod Mesh | 只可作为诊断，不可作为生产方案 | 私有字段可能包含指针、所有权、GPU/Job 状态、版本标志和异步资源；复制或持续镜像会破坏不透明的不变量。逐帧读取也违反现有性能约束。源 Mesh 变化不等于目标 Mesh 可以安全写入同样的字节。 |
| 在源模型创建前替换输入，让游戏按自己的路径创建 Mod Mesh | 推荐的生产方向 | 目标 Mesh 从一开始就进入同一套反序列化、资源注册、原生收尾和 GPU 上传流程。需要找到稳定的资源描述或解析 Seam，而不是拦截所有 Mesh 构造。 |
| 在反序列化阶段把源 Mesh 资源替换成 Mod 资源 | 与方案二合并为唯一实现 | 反序列化结果是游戏的有类型输入，后续流程由游戏完成。必须证明该入口覆盖需要支持的资源类别；覆盖不足时不增加第二条替换路径。 |

本项目只采用一条实现路径：在共享的资源反序列化/解码输入处替换 Mesh 资源，然后继续进入游戏自己的 Mesh 创建流程。方案二和方案三在概念上都指向这个入口，但不会作为两条代码路径并存；部件描述符和 Renderer 层不再定义替换入口。

这里的“创建流程”要精确理解为：游戏先分配并构造 native Mesh，再把资源流中的完整序列化字段消费到这个对象，最后执行 native 收尾。我们要替换的是这笔读取事务的输入载荷，不是把已经构造完成的 native 指针换成 DLL 自建对象。

### 2.1 证据分级

下面的边界必须保持清楚。**已证明**表示有运行期日志、IDA 交叉引用或离线资源解析证据；**部分证明**表示链路方向成立但仍有关键语义未验证；**待证明**表示还不能写入生产实现。

#### 已证明

- 当前生产替换发生在 Renderer 已存在之后，使用自建 Mesh 再提交 `sharedMesh`；这条路径没有让 Mod Mesh 经过源 Mesh 的完整原生创建流程。
- 在当前 UnityPlayer 版本中，目标 body/cloth 的 12 个源 Mesh 都命中 `UnityPlayer+0xD9E70` 的 native Mesh 构造记录，并在同一 native 指针上命中 `UnityPlayer+0x1EA2A0`。
- `+0x1EA2A0` 是这批目标 Mesh 的实际二进制读取路径；本轮目标没有命中旧候选 `+0xDA400`。这个结论只适用于已验证的当前播放器版本和这批目标资源。
- `+0x1EA2A0` 读取 Mesh 名称，并通过跨缓冲流读取后续数据；读取过程不是一段可以直接覆盖的连续内存。
- 当前目标源 Mesh 的读取后 `native+0x1C8` 在观察样本中为 4；现有自建替换 Mesh 的对应状态不稳定。现有 setter 路径没有产生与源 Mesh 相同的原生状态。
- AnimeStudio 已能完整解析指定 PFB 的源 Mesh 序列化对象；源 cloth01 与当前 EFFMESH 替换件的顶点数、submesh 数和 bindpose 数确实不同。
- 冲刺已经确认是对 Renderer 材质槽进行材质实例替换；它不是 Mesh 创建入口，也不要求通过 Render 拓扑识别残影。
- 克隆方案不能承载当前几何布局不同的替换件；克隆后的布局仍被源 Mesh 的布局约束。
- `+0x1EA2A0` 不是一个脱离对象的静态解析函数：IDA 显示它是 Mesh native vtable 槽 18（`+0x90`）经 `+0x4710D0` 转入的实现；它的第一参数是已经由 `+0xD9E70` 构造出的 Mesh native 对象，第二参数是分段读取流。函数在这个对象上写入名称、顶点流、索引、submesh、bindpose、压缩/通道字段和 `m_BonesPerVertex` 等序列化结果。
- 已做离线无风险往返验证：把 AnimeStudio 导出的 `EIEMESH` 源文件按当前作者格式读取后，使用原生对象报告重写，输出大小仍为 2,291,768 字节；顶点、法线、切线、UV、索引和骨骼索引的结构范围有效，位置/法线/切线/UV 字节保持一致，权重只有浮点精度级差异（最大约 `8.94e-8`）。这证明作者侧数据可以映射到已知 Unity 2021 Mesh 区段，但不证明游戏会接受该载荷。
- 旧的 `EIEM_NATIVE_MESH_ENTRY_REPLACE_BUILD` 实验 DLL 曾启用 `finalizer-hooked ... replace=1`。其名称读取器使用 `native+48`（十进制，实际就是 `+0x30`），错误在于假设这里有 MSVC `std::string` 的 size/capacity 布局；当前目标的实际名称可以通过 `+0x30` 指针读取。此前文档将其解释成 `+0x48` 偏移错误是不正确的。旧日志中的 `name-unreadable` 和由此产生的 replace fail 不能用来否定创建 Seam。
- 2026-10-02 的加载崩溃 dump 证明：备份 DLL 仍以 `.dll` 结尾放在 `plugin` 中，被代理的 `plugin\\*.dll` 枚举加载，与新 DLL 同时安装 hook。调用栈同时出现两份插件。备份现已移到工作区 `diagnostics/mesh-entry-crash-20261002/`，不再参与加载。
- 同次及前次加载崩溃均位于 `UnityPlayer+0xC766D0`，路径包含 `EiemBuildNativeMesh -> Object.Instantiate -> native finalizer`。同次 dump 的 `RDX=0x5D50`，指令为 `movzx eax, word ptr [rdx]`，正在读取无效的 16 位顶点通道输入。这证明此前所谓“只读 trace”仍执行了危险的 Mesh 克隆；并不证明日志函数自身引发崩溃，也不证明上游完整载荷不可行。
- source-observation-v2 的后续冷启动记录到 INI 声明的全部 18 个 LZY body/cloth Mesh（body01、cloth01/03 的 LOD0–3，cloth04/06 的 LOD0–2）。它们均经 `+0x133ADE` 调用目标读取实现；这是已有读取链结论的再次验证。日志结束于窗口关闭，未发现新增崩溃报告；该结果只证明此次启动取得样本，不能替代完整游戏稳定性验收。
- source-observation-v3 的实机启动对全部 18 个目标完成外层 `+0x133800` 与内层 `+0x1EA2A0` 的同事务关联。所有样本的对象记录查询、provider、逻辑起点/终点与完整消费量均一致，没有 `[MESH-READ-FAIL]`。18 个目标分属 6 个 owner，provider vtable 均为 `+0x18678D0`。证据见 `diagnostics/mesh-resource-read-owner-20261002/source-observation-v3-targets.json`。
- 使用 AnimeStudio 的现有完整资源索引核对，18 个实测对象键均对应其同名 Mesh 的 PathID，各有一条精确匹配。5 个 LOD0 名称各有两个索引候选，键只匹配其中一个；13 个低 LOD 均来自同一 FBX 所在 bundle，与 owner 分组一致。该证据只确认这些实测键的含义和离线文件对应，不作为静态消费者清单或跨版本固定键规则。
- IDA 沿实测 provider vtable `+0x18678D0` 找到构造 `+0x1BF710`，其输入文件字符串保存在 provider `+0x30`，读取方法沿 `+0xD73BC0` 使用同一字段调用文件服务。`+0xAF980` 的 span 布局已从程序集核对。只读 v4 已添加受 provider 类型和播放器签名约束的路径值记录，未调用 getter/文件服务，未增加 hook；实机路径值仍待下一次启动验证。该读取文件路径和逻辑 `.asset` 容器路径是两层身份，不能直接画等号。
- v4 实机启动已把 provider 的 CAB 身份与 AnimeStudio 的 PathID 索引逐项核对：18/18 目标唯一匹配，5 个 LOD0 的同名候选按 CAB+PathID 正确消歧；13 个低 LOD 共用 FBX 所在 CAB。所有读取事务边界和失败计数均通过，资源身份观测不再是当前阻塞项。证据见 `diagnostics/mesh-resource-read-owner-20261002/source-observation-v4-targets.json`。
- 游戏根目录的旧 Vulkan 代理仍启用了 GPU 捕获，单独部署 `plugin/eff.dll` 不会关闭它。已换为未定义 `EIEM_ENABLE_GFXTRACE_CAPTURE` 的普通代理；独立进程验证继承的捕获启用变量被覆盖，并成功创建 Vulkan 实例而未加载 `renderdoc.dll`/`gfxtrace.dll`。游戏内是否关闭提示需要新进程验证，现有进程不会因磁盘 DLL 更新而改变。

#### 部分证明

- `sub_1800D58A0 -> sub_18019BD80` 是当前播放器中读取后的 native 收尾/注册链，已有 IDA 和代码引用证据；但还没有完成“替换载荷经过该链后所有私有字段都有效”的实机证明。
- `+0x1EA2A0` 开头读取的逻辑 Mesh 名可以和 `mod.ini` 的 asset 名关联，但同名资源确实存在，所以名称不是完整的路径/PathID 身份。
- 离线往返输出与源对象**不是字节完全相同**：当前写出器会重算骨骼 AABB，并对权重做浮点重编码。因此“能写出同样大小的对象”不能作为原生载荷已兼容的证据；写出器还需要逐区段的语义校验和游戏侧试读验证。
- 解包文件、CAB/PathID 和 EFFMESH 资源身份已在离线侧可用；本轮已完成读取事务和实测 PathID 核对，仍缺运行时文件身份绑定。
- 修正后的 source-observation-v2 独立构建禁止 clone/entry-replace 宏组合，并在统一 Render 动作入口停止所有 Mod 写入，只观察游戏源 Mesh；不安装 finalizer hook。仅前 32 个无关对象采样，INI 声明的目标 Mesh 名称样本不限量。记录外层 stream 的 header/cursor/base/limit、调用返回地址及已有 VFS stream-path 表的命中结果。stream 首字段实测为 `0x800101`，不是 vtable；此前日志的 `streamVtable` 命名不正确。已有一次取得全部目标样本且未见新崩溃报告的启动；稳定性验收仍未完成。
- v2 的 18 个目标读取 `streamPath` 均为 `?`，说明现有 managed VFS stream-pointer 表没有直接关联该 native 读取流。v3 已通过外层 `+0x133800` 建立 owner/对象键与 Mesh 的事务关联；v4 从该 owner 的 provider 记录实际文件路径，不重复查询同一张空的 VFS 指针表。取得文件值前仍不能将名称命中提升为完整身份命中。
- source-observation-v3 已完成实机事务/范围验证和 AnimeStudio PathID 核对，owner/provider 的完整资源身份仍未在运行时取得。没有调用会释放数据块的 `+0x1340F0` 来取位置，也没有写入输入载荷。
- 静态分析发现 `+0x127B040` 是游戏自身磁盘资源 reload 的候选，内部会回到 `+0x133800`，但当前未调用。它是否适用于已被 Renderer/VFX 引用的 Mesh、线程与任务完成边界均未证明，不能作为 F10 已可原生重建的结论。
- 资源级替换应覆盖同一源资源的所有消费者，这是本项目的资源语义；残影是否复用同一 Renderer 或 native 对象不是替换前提，也不需要作为额外诊断分支。

#### 待证明

- 已取得的运行时 owner/PathID 事务如何绑定到当前资源文件身份；该问题已由 v4 对 18 个目标完成 CAB+PathID 交叉核对。不能以同名 Mesh 或单独的 PathID 代替完整身份。完整读取边界在本轮样本中已通过。
- 游戏 Mesh 序列化载荷的完整字段布局，以及如何在读取前提供一份兼容的完整 Mod 载荷。
- `+0x1EA2A0` 的替换事务能否安全地换入兼容 Mod 载荷。外层七参数 ABI、内外层关联、读取窗口在 teardown 前的快照已核对并测试，但替换输入后的长度、对齐和 provider 所有权尚未验证。它是在已有 native Mesh 上消费流，不能靠修改当前缓冲区的一小段实现，也不能把它误认为“在 native 对象创建前直接换对象”的入口。
- 游戏是否接受当前 EFFMESH v6 的多 submesh、索引、权重和材质槽布局，并由自己的流程生成有效 native 字段。
- 冲刺材质槽替换面对 Mod 多槽时的槽位映射和 VFX 材质实例 fan-out。
- F10 如何使原生资源缓存失效并重新进入同一读取链。
- `+0x1EA2A0` 的外层 stream 是否能稳定回指完整资源路径/PathID；这决定资源身份绑定应接在 VFS stream、上层资源句柄，还是更早的解码对象上。

以下内容**不需要作为前置条件证明**：残影是否使用同一 Mesh 实例、Render 是否共享同一对象、是否创建额外 Renderer、全场景 Mesh 扫描。这些都不改变资源身份替换原则。

## 3. 推荐架构

### 3.1 资源解析和原生构造分层

```text
原游戏资源身份
    -> EiemResourceResolver（命中 Mod 规则）
    -> EiemMeshPayload（不可变的顶点/索引/槽位/骨骼数据）
    -> 游戏自己的 Mesh 反序列化/工厂/原生收尾
    -> 游戏创建的 native Mesh
    -> 游戏创建 Renderer、VFX、LOD、蒙皮和材质实例
```

`EiemResourceResolver` 只回答“这个源资源是否有替换资源”。它不持有 Renderer，也不修改已经创建的 native Mesh。

`EiemMeshPayload` 是一个不可变的作者资源代际，包含完整 EFFMESH v6 数据：所有 submesh、每个槽的材质身份、索引范围、顶点通道、bindpose、骨骼供体信息和版本号。它不能只保存一个合并后的材质槽。EFFMESH v6 目前是作者侧格式；它能否直接或经适配写入游戏的二进制读取载荷，仍属于待证明事项。

`EiemNativeMeshFactoryAdapter` 负责把 payload 交给游戏已有的创建/反序列化入口。它不是新的 Mesh 工厂，也不调用未知的 `Object.Instantiate` 来猜测私有字段。游戏完成原生对象初始化后，DLL 只做只读登记和规则关联。

### 3.2 唯一替换 Seam

唯一允许的替换入口是：

```text
共享资源反序列化/解码
    -> 绑定源资源身份
    -> EiemResourceResolver 按原资源身份选择原始或 Mod payload
    -> 游戏构造 native Mesh（+0xD9E70）
    -> Mesh vtable 槽 18（+0x4710D0 -> +0x1EA2A0）消费完整 Mesh 流
    -> 游戏把载荷写入自己的 native Mesh 并执行后续收尾
```

当前游戏版本已经确认了目标 Mesh 的读取/初始化链。运行期已验证的是当前播放器中 body/cloth 目标资源的这条链；它是否同时覆盖所有世界、UI、NPC 资源类别仍需验证，但不允许用另一条替换路径补足：

```text
UnityPlayer +0xD9E70
    -> native Mesh 对象构造
Mesh vtable +0x90 / UnityPlayer +0x4710D0
    -> UnityPlayer +0x1EA2A0
    -> 从游戏流读取 Mesh 二进制载荷并写入该对象
sub_1800D58A0
    -> sub_18019BD80
    -> native 资源收尾/注册
```

`+0x1EA2A0` 已经对目标 body/cloth 的 12 个源 Mesh 实际命中；旧候选 `+0xDA400`
没有命中这批目标。它是当前版本的唯一目标读取 Seam，但不是已经验证可写入的替换接口。当前尚未完成的是：确认这次读取对应的完整资源身份、流的所有权与跨缓冲边界，并在不改变对象生命周期的前提下构造游戏能接受的替换载荷。这个问题解决后，替换就在 `+0x1EA2A0` 消费输入的同一条链上完成。

在载荷格式和流事务没有被证明前，代码只能保持原状并继续取证；不能改用 `SubMeshInfo.mesh`、`CreateSMSGO`、`CreateMeshAssets...`、`sharedMesh` setter、Renderer 扫描或其他替代入口。确认失败的结果是“不实现替换”，而不是增加第二条路径。

最终收尾函数（包括目前调查过的低层 native 写入路径）不是替换 Seam。它的职责是处理游戏已经选定的输入，不应成为 DLL 写私有字段的入口。此前的宽范围 finalizer hook 已证明会大量命中非目标对象、产生高频日志，并在错误的身份解析下导致场景加载风险；该实验必须保持关闭。

### 3.3 多 submesh 规则

多 submesh 必须作为一个完整 Mesh payload 进入游戏流程：

- `subMeshCount`、每个索引缓冲区和索引范围一起提交；
- 每个原始槽按“源 Mesh 身份 + 原始槽号”映射到 Mod 材质槽；
- 未声明的槽保留游戏值，不能压缩成一个槽再依靠 Renderer 材质数组补回；
- 骨骼索引、bindpose、顶点通道和 LOD 结构保持 payload 内部一致；
- 游戏创建的 Renderer 继续使用自己的 `bones[]`、`rootBone`、LOD、VFX 和 owner 生命周期。替换判定只依赖源 Mesh 资源身份，不依赖残影是否复用同一个 Renderer 或 native Mesh 实例。

如果一个游戏源 Mesh 只有一个槽，而 Mod payload 有多个槽，不能在成品 Mesh 上偷偷扩槽。必须在游戏接受 Mesh 描述或序列化数据的阶段一次性提交完整的 submesh 结构；如果当前游戏入口不允许扩槽，该资源应报告为不兼容，而不是进入运行时补丁路径。

## 4. F10 和热更新

F10 需要更新的是资源代际，不是正在使用的 native Mesh 的字节：

```text
读取新 Mod
  -> 校验并建立不可变 Generation N+1
  -> 发布资源解析表
  -> 新建实例走游戏创建 Seam，消费 N+1
  -> 已存在实例标记 dirty
  -> 在 owner 支持的重建/重新装配边界重新走同一 Seam
  -> 所有旧引用释放后退休 Generation N
```

关键边界如下：

- F10 不能修改正在被渲染、蒙皮或异步任务使用的 native Mesh；
- 已存在实例如果没有可靠的游戏重建入口，只能延迟到下一次 owner 重建、场景切换或明确的原生 reload；不能伪造一次 `sharedMesh` setter 作为热更新；
- 新增挂载 Mesh 由游戏自己的创建入口自然消费当前代际，DLL 不需要逐帧发现；
- 旧代际保留到引用归零，避免 VFX、LOD 或延迟任务仍指向已释放资源；
- F10 失败时保留整套旧代际，不允许 Mesh、材质和贴图半新半旧。

因此，“从创建阶段开始更新”是正确方向，但不是每次游戏调用任意 Mesh 构造函数都创建一个 Mod Mesh。正确粒度是：**一个命中的源资源，在一个资源代际内通过一次资源解析替换，之后由游戏按自己的共享语义决定 native 对象数量。**

冲刺链已经确定为材质槽替换，属于 Mesh 创建后的材质实例行为。它不需要新的 Mesh 命中规则，也不需要根据 Render 拓扑推断残影来源；需要验证的是完整 Mod payload 的材质槽映射能否为游戏的冲刺材质槽提供对应的 Mod 材质实例。

## 5. 诊断和证据要求

在实现 Seam 前，诊断只记录有边界的、可解释的样本：

| 阶段 | 记录内容 |
|---|---|
| 资源解析后 | 源资源身份、Mod 资源身份、payload 代际、调用线程和 owner 类型 |
| 游戏创建完成后 | managed Mesh 指针、native 指针、顶点/索引/submesh 公共计数、有限范围 hash |
| Renderer 装配后 | Mesh 身份、槽位映射和原生装配结果；Render 拓扑不是资源命中的前提 |
| 冲刺材质事件 | 记录被替换的材质槽和对应 Mod 材质实例；不拦截 VFX Mesh 绘制 |

诊断不读取或写入未知 native 字段，不复制首地址附近的 opaque bytes，不使用猜测偏移作为生产逻辑，不做每帧轮询。现有 runtime snapshot 只能作为有限范围对比工具，不能证明整个 native 对象已复制成功；当前代码没有证明 Mod Mesh 已经进入游戏最早的创建阶段。

## 6. 代码重构边界

| Module | 当前职责 | 重构后职责 |
|---|---|---|
| `eiem_resource_backend.h` | 解析 EFFMESH，并直接构造 Unity Mesh | 只解析并校验 `EiemMeshPayload`；构造交给 Factory Adapter |
| `eiem_render_executor.h` | 已装配 Renderer 上改 `sharedMesh` | 只执行材质、显隐和已验证的实例重绑；不负责源 Mesh 初始化 |
| `il2cpp_trace.h` | 混合 Hook、登记和诊断 | 只保留已验证的资源/owner Seam 观察器 |
| `eiem_native_mesh_runtime_snapshot.h` | 运行时 native 快照 | 独立的有界诊断 Module，默认关闭高频输出 |
| `eiem_native_mesh_entry_probe.h` | 实验性的低层 finalizer 入口 | 从生产构建移除，保留为归档材料或删除 |
| `eiem_mods.h` | Mod 解析和代际发布 | 发布不可变资源代际和解析表 |
| `eiem_model_registry.h` / `eiem_model_lifecycle.h` | 实例登记 | 提供 owner 重建、dirty 和旧代际退休接口 |

建议新增以下 Interface：

- `IMeshResourceResolver`：源资源身份到 `EiemMeshPayload`；
- `INativeMeshFactoryAdapter`：把 payload 交给游戏已验证的创建入口；
- `IModelRebindAdapter`：在 owner 支持的重建边界重新请求装配；
- `IMeshObservationSink`：一次性记录创建、装配和材质槽事件证据。

这样可以保持 **Locality**：资源规则只在资源解析层命中，装配适配器只处理对应 owner，诊断不会反过来改变执行结果。

## 7. 分阶段实施

1. **安全收口**：删除或隔离 finalizer 入口实验；生产路径禁止修改源 Mesh、交换 `m_CachedPtr`、复制未知 native 字节。
2. **建立 payload/factory 边界**：把 `EiemBuildNativeMesh` 拆成 payload 解析和 Factory Adapter 两部分，暂时不改变已验证的材质/显隐功能。
3. **完成唯一读取 Seam**：围绕已确认的 `+0x1EA2A0` 记录资源身份、流对象所有权、完整载荷边界和替换事务；在证据完整前不实现第二个入口，也不猜 native 偏移。
4. **单资源试验**：只对一个 body 或 cloth 源资源替换，在冷启动、残影、冲刺和三端实例中验证由游戏创建的 native Mesh；残影只验证资源身份命中，冲刺只验证材质槽替换。
5. **多 submesh 扩展**：确认游戏入口能接受完整槽位和索引布局；不兼容时明确失败。
6. **F10 代际重绑**：接入 owner 的重建/重装配边界；没有完成点时采用延迟应用，不回退到运行时改 Mesh。
7. **清理旧路径**：移除 post-finalizer Mesh 替换、逐帧同步和无界诊断，只保留材质实例、显隐和已验证的重绑操作。

## 8. 验收条件

- Mod Mesh 的 native 对象由游戏自己的资源/创建/收尾流程产生；
- 顶点、索引、骨骼、bindpose、所有 submesh 和材质槽都来自同一个 payload 代际；
- body、cloth、残影和其他使用同一资源身份的消费者得到一致的 Mesh 资源；
- 冲刺只在材质实例层处理消融，不需要再次改 Mesh；
- 冷启动、场景切换、F10、owner 释放和延迟 VFX 均无闪退、躺地或悬挂引用；
- 没有每帧资源扫描、每帧 native 同步或全场景 Mesh 替换；
- 任何无法证明的创建入口或私有字段都记录为“未验证”，不能用猜测代码填补。

## 当前决策

不采用“运行时复制源 Mesh native 数据”。唯一采用“`+0x1EA2A0` 读取输入替换 + 游戏自己的 Mesh 创建流程”。不增加部件描述符、Renderer 或运行时替换回退。F10 使用资源代际和 owner 重建，不修改现有 native Mesh。下一步工作的唯一前提，是完成该读取链的资源身份、完整流边界和可替换载荷验证。

### 2026-10-02：PFB 装配验证与运行时控制边界

用户同意把作者文件转换为原生兼容的序列化数据，并要求先以冲刺/平 A 实机事件验证 Mesh 到 PFB 的引用，再考虑按规则修改装配资源。本轮离线反向依赖只用于候选过滤和作者侧检查，不生成生产用的消费者命中清单；资源替换依旧按原始资源身份执行。

对当前 18 个目标资源，已有精确 CAB + PathID 身份。离线得到 46 个 PFB 候选，其中 25 个可把实际 Mesh PPtr 关联到 PFB 层级，21 个根仍未解析。运行期的请求、实例完成和池化取用探针已构建，但冲刺/平 A 归属仍待实机日志；不能把加载事件当作绘制事件。证据和限制见 `diagnostics/prefab-resource-validation-20261002/README.md`。

资源初始化和实例控制不是相互排斥的方案。Mesh 中新增形态通道的名称、帧和增量应进入序列化载荷；Renderer 当前权重、INI 变量、按键和 UI 属于运行时控制。插件绑定并控制的是游戏从替换载荷创建的对象，而不是另行注册一个自建 Mesh 来替换它。源资源没有形态键不否定序列化新增通道，但自定义 GPU/蒙皮路径和游戏动画写入器的兼容性仍需验收。

PFB 只在材质数组、bones、初始形态权重等装配数据确实不匹配时需要修改。贴图通常通过 PPtr 引用；新增 Material/Texture 必须有合法对象表、依赖、流数据和资源注册，不能仅改路径。冲刺动态材质变化仍是独立的实例适配问题，不能用静态 PFB 修改宣称已解决。

此前“按键显隐可直接沿用现有控制层”的表述需要收窄：旧实现原地改变 submesh 索引，与新架构禁止运行期修改几何冲突。必须验证新的显示状态接口后才能承诺兼容；本轮没有引入替代 Mesh 修改路径。F10 更新几何仍须证明游戏原生重载和已有消费者引用更新，资源首次读取 Hook 本身不提供缓存失效。
