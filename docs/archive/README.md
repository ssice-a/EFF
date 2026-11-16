# 历史文档归档

本目录保留旧设计、故障证据和迁移背景，不作为新开发/导出的要求。当前入口是 [文档索引](../README.md)、[ADR-0001](../adr/0001-authoritative-mod-directory.md)、[ADR-0002](../adr/0002-runtime-resource-files.md) 和 [实现状态](../format3-transition-status.md)。

## 已替代合约与方案

`compiled-pipeline/`：

- [离线资源包](compiled-pipeline/offline-resource-package-format.md)、[Format 2 INI](compiled-pipeline/mod-ini-resource-format-v2.md)、[编译资源图](compiled-pipeline/compiled-resource-graph.md)。
- [2026-10-15 更新流程](compiled-pipeline/update-workflows-20261015.md)、[旧 Manager 设计](compiled-pipeline/eff-manager-design.md)、[独立修复工具设计](compiled-pipeline/eff-repair-tool-design.md)。
- [Prefab 动态适配旧方案](compiled-pipeline/prefab-runtime-adaptation-plan.md)、[游戏更新自动适配旧方案](compiled-pipeline/game-update-prefab-adaptation-plan.md)。

`effects/`：[已移除的作者试播/命名管道流程](effects/effects-runtime-debug.md)。

## 完整历史快照

- [旧 CONTEXT 工作日志](snapshots/context-before-20261112.md)。
- [旧代码架构](snapshots/code-architecture-before-20261112.md)、[旧三仓库边界](snapshots/repository-architecture-before-20261112.md)、[旧源码导航](snapshots/source-map-before-20261112.md)。
- [Format 3 迁移中间态](snapshots/format3-transition-status-before-20261112.md)、[旧 scoped export](snapshots/blender-scoped-resource-export-before-20261112.md)。
- [游戏 F10 历史记录](testing/hot-reload-testing-through-20261105.md)。

2026-11-12 整理只移动已替代的路线和独立调查工具，保留内容及相对链接。仍有构建/迁移/测试调用者的旧命名代码留在原位；对应 [工具归档说明](../../tools/archive/compiled-pipeline/README.md) 列出审计边界。未完成的研究不因归档而被标为解决；日期诊断和用户数据不批量移动/删除。
