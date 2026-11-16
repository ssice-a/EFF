# 2026-11-15 特效资源装配审查

本轮已按下述审查结果实施资源装配修正。新导出将作者 Mesh、Material、DDS 声明为普通公共资源，特效逻辑只引用资源；游戏对象按当前逻辑路径解析，不复制为作者资源。原生 Prefab/AssetBundle 仍用于保存游戏所需的特效逻辑。

## 已实施的装配规则

- `[AttachEffect…] effect=ResourceEffect…` 表示挂载到命中的 Mesh、Part 或作者 Empty；`[ReplaceEffect…]` 按 `target.path` 替换游戏 Prefab 的全部调用。同一特效资源可被多个关系引用，用途不写在资源里。旧 `EffectAttachment`/`mode` 继续兼容读取。
- 作者 Mesh 使用标准独立 Mesh 资源，Material 使用普通稀疏 EFFMAT，作者像素使用普通 DDS；游戏贴图可只声明 `source.path` / `source.asset`。它们接入同一公共资源图、代际、依赖租约和退休规则。
- CAB、PathID 和重定位位置保存在特效资源的原生元数据中。公开 INI、资源名、挂点路径使用可读名称；重名增加数字后缀。内部完整性校验不作为作者命名或装配语法。
- 已导入和新指定的游戏资源使用相同的逻辑资源重定位；运行逻辑按 Prefab 可达闭包裁剪对象、流与 Bundle 目录。独立作者 Mesh 不带来源 Bundle 的其他对象。
- 导回时以当前 INI 指定的独立 DDS、Material 和 Mesh 为准，覆盖旧作者快照中的对应内容。已有材质来源在旧工程恢复时迁移一次，正常导出不重新加载已导入的材质或查询游戏索引。
- 作者归档仅保守裁剪可证明无用的独立 MonoScript 底稿，保留模板、目录对象、隐式脚本引用与不透明文件，不把离线归档误当成游戏常驻资源。

实现入口：`tools/Blender/eiem_effect_author_resources.py`、`tools/nativepack/effect_assembly.py`、`src/eiem_author_controls_ini.h`、`src/eiem_demand_author_resources.h`、`src/eiem_effect_relocation.h` 和 AnimeStudio 的 `EiemEffectAuthorMeshes` / `EiemEffectRuntimeClosure`。

验证和部署结果见文末。下面保留最初的只读审查证据，描述的是修正前的实现，并非当前状态。原始审查依据游戏 `plugin/mods/lizi`；本轮真实导出使用隔离诊断目录，不覆盖用户测试 Mod。

## 实际链路

| 内容 | 当前保存/运行方式 | 应遵循的装配方式 |
| --- | --- | --- |
| ParticleSystem/Renderer/Transform 与原生脚本 | 作者修改后编译为特效 Bundle，INI 声明 EffectAttachment 与 Bundle | 保留原生表现与游戏生命周期，作为特效逻辑资源 |
| 游戏 Material/Texture/Mesh/Shader | 部分是逻辑路径依赖，部分仍通过来源 CAB/PPtr 保留 | 统一以当前游戏逻辑身份解析，由游戏持有资源 |
| 作者 Material/Texture/Mesh | `assets[].payloadBase64` 注入特效来源 SerializedFile，整体写为 `ResourceEffect` | 使用现有 Material、Texture DDS、Mesh 资源 ID；特效绑定引用同一份资源 |
| 原生重定位信息 | 一部分写进 INI 的 CAB/PathID/字节偏移 | 保存在原生资源内部，INI 只记录资源与逻辑装配 |
| Blender 往返数据 | 整体来源归档与 workspace/edit | 保留足够往返的可达作者数据，预览/来源冗余不作为运行资源 |

## 发现

### P1 · 修改贴图时夹带原游戏对象与流

