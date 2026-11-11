# 武器 F10：把直接加载资产误当成实例代理的调查

日期：2026-11-09。范围：`langzhifei`、`wpn_sword_0022.prefab` 的 F10 失败。
先完成只读调查，再按用户要求修复运行时。修复与验证记录见文末；用户 Mod
与 Blender 工程未修改。

## 结论

修复前 F10 错把“有一个已加载的模型 owner”当成“必定有一个
`PrefabInstantiateProxy`”。游戏没有这个约束。

本机游戏的 `WeaponComponent._LoadWeapon` 在构造 `BaseModelViewPartData` 时
明确设置 `usePool=false`。`BaseModelViewPart` 对这个配置走原始 Prefab 资产
handle 加直接实例化的分支，不经过 `PrefabInstantiateProxy._PreloadPrefab`。
因此没有该路径的 **实例代理** 记录可以是正常状态，不等于游戏没有加载武器，
也不等于插件漏观察了一次相同的实例代理调用。

而 `DemandGenerationReload::ReleasePath` 使用 `!batch.owners.empty()` 决定
是否强制要求观察过实例代理。对上述武器分支，这个条件不成立，结果是：

1. F10 已发布新资源代际，并释放/退役旧 owner 的模型绑定。
2. 实例代理字典找不到该路径，缓存步骤抛出异常。
3. `cacheInvalidated` 始终为 `false`，新的 owner 加载被禁止。
4. 30 秒后该路径超时。等待不会让一个本来不存在的实例代理出现。

这是 F10 对游戏加载方式的错误假设。只扩展 `_PreloadPrefab` 的记录时机，
不能覆盖这个正常的直接加载分支。

## 证据与复核方式

生产代码：

- `src/eiem_entry_full_resource_reload.h`：只有 `ProxyPreload` 调用
  `RememberPrefabProxy`；需要原资源是存活的 GameObject。
- `src/eiem_entry_full_resource_reload.h`：`InvalidatePrefabProxyCache` 在
  `requireObserved=true` 且字典没有对应 hash 时抛出
  `affected prefab proxy was not observed before reload`。
- `src/eiem_entry_demand_generation_reload.h`：`ReleasePath` 传入
  `!batch.owners.empty()`，没有检查 owner 实际使用哪种资产加载方式。
- 同文件的 `StartAfterCacheInvalidation` 门槛和 30 秒超时处理，将上面的
  错误假设变成无法继续加载的路径失败。

本机 `GameAssembly.dll` 与 IL2CPP metadata 的只读检查：

| 游戏方法 | 本机文件 RVA | 已确认行为 |
| --- | --- | --- |
| `WeaponComponent._LoadWeapon` | `0x55FDE20` | 构造配置的第一个字节为 0，并将该配置传给 `AddWeapon` |
| `BaseModelComponent.AddWeapon` | `0x55FC4F0` | 创建 `WeaponModelViewPart`，将收到的配置传给 `LoadModel` |
| `BaseModelViewPart.SetupData` | `0x8E01AB8` | 将配置复制到对象 `m_cfg` 对应区域 |
| `BaseModelViewPart._LoadSync` | `0x55FC310` | 检查配置首字节；为 0 时走资产 handle 完成回调，为 1 时才进入模型池分支 |
| `BaseModelViewPart._OnLoadUseHandleFinish` | `0x55FB620` | 保存资产 handle，取源对象，再调用直接实例化流程 |
| `BaseModelViewPart.ReleaseModel` | `0x59B15A0` | 存在释放原始资产 handle 的正常分支 |

`BaseModelViewPartData` metadata 的首字段是 `usePool`，后续为
`usePersistentPool`、`isSyncLoad` 等。配置创建、复制和读取位置相互对应，
没有靠猜测其他类型的私有对象布局读取运行中进程。

这些 RVA 只描述被检查的本机二进制，不是实现用的硬编码地址。
游戏方法有 iFix 跳转入口；本轮没有读取运行中实例的配置值或执行这些方法，
所以静态结果不宣称覆盖游戏可能安装的所有热补丁行为。

完整报告与机器检查结果保存在
`bin/f10-weapon-proxy-research-20261109/`：

- `static-proxy-lifecycle.json`：模型与实例代理的静态方法、分支、调用关系。
- `viewpart-loading.json`、`model-add-weapon.json`、
  `weapon-component-loading.json`：武器配置到直接加载的完整链路。
