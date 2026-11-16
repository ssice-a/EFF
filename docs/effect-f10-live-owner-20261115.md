# F10 遗漏嵌套武器的活动特效

用户反馈：重新导出特效后按 F10，当前武器仍播放旧特效；切换角色再切回来才更新。

## 实际会话证据

`diagnostics/effect-f10-live-owner-20261115/eff-before.txt` 保留 PID 23096 的会话。11:02:19，F10 检测到一个修改包并成功发布 generation 3。模型目标变化为 0，符合本轮只修改独立特效的输入。

11:02:20，特效声明已重新装配，一份 layer 11 的武器实例播放了新包的 1 个发射器；此前已登记、正在显示的 layer 13 helper 实例没有对应的新绑定或播放记录。11:02:22，角色切换产生的新 helper 完成后，才播放同一新声明的 1 个发射器。此前该类 helper 的记录为 3 个发射器。

这说明声明读取与原生资源更新已发生，遗漏的是当前活动挂点的刷新上下文。

## 根因与修正

`EffectDebug::Owners()` 把角色下面的 helper/clone 根折叠到角色父级，用于列出顶层游戏 owner。特效却可能通过 helper 完成回调绑定在嵌套武器自身，其 Renderer 路径相对于该武器根。F10 只遍历折叠后的 owner，父级无法代替这个相对路径上下文；嵌套武器的原绑定保留，直到下一次 helper 完成回调。

`RuntimeAttachments::ReconcileOwners()` 合并顶层 owner、已有绑定的准确 owner、待处理回调及重试 owner，只保留存活对象并按 Unity 实例 ID 去重。F10 和直接特效重载使用同一集合。集合在移除旧声明的绑定之前取得，确保更换声明 ID 后仍保有正确根物体进行匹配。

该集合只由已登记的游戏生命周期和现有绑定产生。正常空闲帧保持原逻辑；资源变化时继续检查精确 Renderer 路径和 Mesh 来源，原生新实例准备成功后才交换旧实例。

## 验证

回归使用真实生产 `RuntimeAttachments::Frame()`：先创建世界、helper、UI 三份独立播放实例，再把 helper 根放到世界角色下面，确认顶层 owner 枚举会折叠它；修改控制图中的特效原生代际后触发声明刷新，要求三份实例均更新、共享新资源、分别保有自身挂点，且旧 helper 实例完成退休。

旧实现先在 `--attachment-only` 上失败，报错 `F10 retained the old effect on a nested live helper until character switching recreated it`。修正后该用例通过；完整特效生命周期/材质动画、`--lod-attachment-check` 和 `--native-mesh-proof-check` 同样通过，分别覆盖四级 LOD/Empty 动画路径、通用原生 clone 热重载与弱引用释放。日志位于同一诊断目录的 `*-green.log`。

## 本机部署

2026-11-15 11:14，确认游戏已退出后，使用已构建产物执行 `build_deploy_local.ps1 -Component Dll -UseExistingDllBuild`，安装到 `D:\Hypergryph Launcher\games\Endfield Game\plugin\eff.dll`。生产 DLL 校验通过，安装文件与构建文件的 SHA256 均为 `d6f713d9cb9afe1e84f5f73f20fd3dff27988c446da3553c811f3b2cc7a23a03`。

安装回执：`bin/local-deployments/20261115-111418-787/Dll.json`。部署前四个仓库均为 `main`，且本地 `HEAD` 包含远程 `origin/main`。保留用户当前 `mods/lizi` 和 `eff.ini`。

当前状态：离线回归通过、生产 DLL 已安装；待实机验证保持当前角色不切换、重新导出特效并按 F10 后，当前武器是否直接更新。离线原生 API 回归不能代替游戏内验收。
