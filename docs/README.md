# EFF 文档索引

先使用当前契约和操作入口。日期报告、`diagnostics/` 与 `archive/` 保存对应构建的证据，不自动代表最新版本或完整实机验收。

`diagnostics/`、`bin/` 和本机安装回执保留在开发机，不纳入源码提交；日期报告中标注“本机”的路径用于本地复核，不是 GitHub 中的附件。

## 使用与作者操作

- [中文教程](../Mod%20Tutorial.md) / [English tutorial](../Mod%20Tutorial_EN.md)。
- [共享工具与索引](shared-tools-and-indexes.md)：四端职责、选择资源根及共享工具。
- [材质与动态特效](../tools/Blender/docs/material-effects.md)、[统一动画](../tools/Blender/docs/shape-animation.md)、[特效作者操作](../tools/Blender/docs/effects-editor.md)。
- [原生结构特效面板与 lino 模板](effect-native-structure-20261112.md)：真实 GameObject/组件/Module 和引用字段、验证范围。
- [特效游戏资源依赖](effect-game-dependencies-20261114.md)：逻辑 Prefab 加载、游戏依赖租约与跨版本边界。
- [跨来源特效材质导出修复](effect-export-borrowed-material-20261115.md)：修改游戏材质后自动生成作者材质，保留游戏依赖引用和导回编辑。
- [特效冷导出与逻辑引用](effect-export-performance-20261115.md)：移除导出索引扫描，游戏资源及来源 Prefab 按逻辑路径声明，运行时绑定。
- [武器粒子轴向、方框与 Mesh 自旋](sword-particle-axis-and-mesh-spin-20261114.md)：当前测试场景、粒子参数入口、全部 LOD 轴向与实际 Mod 往返验证。
- [武器目标重定向](weapon-retargeting.md)、[全量与选中导出](blender-scoped-resource-export.md)。

## 当前契约与架构

- [领域模型](../CONTEXT.md)、[四仓库边界](repository-architecture.md)、[代码导航](code-architecture.md)。
- [ADR-0001](adr/0001-authoritative-mod-directory.md)：Mod 目录权威与按需资源。
- [ADR-0002](adr/0002-runtime-resource-files.md)：Format 3 文件、DDS-only 与兼容读取。
- [INI 装配](format3-ini-assembly.md)、[通用 INI 控制程序](ini-control-program.md)。
- [实现状态与未验收边界](format3-transition-status.md)、[近期架构修复](architecture-repairs-20261112.md)。
- [DLL / Blender 架构审查](architecture-audit-20261115.md)：F10 控制与动画、作者资源复制/往返、缓存权威和预览失效的当前问题及复现。
- [面板兼容、Issue 7 与资源架构审查](architecture-review-20261116.md)：AMD/Proton 的证据范围、图形失败退出、多目标重复读取、材质多 Action 与 INI 新增 DDS 往返的实证问题。
- [对应逐项修复与本机部署](architecture-repairs-20261116.md)：面板兼容后端和故障恢复、Issue 7 单请求检查、稳定 Action 来源、当前 INI DDS 权威；附回归证据与实机验收边界。
- [统一原生 AnimationClip 验证](native-animation-clip-validation.md)：Transform、形态键、材质同片段五点实机求值已通过；记录证据与尚未完成的生产装配/控制迁移。
- [作者 Empty 特效挂点](author-effect-mounts.md)：挂点选择、自动依赖、统一动画组、原生 Clip 和生命周期；真实特效包离线往返已通过，新增挂点实机验收待完成。
- [动画装配调研](animation-assembly-design.md)、[特效往返调研](effect-roundtrip-architecture-review-20261107.md)：保留当时问题，当前状态优先查上列契约/验证。

## 更新、部署与测试

- [固定本机部署与发布](release-management.md)：四仓库 main、本地优先、摘要回执。
- [Mod Manager / Blender 软件更新诊断](software-update-audit-20261112.md)：检查、下载、退出后安装与网络限流。
- [游戏 F10 热重载](hot-reload-testing.md)：与软件升级区分。
- [性能准备与归因边界](format3-input-architecture-review.md)、[场景测试矩阵](performance-scene-test.md)。
- [AnimeStudio VFS/索引](unpacker-vfs-layers.md)。

## 历史资料

[归档索引](archive/README.md) 收录已替代的 Format 2/compiled-pipeline 合约、旧调试通道和整理前快照。[Release 历史](releases/) 只说明当时发布；旧成功记录不替代本机当前验证。
