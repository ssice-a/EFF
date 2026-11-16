# 统一原生 AnimationClip 验证

2026-11-13 接受的新方向：骨骼、普通 Transform、形态键、材质数值动画共用原生 AnimationClip 与原生求值链路。EFF 继续拥有 INI 装配、逻辑目标绑定、播放控制、Ins、实例生命周期及热重载。Blender 保留统一动画组和可视关键帧编辑，不按轨道类型建立独立播放器。

当前结论（04:30:39，PID 24324）：**最小混合 Clip 原生求值实机 PASS**。同一 Clip 的 Transform、形态键、材质浮点/颜色/UV 在五个时间点全部匹配，第二实例、共享材质及清理检查通过。此结论针对隔离测试对象；完整 Mod 装配、播放控制、真实角色写入顺序与 F10 尚未接通/验收。用户退出测试会话后已恢复固定游戏目录的生产 DLL，摘要与验证前一致，不含动画探针。

这是对旧的 [动画装配方案](animation-assembly-design.md) 中“EFF 逐帧写骨骼，形态键与材质分别写入”部分的调整。现有生产播放器暂时保留，不能在新链路未通过实机验证前切换默认导出或删除旧包支持。

## 管理边界

EFF 管理 Mod 的资源装配、相对路径绑定、实例所有权、统一播放控制、INI/按键/Ins 和 F10 事务。Unity 原生动画系统负责同一个 Clip 内各类曲线的求值。使用游戏进程内的 Unity 求值能力，不等于把 Mod 动画注册进游戏角色的技能配置、资源状态机或共享 AnimatorController。

原生求值通过验证后，三类轨道使用同一执行链路；不让用户为同一个动画选择两套播放器。兼容迁移期间保留旧包执行器，不能把未经验证的新路径直接作为默认。与角色 Animator、物理和 PropertyBlock 的更新顺序及属性所有权仍须独立验收。

## 最小验证

`tools/AnimeStudio/AnimeStudio.GUI/EiemAnimationClipWriter.cs` 用当前游戏的 TypeTree 创建全新的非 Legacy Clip，不修改或复制游戏来源动画的轨道。一个共享采样表包含：

- `Pivot` 的位置和四元数旋转。
- `Surface` 的 `Pulse` 形态键。
- 同一 Renderer 的材质浮点、RGBA 颜色和 `_MainTex_ST` 向量。

资源只有 AssetBundle 容器与作者 AnimationClip 两个对象，无外部依赖，不复制来源特效 Prefab、贴图、材质或 Mesh。路径与材质属性采用实际原生绑定规则；拒绝路径/属性 CRC 冲突、重复目标、非有限数、无效四元数与不同样本时间轴。

运行真实来源回归：

```powershell
dotnet run --project tools/AnimeStudio/AnimeStudio.EffectsTests -c Release -p:TargetFrameworks=net9.0-windows -- --native-animation-clip 'E:\EIEM_Workspace\assets\beyond\dynamicassets\gameplay\effects\prefabs\p_fxbat_liino_relax_react_03.efffx' 'E:\vscode\EIEM\diagnostics\native-animation-20261113'
```

离线验证包括：原生 TypeTree 往返、独立 `AnimeStudio.AnimationClip` 读取器完整消费对象、12 个绑定与 17 个标量通道、样本修改、确定性输出及无来源依赖。每次重新生成样本都会将其 `receipt.json` 初始化为 `engineEvaluated=false`，必须重新实测才能更新结论。本轮生成的确切样本已完成下述实测，回执已记录 `engineEvaluated=true` 并链接实际日志，不能把这个结果继承给后续修改的样本。

## 游戏内求值探针

```powershell
cmd /d /c build.bat animation-probe
pwsh -NoProfile -File tools/diagnostics/deploy_native_animation_probe.ps1
```

这是显式启用的验证构建，输出在独立的 `bin/resource-directory-entry-format3-animation-probe`，不打开 RenderDoc。部署只在游戏正常退出后替换固定游戏目录下的 `plugin/eff.dll`，保存原生产 DLL 和摘要；原 Mods、INI、Blender 工程保持原样。

游戏主线程与现有原生 Bundle 加载器就绪后，探针先检查全部所需方法的精确元数据签名、必要的原生 icall 回退和数值类型大小，再通过同一 archive-stream 加载器打开测试 Bundle。属性块回退先通过空块、浮点、颜色写入/读回和 Clear 的原生自检，失败时不加载 Clip。对象创建、Clip 求值和读回全部使用原生接口，不依赖游戏 Lua 包装器。探针创建两个不可见的临时测试实例，使用相同 Mesh/Material，只对 A 调用原生 `AnimationClip.SampleAnimation`，核验五个时间点的 Transform、形态键和 Renderer 材质结果，以及 B 和共享 Material 不受影响。不要求真实 Mod 的 Mesh 命中或 Renderer 注册。测试代码不使用 EFF 插值器或逐轨道写入方法。读材质时遵循 slot/global PropertyBlock 的优先关系，避免把 sharedMaterial 或被遮蔽的 global block 当成最终渲染值。

