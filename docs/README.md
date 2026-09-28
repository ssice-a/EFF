# EFF 文档索引与当前状态


## 当前状态

| 能力 | 当前实现 | 验证边界 |
|---|---|---|
| 静态 Mesh 替换 | 按原 Mesh 身份命中；替换 Mesh 在游戏原生装配边界进入现有 Renderer；不创建 Partner | 大世界手动 50 次 F10 未躺地；最新 NPC 反馈正常；本轮候选仍待三端实机验收 |
| LOD 模板导出 | Blender 从已导入资源发现 LOD0-4；一份选中 Mesh 由各目标 LOD 的精确 Render 规则共同引用，缺失级别不生成规则 | 真实 Blender 5.0.1 回归通过；模板的权重、bindpose 与源骨骼槽不会被目标 LOD 的原生槽顺序重新解释 |
| 多 submesh / 多材质 | Blender 选择导出生成一个合并 Mesh，保留多个 submesh 和对应材质槽 | 当前 Typhoea 测试包已实机显示；材质和贴图随静态替换生效 |
| 三端实例 | 世界、角色 UI、NPC 共享资源规则，各自沿游戏原生 owner、装配和 LOD 生命周期运行 | 资源命中与实例隔离已验证；随机姿态仍需覆盖全部 owner 并确认最终装配边界 |
| 蒙皮槽位 | EFFMESH v6 逐槽记录“源 Mesh 身份 + 原始槽号候选”；每个模型实例复用游戏已装配的原生 Transform | 已覆盖世界、UI、NPC PFB 中同槽位骨骼名称不同的情况；完整 v6 映射不依赖角色名、骨骼名或层级路径 |
| 诊断 Hook | 无关高频探针默认关闭；插件创建的蒙皮 Mesh 四槽校验和写入在普通构建启用 | 普通候选待三端实机验收 |
| 按键显隐 | Mod 按键使用独立的最小窗口调度器；在同一 Mesh 对象上原地隐藏/恢复目标 submesh 索引，不进入 Partner/Physics 生命周期 | 世界、角色 UI、NPC 实机通过；Mesh、Renderer、骨骼与 LOD 身份保持不变 |
| 按住形态键 | `[Key] type=hold` 按独立采样节奏向目标值连续移动；`type=cycle` 仍是一按一切换 | DLL、INI 解析和 Blender 形态键导出已实现，见 [按住按键](hold-keys.md) |
| F10 热重载 | 新配置先完整解析为候选代际；解析失败时不恢复 Renderer、不发布新代际；现有实例使用统一执行器回放 | 历史版本有成功样本；当前候选仍需验证 UI、NPC、冷启动及手动 100 次 F10 |
| Skeleton / Physics / 碰撞体 | 保留已有格式和研究证据，生产链等待原生工厂与三端能力验证 | 属于后续重构阶段，不纳入当前静态替换验收 |

v1.0.0 是功能参照，不能替代当前 DLL 的稳定性证明。**历史姿态和 UI 闪退已有故障样本；最新 NPC 反馈正常，本轮候选仍需重新实机验收**。Skeleton、Physics 和碰撞体保留为后续阶段，不作为本问题的修复依据。

## 现行文档

| 主题 | 入口 | 职责 |
|---|---|---|
| 项目术语 | [CONTEXT](../CONTEXT.md) | 核心对象与跨模块约束 |
| 代码架构 | [DLL 代码模块](code-architecture.md) | 模块职责、依赖方向和分阶段整理顺序 |
| 架构审查 | [本轮问题与修正](architecture-review-20260926.md) | owner 集合、输入合并、蒙皮刷新及尚待处理的事务边界 |
| 仓库边界 | [三仓库架构](repository-architecture.md) | DLL、Blender、AnimeStudio 的所有权、协议和发布关系 |
| 本地项目管理 | [三仓库整理状态](project-management-20260926.md) | 工作目录、核对结果和发布候选提交；外部发布状态以 GitHub 为准 |
| 更新检查 | [三端 Release 检查](release-update-checks.md) | 各端版本、提示入口、忽略状态和发布约束 |
| 重构基线 | [资源替换重构](refactor-resource-replacement.md) | 目标架构、验证顺序、禁止事项和当前进度 |
| 原生装配链 | [模型创建链路](model-assembly-chain.md) | 世界、UI、NPC 的装配边界与蒙皮条件 |
| Blender LOD 导出 | [LOD 导出](blender-lod-export.md) | 从已导入资源发现 LOD；多个精确 Render 命中共享一份模板 Mesh 与切换状态 |
| 输入与配置 | [条件与按键](conditional-keys.md) | INI、表达式、状态与更新顺序 |
| Blender 网格工具 | [切换作者流程](blender-switches.md) | 选择、显隐、状态和导出 |
| Blender Mesh-only | [Mesh-only 导出](blender-mesh-only.md) | 只导出选中的 Mesh、材质和贴图，跳过骨架、物理及其他作者控制检查 |
| 顶点通道 | [顶点数据契约](vertex-data-contract.md) | 原生通道保留与缺失切线生成 |
| 蒙皮 | [共享骨架绑定](shared-skeleton-binding.md) | EFFMESH v6 源 Mesh/槽位候选、bind pose、实例隔离和回退边界 |
| 统一装配当前状态 | [候选与验收](unified-assembly-status-20260926.md) | 四槽实验、三端缺口、代码边界、候选 DLL 与验证门槛 |
| 调查归档 | [CPU 蒙皮调查](archive/cpu-skin-random-pose-current-20260925.md)、[GPU 蒙皮证据](archive/gpu-skinning-evidence.md) | 当时的证据和失败假设；当前状态以本页及装配状态为准 |
| 形态键 | [形态键控制](shape-controls.md) | 作者通道、实例权重与游戏通道归属 |
| 作者状态 | [默认值、持久化与材质](author-state-materials.md) | `.blend` 数据、`state.ini` 和材质导入 |
| Mod UI | [Lua UI](lua-ui.md) | 脚本 API、窗口和变量事务 |
| 相机 | [反虚化契约](camera-fade.md) | CameraMono 原评估后清理 |
| 物理设计 | [三端物理契约](physics-authoring-design.md) | 解包、Blender、DLL 的完整链路要求 |
| 物理文件 | [作者格式](physics-authoring-v5.md)、[源图格式](physics-authoring-v2.md) | 当前作者 v5 与独立的原生源图 v2 |
| 物理运行时边界 | [Physics 运行时重构](physics-runtime-redesign.md) | 所有权边界、禁止操作、P0-P5 取证与实施计划 |
| 工具运行 | [资源浏览器](../tools/README.md)、[Blender 插件](https://github.com/ssice-a/EIEM-blender/blob/main/README.md) | 启动、安装和开发操作 |

## 维护规则

1. “已实现”“构建通过”“已部署”“游戏内验收”是四种不同状态，文档必须明确区分。
2. 现行文档描述契约和当前能力，不追加每次 DLL 哈希、启动日志或失败尝试。
3. 身份、装配和高频资源探针默认关闭，只在有明确问题和退出条件的验证阶段临时启用。
4. 静态替换、按键、F10、Skeleton 和 Physics 分阶段验收；一个阶段通过不能替代另一阶段的实机结果。
