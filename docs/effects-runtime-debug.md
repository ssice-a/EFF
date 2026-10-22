# 历史开发调试通道

当前作者流程已移除 PID、实例选择和试播管道。特效包含在普通 eff.dll，由 Mod 声明和游戏 F9 驱动；effects-debug 仅为内部诊断开关。本页保留历史实现记录，操作以[空物体编辑说明](../tools/Blender/docs/effects-editor.md)为准。

2026-10-19 开发候选。默认生产 DLL 不包含此通道；需 `build.bat v35 effects-debug`，输出到独立的 `bin/resource-directory-entry-v35-effects-debug`。此文不表示安装或游戏验收完成。

同日后续：原生准备/实例生命周期已复用于正式 Mod 挂载声明，普通构建包含挂载执行器但不启动本地管道。Blender 已加入试播与 `RELOAD_ATTACHMENT` 独立更新入口。以下为最初调试阶段记录；最新作者流程、支持范围和验证见[动态材质与特效测试](dynamic-material-effects-testing.md)。

## 当前闭环

AnimeStudio 的 `--build-eff-effects` 另外生成 `runtime.bin` 与 `runtime/bundles/*.ab`。包括未修改来源的首次播放调试包，不要求为了播放伪造一个参数改动。离线完成 SerializedFile/UnityFS 重建、CAB/stream/external 路径隔离、逐成员重新读取，根 GameObject 在包内设为 inactive，避免尚未放置就触发 OnEnable/playOnAwake。原始来源和正式 Mod 不改变。

运行包引用的未改终端依赖仍属于游戏。DLL 先取得原特效路径的 ResourceManager 句柄，再异步加载隔离的 Bundle 和精确 Prefab。来源句柄不是对作者全局索引的查询，也不会调用离线编译器。游戏更新后来源资源能否兼容仍需实际校验。

DLL 在 Unity 主线程执行：

1. PREPARE 保留正在播放的旧实例，只准备新代际；核对原生 Bundle 不与已加载容器或同组输入混同，根名/根 inactive/实际粒子数量符合离线描述。
2. PLAY 按现有模型生命周期登记选择精确 GameObject 实例 ID，再在该实例的完整 Transform 树按精确地址取挂点。支持已登记的世界/展示部件与 UIModelLoader 实例，范围不受单一 Renderer 的 m_Bones 限制。不扫描场景 Renderer、不按同名骨骼猜测。
3. 通过已有原生克隆 trampoline 创建 inactive 子实例，使用局部坐标挂到父节点；调用确切的 `EffectManager.AddUnmanagedEffect(GameObject) -> void`，激活实例。只显式播放处于激活节点且启用 playOnAwake 的粒子，保留关闭的子发射器意图。
4. 新实例成功后才退休旧实例。缺失挂点、异步加载失败、原生内容不符、登记失败均不撤掉旧的成功实例。
5. STOP/换代/owner 释放时：停止和清理粒子、设 inactive、`RemoveUnmanagedEffect(GameObject)`、脱离原挂点、公共 `Object.Destroy`。模型进入游戏池前执行同样清理。持有资源到延迟 Destroy 后 native 对象确实不再存活，再调用 Bundle.Unload(false) 和释放来源句柄。
6. 未完成的异步请求即使取消也保留到完成。无法证明容器所有权或清理失败时保留隔离代际，并停止反复清理。Unload(false) 不是任意自定义脚本、音频或 GPU job 已退出的证明。

相同完整原生输入可复用已经准备的代际，不二次加载同一 CAB。准备期间仍占用相同命名空间的取消请求不会再次提交；先等待该请求完成。当前只保留最新 PREPARE/当前播放及仍在退出的资源，不以每帧读文件或全游戏目录遍历发现变化。空闲通道只读一个待命令标记与主线程状态，不调用 Unity 查询。

## 本地开发协议

管道为 `\\.\pipe\EFF-Effects-<游戏PID>`，仅接受本机客户端。管道线程做文件读取、路径边界和 SHA256 核验，接收命令；不读取 GC handle，不调用 IL2CPP。队列处理及资源状态只由 Unity 主线程持有。STATUS/RESULT 返回线程安全的纯字符串快照。

每个连接发送一个 UTF-8 message，字段以 TAB 分隔。成功排队返回 `QUEUED<TAB>ticket`，使用 RESULT 查询完成结果。客户端读完整响应后发一个 ACK，服务端避免阻塞 FlushFileBuffers；挂起客户端 I/O 有超时。命令：

| 命令 | 参数 | 用途 |
| --- | --- | --- |
| STATUS | 无 | 当前准备/播放代际 |
| OWNERS | 无 | 已登记模型的有符号 Unity instance ID 与名称 |
| NODES | instance ID | 该实例的完整精确挂点地址与名称 |
| PREPARE | runtime.bin 绝对路径 | 后台核验文件并主线程准备原生资源 |
| PLAY | instance ID、NODES 中的地址 | 播放准备完成的特效，`-` 表示模型根 |
| STOP | 无 | 退休测试实例，保留已准备资源 |
| CLEAR | 无 | 停止并退休准备代际，取消待处理的准备 |
| RESULT | ticket | 查询 Unity 命令结果，超时不隐含取消 |

地址沿用项目现有 `UTF8字节长度:节点名:同名兄弟序号/` 格式。应复制 NODES 的结果；不是普通的 slash 名称路径。实例释放或缺失该地址则拒绝，不自动换绑其他角色。负数 Unity instance ID 合法。

客户端：`tools/diagnostics/eff_effect_debug.py`，无需安装 Python 包，也不需要索引。

```powershell
python tools/diagnostics/eff_effect_debug.py --pid 1234 owners
python tools/diagnostics/eff_effect_debug.py --pid 1234 nodes 5678
python tools/diagnostics/eff_effect_debug.py --pid 1234 prepare 'E:\FX\test.efffxbuild\runtime.bin'
python tools/diagnostics/eff_effect_debug.py --pid 1234 play 5678 '-'
python tools/diagnostics/eff_effect_debug.py --pid 1234 stop
python tools/diagnostics/eff_effect_debug.py --pid 1234 clear
```

## 验证与性能边界

离线作者回归检查实际原生包、SHA256、依赖输入、inactive 根和确定性输出。C++ 隔离测试直接包含生产 TU，调用真实调试状态机/队列/退休代码，替换 Unity 调用边界；不是另一份模拟调试器。另有真实 Windows named pipe + Python 客户端测试。

测点分别记录 PREPARE 请求到原生资源 ready、PLAY 排队到播放调用、之后第一个 Unity 帧及单帧主线程耗时。后台校验计入 PREPARE 总时间；单次原生调用不能被软帧预算中断。日志中的模拟器微秒数不是游戏性能证据，下一 Unity 帧也不是 GPU 已显示。必须实机验证画面、音效、显隐、切图和连续重载，才可评价端到端毫秒级目标。

当前不接管技能、动画事件或战斗 EffectInstance 池，不修改其正常触发，不支持新发射器拓扑/外部依赖的发布。SPEED 等实时参数 setter、Blender 调试按钮、正式 Mod 挂载声明还未接入；不返回虚假的成功。全局 F10 不读取此产物，已有 Mod 的热重载逻辑保持原路径。

宿主禁用插件时只排队 CLEAR，仍由下一 Unity 帧退出原生对象；禁用期间拒绝新的准备/播放动作。管道线程不会在导出控制函数里直接销毁实例。
