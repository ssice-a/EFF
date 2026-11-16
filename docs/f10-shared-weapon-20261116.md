# F10 等待世界与 UI 共用的武器 Prefab

## 现场证据

用户反馈重新导出后 F10 仍显示旧资源，手动切换角色后才刷新。
`diagnostics/f10-shared-weapon-20261116/eff-before.txt` 保存 PID 16156 会话。

- 12:52:56.624：新声明已准备，`modified=1`。
- 12:52:56.729：generation 2 已发布，仅捕获 1 个武器 owner；释放后 `wpn_sword_0022.prefab` 的原生 AssetProxy 仍有 1 个引用，不能重载路径缓存。
- 12:53:17.114：旧 AssetProxy 才完成卸载。
- 12:53:17.269：新武器 root 建立，12:53:17.365 事务结束。

因此这次问题发生在实例和路径缓存交接阶段，并非 F10 没读取新文件或动画未进入资源代际。旧日志没有给最后一个句柄标注持有者，不能仅凭 `handles=1` 断言它属于大世界。

## 缺口与修正

世界武器由 `BaseModelViewPart` 管理；角色 UI 的 `CharUIModelMono` 另持有普通/静态武器列表，每个 WeaponBundle 有 `modelPath` 与 `weaponAssetHandle`。后者不会必然出现一个可独立刷新的世界 view part；武器网格尚未挂入层级时，遍历 UI Renderer 也发现不了该句柄。

当前游戏 metadata 与 PE 的只读类型检查确认两个字段均为 `List<WeaponBundle>`，其路径类型为 `System.String`，句柄类型为 `Beyond.Resource.FAssetProxyHandle`。检查脚本和结果留在同一诊断目录，不加载或调用游戏 DLL。

F10 现在读取已登记 UI 根上的这些原生列表，将其对受影响逻辑路径的依赖加入 owner 收集。仍通过游戏现有 UIModelLoader 和页面绑定生命周期释放、重建，不主动 Dispose 游戏的武器句柄，也不强制卸载有引用的 Prefab。UI 页面启动重载之前，必须等待它依赖的受影响武器路径完成缓存交接，防止先重建的 UI 又占用旧武器缓存。

依赖查询仅发生在 F10 owner 捕获时；读取已存在的原生对象，不加载新游戏资源、不查离线索引或扫描场景。无关页面不会因其他武器的修改而被加入。新日志 `DEMAND-F10-OWNER-CAPTURE` 与 `DEMAND-F10-OWNER-DEPENDENCY` 用于确认实际纳入的持有者。

## 验证与边界

`full_resource_reload_smoke.exe --shared-world-ui-weapon` 使用生产 owner 收集、原生 UI 列表读取和路径释放/加载流程：世界与一个没有武器 Renderer 的 UI 页面共用同一 raw Prefab，初始引用为 2。要求收集两种 owner，世界释放后保留安全门槛，UI 释放后引用归零；UI 不能提前请求加载；路径完成交接后，两侧可继续重建，无需角色切换。

该回归已先复现 UI owner 遗漏，再验证修正；完整事务回归覆盖原有延迟卸载、共享句柄保护、失败后未修改文件的重试、父角色与武器生命周期和 UI 页面异步交接。实机是否立即刷新须由新 DLL 下的 F10 操作确认，离线通过不代表已验收。

## 本机部署

2026-11-16 13:16，确认游戏进程已退出后，生产构建与正式 DLL 检查通过，已部署到 `D:\Hypergryph Launcher\games\Endfield Game\plugin\eff.dll`，保留当前 Mod 与 `eff.ini`。

- 安装与构建 SHA256 均为 `34806F9E49EB82BE988DF439CA220FC6AA711E1679F8D8BA6FA6CBE83B6C8EB1`。
- 部署回执：`bin/local-deployments/20261116-131622-015/Dll.json`。
- 定向及完整回归日志：`diagnostics/f10-shared-weapon-20261116/shared-green.log`、`full-green.log`。
- 未提交、推送或发布。本轮实机 F10 验收待用户操作。
