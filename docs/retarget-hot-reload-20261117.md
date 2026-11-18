# 更换目标后的原版恢复与代际退休审查

用户在 Manager 中更换武器目标后，旧目标仍显示作者网格且材质发紫。本轮检查了 DLL 的原版恢复、代际路由、世界/UI 所有者刷新、特效挂载/PFB 替换与最终资源释放。

## 现场证据与根因

日志副本为 `diagnostics/retarget-reload-20261117/initial-eff.log`。PID 39352 的会话先替换 `wpn_sword_0014`，07:05:53 F10 改回 `wpn_sword_0022`。世界角色与旧武器所有者被正确捕获，角色完成原生生命周期重建；因此不是 F10 没触发。

旧武器 Mesh 没有公开独立 RuntimeManifest 项，也没有 `target.bundle` 提示。新代只对当前目标做请求内发现，旧目标无法进入原版输入准备流程。随后原生 VFS 请求 `55EFF2D7974CA2FD`、`8BEF9CAA4C3839F3` 命中了仍存活的旧 token 1、2，提交了隔离之前的替换输入，而非新代原版输入。这条旁路还绕过了已经装配好的材质依赖图，能造成旧网格残留和 Shader/材质引用错配。

修正后，安装代际保留被移除目标的**逻辑身份**。真实游戏请求到来时，在其依赖范围内精确发现源 Mesh，并以当前代际准备原版输入；不恢复作者绑定，不读取全游戏索引，不提前加载游戏对象。后续无变化 F10 仍保留恢复能力；重新添加目标后，当前声明优先。

生产模式关闭原生文件名、物理文件地址和 VFS 源键到旧 prepared input 的替换旁路。替换统一走现有代际代理和已隔离的资源图；旧在途请求仍按明确的代际别名读取自己的输入。

## 同类缺陷

| 问题 | 修正与验证 |
| --- | --- |
| 同一特效 ID 从挂载改为 PFB 替换，旧挂载残留 | 挂载存活检查同时验证关系类型；切换用途时释放旧实例，反向切换重新绑定。真实 Frame 回归先失败、修复后通过。 |
| 旧 SharedScope 回收后 VFS 弱路由遗留 | 最后一个原生代理释放后同步删除代际路由与 prepared input；有世界/UI 持有者时继续保留。 |
| 回收 SharedScope 时提前销毁其内部互斥锁 | 回收循环持有独立 shared_ptr，确保锁守卫先析构，再销毁 scope。原先引用 map 内的 shared_ptr，erase 最后一个持有者后存在解锁已释放内存的风险。 |
| 已准备 Prefab 的来源键与游戏缓存别名存活根不同 | 保留前一轮修正：来源键对应的代际别名一起作为存活根，最终所有者释放后正常回收。见 `weapon-generation-lifetime-20261117.md`。 |

## 回归与边界

新增回归位于 `tools/diagnostics/retarget_generation_checks.h`。`--retarget-native-routes`、`--removed-dependency-mesh`、`--retired-generation-index` 分别验证旁路关闭、删除最后一个 Mod 后恢复原生输入、最终路由释放。修复前的失败和修复后的通过日志位于同一诊断目录。

原版恢复测试通过生产 Catalog、Current、PrepareDependencyGroup 和原生输入隔离路径，先实际用 Mod 隐藏源 Renderer，再删除 Mod，连续三代恢复原 Mesh、Renderer、Shader/Texture 引用；同时保留旧代代理以模拟世界/UI 并存。测试不加载 Unity 对象、不扫描场景。

完整 `full_resource_reload_smoke`、特效与材质动画测试、挂载/PFB 双向切换、PFB 缓存重试/增删、Manager 重定向代际、原生来源证明、四级 LOD、游戏资源租约、共享资源图、材质删除操作、代际差异和 RuntimeShapeBridge 均通过。生产 DLL 通过无探针/无 RenderDoc 注入检查与真实 LoadLibrary/导出接口循环测试。优化测试与构建日志保存在诊断目录。

这些验证不等同于实机画面验收。发布前需在 Manager 中执行 A→B→A，并分别检查大世界、武器展示与隐藏后重新打开的 UI；再删除/禁用 Mod 后 F10，确认原版网格和材质恢复。应核查日志未再出现旧 token 的 `DEMAND-NATIVE-SUBMIT`，且原版恢复经过当前代际 `DEMAND-BASELINE-DISCOVERED` / `DEMAND-SHARED-GROUP` 路径。

## 本机部署

四仓 main 已 fetch，均包含 origin/main。生产 DLL 已安装到 `D:\Hypergryph Launcher\games\Endfield Game\plugin\eff.dll`，与 `bin/resource-directory-entry-format3/eff.dll` 的 SHA256 一致：`4A628C393499DDD203322EB969728D9C1476D7D32BC5B01C77EB63D42037EA39`。安装使用固定部署器，保留用户配置与 Mods；回执为 `bin/local-deployments/20261117-074709-360/Dll.json`，日志为 `diagnostics/retarget-reload-20261117/deployment.log`。

用户随后回复“已测试，恢复正常”，确认更换目标后的原版恢复和紫材质修正。后续审查使用的 PID 23424 日志中两次 F10 均为 `paths=0 owners=0`；它确认新请求经过新代际，不能扩大为所有活动世界/UI owner 原地刷新已验收。长期退休及发布风险见 [DLL 发布前审查](dll-release-audit-20261117.md)。本轮不创建远程提交、标签或 Release。
