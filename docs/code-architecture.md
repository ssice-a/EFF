# EFF 当前代码架构

本文描述 2026-11-12 工作区的生产路径。历史 compiled-set、v35/v36、旧调试面板和迁移中间态保存在 [旧架构快照](archive/snapshots/code-architecture-before-20261112.md)；不按旧文档重新启用运行后端。

## 运行时边界

`src/eiem_resource_entry.cpp` 是生产 DLL 的入口翻译单元。`build.bat` 构建，固定本机部署由 [部署脚本](release-management.md) 完成。默认构建不包含临时测量和捕获探针；开发构建与游戏实机验收分开记录。

| 责任 | 主要源码 |
| --- | --- |
| INI、资源声明及校验 | `eiem_resource_directory.h`、`eiem_author_controls_ini.h`、`eiem_expression.h` |
| 按需作者载荷与只读缓存 | `eiem_demand_author_resources.h`、`eiem_demand_shared_graph.h` |
| 当前游戏逻辑身份解析 | `eiem_demand_logical_asset_lookup.h`、`eiem_demand_dependency_source.h` |
| Mesh/材质及原生输入准备 | `eiem_demand_mesh_preparation.h`、`eiem_demand_material_assembly.h`、`eiem_demand_material_build.h` |
| 游戏加载与代际刷新 | `eiem_entry_bundle_dependencies.h`、`eiem_entry_demand_generation_reload.h`、`eiem_entry_full_resource_reload.h` |
| 共用控制执行、偏好与 Ins | `eiem_control_program.h`、`eiem_entry_controls.h`、`eiem_entry_preferences.h`、`eiem_control_panel_win32.h` |
| 统一动画与样本租约 | `eiem_shape_animation_types.h`、`eiem_shape_animation_player.h`、`eiem_animation_samples.h` |
| 特效挂载和运行时材质动画 | `eiem_entry_effect_attachments.h`、`eiem_entry_material_animation.h` |
| 作者 ABI / 原生文件写出 | `tools/eff_author_assets.cpp`、`eiem_author_control_metadata.h` |

## 文件与实例所有权

`mod.ini` 装配 Replace、Part、Resource、控制和动画关系；资源 ID/相对路径标识作者资源。实际游戏请求决定准备哪些输入。未命中的资源不因软件启动或 F10 全量读入。

多个 Part/目标可复用一份作者载荷；目录副本拥有独立运行实例身份。材质替换是显式选择：省略 `material` 保留对应 PFB 的源材质。新 Texture 由 DDS 与 INI 来源/采样设置组成；旧 scaffold 仍兼容。

F10 与软件升级不同：F10 重新读取 Mod 声明、核对资源变化并刷新游戏 owner，不下载仓库更新。资源样本与缓存按活动 owner/读取者租约持有；资源代际与控制专用修改有不同迁移规则。实机失败恢复、跨场景与长期资源退休的覆盖范围以 [测试入口](hot-reload-testing.md) 为准。

## Blender 作者状态与特效预览

Blender 特效根上的原生编辑文档保存 GameObject、组件、ParticleSystem 模块和 PPtr；作者 Mesh、Material 与 Image 仍由各自 Blender 数据块拥有。`EffectAuthorSync.flush()` 是作者字段、Transform、材质、曲线和渐变投影进入原生编辑文档的统一入口，各类编辑器只负责各自表示的转换。

每个特效根的 `PreviewSession` 缓存已同步的编辑文档。依赖图更新负责标记作者对象/资源失效；稳定播放帧复用该会话，不扫描完整作者签名。发生失效时才核验签名、同步并更新预览。预览 Geometry Nodes 只显示模拟结果；作者 Mesh 的 Geometry Nodes 修改器通过评估后的 Mesh 快照烘焙导出，不序列化预览节点树。

## 保留代码与归档界限

旧格式 reader、迁移/作者工具、发布回归可能仍依赖 `compiled` 或 `offline` 命名的模块，不能按关键词批量删除。`compiled_mod_set_smoke` 仍由发布脚本构建；`inspect_payloads.exe` 仍服务贴图回归。无当前调用者的五组独立调查工具移至 [`tools/archive/compiled-pipeline`](../tools/archive/compiled-pipeline/README.md)。

当前四端关系见 [仓库边界](repository-architecture.md)，具体格式见 [INI 装配](format3-ini-assembly.md)，近期修复与未验收项见 [架构记录](architecture-repairs-20261112.md)。
