# 完整资源热重载验证

更新：2026-10-05。当前为已部署 v35 资源输入入口及 F10 路径匹配修复。首次加载使用最新完整代际；F10 直接加载当前 Mod，世界与已记录的 UI 模型部件用游戏生命周期重建。完整实机 COMMIT 和画面尚未验收。格式、性能和范围以 [离线包契约](offline-resource-package-format.md) 与 [实施结果](offline-resource-reload-plan.md) 为准。

## v35 构建入口纠正与 F10 路径匹配

PID 41412 已加载候选资源，但游戏返回 `Assets/Beyond/...`，清单记录 `assets/beyond/...`；直接字符串比较使 owner 被跳过，随后 `live Renderer has no recorded game model-part lifecycle` 拒绝提交。世界 owner、已记录展示部件和重建后的根验证现共用规范化路径查找。完整 Frame 回归覆盖世界/UI 路径大小写和斜杠差异，并通过连续八次重载、回滚和容器退休测试。

上一轮错误地部署了 `build.bat` 生成的旧入口 `bin/eff.dll`（SHA `A601B146E7889FC3526082FE59F59B3DA64E393A20D00C30CB7616B0F597CC03`）。PID 16096 日志报 `Unknown section: Mod`、`accepted=0`，因此源 Mesh 保留；该产物没有资源输入启动替换或完整资源 F10 入口。正确构建命令为 `tools/diagnostics/build_resource_directory_entry.cmd`，产物为 `bin/resource-directory-entry/eff.dll`，编译源为 `src/eiem_resource_entry.cpp`。

12:25 已确认游戏停止并原子部署正确产物，安装 SHA `F204B3136FD53745DCAE06D39DF1C903DD98A0F71423A2298ADE1F503A7F49E2`。部署前后 29 个 Mod、配置、current 与普通代理文件 hash 保持；GFX 关闭、反虚化开启。实际生产启动/F10 磁盘验证通过 60 条启动替换、68 个候选输入与 18 个闭包；这不调用 Unity 模型迁移 API。备份和证据记录在 `diagnostics/f10-entry-deployment-fix-20261005/deployment.json`。纠正后的首次画面与 F10 游戏内提交仍待重启验收。

## v34b 辅助文件与贴图改名修复

用户放入 `LZY/textures/WHITE.png` 和 `eff_texture_convert.exe` 后，v34 启动外部编译返回 `unreferenced/source files in Mod directory`，随后 ENTRY-FATAL，替换入口未启用。INI 声明正确；错误来自将辅助文件也要求声明的目录校验。

v34b 的 DLL/C++ 编译器/Blender reader 同步排除 PNG、PSD 和转换器 EXE；辅助文件不参与作者摘要和监视器构建触发。启动编译失败会尝试读取已存在且通过校验的完整上一代，而不直接放弃所有替换。F10 在贴图改名后先判断 INI 变化，再取得作者文件时间戳；删除旧 TEX 不再让构建请求提前中断。

转换器保留 PNG 的完整文件名，只把扩展名改为 `.tex`，更新全部共享 `Texture.path`，材质 ID/槽和贴图参数保留。旧 TEX 与 INI 归档到 Mod 外。任意名称可在拖入窗口选择已有声明或用 `--texture` 指定。两处已安装转换器均更新，用户 WHITE.png 未自动转换，正式资源未修改。

实际 Mod（保留用户 PNG/EXE）通过安装后编译器和生产启动/F10 磁盘验证，生产 key 未改变。隔离链路两次执行 TEX 改名/旧文件删除，生产 Prepare/PollOffline 调用外部编译器并选中新候选，已准备图和重新读取清单的路径均通过。43 Mesh、5 Material、11 Texture 的独立原生回读通过；Blender 打包 reader 和实际编译模块通过。以上不代替游戏内 F10 原生装配验收。

普通 DLL 在确认游戏停止后安装到实际 plugin/eff.dll，SHA `B953062AA9BB77C3F2259D2D665041A9BA1E9AEBE283B9C218C496E12EBFD521`；游戏与 Blender 编译器 SHA `1A87173C4C410604267AD587F6C69B7269CA418C88A3F9287303305EA007CC4F`。GFX 保持关闭，反虚化开启。部署记录 `v34-direct-full-reload/deployment-auxiliary.json`，回归 `rename-reload-verification.json`、`production-loader-installed.txt`、`blender-installed-followup.txt`。首次部署跨盘 File.Replace 备份失败，已回退；最终安装改用目标盘备份并完成 hash 核对。

用户测试：重启进入场景，确认初始模型；把修改后的 PNG 拖到更新后的 EXE 并选目标声明，看到同名 TEX 输出后按一次 F10，观察贴图及模型/形态/残影。未准备新包时外部编译仍约 19 秒；不是毫秒级端到端热重载。操作见 [手动转换](manual-png-texture-conversion.md)。

## v34 基础验证

普通 DLL、游戏/Blender 同 ABI 编译器已安装；GFX 关闭、反虚化开启。临时青绿色身体贴图已恢复原作者内容，其他作者文件、普通代理和静态回退表保持。部署及 hash 记录见 `diagnostics/resource-directory-entry-20261005/v34-direct-full-reload/deployment.json`。

`geometry-cache-verification.json`：真实冷/缓存构建的 136 文件一致，像素/材质/控制/Mesh 编辑的发布和失效正确，损坏作者保留旧 current。`production-native-payloads.txt`：43 Mesh、5 Material、11 Texture 的原生数据与作者匹配。`production-loader.txt`：最新启动加载表、60 实际替换、68 图输入、18 锁定闭包、控制及 20 次后台 F10 请求通过。`transaction-smoke.txt`：世界/UI 完整 Frame、连续 F10、回滚、共享 lease、容器退休通过；`idle-pool-red.txt` 保存遗漏闲置路径池的失败回归，已修正。

现版本支持当前 LZY 单 Mod/世界及已跟踪 UI 模型部件；persistent pool 和未记录生命周期的活 Renderer 仍拒绝迁移。旧容器 Unload(false) 与原生 Release 保留仍被使用的对象，不声明穷尽 job fence。

测试：重启一次进入场景，编辑/导出/转换实际 Mod 资源，按 F10 检查 Mesh/贴图/材质/配置变化、按键/形态以及走动和平 A 身体残影。没有修改资源时重装相同内容，外观应不变。首次提交不会自动青绿色；此前自动或临时颜色步骤仅为历史。

下文 v33a 及更早部署/故障/实验记录为历史，不作为当前操作说明。

用户报告卡在加载阶段。真实会话 run-31504 在 07:42:30.608、目标世界绑定为零时自动开始额外候选预加载；07:42:31.175 原版 PFB 租约失败，随后排空。本轮没有 F10、候选 Bundle 或 COMMIT。Player.log frame 69 在登录 LoadScene／WaitForAllAsyncOperationsToComplete 中记录两个 Bundle 加载错误。帧回调之后继续运行，不能据此宣称主线程冻在我们的 Frame 内，或将整个加载停滞的机制视为已完整证明。

v33a 给额外候选增加 AwaitWorld：准确目标 Renderer／helper／Entity／ModelComponent／根／主模型部件及父子归属成立，同一根至少观察 1000 ms，才允许 ResourceManager／Bundle／PFB 请求。首次 Mod 替换继续随游戏原有资源加载完成。首个请求前 owner 丢失回到等待；已有在途请求则拒绝并完成 drain。提前 F10 保留，等待世界角色与原生预加载时间分开记录。

生产 Frame 回归先复现无世界 owner 仍发资源请求；修复后登录期有／无早按 F10 均零请求，十类 UI／部分装配／released／外部根与路径／无关 marker 不打开条件，根变化重置稳定窗口、在途释放及 Dispose 一次通过。原有骨骼、枚举 ABI、异步并行调度、过期票据、重建失败回滚与 GC handle 回归保持通过。实际生产磁盘 worker 仍验证 68 输入／18 锁定闭包，UnityCalls=0，初次 1246.5 ms；20 次请求平均 0.282795 ms、最慢 0.4161 ms，均不是完整原生迁移性能。