- `resource-manager-instantiate.json`、`bundle-resource-instantiate.json`：
  池化分支确实存在实例代理，与直接资产分支有区别。
- `raw-asset-cache-lifecycle.json`：资产 handle 释放及 AssetProxy 自身生命周期。
- `raw-clone-call.json`、`static-checks.json`：直接实例化调用及五项静态断言。
- `eff_log-pid34412.txt`：本轮日志的固定副本。

日志 PID 34412：

- `10:32:57.925`：`.001` 的四级 Renderer 完成绑定。
- `10:33:30.999`：F10 发布 generation 2，并退役独立武器的 8 个 Renderer 绑定。
- `10:33:31.002`：独立武器路径缺少实例代理记录，缓存失效步骤被阻止。
- `10:33:31.505`：新一代 `.001` 和 `.012` 已有新的 Mesh/Renderer 绑定。
- `10:33:31.508`：大世界角色 owner 完成刷新。
- `10:33:31.602`：UI 页未接受完成的新模型，这是另一条独立失败。
- `10:34:01.012`：独立武器路径超时，`cacheInvalidated=0`。

因此这轮是部分成功，不能把武器路径超时理解成所有武器实例都仍是旧资源。
本轮新增 `.012` 的记录也不能用来证明上一进程删除 `.012` 已成功。

## 大世界、UI 与父子 owner 的关系

多个实例同时存在是正常用法，不应因此热重载失败。三种 Prefab 路径不能
不加区分地共用一个刷新结果。

但角色和武器并非完全没有生命周期联系：

- `Affected` 会检查 owner 层级中的 Renderer，挂在角色下的武器也能使角色
  owner 被选中。
- `ReleaseConsumers` 调用游戏 `WeaponComponent.UnloadWeapons`。
- `ReloadConsumers` 更新挂点后调用游戏 `ReloadWeapon`。

于是同一轮可能同时捕获父角色与子武器；刷新父角色时，游戏会自行退休旧
武器 part，再创建新的武器 part。捕获的旧 part 不能无条件作为仍需单独
重建的 owner。上一轮出现 `old-owner-ended` 却仍因 owner 列表非空强制要求
实例代理，也是同一错误假设在已结束 owner 上的体现。

## 修复方向

不能全局关闭 `requireObserved`，也不能把“没有观察到”统一当成缓存已清理。

1. 在真实加载/完成事件中记录 owner 的加载方式和实际资产 handle 归属。
   对直接资产分支使用原始 `AssetProxy` 与 handle 的生命周期证据；对实例
   代理分支保留原来的实例代理退休要求。依据真实配置与 handle 类型，
   不依据武器名称建立特例。
2. 将“旧 owner 已释放”“模型池已处理”“所用资产缓存允许新请求”分别表示。
   直接资产分支不应执行一个不存在的实例代理的清理步骤；仍须确认旧资产
   handle 由游戏释放，不能把 Bundle 代际隔离误当成 AssetProxy 缓存已刷新。
3. 角色与武器同时受影响时，按实际父子和组件归属安排刷新。父角色重建的
   子武器由游戏重新创建并重新登记；确认旧 part 已结束后，不向旧 part
   重复发起加载。未被父刷新覆盖的独立武器 owner 仍需单独刷新。
4. 同一路径多实例应逐个处理其真实持有者；UI 仍持有资源时保留其有效 lease，
   不直接清空全局缓存或强行修改引用计数。
5. 失败路径保留重试信息，下一次 F10 即使磁盘清单没再变化，也能继续处理；
   不能用 `changed=0 paths=0` 把上次失败变成空任务成功。
6. 以新实例实际使用的新 Mesh/Material/Texture 验收，不能只以 generation
   提交、root 改变，或把已结束 owner 计作 `refreshed` 认定成功。

以上为调查阶段确定的修复要求；下方记录实现与当前验证范围。

## 回归盲区与应补场景

`tools/diagnostics/full_resource_reload_smoke.cpp` 的实例代理回归手动调用
`RememberPrefabProxy` 建立记录，然后验证卸载成功或游戏拒绝卸载。
这验证了“已知实例代理”的清理，但没有覆盖
“`usePool=false` 的正常 owner 根本没有这种代理”。

后续回归须覆盖：

- 只有直接资产 handle、没有实例代理的武器，增加和删除第二个 Part 后 F10。
- 一个父角色与其直接加载的武器同时被选中，旧子 part 随父重建而结束。
- 大世界与 UI 同时持有同一武器资源，新请求取得新资源，旧持有者不会被强卸。
- 真正使用实例代理的模型缺少追踪时，仍须报告缺失证据，不静默放行。
- 一次失败后未再次修改磁盘，按 F10 能重试失败路径。
- `.001` 与 `.012` 在 LOD0–LOD3 的增删都核对最终部件集合及资源身份。