`lizi/effects/0.effres` 含两个 Texture2D：原图和作者副本；原图 `.resS` 的 21,872 字节与来源完全相同，SHA256 首尾为 `842B1BBD…5ECB338`。`effects/1.effres` 中同样保留原对象与作者对象：各两个 Material、ParticleSystem、Renderer。多出对象不等于游戏会全部实例化，不能据此直接认定性能问题，但它们确实构成冗余载荷。

原因：[`EiemEffectAssetEdits.cs:111`](../tools/AnimeStudio/AnimeStudio.GUI/EiemEffectAssetEdits.cs:111) 保留来源全部对象后追加作者对象；[`EiemEffectRuntimeWriter.cs:80`](../tools/AnimeStudio/AnimeStudio.GUI/EiemEffectRuntimeWriter.cs:80) 按整个来源成员合成；其[流处理](../tools/AnimeStudio/AnimeStudio.GUI/EiemEffectRuntimeWriter.cs:168)继续保留关联原始流。当前剪贴板/作者新增资源的可达性裁剪没有覆盖这里的原始成员。

修复需要按运行 Prefab 根的实际 PPtr 闭包裁剪 SerializedFile 对象与流，重写关联引用和布局；不能仅删磁盘文件或未经重编码就删成员。

### P1 · 作者资源不具备通用资源 ID

[`NativeAssets.add`](../tools/Blender/eiem_effect_assets.py:475) 将 Mesh/Material/Texture 编成特效专用新增资产，经 [`EiemEffectAssetEdits`](../tools/AnimeStudio/AnimeStudio.GUI/EiemEffectAssetEdits.cs:26) 注入模板 CAB。最终 [`author_format3.py`](../tools/nativepack/author_format3.py:1464) 声明整个 Bundle，没有让 Renderer 引用既有 `[Mesh…]`、`[Material…]`、`[Texture…]` ID 的消费链。

因此，相同作者材质被多个特效或普通 Mesh 使用时，当前可能重复合成。新材质适配器复用了 Blender 数据与字段写入器，但其输出仍是上述专用资产，不能称为运行装配已统一。

### P1 · 已导入游戏引用未统一运行时重定位

[`EiemEffectGameReferences.cs:43`](../tools/AnimeStudio/AnimeStudio.GUI/EiemEffectGameReferences.cs:43) 为未导入的新逻辑路径生成运行重定位；[已有导入身份分支](../tools/AnimeStudio/AnimeStudio.GUI/EiemEffectGameReferences.cs:73)保留来源 CAB/PathID。运行时 [`GameLeases`](../src/eiem_effect_debug.h:95) 按逻辑路径取得当前游戏依赖，但没有对应 patch 的旧 PPtr 不会自动随之改变。

这不证明当前版本资源已经失效，却证明不能据“面板显示逻辑路径”承诺跨游戏更新有效。所有游戏依赖必须走相同的当前逻辑资源绑定；旧原生身份只作离线来源证明。

### P2 · INI 泄露二进制布局

[`author_format3.py:1178`](../tools/nativepack/author_format3.py:1178) 输出 `game.N.relocation_cab`、`relocation_id` 和 `patch.N.bundle/offset/kind`；[`eiem_author_controls_ini.h:142`](../src/eiem_author_controls_ini.h:142) 直接消费。这违反 [ADR-0002](adr/0002-runtime-resource-files.md) 第 2 条：INI 不应包含 CAB、PathID 或文件偏移。

应将这些信息迁回原生特效资源的自描述元数据，并兼容旧包；INI 声明游戏逻辑依赖或作者资源 ID。不能在仍依赖旧位置的读取器上只改写出端。

### P2 · 独立特效 Mod 的额外资源禁令

[`author_format3.py:2211`](../tools/nativepack/author_format3.py:2211) 禁止 `type=effect` Mod 声明 Mesh、Material、Texture 等资源。这不是原生特效要求，而是专用消费链形成的实现限制。应在补齐通用资源绑定后移除；现在单独删除会让声明通过但不被特效消费。

### P2 · 往返归档含无关来源数据