普通 v33a DLL 于 08:02:38 原子部署至实际 plugin/eff.dll，SHA `D52931A1A26BD069E28F3F40BBE9C2E8C0DCC984CD7317202AD08023F18DE593`。部署前再次确认游戏停止，v33 回退保留。初始输入表／startup、青绿色作者与 current、共享编译器和 Blender、普通 proxy、反虚化设置保持，GFX 关闭。证据为 `diagnostics/resource-directory-entry-20261005/v33a-startup-prewarm-gate/{deployment.json,red-smoke.txt,green-smoke.txt,production-loader.txt,startup-hang-findings.json}`。

下一次合并测试：启动进入同一世界场景，正常模型显示后等约 10 秒，按一次 F10；成功提交应使 body 明显青绿色。走动、平 A、部件与形态按键检查身体残影和 toushi，再反馈进场／变色／异常。日志必须先有 PREWARM-WAIT，再有具备世界 root ID 的 PREWARM-START，后续 PFB／OWNERS／MIGRATED／COMMIT 与视觉报告一同验收。未观察实机结果前，启动问题及完整热重载仍为待验收。

v33 已于 2026-10-05 在停止游戏后部署至实际 plugin/eff.dll，SHA `AC4320469AA2B0E97BB61052C25034C72F73214AC6965062C1CC36ED38027928`。游戏与 Blender 编译器 SHA 均为 `0E564DCBB44D1096FD060D18D8D2A65B1FB7D0E5342A4027A40E4DBD09A65070`。首次部署时正式作者、普通 proxy、60 行输入表和基线保持；GFX 关闭、反虚化开启，v32c 回退保留。证据为 `diagnostics/resource-directory-entry-20261005/v33-offline-prewarm/deployment.json`。

用户要求不同资源后，另行发布青绿色 body BaseMap 候选 key `249A7CC976625A5CAFAF99362BAC40813E37CF1C4A8331C0A387A46490ADCD29`，清单 SHA `0A8A54173D2D512AA6F13650F8782025CF6C0A0D7BB2677E87B9EA6A580F41A6`。只有 textures/body_010de4985b31.tex 临时改变；24 其他作者文件、几何／槽／形态／控制／toushi 选择保持，所有 mip 的 alpha 完全相同。68 个新原生图的对象／数值引用／stream 布局与原代一致，broken reference／stream 为零；编译清单独立回读通过，原始输入表／startup／DLL 未动。34.4873 秒为进场前离线冷编译，不是 F10 耗时。证据为 `v33-different-resources/{visual-local-verification,visual-deployment}.json`。

本轮第一次 F10 成功应使身体明显青绿色；第二次 F10 不会自行恢复，须先恢复备份的作者贴图并发布原代。恢复工具为 deploy_offline_visual_witness.ps1 -Restore。实机 COMMIT、GPU 像素与完整资源热重载仍未验收。

## 当前事实

v26 F10 重读控制和全局配置，资源契约变化会被拒绝并保留旧代。已有离线构建器能重建 Mesh／Material／Texture／PFB 输入；当前 60 条替换输入和 43 个 part/LOD 身份已验证启动加载。它们不证明新代能与已加载资源共存，也不证明现存实例能无损重装配。

本次静态证据使用当前 global-metadata.dat 和 GameAssembly.dll，重新验证先前的 CodeGenModule 身份、方法 token 和 PE 范围。RVA 仅用于这一份文件的调查；实际探针按 namespace/class、完整参数、返回类型和 static 标志解析，不以 RVA 安装。

候选入口：

- EntityRenderHelper._InitRenderAndMaterial：静态正常路径包含 MaterialController.Init 和 VisibleController.Init。
- EntityRenderHelper.ReFindRenderersInChildren：有完整元数据入口；直接反汇编含 IFix 分支／跳转表，不能把整个 PE 范围视作已证明的可达调用图。需要自然调用证据。
- ModelComponent.ChangeMainModelSync：正常静态路径调用 SetModelId、RemoveMainPart、LoadMainModelSync，并处理跟随特效。需要确认模型 owner、释放、装配和动作／战斗状态的生命周期，不能仅以方法名判定适合 F10。
- BaseModelComponent.SetModelId／LoadMainModelSync／RemoveMainPart：观察正常主模型装卸及其嵌套显示初始化。
- BundleResourceManager／ResourceManager：本轮只登记元数据；FAssetProxyHandle 是值类型，尚未验证的原生返回 ABI 不安装 detour。

## R0：本轮实机观察

启用文件：game/plugin/resource-input-diagnostics/full-reload-observation.enabled。移走该文件后下次启动关闭观察。继续使用普通代理，GFX 关闭。

十个逻辑入口只透传游戏自然发起的操作：helper 初始化／重查／释放、material controller 初始化／释放、model change、base model 的 set/load/remove、AssetBundle.Unload。安装过程不调用这些游戏业务函数。保留实例、全部参数、原 MethodInfo、调用次数、原异常和 TLS 嵌套归属。

只读装配快照通过当前元数据字段和已验证的公共 getter 读取：输入 Renderer 数、登记 RendererInfo 数、作者 Mesh 标识、骨骼槽／bindpose 数、rootBone、skinningRoot 和控制器身份。字段缺失／类型不符仅拒绝诊断快照，不影响已完成的原调用。部件细节过滤不参与生产资源匹配。

资源观察复用已安装的 LoadAsset 入口，只记录已提交替换输入所属 bundle 的加载结果。相同 bundle／asset 返回相同对象指针不是缓存失效证明；本轮没有新的候选 bundle，也没有显式 unload。

### 用户操作

1. 进入此前的 LZY 场景，静止约 10 秒。
2. 按一次 F10，等待约 5 秒，再走动、平 A 触发残影；试一次已验收的部件按键。
3. 打开角色展示页，在另一角色与 LZY 之间切换，再返回场景，停留约 10 秒。
4. 正常退出。记录是否卡顿、模型／残影／按键是否异常。

本轮不要求修改资源文件。F10 测量的是现有配置提交，不能作为完整资源重载成功或其最终耗时的结论。若展示切换未自然触发某候选入口，结果为未观察到，而非入口验证通过。

### 数据与判定

- RELOAD-PROBE-READY：逐项确认 10 个准确签名 hook 是否安装。缺失入口列为未覆盖。
- RELOAD-PROBE-EVENT：以事件 ID 核对 enter/return、helper/model owner 的嵌套、MethodInfo、原异常。
- RELOAD-PROBE-ASSEMBLY／PART／HELPER：核对新建／重建和释放的对象身份、登记数、逐 Mesh 骨骼槽与 bindpose 数。数量相等不等于矩阵和槽序正确。
- RELOAD-PROBE-ASSET／UNLOAD：记录自然加载和释放身份，供后续缓存／寿命调查。
- RELOAD-PROBE-PREPARE／COMMIT：后台准备与现有配置提交各自的 QPC 耗时。
- RELOAD-PROBE-FRAMES：每 120 个主线程回调间隔输出一次 mean/p95/max 及长间隔计数；每轮最多 30 个窗口，F10 准备记录后重新开始。不是 GPU 计时，也不等同于已测显示帧率。
- 原函数 elapsed 与诊断读取 elapsed 分开记录。日志 I/O 和探针仍可能扰动实机耗时，因此最终性能验收还需关闭探针做对照。

汇总：python tools/diagnostics/analyze_full_reload_observation.py <entry.log> <new-output.json>。报告明确保留 safeExplicitRefreshEntryVerified、generationCacheIsolationVerified、existingInstanceResourceMigrationVerified 和完整重载标志为 false。

## R1：候选代资源加载／缓存隔离

