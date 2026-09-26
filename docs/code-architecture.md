# DLL 代码架构

## 三端资源契约

AnimeStudio 负责游戏资源解包和初始 EIEM 导出；Blender 负责编辑、校验及再次导出；DLL 负责解析 Mod、匹配游戏实例和提交资源。当前仅接受 EIEMESH v6、EIESKEL v2、作者 EIEPHYS v5。原生 Physics 源图 v2 是单独的文档类型。旧版作者资源需要重新导出。

## 运行时边界

1. `il2cpp_trace.h` 提供 IL2CPP 和 Hook 入口；`eiem_world_ui_owner.h`、`eiem_npc_model_owner.h` 将世界、UI、NPC 的创建和释放转换为同一种模型 owner 事件。
2. `eiem_model_registry.h` 保存模型实例与 owner 集合，并为 F10 提供快照。最后一个 owner 释放后才退役实例。
3. `eiem_mod_document.h`、`eiem_mods.h` 解析 INI 和资源声明；`eiem_mod_reconcile.h` 负责重载事务。每次 F10 都重建资源代际，包括源模型未改变的情况。
4. `eiem_render_executor.h` 对当前模型根取 Renderer 快照、匹配 Render 规则并提交 Mesh、材质、贴图、骨骼、形态键和 submesh 显隐。`eiem_render_override.h` 保存原始状态，失败时恢复；恢复不完整时阻止新代际发布。
5. `eiem_assembly_binding.h` 与 `eiem_skin_resolver.h` 仅从当前实例中的原生 Mesh 供体解析骨骼槽。每个 v6 槽声明“源 Mesh 身份 + 原始 bones[] 槽号”候选；候选冲突、缺失或失效均拒绝绑定，不猜测名称或局部 LOD 下标。
6. `eiem_resource_backend.h` 创建资源并管理缓存。蒙皮 Mesh 使用 `InternalSetBoneWeights`，提交前验证 UnityPlayer 布局与生成 Mesh 的原生四槽字段。修正只作用于插件新建的蒙皮 Mesh，不修改游戏源 Mesh。

所有入口使用同一个 Render executor，不针对角色名写特例。`handling=skip` 跳过源 Renderer；导出的 Mesh 是替换资源。多部件以一个 Mesh 的多个 submesh 表达。按键状态按 Mod 管理，UI 选中的 Mod 接收按键输入；形态键可独立于按键由 UI 滑块控制。

Skeleton 和 Physics 作者资源仍未完成游戏内验收。`eiem_runtime_features.h` 中的实验 Physics 与广域诊断默认关闭；诊断不得成为生产决策依据。

## 验证边界

DLL 编译、MSVC 宿主测试、Blender 后台导出测试、AnimeStudio 编译分别验证代码和文件契约。它们不证明游戏内稳定性。部署后仍需分别在大世界、UI、NPC 和冷启动场景检验材质、姿态、显隐与手动 F10；稳定性目标是连续手动至少 100 次 F10 且无躺地或闪退。
