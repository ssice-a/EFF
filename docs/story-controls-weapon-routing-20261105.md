# 剧情控制同步与武器依赖路由修复

日期：2026-11-05。状态：代码与本机部署完成；22:42 用户武器页反馈基本通过，剧情控制仍待原场景验收。

22:42 更新：PID 30544 的 sw 网格、内嵌材质和请求完成链路通过，用户表示武器看起来已生效；本轮没有剧情 Clone 或 F10 事件，不能据此关闭剧情控制验收。详见 [本次日志核对](runtime-log-review-20261105.md)。

21:16 实机复测更新：四级武器目标登记已生效，但详情页仍未替换，进一步失败于 PFB
内嵌材质寻址；继续验证还发现同包 Texture 回指形成的依赖环。后续修复与验收状态见
[武器详情页内嵌材质诊断](weapon-detail-embedded-material-20261105.md)。本页的网格 4/4 结果不代表材质链路或详情页画面通过。

## 武器未替换的根因

现场文件位于 `bin/story-state-weapon-20261105/`。20:27 启动的 PID 32664 会话中，
目标声明共 192 个，仅解析到 188 个。`Player.log` 对武器四级 LOD 均报告
`Can not find Asset In RuntimeManifest`。本机 `sw` Mod 在 20:19 已导出，早于该会话，
因此这次不能归因于运行中换包但未按 F10。

目标仍是完整逻辑身份：

```text
path:  assets/beyond/arts/entity/wpn/sword/wpn_sword_0022/models/s_wpn_sword_0022_01.fbx
asset: S_wpn_sword_0022_01_lod0 / lod1 / lod2 / lod3
```

这些 Mesh 是 PFB 的依赖子资源，当前 RuntimeManifest 没有它们各自的公开资源入口。
旧实现查询 `path##asset` 失败便跳过登记，所以之后游戏实际加载它们所在的 Bundle 时，
没有相应替换路由。问题发生在登记阶段，并非按 Mesh 显示名搜索失败。

真实来源证据：

- EFF：`E:/EIEM_Workspace/assets/beyond/dynamicassets/gameplay/prefabs/weapons/wpn_sword_0022_refined.eff`。
- 武器 PFB Bundle：`bundles/windows/main/fc74df0a1ba9e422c7b26842.ab`。
- Mesh carrier Bundle：`bundles/windows/main/eed67697d738e28720e06058.ab`。
- 游戏 manifest 中 carrier 的实际 key：`F0FFD8EC6AA24E24`；保留高位，不自行截断或猜测哈希。

导出端现在从 EFFSOURCE 的完整资源身份解析 `sourceBundle`，写入可选的
`target.bundle` 和 `lod.N.bundle`。运行时用游戏已驻留的 Bundle 清单取得真实 key，
先登记路由；游戏实际请求该 carrier 后，再在真实输入中按完整 `path + asset` 校验并装配。
路由声明不读取 Mod Mesh/纹理载荷，不创建额外游戏加载请求。

同名不同路径的资源不会合并；来源对同一身份声明冲突 Bundle 时明确拒绝。
删除或修改目标的 F10 差分保留旧 carrier 路由，以便恢复原始输入。
未解析目标现在输出 `[DEMAND-TARGET-UNRESOLVED]`，不再静默消失。

## 剧情按键状态不同的根因

同一个 Mod 的变量原本已经共享。缺口在实例控制登记：剧情 Timeline 可直接 Clone
已加载的 Prefab，绕过世界/UI 模型完成回调，导致新实例没有继承当前控制状态。

现在仅记录实际命中且具有作者资源来源的 Prefab，使用弱引用：同步 Clone 返回后登记
真实 GameObject；异步 Clone 仅追踪实际返回的 operation，原生完成后通过公开
`get_Result` 取得实例并登记，不把 operation 当作 GameObject。
控制登记暂停时保留未消费的异步任务。原生隐藏状态仍与作者可见性共同生效。

该路径不扫描全场景、不预载所有 Mod、不持有额外 Bundle 租约，也不修改共享 Prefab
本身的 enabled。静态武器使用 MeshFilter 的来源 Mesh，不调用 SkinnedMeshRenderer 的骨骼接口。

## 验证范围

- 修前回归分别复现：依赖 Mesh 无登记路由；无世界/UI 回调的剧情 Clone 未继承开关。
- 修后覆盖静态/蒙皮实例的同步与异步 Clone、异步未完成、暂停登记、共享状态、原生隐藏及无关实例隔离。
- Bundle 路由回归覆盖真实高位 key、缺失公开资源入口、精确身份延迟校验及删除目标恢复。
- Runtime manifest、按需资源、静态装配、形态桥接、完整 F10/世界/UI 模拟回归通过。
- Blender author_format3：21 项；导出核心：57 项（1 项跳过）；本机安装测试集：112 项（1 项跳过）。
- 生产 DLL 构建、生产校验及使用游戏运行库的隔离加载通过。
- 发布准备流程增加 manifest 路由与导出路由测试门槛。

正式安装的 `sw` Mod 对真实武器 PFB 的离线装配结果：

```text
targets=4 parts=4 matches=4 changedObjects=9 addedObjects=0
readBytes=71357 inflatedBytes=131072 cachePeak=131072 prepareMs=0.9247
```

以上为真实资源装配与模拟生命周期验证，不代表武器详情页或剧情画面已实机验收。
异步 API 在本机 metadata 中存在；实际游戏启动后的绑定仍需新会话确认。

## 本机部署与回执

2026-11-05 21:04 部署 DLL 和 Blender，21:06 原子更新正式 `sw/mod.ini`。
部署前确认 Endfield 和 Blender 已退出。DLL 为 3,014,656 B，SHA256：

```text
4F8793F2FF7179354851B72F030C8B692C57ABCE7C723B220C9020D3728E96C8
```

回执目录：`bin/local-deployments/20261105-210407-585/`：

- `Dll.json`、`Blender.json`：安装位置及原文件备份。
- `WeaponBundleRoute.json`：正式 `sw` INI 的旧/新 SHA256、备份和三个资源载荷的摘要。
- `Verification.json`：DLL 与构建一致；Blender 构建清单内 56 个文件全部校验；四级武器路由和三个未改载荷复核通过。

Blender 的导出模块与随包 `nativepack/author_format3.py` 和当前源码一致。
`nativepack/ini_reader.py` 是打包脚本明确排除的旧运行时模块，缺席不代表安装遗漏。
工作树含未提交修改，部署回执的 revision 字段仅记录当时 HEAD，内容以实际文件摘要为准。
本轮未推送或发布 Release。

下一次实机复测：重新启动游戏，打开这把剑的武器详情页，再检查剧情和世界的按键切换同步；
核对 `[DEMAND-TARGET-BUNDLE-ROUTE]` 的 carrier key、`[CONTROL-RUNTIME-CLONE]`，
以及是否出现 `[DEMAND-TARGET-UNRESOLVED]` 或 `[CONTROL-RUNTIME-GAP]`。

前一轮剧情黑屏的读取修复及 20:26 部署是历史记录，见
[剧情黑屏诊断](story-black-screen-20261105.md)；当前本机 DLL 以本页 21:04 回执为准。