前提是 R0 已取得真实加载／owner 边界和准确线程。先从原始输入构建内容不变的候选代，验证候选 bundle/CAB/member 身份和依赖闭包处理；候选映射仅对候选加载作用域可见，尚未发布到生产加载表。全部候选操作使用已验证的原生加载 ABI，覆盖同步、异步完成及显式持有。

通过标准：原生接受候选；实际创建的 Mesh／Material／Texture 与候选内容、跨文件引用相符；旧实例继续工作；候选失败保持旧代；跨代异步结果有归属；卸载不使活动实例失效。把路径改名或修改缓存字典不是通过标准。

## R2：现存实例迁移／回滚

先验证装配拓扑不变的一组资源变化。以自然调用证据确定显示 owner 的支持边界，明确该边界是否同时更新材质、可见控制、LOD、capture 和原生登记。候选创建仍由序列化输入和游戏工厂完成，现存迁移不能退回私有 Mesh／Renderer／Job 写入。

通过标准：无需退出场景，当前和后续实例一致使用新代；逐 Mesh 骨骼路径解析到同一角色，槽序和 bindpose 配对；形态键按名迁移；位置／动作／战斗状态满足保留目标；旧残影保留至原生释放，新残影采用新代当前形变；故障注入能恢复旧代。仅 sharedMesh 变化或源部件消失均不是完整验收。

## R3：完整结构和性能验收

覆盖部件增加／删除、Renderer 数变化、LOD、skip、形态通道数量和顺序、骨骼槽变化，以及 Mod 增删。分别覆盖世界、UI 和有直接证据的 NPC 消费者；专用描述符未验证时应明确拒绝，不报告完整支持。保留原版与自定义贴图选择，特别是 toushi。

性能分开报告总准备耗时、主线程候选创建、提交帧成本和旧代回收。相同场景、画质和操作做探针关闭对照；只改变少量内容时检查增量重建范围。至少包含连续重载、失败回退、旧代异步完成和活动残影。所有阶段通过后才命名为完整资源重载。

## R0 实机反馈与 v28 修正（2026-10-04）

两轮 v27 日志确认配置 F10：后台准备分别 594.5424／586.7047 ms，主线程提交分别 0.1907／0.2225 ms，均提交 generation=2、55 个绑定 Renderer。这些不是完整资源重载耗时。日志同时记录身体形变残影快照与正常 Release；视觉验收尚未由用户确认。

v27 实机发现两个探针覆盖缺口：MaterialController.Init 已由正常形态键模块挂接，第二个 detour 被去重拒绝；SetModelId 实际返回 System.Boolean，Void 签名被准确校验拒绝。没有以不匹配 ABI 调用它们，也没有将缺失证据记为通过。

v28 将材质观察接入同一个正常 Init detour，使用同一个原函数 trampoline，不新增第二层 MinHook；初始化的原参数、MethodInfo、一次调用及形态键注册仍保留。SetModelId 按 Boolean 签名记录并原样返回 true／false。新增真实 MinHook 回归覆盖共享边界、原异常、Boolean 结果。采样预算改为每个边界 128 次，避免初始化／释放噪声耗尽后续模型入口证据。

下一轮仍执行 R0 操作，特别是角色展示页切换。需要得到 RELOAD-PROBE-SHARED、materialInit=1、setModelId=1，以及 ASSEMBLY／PART 的实际登记记录；缺失自然模型切换调用仍是未观察到。资源加载观察没有获得候选代缓存隔离证据，R1/R2 仍未验收。

## R0 v28 实机结果（2026-10-04 09:24–09:26）

完整日志为 `run-35048-01DD539F2679008F`；`run-44540-01DD539F3416403F` 只有启动两行，不作为闪退证据。已保存日志、解析结果和 `runtime-findings.json` 于 `diagnostics/resource-directory-entry-20261004/v28-shared-assembly-observation/`。

- 共享 Init 和 Boolean SetModelId 入口均安装，全部十个入口覆盖成功。632 次有界采样各有一次配对返回，无重复 ID、对象身份／MethodInfo 变化、原异常或诊断字段读取错误。初始化和材质边界各到达 128 次采样上限，统计不是全程调用总量。
- 128 条装配采样中输入 Renderer 数与登记 RendererInfo 数均一致；79 条作者部件记录覆盖 43 个 part/LOD 标识，未出现骨骼槽／bindpose 数量不匹配。身体在三个 helper 实例中均为 14693 顶点、2 个形态键、84 个骨骼槽和 84 个 bindpose。计数不证明矩阵、槽序或视觉效果。
- 身体残影快照 7 次，释放 7 次，最后 live=0。用户确认操作完成，尚未明确报告本轮外观和卡顿情况。
- 一次配置 F10 后台准备 594.5041 ms，主线程提交 0.1978 ms，55 个活动绑定 Renderer，generation=2、resourcesChanged=0。角色展示过程中观察到 113 次 SetModelId、11 次 ReFindRenderersInChildren，但未观察到 ChangeMainModelSync／LoadMainModelSync／RemoveMainPart 的自然调用。仍不证明显式刷新入口安全。
- 16 次替换输入提交，原 managed getter／String LoadAsset 的返回观察计数仍为零；Mesh 装配已实际存在，因此该计数表示观察覆盖缺口，不能解释为加载失败。

## v29：原生完成和哈希资源返回观察

当前 PE 的已验证 wrapper 直接把 managed self 转给 `UnityEngine.AssetBundleCreateRequest::get_assetBundle()`，并将 self／Int64 hash／Type 转给 `UnityEngine.AssetBundle::LoadAssetByHash_Internal(System.Int64,System.Type)`；均为指针返回，未使用 injected／值类型返回 ABI。静态证据保存于 v28 `loader-return-static.json`。RVA 只用于这一份 PE 的审计，探针仍按当前完整 managed 签名校验并在运行时 resolve icall 地址。

待检验的预测按优先级为：

1. 实际资产由哈希路径返回：nativeHashAssetCalls 和关联资产记录应出现，而原 String 观察仍很少或为零。
2. 异步完成通过原生绑定绕过被 hook 的 managed wrapper：nativeBundleGetterCalls 与关联 request→bundle 记录应出现，原 managed 调用缺失。
3. 若实际 binding 有调用但关联对象缺失，应出现 unmatched completion；继续调查调用方／对象归属，不能将零关联当作成功。

探针只透传游戏自然调用，原参数、一次调用、返回值和异常保留。仅诊断所需的 request／bundle 使用弱 GC handle，最多 256 个；按实际弱引用检查对象身份、回收失效记录，在游戏自然 Unload 返回后清除关联。没有强持有、主动 getter／加载／卸载，也没有新候选发布。

每条关联同时保存加载输入的发布序号及已验证的序列化文件 SHA。当前配置 F10 也会递增加载表发布序号；它不能称为新的原生资源内容代。相同内容和不同配置发布可以复用同一个活 bundle；不同序列化 SHA 返回同一活 bundle 则记录 conflictingContent，保留原关联。地址复用、重复指针及这些观察都不证明候选隔离或卸载安全。

本地真实 MinHook 回归覆盖准确 ABI 拒绝、原参数／hash／Type／返回／异常、managed/native 两层不重复计数、空结果、弱引用失效／地址复用／自然释放、相同内容配置发布、不同内容共享活对象的拒绝、采样预算和关闭透传。实机通过标准为捕获实际完成及资源返回，并能关联到各提交输入的序列化 SHA；这是 R1 的加载基线，仍不等于候选代隔离、现存实例迁移或完整重载通过。

v29 普通 DLL 已于 2026-10-04 09:46 部署到实际 `D:/Hypergryph Launcher/games/Endfield Game/plugin/eff.dll`，SHA256 为 `72DEFF5BAE26EE2D8E48813D2F747E6E55454C3CB7884723F940BA2114CD56DE`。部署时确认游戏进程结束，已保留 v28 DLL 和原 marker 的回退备份。普通代理与 60 行加载表 hash 均保持原验证值，GFX 关闭。真实 MinHook 的装配共享／native loading 回归和实际 LZY 按键／形态／残影桥接回归通过；新原生加载探针尚未取得实机日志。