[`_effect_source_archive`](../tools/Blender/eiem_format.py:699) 归档来源目录；[`author_projection`](../tools/Blender/eiem_effect_source.py:20) 主要排除终端游戏 Shader 程序。实包作者资源约 1.00 MB，37 个文件解压约 2.06 MB，一个来源文件包含 1083 个 MonoScript，当前目录对象只用到两个。

这些是 Blender 作者离线数据，不能与游戏按需读入的运行 Bundle 混为一谈；但会增加导出、复制和往返成本。裁剪需要保留类型模板与实际编辑闭包，不能用全来源归档长期替代作者资源集合。

## 修正顺序与验收

1. 先补原生运行根可达性与流裁剪：实际产物仅含可达作者对象/必要原生逻辑，不夹带已被替代的游戏对象。
2. 补统一 PPtr 绑定入口：依赖仅分为游戏逻辑资源或作者 Resource ID；复用普通 Mesh/Material/DDS 的读取、代际、共享和退休规则，不再增设一套贴图/材质加载器。
3. 迁移原生重定位元数据，统一新路径和已有导入身份的运行绑定；按命中在当前游戏资源清单中定位，不在 Blender 导出时查询游戏索引。
4. 接入 Renderer/Material/ShapeModule 等实际引用字段的 INI 装配、Blender 往返和特效资源导入，随后放开独立特效 Mod 的通用资源声明。
5. 裁剪作者归档；保留旧包兼容和原生 Prefab 表现，不以删掉全部 Bundle 作为验收标准。

验收至少包括：普通 Mesh 与两个发射器共享同一作者材质/贴图；只换 DDS 后 F10 生效；游戏贴图不进入作者载荷；源 CAB/PathID 改变而逻辑路径相同时仍绑定当前资源；单发射器和整根导出/导回；依赖移除和连续热重载后资源按真实 owner 退休。离线构建通过不能代替这些游戏行为验收。

## 本轮验证与本机部署

- Python 作者/导出/控制/资源/归档回归：243 项，1 项按既有规则跳过，其余通过。
- 真实 Blender 多材质、跨来源材质、无 UV Mesh 测试通过；材质来源已核对为实际 liino/lightspot/jsspsi，未误用通用结构模板的 Shader 来源。
- 游戏 EFFFX → 带动画 Empty 的独立 Mod → 可编辑 Blender 导回通过，公开挂点路径与原生 AnimationClip 绑定保持一致。
- 当前打开的刀工程中“基础粒子_柔光”已真实导出至 `diagnostics/effect-resource-assembly-20261115/live-scene-mod`：5.71 秒，其中作者数据整理 3.68 秒、原生构建 0.80 秒、校验 0.06 秒、发布 1.17 秒。未覆盖用户测试 Mod，未保存用户工程。该测量不代表导出性能优化已经完成。
- 这份当前场景产物的 Catalog 验证通过：4 个 Material、1 个 Mesh、7 张作者纹理、1 个原生逻辑 Bundle、20 个作者重定位位置；来源 Prefab 加载数为 0。对同一逻辑包分别绑定不同资源时，原生身份独立、Mesh 共享、VFS 发布与最终退休均通过。实际序列化输入结合模拟游戏接口验证，不能代替游戏实机。
- 外部改写当前产物的 DDS 和稀疏 Material 后导回，修改保留；再次导出在禁止重新读取材质来源的条件下通过。只供特效引用的独立 Mesh 导回后隐藏，不额外显示一份模型。
- 完整热重载模拟与特效生命周期回归通过；后者含 1,401 项生命周期断言及替换、挂载、LOD 检查。详见 `diagnostics/effect-resource-assembly-20261115/runtime-validation.md`。
- 实际 `lizi` 作者归档中的 MonoScript 从 1083 个保守裁剪至 2 个，压缩归档 257,621 → 178,114 字节；导回、应用编辑和重新编译通过。这只是离线归档体积，不能作为游戏内存或帧数收益。

固定本机部署已核验：

