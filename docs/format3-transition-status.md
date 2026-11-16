# Format 3 实现状态

本页核对到 2026-11-12 的工作区与本机证据，不代替 [ADR-0002](adr/0002-runtime-resource-files.md)。[旧迁移中间态](archive/snapshots/format3-transition-status-before-20261112.md) 已归档；其中“未跑 Blender 往返”“Manager 迁移未实现”等不是当前状态。

| 端 | 已接入的正常路径 | 验证边界 |
| --- | --- | --- |
| DLL | Format 3 声明、逻辑目标、原生按需输入、DDS-only、INI 控制与统一动画；兼容旧 Texture scaffold/控制资源 | 回归覆盖与实机画面分开；跨场景、长时间资源退休和 GPU 成本需独立测量 |
| Blender | 直接导出/导入 Format 3；可视控件生成 INI；Mesh/材质/纹理/动画往返；原生结构特效编辑 | Blender 5.2 真实后台往返、实际安装包和本机摘要已有验证；任意跨图资源引用仍未完成 |
| Manager | 独立安装与启停；共享原生声明校验；Format 2→3 迁移及索引获取链路 | 迁移需兼容工具集；本地/手动/网络/本地构建索引不要求额外安装解包器；缺失作者资源不可凭空修复 |
| AnimeStudio | 当前游戏索引、来源目录和原生特效图导出 | 来源可解析不表示任意更新后的游戏原生结构都兼容 |

新 Texture 使用普通 DDS 加 INI 逻辑来源/采样参数，新控制直接写 INI；数值动画采样与原生特效包保持独立。旧包兼容读取不是新导出的要求，见 [INI 装配](format3-ini-assembly.md)。

## 证据入口

- [架构修复与四端验证/部署](architecture-repairs-20261112.md)。
- [原生特效面板、lino 模板及已安装插件验证](effect-native-structure-20261112.md)。
- [目标重定向及同资源多实例](weapon-retargeting.md)。
- [软件热更新调查](software-update-audit-20261112.md)；与游戏 [F10 测试](hot-reload-testing.md) 分开。
- [性能测试准备](format3-input-architecture-review.md)：使用固定场景/资源/构建；历史编译包大小不能代表运行时内存。

自动测试、源码提交、本机部署、远程发布和用户实机验收各有自己的证据。部署回执中的 HEAD 不能单独表达未提交改动，须同时核对构建与安装文件摘要。