下一轮操作：进入同一场景，按一次 F10，走动／平 A，切换一次角色展示页，然后正常退出并报告模型、身体残影与 F10 卡顿情况。检查 RELOAD-LOAD-READY 的两个 native binding、提交 SHA→request→bundle→hash asset 的真实关联，以及 conflictingContent／过期弱引用记录。自然 ChangeMainModelSync 仍未观测到；后续 R2 显式迁移须独立确定支持入口。

## v29 原生 Bundle 完成实机结果（2026-10-04 09:54–09:55）

完整日志为 `run-41304-01DD53A33A22969B`；`run-21040-01DD53A3482AC256` 只有启动两行。16 条提交输入的序列化 SHA／serial 均一一匹配到非空的 native.get_assetBundle 完成，16 个活 Bundle，pending=0，conflictingContent=0，无诊断错误。native getter 的 13240 次调用说明原 managed wrapper 未覆盖实际自然路径；core native 完成关联基线通过。

同步 native hash 入口记录 926 次游戏调用，但对这 16 个目标 Bundle 的关联资产为零，尚未验证真实资源返回。649 次装配入口采样均配对，骨骼槽／bindpose 数与装配登记数无不匹配。F10 配置准备 575.7772 ms、提交 0.1801 ms，55 个绑定 Renderer、resourcesChanged=0。身体残影快照／释放记录均为零，用户只报告照做，不能重新报告本轮残影视觉验收。

当前元数据确实提供 String／Int64 两种 LoadAssetAsync 入口。重新校验 AssetBundleModule 的 CodeGenModule 身份及当前 PE：LoadAssetAsync_Internal、LoadAssetAsyncByHash_Internal 和 AssetBundleRequest.GetResult 都为直接尾转发到相应原生绑定，返回指针；GetResult 与已验证 UnityEngine.Object 的同步 LoadAsset 使用同一返回类型。此游戏的 request 类只有 GetResult／asset getter，没有可用 allAssets 成员。静态证据保存于 v29 `async-asset-static-audit.json` 和 `asset-request-result-static.json`，不按静态 RVA 挂接。

## v30：异步资产请求与实际对象返回

保持 v29 原生完成关联，补 native String 同步、String／Int64 异步资产请求及 AssetBundleRequest.GetResult 的观察。各入口先按当前完整 managed 签名校验，再运行时 resolve icall；不安装不匹配的返回 ABI。String 同步由 native 层观察时，原 managed 层不再重复计数。

异步请求保存已校验 Bundle 的输入 SHA／发布序号／serial、弱 request 身份、复制的 String 名或 Int64 hash，以及 request 实际类型和 GetResult 方法身份。首次空结果保留请求，首次非空结果记录归属并退役 request；重复 getter 原样透传且不重复计数。自然 Bundle 卸载后，已经观察到的在途请求仍可报告其原始输入归属；这不证明任何主动卸载／Job completion fence 安全。诊断身份总数上限 256，失效弱请求回收，不强持有资源。

排序后的待验预测：目标由 async hash 返回；目标由 async String 返回；若目标资产请求出现但原生 GetResult 无关联，依据 request 实际类型和方法身份继续核对其完成路径。探针未发起加载、获取、卸载、实例重装配或候选发布。GetResult 无参数、Object 指针返回 ABI 的本地真实 MinHook 回归覆盖两种 async、空／重复／失败结果、请求地址复用、在途结果的源 Bundle 自然释放、String 层去重和关闭／预算透传。只有实机获得 SHA→Bundle→assetRequest→实际资源对象关联后，才记录资产返回基线通过；缓存隔离、候选代与迁移仍另行验收。

v30 普通 DLL 已于 2026-10-04 10:24 部署到实际 `D:/Hypergryph Launcher/games/Endfield Game/plugin/eff.dll`，SHA256 为 `D6BA7C43E37357D38AD14017F55BFC1FD5F5531B0441CDB55978F626FAC386EB`。部署前确认游戏停止，已备份 v29；普通代理、加载表与原生资源输入保持验证值，GFX 关闭。本地六个当前 native binding 的真实 MinHook、共享装配入口、实际 LZY 按键／形态／残影桥接及真实 v29 日志解析回归通过。新 async 观察尚无实机记录。

下一轮进入同一 LZY 场景，按 F10，平 A 触发身体残影，再切换一次角色展示页并正常退出。报告模型、残影和卡顿情况。核对 nativeAsyncHash／nativeAsyncString／nativeAssetResult 的安装标志及计数、实际 request 类型与 GetResult 方法身份、输入 SHA→Bundle→assetRequest→非空 Object 的匹配。未记录残影时只报告该边界未观察到；未建立资产返回或迁移证据时，不升级为完整资源重载通过。

## v30 原生异步资源返回实机结果（2026-10-04 10:26–10:29）

完整会话 `run-44748-01DD53A7DA6B5211` 中，10 个装配入口与 6 个 native loading binding 均安装；635 次采样 enter/return 配对，无原异常、MethodInfo／实例身份变化、读取错误和骨骼槽／bindpose 数不匹配。16 条目标输入均关联非空 Bundle 完成，pending=0，liveBundles=16，无冲突内容。另 16 条未关联的自然 Bundle 完成没有目标输入归属，不计为这 16 条提交失败。

async hash 共 10159 次调用，GetResult 共 10159 次调用；3 个关联请求均返回非空 UnityEngine.GameObject，pendingAssetRequests=0。分别为世界 PFB `input-033.vfs` 一次、展示 PFB `input-068.vfs` 两次；展示两次返回同一对象，不证明缓存失效。String 路线本轮没有自然调用。直接观察的是 PFB 根对象，未单独观察 Mesh／Material／Texture 的依赖返回，不将根加载基线扩展为全部资源验证。

身体残影 8 次 immutable snapshot、8 次 Release，最后 live=0。F10 配置后台准备 571.7971 ms、主线程提交 0.1855 ms、55 个绑定 Renderer、resourcesChanged=0。这些仍是配置重载的实测。用户报告已照做，没有单独报告本轮视觉／卡顿验收。

加载入口基线已有证据，下一步从当前作者文件及校验后的原始输入构建候选，核对外部 CAB／资源流依赖和旧代共存。无需再重复同一轮被动采样。GFX 继续关闭；显式刷新入口、代间缓存隔离、现存实例迁移和完整资源热重载标志仍为 false。证据见 `diagnostics/resource-directory-entry-20261004/v30-native-async-asset-observation/runtime-findings.json`。

## R1 离线候选准备结果（2026-10-04）

当前作者包 25 个资源文件和 68 个原始输入冻结；每条原始输入与当前物理 VFS 范围和 SHA 相符。现有 DLL 侧 Mesh／Renderer／Material／Texture 构建器和控制标识准备器重建两次，68 个最终输出逐字节一致，且全部逐字节复现当前 v26 资源输出。实际生产表仍为 60 行，没有发布新代。

新增只读 loading graph 审计和离线 generation namespace 准备器，独立于 production v3 替换表。68 个 CAB、3 个 resS、68 个 AssetBundle 名称隔离；611 处 external path、22 处 stream path 结构化重映射。AssetBundle 的 m_Name 和 m_AssetBundleName 都处理；未知字段或 opaque managed payload 中出现待隔离名称会拒绝构建。Numeric PPtr fileID／PathID 不变，metadata GUID／type 不变，原对象布局、骨骼表、形态数据、材质和像素保留。独立审计比较 1607559 次引用使用，内部引用和本地 stream 范围均无错误。

