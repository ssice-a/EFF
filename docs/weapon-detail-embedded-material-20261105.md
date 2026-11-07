# 武器详情页：内嵌材质寻址与同包资源闭包

日期：2026-11-05。当前状态：用户本次反馈武器替换看起来成功，日志确认网格、内嵌材质和请求完成链路通过。

22:42 验收更新：恢复普通 DLL 后的 PID 30544 已实际命中 sw 四级路由，22:41:42.331 的材质解析记录 `containerPreload=1`，随后武器 Renderer 绑定完成；全会话 170/170 请求完成，资源准备无 GAP。结合用户画面反馈，本次详情页检查基本通过；其它画面/LOD 与剧情控制仍单独验收。日志与剩余问题见 [本次日志核对](runtime-log-review-20261105.md)。下方待实机状态为本次复测前的历史。

22:04 更新：本页修复已随场景生命周期修复部署，DLL SHA256
`8D0D490C13F91FFBED20FDB9DEB6E2FF7B5B07C6F810845790A0691A4AD43F46`，回执
`bin/local-deployments/20261105-220427-782/Dll.json`。下文的旧候选 SHA/待退出状态为部署前记录。
武器详情页画面仍待验收；部署本身不代表替换效果已通过。参见[场景加载诊断](scene-load-slow-20261105.md)。

## 新会话的证据

用户确认上一轮部署后，武器详情页仍显示原版。PID 36816 于 21:14:29 启动，
实际加载的 DLL SHA256 为 `4F8793F2FF7179354851B72F030C8B692C57ABCE7C723B220C9020D3728E96C8`。
现场快照在 `bin/weapon-detail-20261105-2116/`。

- 四个 `sw` LOD 均已记录 `[DEMAND-TARGET-BUNDLE-ROUTE]`，当前为 declared=192、resolved=192。
- 21:16:12.932，Mesh carrier 已准备成功：`carrierMods=1 carrierParts=4 mods=sw`。
- 21:16:12.939，请求根 Bundle `A5E21E72A5C28127` 的共享依赖准备失败：
  `declared game dependency has no unique current native identity`，指向武器 PFB。

因此上一轮目标登记修复已经生效，但不足以完成这把武器的材质准备。
上一轮 4/4 离线网格装配没有覆盖材质来源解析与依赖图发布，不能作为详情页替换验收。

## 根因一：材质没有独立容器条目

源 PFB：`assets/beyond/dynamicassets/gameplay/prefabs/weapons/wpn_sword_0022_refined.prefab`。
源 Bundle：`bundles/windows/main/fc74df0a1ba9e422c7b26842.ab`。
材质：`M_wpn_sword_0022_refined_01`。

直接调用生产 `DependencyGroup::ResolveLogicalDependency`，真实资源稳定复现同一异常：

```text
containerAssets=1 directMatches=0 namedMaterialObjects=1
FAIL declared game dependency has no unique current native identity
```

材质内嵌在 PFB 中，容器只列出 GameObject 根条目。旧代码把容器条目当作完整子资源索引，
没有检查该精确容器的原生 preload 区间。

现在保持完整逻辑身份，先按来源路径选择唯一容器范围，再从其原生依赖指针验证
`source.asset + classId`，解析到当前的 CAB/PathID。候选必须属于所声明 PFB 的依赖表；
不按显示名搜索整个 Bundle、其它 PFB 或场景。重复指针去重，歧义仍明确拒绝。

真实解析结果：`CAB-b2a89ec1ff00a03a2ad0e18804a9d5c2:990062077401294749`，class=21。

## 根因二：内嵌贴图回指源 PFB

修正寻址后，继续执行真实 Material 构建及生产依赖图提交，进一步复现：

```text
FAIL shared native dependency cycle
```

该材质引用同包 Texture2D。若新材质依赖原 PFB 来取得它，会形成
`PFB → 新材质 → 原 PFB` 的 Bundle 依赖环；仅解决索引仍不够。
原先单对象载体还会删除这个 local Texture，却保留指向它的本地指针。

现在新材质载体只保留该材质实际引用的本地资源闭包，把同包 Texture 与材质一起封装，
维持正确的本地对象引用。外部 Shader/Texture 继续保留原生依赖。
Texture 的 `archive:/...` 路径及流偏移重定位到实际输出载体；选中像素范围使用原文件视图，
不提前复制整张贴图，不打开未命中的 Mod 资源，不增加任何游戏资源加载请求。

## 验证

- 真实 VFS、正式 `sw` 配置：来源解析、Material 构建、Renderer 绑定及依赖图发布通过。
- 真实输出载体：仅保留被引用的一个内嵌 Texture；本地对象引用完整，流路径正确。
- 源贴图与输出贴图的首部、中部、尾部像素片段一致。
- 错误路径、子资源标识或类型均被拒绝。
- 同名不同 PFB、重复 preload 指针、非法区间隔离回归通过。
- 默认原生随机读取套件新增材质闭包回归：1 MiB 源像素中只引用 11 字节；准备不读取整块像素，
  输出仅携带选中 Texture，未引用的同名 Texture 不进入载体。
- 完整 F10/世界/UI/剧情 Clone 模拟回归通过。
- 生产 DLL 校验及使用游戏目录运行库的隔离 attach、启动日志、导出控制锁测试通过。

真实回放入口为 `full_resource_reload_smoke.exe --material-vfs`；依赖清单由该 EFFSOURCE 的
closure 生成，仅作为离线夹具，清单寻址的托管 API 被替身化。实际 Bundle 内容、载体构建和
依赖图提交使用生产代码。测试不等于游戏内 GPU、材质显示或详情页画面已通过。

## 部署与实机验收

候选 DLL：3,038,208 B，SHA256：

```text
3D5CEB914DDB502FB820900C275B0D09252FC64E55828687B06DC0BA116981D3
```

当前游戏 PID 36816 仍加载上一轮 DLL；F10 不会更新进程内 DLL 代码。游戏退出后再部署。
本轮不需要重新导出 `sw`，
也不需要更新 Blender。仍需启动游戏打开同一武器详情页，确认实际画面并检查
`[DEMAND-SHARED-LOGICAL-DEPENDENCY] containerPreload=1` 和 `[DEMAND-SHARED-GROUP]`，
排除新的 `[DEMAND-SHARED-GAP]`。

上一轮修复记录见 [剧情控制与武器路由](story-controls-weapon-routing-20261105.md)。