日志位于 `plugin/eff_log.txt`，标记为 `[NATIVE-ANIMATION-PROBE]`。只有出现最终 `result=PASS` 才能认定这一调用路径已验证。缺少 UI/Default Shader、元数据方法或原生动画支持均必须记录 FAIL，不能当作通过。测试资源限制为 1 MiB；成功或失败后先同步销毁探针拥有的临时对象，再卸载测试 Bundle，清理失败不能报告测试通过。

退出游戏后恢复生产 DLL：

```powershell
pwsh -NoProfile -File tools/diagnostics/deploy_native_animation_probe.ps1 -Restore
```

`verify_production_dll.py` 拒绝包含探针标记的 DLL，因此验证构建不能混入正式发布。

## 2026-11-13 首次实机失败与恢复

03:31 启动的 PID 38328 使用诊断 DLL `e2bffa9358f130009938475ed86b5f3ec2c14918cc2a1613a349e9d36c87cf08`。03:33:02 探针报 `attempt to call a nil value (field 'LoadFromMemory')`：游戏 Lua 环境没有暴露该静态接口，失败发生在加载 Bundle、创建临时测试对象及执行 Clip 之前。不能据此宣称 Clip 已反序列化或执行，也不能将闪退认定为 Clip 样本问题。

同一会话 03:33:08 被 Windows 记录为 `0xe06d7363`，异常线程 43344。394,020 字节的 dump 有 247 个线程上下文，但内存范围与线程堆栈均为 0，不能恢复抛异常调用链。dump 系统计数记录提交量 39.644 GiB / 上限 40.418 GiB，历史峰值 40.413 GiB；游戏私有提交量 6.852 GiB。同时进行的诊断 DLL 重编译报 C1060（编译器堆空间不足）。这证明存在提交内存压力，不能单靠这些数据确定未处理异常的类型或归属。实机验证期间停止大型编译，先构建后测试。

崩溃日志、Player.log、Windows 事件、原诊断 DLL/PDB、dump 和计数提取结果保存在忽略的 `diagnostics/native-animation-20261113/crash-0333/`。正式 DLL 已恢复到固定游戏插件目录，摘要为 `fd1bd32e3825dfeb3f03df3fb7e72128fbd74069e6b06f3e7a7b17ea250785d4`，通过生产校验；部署回执状态为 `restored`。

用户再次启动的 PID 44552 使用上述正式 DLL，截至 03:45 仍在运行，无新探针记录和新 Windows 崩溃记录。本轮仍观察到 175 次 `DEMAND-TARGET-DISCOVERY-GAP`（含读取预算与 CAB 归属检查失败），不能把本轮稳定观察等同于资源链路完全正常。此日志快照保存在 `diagnostics/native-animation-20261113/production-44552/`。

用户正常退出 PID 44552 后，探针已完全移除 Lua 实现，改用现有 `NativeBundle::LoadBytes` 原生 archive-stream 加载器与精确元数据签名。03:51 的修订构建通过，DLL 为 3,412,992 字节，SHA256 `fa3d3279753297c1e67a29fd5dbd4595f2a8ea4f97e0b0a658044c88100e0016`。用户 Mods 未修改；首次离线 Clip 样本及摘要不变。**此时尚未获得引擎求值结果**；`engineEvaluated=false` 不变。诊断构建仍需游戏验证后恢复生产 DLL，不能作为正式发布产物。

03:52:49 已将该修订部署到固定游戏目录，安装摘要一致，正式 DLL 有独立备份；四仓库 fetch 后均为 main 且与 origin/main 一致。构建、生产发布拒绝检查与测试资源摘要核验通过；安装回执副本为 `diagnostics/native-animation-20261113/deployment-native-0352.json`。等待新会话的探针结果，期间不运行大型编译。

03:54 启动的 PID 27672 未崩溃，探针在 `MaterialPropertyBlock.get_isEmpty` 元数据检查处返回 FAIL，尚未加载 Bundle 或创建测试对象。现场保存在 `diagnostics/native-animation-20261113/native-27672/`。静态核对实际游戏 metadata 与 CodeGenModule 后发现，`isEmpty`、`HasProperty`、`GetColor` 的 C# 包装器缺失，但 UnityPlayer 保留 `get_isEmpty`、`HasPropertyImpl`、`GetColorImpl_Injected` 原生入口。现有生产材质播放器已使用第一种回退，探针不应重新强制要求不存在的包装器。