1163 个图外原版 CAB 和 1 个 Unity 内置资源保持原始身份；所有原版 CAB 均能在当前 locator index 定位，但尚未验证其原生加载和共享寿命。4 个使用 ReferencedObjectData 的脚本对象尚未完整解析，payload 逐字节保留；不将“未改写”记录为 managed-reference 格式支持通过。

准备过程中发现 EndfieldInput::Encode 的源检查固定为 64 MiB，会拒绝合法的 129 MiB 已构建输入。已修正为 source/output 使用同一个显式 maximum（上限 512 MiB），实际候选全部成功 encode/decode。审计／namespace 原生 smoke、实际候选交叉验证、四种故障候选拒绝和普通 DLL 编译通过。离线全量构建＋控制标识准备分别 29.54／29.29 秒，不能作为 F10 主线程成本或完整游戏重载延迟。

证据和候选：`diagnostics/resource-directory-entry-20261004/r1-candidate-graph/progress.json`、`baseline-01/manifest.json`、`namespace-03/verification.json`。生产 DLL／proxy／table SHA 保持 v30 已部署值；GFX 关闭，未修改 Blender。下一步为共享依赖持有、原生候选加载和旧代共存，再做 owner 迁移／回滚。generationCacheIsolationVerified、existingInstanceResourceMigrationVerified 和 completeResourceHotReloadImplemented 仍为 false。

## v31：R1a 身体 Mesh 原生候选加载（2026-10-04 11:35 部署）

普通 DLL 已部署到实际 plugin/eff.dll，SHA256 `848EE6D0B8889AFEEC39635FB22C6235559F48D52FBE9A99785BE47A3263678D`。普通代理／60 行加载表不变，GFX 关闭，回退保存 v30。新启用文件为 `plugin/resource-input-diagnostics/native-candidate-validation.tsv`，F10 在保留配置重载的同时准备一次独立 witness。

限定 dependency-free body LOD0：1379124 bytes，14693 vertices，2 shape channels。worker 锁文件并核对候选 SHA、旧输出 SHA、输入契约和全部 namespace 以外的 payload；Unity frame 调准确 managed LoadFromFileAsync → isDone → Bundle → LoadAssetAsync_Internal(String,Type) → isDone → GetResult。候选 Bundle／Mesh 必须与来源对象的 Unity GetInstanceID 不同。来源 Bundle 从 verified SHA 观察关联获得，来源 Mesh 从原 Renderer getter 获得。

分帧核对两份 Mesh 的顶点／法线／切线／索引／权重／bindpose 和完整形态帧，保持原场景 Renderer 不变，观察 10 秒共存后仅卸载自己的 Bundle，参数 unloadAllLoadedObjects=false。候选 Mesh witness 保留到进程结束，每进程去重一次。失败排空在途请求后清理自己的 Bundle；Bundle ownership 未证实时保留并拒绝卸载。这里不宣称全图缓存失效、原版共享依赖寿命或完整资源代回收通过。

真实资源 preparation 与 failure/lifetime fixture、配置重载、shape/capture bridge 和 native loading MinHook 回归通过，普通编译通过。实机结果尚未收到。操作：进入同一 LZY 世界场景约 10 秒后按一次 F10，继续停留至少 20 秒并走动／平 A／部件按键，然后正常退出。外观预计不变，后台新 Mesh 的 native arrays 和 identity 由日志验收。保持同一 source consumer，在 20 秒内不切换角色／展示页。

日志汇总：`python tools/diagnostics/analyze_native_candidate.py <entry.log> <new-output.json>`。nativeBodyWitnessVerified 在完整日志通过前仍为 false；sharedDependencyLifetimeVerified、materialTexturePrefabCandidateVerified、existingInstanceResourceMigrationVerified 和完整资源 F10 标志仍为 false。部署和范围说明见 `diagnostics/resource-directory-entry-20261004/v31-native-body-candidate/deployment.json`、`validation-report.md`。

### v31 首轮实机观察（2026-10-04）

run-34924-01DD53B223CA823F 已验证 F10 请求、独立原生 Bundle／Mesh 加载以及全部 Mesh arrays／shape frames 相等。身体 Mesh native ID 为 184794／376688，14693 vertices、84 bones、78762 indices、2 shapes。日志停在 Coexist，没有 CLEANUP／PASS 或候选错误；原生加载与数据相等记录为通过，完整 body witness 共存／卸载验收仍待补齐。30 次身体残影 snapshot 与 30 次 release 配对，末次 live=0，视觉验收未报告。run-19792 只有启动日志，不能判断为闪退。保持 v31 和当前资源，下一次 F10 后同场景至少 20 秒再正常退出；暂不发布生产候选。证据：`diagnostics/resource-directory-entry-20261004/v31-native-body-candidate/runtime-run-34924-01DD53B223CA823F/runtime-summary.json`。完整资源 F10／现存实例迁移标志仍为 false。

### v31 R1a 实机验收通过

实机 R1a 验收通过（2026-10-04 11:51）：run-12056-01DD53B375782AD1。F10 后独立加载 Bundle／Mesh，全量基础数组和两条形态帧 SHA 与首轮一致；source native ID 186638，candidate 379262。共存 1024 次采样，10 秒后仅卸载候选 Bundle（Unload(false)），保留候选 Mesh、原 Mesh 与原 Renderer；11:51:31.994 记录 CLEANUP 和 PASS，耗时 10062ms。该耗时包含刻意设置的 10 秒观察窗口，不是 F10 资源加载阻塞时间。身体残影 9 次 snapshot／9 次 release，最后 live=0；shape runtime gap 0。用户报告已完成测试，未提供明确视觉结论。R1a body witness 通过，完整资源 F10／共享依赖／材质贴图 PFB／现存实例迁移标志仍为 false。证据：runtime-run-12056-01DD53B375782AD1/runtime-summary.json。

### R1b 材质／贴图引用预检（2026-10-04）

8 个候选输入与已验收 source 仅 namespace 差异；5 个材质的逐属性贴图选择、11 个自定义贴图像素 SHA 与作者 INI／payload 匹配。toushi 使用的 cloth_01 Material whole object payload 与 pristine 原版完全一致，未声明 texture override 保留原版引用。已定位 8 个直接共享原版 Shader／Ramp／遮罩 CAB，传递依赖和原生持有／寿命尚未验证。新增只读 tools/diagnostics/inspect_material_candidates.cpp 与 verify_material_candidate_plan.py；坏像素、原版 Material payload、未声明贴图引用三种完整性拒绝通过。保持 v31 DLL 和生产资源，证据 `diagnostics/resource-directory-entry-20261004/r1b-material-dependency-plan/loading-expectations.json`。这不构成贴图原生/GPU验收、PFB 验收或完整 F10。

## v32：世界场景完整资源事务测试

新增 `eiem_resource_reload_plan.h` 和 `eiem_entry_full_resource_reload.h`。F10 后台从当前作者资源与 SHA 校验过的 68 个 pristine 输入重建 Mesh、Renderer/PFB、Material、Texture 和控制配置；每代独立 CAB、Bundle 和 resS 名称，候选输出完整写入后才发布 ready。当前两个选定根为世界 postmodel 和 UI model，原生候选加载闭包为 18 个输入。构建全图不代表已迁移全部 68 个输入对应的所有活实例。

Unity frame 通过游戏 ResourceManager 的 boxed StringPathHash/FAssetProxyHandle 持有原版世界/UI 根及其游戏管理的依赖，独立异步加载候选 Bundle 和 PFB。所有文件锁与 SHA 校验在 worker 完成，锁和异步请求保留到完成。候选 PFB 检查完整 Renderer 标识覆盖、顶点/shape 名称顺序、各 Mesh 自己的骨骼槽序与 bindpose 数、rootBone、材质和逐属性原版/自定义贴图选择。引用数组、Renderer、Mesh 和根使用 GC handle 持有。