上述为完整验收要求，自动回归与实机验收须分别记录。

## 2026-11-09 修复

运行时改动：

- `MakePartOwner` 读取真实 `m_cfg.usePool`。直接加载的 part 从有类型的
  `m_assetHandle` 取得原始 `AssetProxy`；不根据武器名字或路径猜测分支。
- F10 先让游戏释放 owner/句柄，再清理路径池和已观察到的实例代理。直接
  加载分支不要求一个不存在的实例代理；池化分支仍要求有该代理的证据。
- 命中过的原始 Prefab 只保留弱观察，不增加游戏的资源引用计数。等待游戏
  `IAssetProxy.isUnloaded` 的实际实现确认卸载，尚被其他句柄持有或异步
  卸载尚未完成时不开始新加载，不调用 `AssetProxy.Release` 强清句柄。
  `Loadable` 另有同名非虚属性，不能用它代替资产接口实现；这里通过
  `il2cpp_object_get_virtual_method` 解析实际实现。
- 父角色与其下的 `WeaponModelViewPart` 同时选中时，旧武器交给父角色的
  `UnloadWeapons` 结束，待武器路径缓存就绪后，父角色才开始加载和
  `ReloadWeapon`。不向已退休的旧武器 part 再次发起加载。
- 缓存阶段超时保留已释放 owner 的阶段信息。下一次 F10 即使磁盘内容不变，
  也继续重试；父角色因子武器缓存失败而延后的任务一起保留，并重新捕获
  失败后同一路径新建的持有者和缓存，不遗漏新出现的实例。
- 已结束 owner 单列 `ended`，不再计入 `refreshed`。

未替换任何游戏方法地址，未强改引用计数、Renderer Mesh 或骨骼矩阵。
本次没有修复日志里独立的 UI 页拒绝绑定问题；缓存就绪后的加载/页面绑定
失败也不属于本次“缓存阶段失败重试”的保证范围。

自动验证使用真实生产入口的 `full_resource_reload_smoke.cpp`，只替换
游戏 API 为确定性夹具。先新增直接加载回归，再修改运行时；首次回归准确
失败于 `affected prefab proxy was not observed before reload`。修复后覆盖：

- 无实例代理、只有直接资产句柄的 view part 能经游戏释放/加载流程刷新。
- 接口方法必须解析实际实现，不能直接调用抽象接口或同名基类 getter。
- 原始资产仍有额外句柄或异步卸载未完成时，不重新使用旧 Prefab。
- 运行真实 `Frame` 超时/收尾流程后，未修改磁盘的下一次 F10 能恢复加载，
  不重复释放旧 owner，也能捕获失败后才出现的不同 part 持有者。
- 父角色与子武器两条路径被实际捕获，子路径先被访问、延迟卸载、超时，
  以及随后不变内容重试，最终只由游戏 `ReloadWeapon` 创建新武器。
- 池化路径缺少实例代理证据、或者游戏拒绝卸载该代理时仍被拒绝。
- 现有整套资源重载回归通过，包括跨代 Bundle 路由、删除目标恢复原版、
  UI/世界多 owner、静态武器、异步加载与无关 Prefab 零额外拆箱。

运行命令：

```powershell
cmd /c tools\diagnostics\build_full_resource_reload_smoke.cmd
bin\resource-directory-entry-v35\full_resource_reload_smoke.exe
cmd /c build.bat
```

自动回归日志：`bin/f10-weapon-proxy-research-20261109/reload-smoke-fixed.log`。
生产构建日志：同目录 `production-build.log`。游戏尚需用新 DLL 验收：
同时导出 `.001/.012`，F10；再只导出 `.001`，F10；检查世界/UI 和
LOD0–LOD3 的最终 Part 集合。自动测试不是 Unity 实机画面通过的证明。

## 首次本机部署（实机启动失败，已查明）