修订探针为这三项提供元数据优先、原生 icall 回退的适配；预检会一次记录全部缺项。新增只读的 `tools/diagnostics/audit_native_animation_probe_api.py` 检查实际游戏文件：仅要求 C# 包装器时准确失败三项，启用源码声明的回退后 42 项接口候选全部存在。该静态结果明确不验证注册后的 ABI，仍须通过上述原生属性块自检与 Clip 实机测试。可单独运行：

```powershell
python tools/diagnostics/audit_native_animation_probe_api.py 'D:\Hypergryph Launcher\games\Endfield Game' diagnostics/native-animation-20261113/probe-api-audit.json
```

04:04:42 已部署属性块回退修订：3,419,136 字节，SHA256 `13b09e7ebf2b6373dd4b1f165f007fb9ab402b335a6038babe3b3a7aedf15680`，目标 DLL 摘要相同。C++ 构建、真实 metadata 负例/回退候选检查、生产包拒绝检查与样本摘要核验通过。安装回执为 `diagnostics/native-animation-20261113/deployment-block-fallback.json`；仍等待原生属性块与 Clip 的实际执行结果。

## 2026-11-13 Bundle 兼容字段修正

PID 6740 在 04:06:14 实际通过属性块原生回退自检（空块、HasProperty、浮点、颜色、Clear）。接着 Bundle 返回空，Player.log 报 `not compatible with this newer version of the Unity runtime`。尚未调用 `LoadAsset_Internal` 或 `SampleAnimation`；不能据此判断原生 Clip 是否可执行。

独立 TypeTree 解码确认测试包的 `AssetBundle.m_RuntimeCompatibility=0`，游戏来源包为 `1`，SerializedFile 的 Unity 版本都为 `2021.3.34f5`。只读分析本机 UnityPlayer 进一步确认：序列化 `m_RuntimeCompatibility` 的成员偏移为 `0x38`；兼容检查在该成员小于 1 时进入上述报错分支（本机二进制 RVA `0x1b8a19`，仅作此次证据）。这次错误来自作者 Bundle 零初始化遗漏，而非已经证明的 Clip 求值不兼容。

新增共用的 `EiemEffectPrefabWriter.CreateBundle` 初始化合法兼容值，SerializedFile 构建阶段拒绝兼容值为 0 的作者 Bundle。“从零新建特效”也经过同一初始化，避免该入口产生同类坏包。回归覆盖新建特效完整往返、混合 Clip 序列化、坏兼容值拒绝；另有 48 项既有特效断言通过。

修订测试包与失败版本长度相同（189,796 字节），逐字节比较只改变偏移 189,768 的兼容字段 `0→1`；Clip 对象、全部轨道与样本未改动。修订包 SHA256 为 `db3720d044cf8c1d7dcc3a12ef3af78ec41a63908f8f6af18a6d2fac8eea02c7`。现场日志、独立比较和引擎错误调用点保存在 `diagnostics/native-animation-20261113/native-6740/`。

04:20:23 已部署修订测试包，仍使用上述 `13b09e7e…` 探针 DLL，没有重编译 C++；安装摘要一致，回执为 `diagnostics/native-animation-20261113/deployment-bundle-compatibility.json`。修复后的 AnimeStudio 已部署到固定本机目录，回执为 `bin/local-deployments/20261113-041930-809/AnimeStudio.json`。等待新会话的实际 Clip 采样结果，`engineEvaluated=false` 保持。

PID 7916 在 04:21:42 已成功加载修订 Bundle 和 Clip，读回 `duration=1 legacy=0`。第一次采样虽正常返回，但 Player.log 明确报 `Non-Legacy animations cannot be sampled outside the Editor without an Animator`，材质浮点仍为初始值 2（期望 0.1）。这次实际未完成求值，不能将“方法返回”当成播放成功。现场保存在 `diagnostics/native-animation-20261113/native-7916/`。

探针源码已补齐临时根节点的 Animator，保持独立、无 Controller、Renderer 禁用，并激活测试层级以完成组件生命周期。数值不匹配改为收集所有时间点和通道后统一 FAIL，避免每次只暴露第一个失败项。实际游戏 metadata 已确认 Animator 类型存在，新 Renderer.enabled 包装器在 43 项候选检查中通过。

确认游戏进程退出后进行了 C++ 构建，生成 3,422,720 字节的探针 DLL，SHA256 `fde76267a56d461a0d2649e1d725c6c9301728f74edccd13e4882517e770f2b3`，构建与原生接收检查通过。但 PID 39520 于 04:26:23 再次启动，部署脚本按运行进程检查中止覆盖。**Animator 修订已构建、尚未部署**，当前游戏目录 DLL 仍为 `13b09e7e…`，需当前游戏退出后安装。构建日志为 `diagnostics/native-animation-20261113/native-probe-animator-build.log`。