现存世界 owner 经 EntityRenderHelper → Entity → ModelComponent 精确确认并检查 Renderer 所属根。候选 source PFB 的指针和 native ID 同时匹配才启用已登记 Unity clone icall 路由；原始 clone 调用保持一次。游戏自己执行 `RemoveMainPart → DiscardPooledObjects(path) → LoadMainModelSync → ChangeEntityFollowEffect`。移除后再清理该路径对象池，避免释放的旧实例又进入池。新根完整验证成功后才交换配置/控制/作者图和 generation。失败恢复旧 clone 路由，再沿游戏自身生命周期重建旧根，并核对原 Mesh native ID 和 Renderer 覆盖。

明确限制：当前活的无 Entity 展示模型和 `usePersistentPool=true` owner 会拒绝事务。这两类 owner 的独立释放/重建入口尚未实现。候选根存在 UI 资源不代表当前 UI 实例迁移通过。原生 job/streaming 消费者没有穷尽的 retirement fence，已曝光代、bundle 与原版资源 lease 保留到进程退出，每进程最多保留 3 次尝试。图外原版 Shader/Ramp 等使用游戏根 lease 维持依赖；其实际原生寿命仍要由实机核验。旧加载替换表保持原 namespace 和原 60 行，新实例通过精确 PFB clone route 接收新代。

本地检查：生产和青绿色候选各 68 输入、43 个 Mesh part/LOD、2401 个重建 Renderer 部件、5 个材质、11 个自定义贴图。独立图审计均为 1530761 个内部引用、76798 个边界引用，broken reference/stream 为零；4 个 opaque 脚本对象逐字节保留，未认定其格式解析支持。青绿色作者副本仅改变 body BaseMap 像素资源，全部 mip RGB=(16,220,160)、alpha 保留，其余 24 个作者文件逐字节相同，toushi 的原版材质/贴图选择保留。29 个所需 API 的已编译候选静态存在性通过；准确 managed 类型签名与 icall 在运行时再次验证。mipmapCount 在该玩家中声明于 Texture，已修正 owner。

事务 mock 回归覆盖正常装配顺序、原 clone 一次、创建新实例后校验失败的回滚、GC roots、在途请求排空、Unload(false)/Dispose 一次、Dispose 副作用后抛异常的隔离与禁止重复释放。真实作者配置重载、原生 body witness 和形态/残影桥接回归通过。这些不证明新事务的实机 ABI、迁移和 GPU 贴图成功。接口安装失败会拒绝完整重载，不退回配置重读伪装成成功。

部署工具：`tools/diagnostics/deploy_full_resource_reload.ps1`。停止游戏后备份 v31 DLL/配置，安装普通 v32 DLL、校验后的 baseline 和隔离 test author；代理、正式 Mod 和原加载表保持已验收内容，GFX 关闭、反虚化开启。部署结果及 DLL SHA 以 `diagnostics/resource-directory-entry-20261004/v32-full-resource-reload/deployment.json` 为准。Blender 未修改。

一次合并测试：

1. 重启游戏，进入同一 LZY 世界场景，等待模型正常显示；本轮保持世界场景。
2. 按一次 F10，等待最多约 60 秒。首次成功提交应使身体贴图变为明显青绿色，toushi 保持原版选择。本地全量重建约 30 秒，不代表最终优化后的时延，也不代表主线程卡顿。
3. 走动、平 A 触发身体残影，试 NUMPAD1–5 和已绑定方向键形态控制，观察是否撕裂、残影丢失、贴图变化或卡顿。
4. 再按一次 F10，等待完成，原 body 贴图应恢复；再走动/平 A，然后正常退出。
5. 一次报告青绿色是否出现、是否恢复、按键/形态/残影/toushi 是否正常，以及卡顿或退出情况。未变色时使用日志确定失败阶段，不判定为资源替换通过。

日志分析：`python tools/diagnostics/analyze_full_resource_reload.py <entry.log> <new-output.json>`。需获得 FULL-RELOAD-READY、PREPARED、两根原版 lease、18 个独立 Bundle、完整 prefab 校验、OWNERS、MIGRATED 和 COMMIT。第二次提交 testAuthorFirstCommit=0；确认连续两代、旧代共存、回滚缺口和分帧 mainMs，再结合用户的青绿色/恢复与残影视觉报告。完整资源支持、现存 UI/持久池迁移和原生代回收不因此本地测试自动升级为通过。

### v32 实机拒绝与 v32a 枚举修正（2026-10-04）

用户报告未热重载。`run-5556-01DD53F256B562C3` 明确加载了 v32 DLL，19:20:47.788 在安装阶段拒绝：`reload enum underlying width differs`。19:21:35.121 收到 F10（vk=121），随后因完整事务未启用拒绝 trigger；没有 PREPARED/BEGIN/COMMIT。该结果不能归因为按键拦截，也没有发生本轮新模型迁移。另一会话 run-9996 只有两条启动日志，不能据此认定闪退。

原 Enum helper 将所有枚举强制限定为 4 字节，本地 1 字节枚举 fixture 在原实现复现相同拒绝。v32a 从当前枚举的 value__ 字段读取准确底层类型，检查枚举 class_value_size 与 System.Byte/SByte/Int16/UInt16/Int32/UInt32/Int64/UInt64 的已知宽度一致，核对静态 literal 的类型身份与 flags，再以真实底层值地址调用 managed invoke。不猜测 enum 数值、不关闭类型验证。增加底层类型/宽度/literal bytes 日志；缺失 API 记录完整 owner/参数/返回/static 签名。

先失败后通过回归证据在 `v32a-enum-abi-fix/red-smoke.txt`、`green-smoke.txt`：8 种有符号/无符号底层类型、1/2/4/8 字节、高位值、invoke 参数指针和畸形枚举拒绝。原实例重建/回滚/GC roots/异步排空回归继续通过。29 个已编译 API 候选静态检查和普通 DLL 编译通过。准确实机枚举类型及 native ABI 仍待下次日志确认，本地 fixture 不替代实机验收。

v32a 于 19:30 部署到实际 `plugin/eff.dll`，SHA256 `1506BE7A53450B5EE8B6A393C0CD419F5BB6910D112E20612139EE80CC36D2DB`，部署前确认游戏停止；保留 v32 回退。baseline、隔离测试作者、正式 Mod、代理和原表保持部署前内容，GFX 关闭、反虚化开启。首次成功 F10 青绿色、第二次原图的测试步骤不变。热重载实机验收仍为 false。

### v32a 接口启用与准备中手动退出（2026-10-04 21:55）

run-27804-01DD5407BB643148 已确认 RootCategory 底层 System.Byte/1 字节、Main=0；EResourceRequestPriority 底层 System.UInt32/4 字节、Default=10。FULL-RELOAD-READY 出现，准确 managed 签名和两个 clone bindings 的安装检查通过。21:54:50.166 收到 F10，21:55:01.240 构建 8/68，21:55:05.620 构建 16/68；21:55:06.133 最后场景 release 后进程结束。用户明确确认手动退出。该轮没有 PREPARED/BEGIN/COMMIT，也没有新的拒绝，不能据此将后续原生加载或实例装配判为失败/成功。接口修正的实机启用已确认，完整迁移仍未验收。

使用已部署的隔离视觉作者和 pristine baseline 完整离线重建成功，耗时 42980.1943 ms；这是磁盘/CPU 构建时间，不包括 GameAssembly SHA 检查、文件锁、原生候选加载/校验和模型装配。无需本轮改 DLL。下次 F10 后保持同一世界场景约 90 秒，等候变色或日志事务拒绝；首次成功变色后第二次 F10 同样等候恢复。证据：`v32a-preparation-interrupted/findings.json`、`entry.log`、`rebuild.txt`、`rebuild-timing.json`。

### v32a 完整准备后的骨骼路径误拒绝与 v32b 修正（2026-10-04 22:19 部署）