2026-11-09 11:27，用户正常退出游戏后，用现成生产构建执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/release/build_deploy_local.ps1 -Component Dll -UseExistingDllBuild
```

本机没有 `pwsh` 命令，使用 Windows PowerShell 运行同一部署脚本。
此前尝试因游戏 PID 43312 仍在运行而被脚本阻止，没有替换正在使用的 DLL。

- 安装目标：`D:\Hypergryph Launcher\games\Endfield Game\plugin\eff.dll`。
- DLL SHA256：`752855CDEA31714C76472B3126848E34B58380FEA8E94072D406D46EC600355D`。
- 回执：`bin/local-deployments/20261109-112755-661/Dll.json`。
- 旧版备份：游戏 `plugin/.eff-deployment-backups/local-20261109-112755-661/`。
- 同时校验并安装协议文件；保留 `eff.ini` 和全部 Mods。
- EFF、AnimeStudio、Mod Manager、Blender 四仓库均在 `main`；部署前 fetch
  后本地 HEAD 均包含 `origin/main`。未提交、推送或发布 Release。

生产 DLL 的检查通过：Format 3 路由/代际功能存在，无诊断探针与动态 MSVC
运行库依赖。安装后文件 SHA256 与生产构建相同，但下方实机日志证实这版
未成功启用替换链；构建、二进制检查和运行中 API 夹具未覆盖启动绑定。

## 首次部署的启动回归

用户重启后发现全部模型回到原版。PID 20116 的实机日志显示：

```text
[FULL-RELOAD-REJECT] phase=install reason=reload exact managed signature absent: Beyond.Resource.IAssetProxy::get_isUnloaded()->System.Boolean static=0 configurationFallbackRefused=1
[DEMAND-PROBE-GAP] phase=bind reason=creation-before probe requires directory/loader metadata
[ENTRY-READY] ... replacementArmed=0 demandReplacementArmed=0 controlArmed=0 ...
```

这不是 Mod 内容或安装位置的问题：新增的接口属性使用了只接受有原生函数
地址的方法绑定器。`IAssetProxy.get_isUnloaded` 是抽象虚方法，具有完整元数据，
但原生函数地址为空，因此初始化异常，后续全部资源替换未启用。

修正保留 `Exact`/`Method` 对可执行方法的地址检查，仅对该卸载状态接口
增加严格的元数据绑定：名称、零参数、Boolean 返回、非静态、虚方法及唯一
匹配均须成立。运行时仍通过 `il2cpp_object_get_virtual_method` 在实际代理上
解析实现，然后调用该实现；不调用抽象接口或 `Loadable` 的同名非虚属性。

新增回归直接调用生产 `BindRawAssetApi`，先在修复前复现同一启动异常。
修复后除有效抽象槽位外，还验证错误返回类型、带参数、静态、非虚、歧义、
缺失接口和缺失具体方法地址均被拒绝；通过实际 `ReleasePath` 验证此入口
绑定的接口确实分派到具体实现，并保留未卸载资产的等待条件。

证据保存在 `bin/f10-weapon-proxy-research-20261109/`：

- `eff_log-startup-reject-pid20116.txt`：失败进程的固定日志副本。
- `startup-binding-smoke-build.log`：新增回归的构建日志。
- `startup-binding-red.log`：修复前运行输出；错误原文同时记录于工具运行结果。
- `startup-binding-fixed.log`：修复后完整资源重载回归通过。
- `production-startup-binding-build.log`：修正版的生产构建日志。

修复后完整 smoke 通过，包括原有 F10 事务与直接 Prefab owner、共享句柄、
延迟卸载及父角色/武器生命周期回归。生产构建通过 `verify_production_dll.py`，
检查确认诊断探针已排除、保留所需生命周期绑定。源码四仓库均位于 `main`，
且 `HEAD` 与 `origin/main` 相等；本轮没有切换分支或发布远程版本。

2026-11-09 11:45 用本地产物完成 DLL 部署：

- 回执：`bin/local-deployments/20261109-114535-323/Dll.json`。
- 安装目录：`D:\Hypergryph Launcher\games\Endfield Game\plugin`。
- DLL SHA256：`1CAA14900410D87DACF89ACFAD45A6744C4111F76BE2297BDCB5F6718273AA29`。
- 协议 SHA256：`0FA79FB28F09A15969B5CDF92A6A3E05D4A04EFC391FBEED89A1FF4D37FDD013`。
- 修复前 DLL 已由部署脚本备份；部署完成时游戏进程未运行。

游戏尚未用此修正版重新启动，实机 `ENTRY-READY` 与 Mod 替换结果待确认。
重启后若仍有问题，应先查看新时间段的 `eff_log.txt`，核对启动错误及
`replacementArmed` 状态。HDR 材质编辑修改继续暂停，本次没有修改用户 Mods。