用户确认退出且进程检查通过后，04:28:59 已部署上述 Animator 修订到固定游戏目录，DLL 和未改变的测试包均通过摘要核对。四仓库 fetch 后均为 main，且与 origin/main 一致。安装回执为 `diagnostics/native-animation-20261113/deployment-animator.json`；原生产 DLL 备份未变。当前状态为**已部署、等待新会话采样结果**，不是原生动画验证通过。

## 2026-11-13 混合 Clip 实机通过

PID 24324 于 04:30:39 完成全部五个采样点（0、0.25、0.5、0.75、1 秒），每次 `cumulativeMismatches=0`，最终日志：

```text
[NATIVE-ANIMATION-PROBE] phase=cleanup result=PASS
[NATIVE-ANIMATION-PROBE] result=PASS nativeClip=1 samples=5 transform=1 shape=1 materialFloat=1 materialColor=1 uv=1 instanceIsolation=1
```

这次游戏实际完成了 Bundle/Clip 反序列化、对 A 的原生 SampleAnimation，以及位置、四元数旋转、Pulse 形态键、材质 `_Stencil`、RGBA `_Color`、UV `_MainTex_ST` 读回，容差为 0.003。B 共享 Mesh/初始 Material，但其已覆盖的属性保持初值；共享 Material 未被改动。两个测试根有独立 Animator，均未赋予 Controller，渲染已禁用。销毁所有测试消费者和作者 Mesh/Material 后再卸载 Bundle，清理成功。Player.log 不再出现采样错误或 Bundle 兼容错误。

证据为 `diagnostics/native-animation-20261113/native-24324/engine-validation.json`，同目录保留本轮 `eff_log.txt`、`Player.log`、安装回执及确切测试包；证据文件记录 SHA256。测试包、Clip 字节和探针 DLL 摘要与部署回执一致。

本轮没有验证真实角色的 Animator/物理写入顺序、渲染画面、连续播放调度、暂停/停止恢复、按键/Ins、LOD 或 F10；不能将最小求值 PASS 写成“整个动画功能已完成”。后续首条生产链路应以一个作者武器实例为单位接通资源绑定和统一控制，复用现有实例/代际事务。

用户正常退出后已运行 `deploy_native_animation_probe.ps1 -Restore`，恢复固定路径 `D:\Hypergryph Launcher\games\Endfield Game\plugin\eff.dll`；3,378,176 字节，SHA256 `fd1bd32e3825dfeb3f03df3fb7e72128fbd74069e6b06f3e7a7b17ea250785d4`，生产校验通过且不含探针。恢复回执为 `diagnostics/native-animation-20261113/production-restored-after-pass.json`。本轮最小验证与生产恢复均已完成，未提交、推送或发布。

## 进入生产链路前的门槛

下一轮按用户提供的 ZFY 形态键 Mod 开展真实 Renderer 验证，详见 [ZFY 实验](zfy-native-animation-experiment.md)。这是可复用既有控制与采样的较小入口，不等同于静态武器作者骨架链路已完成。

1. 最小混合 Clip 的同片段求值、反序列化和已覆盖双实例隔离已通过。接下来验证真实武器的作者骨架、蒙皮 Renderer 和来源绑定；当前样本通过不覆盖任意来源 Clip 或新增轨道组合。
2. 用同一原生执行器接通 EFF 共享时钟的播放、暂停、停止恢复、速度和循环。`SampleAnimation` 是验证入口，正式播放方式仍需比较原生 Playables 的绑定和生命周期；不另建特效专用动画时钟。
3. 装配后按实例解析相对路径、Renderer、形态键及材质槽。一个根下的 Clip 统一求值；跨独立根节点通过统一播放控制建立相应输出，禁止按场景同名节点猜绑定。
4. 明确与游戏 Animator、物理、MaterialPropertyBlock 的属性所有权及更新时序。孤立对象测试通过不代表实际角色的写入冲突已经解决。
5. 补齐 Blender Action 的普通 Transform、Shape、Material 投影与原生 Clip 编辑往返。支持的轨道可编辑；未知来源绑定必须保留，不能用新 Clip 重建来静默删除原片段内容。
6. F10 按资源代绑定新对象，损坏 Clip 保留旧代。实测世界/UI 多实例、LOD、隐藏和 owner 退休。

上述通过后才迁移默认导出与现有执行器。INI 的变量和即时按键控制仍属于控制层；原生 Clip 承载时间轴曲线，不承载 INI 表达式程序或任意 UI 逻辑。
