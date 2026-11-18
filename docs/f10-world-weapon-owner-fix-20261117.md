# F10 大世界武器归属与历史代理修正

## 现场

用户复现：F10 后 UI 页面刷新，大世界没有刷新，平 A 后持续异常。现场会话 PID 41420，使用生产 DLL `e9c7e229cebed1f88d2e42665fcd4a96abca4fbc494da4bc35d1d307ce0e1575`。

证据保存在 `diagnostics/discovery-bug-20261117-105046/`。首轮 10:46:50 只捕获 `wpn_sword_0022.prefab` 的 `view-part`，没有其所属的 `world` 角色；缓存记录为 `proxies=3 unloaded=0 remaining=3`，随后进入重试。后续 UI 通过自然生命周期重新加载，不能据此认定大世界的刷新事务成功。采集快照时游戏已不在运行，不能由进程缺席判断其退出原因。

## 已复现的代码问题

1. 武器归属关联在筛选 owner **之后**才执行，并且要求武器仍位于角色 Transform 下。仅武器资源变化时，角色可能未被筛选；攻击期间武器也可能脱离骨架。旧代码会将受角色 WeaponComponent 管理的武器当作独立部件释放、SetupData 和重新加载，绕开组件的卸载、挂点和动画生命周期。
2. Prefab 缓存清理把 `TryUnloadImmediately=false` 一律解释成仍被持有。游戏的该方法要求代理处于待卸载队列；已经 Clear、但尚未 GC 的代理也会返回 false，导致历史弱引用永久挡住刷新。
3. owner 采集循环在可能抛异常的操作前递增迭代器，catch 又递增一次，存在漏掉下一个 owner 的问题。

静态检查使用本机 GameAssembly 和 IL2CPP 元数据，未执行游戏 DLL。`WeaponModelViewPart.m_modelComponent` 的真实声明类型是 `Beyond.Gameplay.View.IModelComponent`，构造函数保存调用方提供的 owner。Prefab 的 `get_isUnloaded` 是真实托管 Boolean getter，Clear 设置卸载状态并清理等待状态。运行时通过类型检查、IL2CPP 字段 API 和精确方法签名使用这些关系，没有新增固定偏移/RVA 调用或私有内存写入。

## 修正

- 采集武器时保存原生模型归属，在路径筛选前展开所属世界角色。主部件尚未被观察到时，从该角色的真实 `m_mainPart` 和当前模型恢复快照。
- 武器通过已有的角色生命周期释放和重建，不单独复活被 WeaponComponent 移除的旧部件。保留原有动画恢复、挂点刷新、UI 依赖、原始资源引用和失败重试顺序。
- 使用 `PrefabInstantiateProxy.get_isUnloaded` 退休已卸载的历史观察记录；仍未卸载且游戏拒绝释放的代理继续阻挡刷新。没有绕过活跃资源的引用屏障。
- owner 采集迭代器只前进一次。

## 回归与验收边界

先增加武器独立变化回归，旧代码失败于 `parent and direct weapon were not captured as distinct logical paths`。修复后覆盖：只有武器变化、武器脱离父层级、主部件未登记、无关角色隔离、子资源延迟卸载、无磁盘变化重试、由父组件重建一次武器。

历史代理回归在修复前失败于 `historical unloaded proxy blocked F10 or a still-retained live proxy bypassed the cache barrier`；修复后既移除已卸载记录，又保留真正活跃代理的屏障。

完整资源重载 mock 回归已通过，日志为上述证据目录的 `full-runtime-fixed.log`。mock 和静态反汇编证明代码缺陷及其修正，不能证明用户游戏画面的异常已消失；部署后仍须实机复测 F10、UI/大世界往返和连续普攻。

特效完整回归、生产二进制验证和 System32／游戏目录两组实际 DLL 加载均通过。11:03 已部署生产 DLL 到固定游戏目录，大小 3,755,520 字节，SHA256 `5c8ee8b8a22c1ef14664868d64fb17626c2c40de76a33881b2b93b033944c6a8`；回执 `bin/local-deployments/20261117-110338-333/Dll.json`。安装后的 `eff.ini` 和协议摘要与修复前一致。四仓库均为 main，部署前 fetch 确认 HEAD 等于 origin/main；没有提交、推送或发布。