用户再次报告没有反应。真实会话 `run-15744-01DD5408B7966C4F` 中，22:02:04.961 收到 F10，22:03:22.009 完成 68/68 输入后台重建，workerMs=77048.1156。随后获得原版 world/UI 根的游戏资源 lease，并加载全部 18 个不同 native ID 的候选 Bundle。22:03:22.974 在 PFB 预检拒绝 `reload per-Mesh bone slot differs: PartS_actor_lizhiyan_body_01_lod0_0/0`；22:03:22.989 完成仅自有 Bundle 的 Unload(false) 和 lease Dispose，旧代保留。没有 OWNERS/MIGRATED/COMMIT，画面未变化符合日志。另一会话 run-34084 只有启动两行，不作为闪退证据。

误拒绝来自作者 Mesh 的完整相对路径和运行时 `Transform.name` 末级名称直接比较。例如身体第 0 槽作者路径为 `Root/Bip001/Bip001_Pelvis/Bip001_L_Thigh`；toushi 第 0 槽为 `Root/Bip001/Bip001_Pelvis/Bip001_Spine/Bip001_Spine1/Bip001_Spine2/Bip001_Neck/Bip001_Head`。作者数据和序列化骨骼表无需修改。

v32b 持有所属 PFB 根 Transform、骨骼和完整父链，在该根内寻找 rootBone 与 Renderer 的共同骨架锚点，要求每个 Mesh 所有槽的完整路径按作者顺序匹配且锚点唯一。重复骨骼指针、rootBone 不在 palette、跨根骨架、错误分支、循环/超限仍拒绝。另修正现存 owner 根 Transform getter：GameObject 使用准确 `GameObject.get_transform`，Renderer 使用 `Component.get_transform`；不对错误接收类型调用方法。新增准确 API 的编译候选数为 30，运行时仍核对完整签名。

旧实现对完整路径的最小复现先失败，修复后事务回归通过；同末级名称的左右分支交换、重复指针、外部骨架、rootBone 缺失、父链循环均拒绝。原枚举 ABI、实例重建、失败回滚、GC roots、原 clone 一次、在途请求排空和 Dispose/Unload 一次回归保留通过。普通 DLL 编译通过。证据为 `v32b-bone-path-validation/red-smoke.txt`、`green-smoke.txt`、`api-static-presence.json`、`build.log`。

只读诊断 `inspect_reload_bone_paths.cpp` 从上一轮真实 generation `15744-30701875` 读取并校验 18 个输入的 ready SHA，从序列化 PPtr 和父链独立核对 67 个 Renderer、43 个 Mesh 标识、4561 个有序槽，完整作者路径和唯一共同骨架锚点均通过。旧末级名称比较会拒绝全部 4561 个槽。该审计不调用 Unity，不修改资源，也不等于修正版已通过原生装配。证据为 `actual-generation-bone-paths.json` 和 `.txt`。

普通 v32b DLL 已安装至实际 `D:/Hypergryph Launcher/games/Endfield Game/plugin/eff.dll`，SHA256 `6492819DB43E37A0B955AD2CD56DA2768BEEAFD4222C7FC215B09511F2AA5690`。部署前及原子替换前均确认游戏进程停止，v32a 已备份。正式作者文件、普通 proxy、60 行输入表、baseline、隔离视觉测试配置和反虚化设置的 hash 保持不变；GFX 关闭，Blender 未修改。部署脚本为 `deploy_full_resource_reload_bone_fix.ps1`，记录在 `v32b-bone-path-validation/deployment.json`。

下一次合并测试：重启进入同一 LZY 世界场景；按一次 F10 后保持同场景约 2 分钟，首次成功提交应使身体贴图明显青绿色。变色后走动、平 A、NUMPAD1–5/方向键检查部件、形态和身体残影，以及 toushi。再按一次 F10，同样等待约 2 分钟，成功应恢复原 body 贴图。若仍无变化，2 分钟后报告即可，不必连续触发。77 秒是上一轮后台构建耗时，不是主线程冻结或性能验收。修正版仍需获得两个 PFB 预检、OWNERS、MIGRATED、COMMIT 和视觉报告，才能认定当前世界资源热重载通过；当前 UI/持久池迁移和完整原生代回收仍未支持/验证。

### v32b 准备中退出与完整冷重建性能测量（2026-10-04 23:34）

真实会话 `run-32500-01DD5411CA66A07A` 明确加载 v32b。23:06:50.563 收到 F10，23:07:30.670 重建到 48/68，23:07:31.682 最后场景 release 后进程结束。用户确认手动退出。该轮没有 PREPARED/BEGIN/PFB/COMMIT，也没有新的预检拒绝；不能报告 v32b 的骨骼修正已通过或失败。`run-3436` 只有启动两行，不作为闪退证据。

用户要求说明刷新为何需要一分钟，并质疑 77 秒是否均为刷新成本。旧 `workerMs=77048.1156` 从后台 `Prepare()` 开始到发布候选 ready 计量，是墙钟时间，包含作者读取/校验、GameAssembly hash、完整图重建、namespace、编码/写盘、候选文件锁/hash 与作者变更复核。它不含后续 Unity lease/Bundle/PFB/owner 装配，也不等于 worker CPU 时间；旧版缺少线程 CPU 和内部阶段统计，不能将其中每一秒都归为有效 CPU 工作。该轮资源阶段 68/68 在 22:02:57.740，ready 在 22:03:22.009；这约 24 秒包含元数据/namespace/发布和锁/变更复核，不是等待用户或原生 witness 的 10 秒窗口。

使用实际已部署的青绿色作者与 pristine baseline 进行独立完整冷重建。首次阶段测量 52403.4 ms；细分版本测得 55996.5 ms，userCpuMs=49171.9、kernelCpuMs=4625，峰值工作集 1179242496 bytes。细分墙钟累计：Renderer 22946.7 ms、namespace 15092.2 ms、重复 Mesh 构建 3171.9 ms、初次 Mesh 构建 3392.32 ms、Texture 2754.99 ms、marker 1121.1 ms、最终 encode 203.695 ms、verify decode 288.389 ms。source/resource/metadata/publish 的阶段日志保留。这些是游戏停止时的磁盘/CPU 准备测量，不是实机端到端或主线程阻塞测量。

v32c 保持完整 68 输入/43 Mesh 标识/2401 Renderer 部件/5 Material/11 Texture 的每次冷重建，不增加未改资源快捷路径。修正三处重复工作：首次已构建 Mesh 输出直接供下一步使用，不再构建第二遍；同一不可变作者 Package 内按 mesh.resource 和 root bindpose 槽复用 local AABB（2401 个部件共 17 次计算、2384 次复用），各 Renderer 的 bones/root/层级仍分别解析；CAB 与当前 `main/<24hex>.ab` 旧身份按固定前缀扫描并精确查表，替代每个旧名称都扫描整份 bytes。仍拒绝 schema/metadata patch 范围外出现的旧身份，通用 Bundle 名称保持原逐名检查，patch 重叠/范围检查保留。

逐项测量及完整输出核对：namespace 修改后 52278.3 ms；再加 bounds 复用后 41198.0 ms。最终一次冷重建与两个并行编译竞争时 47368.2 ms，该数值保留但不用于独占性能结论；全部编译结束后再独立测得 38839.1 ms，userCpuMs=34531.2、kernelCpuMs=2703.12，峰值工作集 1113206784 bytes。最终 Renderer 11616.5 ms、namespace 12616.7 ms；bounds 首算合计 63.0058 ms。完整冷重建仍需几十秒，不将此记录为日常热重载性能达标。

以上每次使用相同 nonce、作者及 baseline，ready 的 generationKey/author/contract 和 68 个输出文件 SHA/大小完全相同，共 406223808 bytes。独立脚本重新读取全部文件校验 ready SHA，并比较全部输出，而非仅比较 Renderer 数量或快照统计。相关证据在 `v32c-reload-preparation/profile-preparation.txt`、`namespace-comparison.json`、`bounds-comparison.json`、`green-comparison.json` 和 `isolated-comparison.json`。隐藏第二个 CAB/Bundle 身份、通用名称、patch overlap、shared builtin 回归通过；完整骨骼路径/枚举/原生事务/回滚 mock 回归通过，普通 DLL 编译通过。

