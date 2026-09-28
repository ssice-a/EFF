# DLL 代码架构

## 三端职责

AnimeStudio 负责游戏资源解包和初始 EIEM 导出；Blender 负责编辑、校验及再次导出；DLL 负责解析 Mod、匹配游戏实例并在原生装配完成后提交资源。当前资源契约为 EIEMESH v6、EIESKEL v2 和作者 EIEPHYS v5。旧 Mesh 必须重新导出。

## 运行时边界

1. `il2cpp_trace.h` 提供 IL2CPP 与 Hook 入口；世界、UI、NPC owner 都进入同一套模型注册流程。
2. `eiem_model_registry.h` 保存模型实例和生命周期；F10 只重建当前实例代际。
3. `eiem_mod_document.h`、`eiem_mods.h` 解析 INI 与资源声明；`eiem_mod_reconcile.h` 管理恢复、重载和重新应用。
4. `eiem_render_executor.h` 在写入 Renderer 前准备 Mesh、材质、形态键、submesh 和 Skeleton 依赖，失败时保留源状态。
5. `eiem_skin_resolver.h` 为当前 Renderer 根建立结构索引表，并按 `boneIndexPaths` 生成完整 `bones[]`。它不读取源 Mesh 槽候选，不跨实例借用 Transform，也不按 LOD 局部序号猜测。
6. `eiem_skeleton_runtime.h` 管理 EIESKEL 节点。没有原生供体的作者骨骼可以由 Skeleton 实例提供；物理求解器暂不在 DLL 内实现。
7. `eiem_resource_backend.h` 只接受完整的 canonical bone index table；源候选数据只作为作者侧 provenance 保留。

## 性能原则

模型事务内按根节点缓存一次层级表，随后每个 Mesh 槽只做哈希表查找。移除了全量 Renderer 供体扫描、LOD 供体排序和装配快照候选合并。缓存不跨角色实例、不跨 F10 代际，也不会因为源 Mesh 未变化而跳过用户要求的刷新。

## 验证

MSVC 单元测试和 Blender 后台导出测试验证格式、路径、Skeleton 新节点与失败回滚；它们不替代游戏内冷启动、F10、世界、UI、NPC 和长时间稳定性验收。