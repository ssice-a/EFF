# 武器 Prefab 代际数据的存活期

用户反馈角色装备武器后不可见。现场会话 PID 38248 中，普通武器
`wpn_sword_0022.prefab` 已完成替换与特效绑定，Mod 没有整体隐藏武器的配置。
原始日志保存在 `diagnostics/weapon-invisible-20261117/initial-eff.log`。

## 已确认的缺陷

scope 13 为武器来源 `A5E21E72A5C28127` 准备了代际别名
`6C6AB703F98DBE91`。05:26:45 的无关资源退休清理了 5 个输入；之后三次
请求该别名都进入原生目标发现，并报 `requested native dependency has no VFS entry:
<unreadable-string>`。别名属于 EFF 的代际资源，游戏原生清单没有它的名称。

生产代码的 `preparedGroups` 使用原生来源键记录持有关系，游戏缓存则使用
代际别名。`RetireSharedLocked` 原先只把来源键加入存活根；来源键指向的
作者材质等依赖仍可达，代际 Prefab 输入本身却可能被回收。游戏复用已加载
代理时不保证再次触发加载回调，因此不能仅靠 `activeAuthorProxies` 补全该关系。

修正将仍有持有者的来源键所对应的代际别名同时作为存活根。没有额外加载、
扫描或永久保留全部历史代际；最后一个持有者退休后仍走相同的不可达回收。

## 验证

`PreparedGenerationRootLifetimeRegression` 使用生产回收函数复现
“准备武器 → 复用已加载代理 → 退休无关资源 → 再访问武器输入”。修复前
失败，修复后保留正确的 Prefab 输入与依赖；最终退休后，输入、持有记录和
材质缓存全部释放。定向入口为 `full_resource_reload_smoke.exe
--generation-root-lifetime`。

完整 `full_resource_reload_smoke.exe` 通过，覆盖 F10 增删、共享世界/UI 武器、
原生异步退休、失败重试、动画、特效共享资源和最后持有者释放。测试使用原生
API fixture；它确认生命周期缺陷与修复，不代替本轮实机可见性验收。

构建、部署与实机反馈分别以该诊断目录中的回执和后续日志为准。

## 本机部署

用户确认退出游戏后，已使用现有部署器安装生产 DLL 与协议，并核对两者
均与构建产物一致。未修改 `eff.ini` 或 Mod 文件。安装 DLL SHA256 为
`4F2B964DC145A95CA1E2DFEC7D37B8E69F00A42C795C9BE24B95DCC3F2CD916F`，
回执为 `bin/local-deployments/20261117-054130-640/Dll.json`。未提交、推送或发布；
本轮实际武器可见性及多次进出详情页后的表现待实机确认。