新版实机日志 `FULL-RELOAD-PROGRESS` 区分 author/source/resource/metadata/publish/file-validation 阶段和 completed/total；`FULL-RELOAD-COST` 给出内部操作累计成本（renderer-bounds 是 renderer 的子项，不能重复加总）；`FULL-RELOAD-PREPARED` 保持原 workerMs 并补充 workerUserCpuMs/workerKernelCpuMs/cpuMeasured。线程 CPU 从同一个后台线程前后 GetThreadTimes 差值读取。日志分析器新增 preparationProgress/workerCosts，不把这些记录当作 native COMMIT。

普通 v32c DLL 已在停止游戏并再次核对进程后原子安装到实际 `D:/Hypergryph Launcher/games/Endfield Game/plugin/eff.dll`，SHA256 `CF2961B2A95F5C03830B1AC8EE05BE27EB17499C44C3C881D279144E61D8AF8B`，v32b 回退保留。正式 Mod、普通 proxy、60 行表、baseline/视觉配置、反虚化、原生 API 和装配入口保持不变，GFX 关闭，Blender 未改。部署记录为 `v32c-reload-preparation/deployment.json`。下一轮用户重启进入同一世界场景并按 F10 后，以日志观察到 PREPARED、原生明确拒绝或 COMMIT 为界报告；首次成功仍应为青绿色，第二次成功恢复。实机准备性能、模型迁移和完整资源热重载仍未验收。

### v35b 管理器字段访问修正（2026-10-05）

用户报告游戏画面没有变化。真实会话 PID 38328 在 12:46:41 收到 F10，新资源原生加载耗时 1172 ms，找到 1 个世界 owner，随后拒绝 `reload field type/static contract differs: modelManager`。该轮未调用模型释放/重建，没有 MIGRATED/COMMIT；新贴图已导出并加载，问题发生在提交前。

原代码将 GameInstance 的 modelManager 和 effectManager 都固定当作实例字段。完整 Frame fixture 改为静态字段后复现相同拒绝。v35b 在安装时按 IL2CPP 元数据绑定两个字段，保留准确声明类型校验，并按真实 static flag 选择 `il2cpp_field_static_get_value` 或 `il2cpp_field_get_value`。新日志 FULL-RELOAD-FIELD 记录真实 type/flags/getter；其他生命周期字段继续要求实例字段。旧日志没有记录实际 flags，实机静态性及成功提交仍须下一轮确认。

静态/实例读取、静态读取不依赖 GameInstance、错误类型拒绝、完整世界/UI Frame 连续重载及回滚测试通过。实际生产目录离线检查通过 60 个启动输入、68 个候选、18 个闭包输入；此检查不代表 Unity 迁移通过。仅使用 `tools/diagnostics/build_resource_directory_entry.cmd` 编译并部署现行资源入口。

停止游戏后已部署至实际 plugin/eff.dll，build=`resource-directory-entry-v35b-f10-manager-field-contract`，SHA256=`F0EC8EBDA97DEB1E8679954A847566A24650C2A3F0762E47F8FF308B5CAD33B6`。证据及回退路径见 `diagnostics/f10-manager-field-fix-20261005/deployment.json`。重启后再修改 body.png、Blender 仅导出贴图、按 F10，以 MIGRATED/COMMIT 和视觉贴图变化验收；不再使用旧版自动青绿色轮换步骤。

### v35b T pose 与 v35c 原生消费者链补齐（2026-10-05）

2026-10-05 17:29 的无改动 F10 实机检查（pid 35748）完成了 v35c 的首次真实提交。17:29:32.068 收到请求，保留两个原版 PFB lease，18 个候选 Bundle 均取得独立 native ID；世界/UI 候选 PFB 分别校验 107/25 个 Renderer、55/12 个作者部件。当前世界 owner 为 1、展示 owner 为 0。游戏 `RemoveMainPart` 后进入 `PrefabInstantiateProxyManager.OnProxyDispose`，`LoadMainModelSync` 内经 `Internal_InstantiateAsyncImmediatelyWithParent` 路由源 PFB ID 176354 到候选 424730，`OnModelLoaded` 返回；动画 `ChangeAnimationConfig(data, true)` 返回 1，武器引用刷新。旧世界根 ID -73290 变为 -309960，新根的 Mesh/Texture 对象身份与候选 PFB 一致，17:29:33.137 提交 generation 2；请求到提交 1069.9 ms，提交帧主线程约 58.9 ms。没有 REJECT、ROLLBACK 或探针原函数异常。分析产物为 `diagnostics/f10-native-lifecycle-audit-20261005/unchanged-f10-pid35748-resource.json` 与 `unchanged-f10-pid35748-observation.json`。

本轮 Mod 内容未改，画面相同是预期，用户的动作/外观反馈另记；该日志只证明同内容的新代资源加载和当前世界实例迁移。UI PFB 已加载而 F10 后 UI 实例未创建；展示、闲置池、编辑后像素/几何、Renderer 增减、反向恢复及多代退休未验收。`BundleResourceManager.UnloadUnusedAssets` 返回协程对象的调用在 F10 前出现两次，不能证明协程执行或 GPU 对象销毁。代理 `OnProxyDispose` 在 F10 中触发，未观察到 `AssetBundle.Unload`；事件每边界 128 次采样，上限后的缺失不作未执行结论。

PID 5072 真实启动确认两个管理器字段均为 static，flags=0x16，v35b 字段读取已接通。13:02:50 收到 F10，1156 ms 完成原生候选加载，两根 PFB 与一个世界 owner 通过；随后模型重新加载未经过所拦截的两个 Clone 入口，拒绝提交并回滚。用户观察到 T pose、贴图未变。旧 `ownersRestored=1` 仅验证 Mesh，不能证明动画恢复；原 mock factory 强制使用 Clone，也掩盖了真实创建入口的覆盖缺口。

静态核对游戏实际链：ModelManager.Alloc → ResourceManager.InstantiatePrefab → BundleResourceManager.Instantiate → PrefabInstantiateProxy → Unity 的位置/父节点及异步创建分支。另在 AbilitySystem._ApplyModeChange 中，ChangeMainModelSync 后还调用 ComplexAnimatorComponent.ChangeAnimationConfig(data, true)，并刷新武器挂点及音效 Transform 引用。原 F10 只组合模型释放/加载及 follow effect，没有完成这条消费者链。

v35c 补上 runtime resolve 的位置/父节点 Injected、Async/Immediate Instantiate 入口，仍由游戏原函数完成创建；参数和非目标对象保持原样。世界 owner 捕获原动画配置，使用原生 ChangeAnimationConfig 的动画恢复功能，并在成功及回滚时更新武器和音效引用。提交验证核对 Animator 属于当前模型，并比较实例与候选的 Mesh/自定义 Texture 对象身份，避免同名同尺寸旧资源误判成功。没有修改 Animator 内存、原生 job、骨骼指针或 controller 私有字段。

六种工厂参数转发、原始释放/加载造成旧动画引用的复现、原生消费者恢复、失败回滚动画恢复、相同 Mesh 下旧 Texture 身份拒绝、世界/UI 连续 Frame 重载回归通过。49 个已编译 API 候选存在，准确运行时签名与绑定仍需新进程验证。v35c 已停止游戏后部署至实际 plugin/eff.dll，SHA256=`91DFC45562D6A3E2B7B51011D3A265999331470D084CE43F438776326B2542C4`。证据在 `diagnostics/f10-native-lifecycle-audit-20261005/`。完整热重载仍未实机验收，不能把这些本地回归记作通过。