| 组件 | 安装结果 | 回执 |
| --- | --- | --- |
| Blender | 0.50.26，固定 Blender 5.2 插件目录，71 个文件逐项核验；依用户要求热重载，保留 6 个特效根、选择、工作区与已注册回调 | `bin/local-deployments/20261115-effect-assembly/Blender.json` |
| 生产 DLL | `D:\Hypergryph Launcher\games\Endfield Game\plugin\eff.dll`，3,689,472 字节，SHA256 `E747A7E3B52979BE22C9EA6DFCD7F2CF567B73FE2B60654AAB026258731D1DFC`；生产检查通过，无捕获探针 | `bin/local-deployments/20261115-152457-337/Dll.json` |
| AnimeStudio | 已重新构建并部署至 `E:\AnimeStudio-net10-1ccfbc16bf7fe625e8295bac8074ac3b1b9a065b`，包含独立 Mesh 逻辑路径小写修正 | `bin/local-deployments/20261115-152033-238/AnimeStudio.json` |
| 共享作者工具 | 已发布本机不可变工具代际并注册，供 Blender 使用 | `bin/local-deployments/20261115-effect-assembly/shared-author/SharedAuthor.json` |

回执 revision 标记工作区所在 `main` HEAD，实际部署内容由文件摘要记录；本轮仍为未提交修改，未推送或发布 Release。Manager 本轮未改动。

尚未完成：新版统一链路在真实游戏中的粒子画面、仅修改 DDS 后 F10 的即时更新、世界/UI 实例位置和长期内存退休验收。上述离线验证不证明这些实机行为已经正确。

## 2026-11-16：特效消失回归与来源容器修复

重新导出 lizi 后，01:40:09 的 EFFECT-ATTACHMENT-GAP 日志显示 m_fx_liino_kv_139.mat 不在公开逻辑资源入口，整组特效准备中止。世界与 UI 挂点已命中，失败发生在材质准备阶段。上一版外部化材质时遗漏了真实来源 Prefab，不能把有逻辑路径等同于有独立公开加载入口。

修复保留可读的 source.prefab，并由普通 Material 解析链路消费。星芒材质的真实来源是 assets/beyond/dynamicassets/gameplay/effects/prefabs/p_fxbat_liino_attack_05_star_hit_small.prefab，不是通用结构模板。运行时读取当前预加载闭包定位材质，不实例化来源 Prefab。旧场景仅在恢复时迁移一次，正常导出不重新读取 donor 或游戏索引。

- 实际游戏 VFS 字节回归通过：公开接口明确拒绝 Material 路径，仅接受真实 Prefab 路径；复现原错误后确认修复。来源 Prefab 加载、租约和场景查询均为零。完整资源热重载模拟与 73 项相关 Python 回归通过。
- 当前 Blender 工程已真实导出到 diagnostics/effect-resource-assembly-20261115/missing-material-20261116/live-mod；三个对应材质保留正确来源提示，未保存用户工程。
- Blender 0.50.27 已事务安装并按用户要求热更新。逐文件校验回执：bin/local-deployments/20261116-effect-material-container/Blender.json。
- 生产 DLL 于 02:02（Asia/Shanghai）部署到固定游戏 plugin/eff.dll，3,689,984 字节，SHA256 1FB491101E8D28B2FB2750E7E4F1DFE62E03E280A299DEE4E37C65A5C333D780。生产检查通过，无捕获探针。回执：bin/local-deployments/20261116-020213-757/Dll.json。
- 当前 mods/lizi/mod.ini 仅补充星芒材质来源，未覆盖网格、贴图、发射器参数或其他 Mod。原始 INI 和最小修复记录保存在上述诊断目录。

用户随后明确选择保留稀疏材质修改。普通 Material 保存来源与作者修改差异，运行时在资源准备阶段建立并缓存材质变体，不在每帧重新查找来源或应用参数；完整材质快照实验已撤回，未部署。游戏 Shader 和仅引用的游戏贴图仍归游戏管理。

真实游戏画面恢复尚待复测；离线回归不替代实机验收。本轮没有提交、推送或发布 Release。
