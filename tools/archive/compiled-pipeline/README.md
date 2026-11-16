# 历史 compiled-pipeline 调查工具

2026-11-12 从 `tools/diagnostics/` 归档。当前构建、部署和发布回归没有调用以下五组工具；保留旧调查的源码与手工 builder，不再把它们作为生产入口。

| 工具 | 历史用途 |
| --- | --- |
| `prepare_loading_generation.cpp` / `build_loading_generation.cmd` | 旧离线加载图，显式不提交生产表 |
| `benchmark_native_cold_compile.py` | 旧 `compiled.bin` 冷编译对比 |
| `inspect_offline_resource_package.cpp` / builder | 旧离线缓存检查与回放 |
| `afterimage_shape_control.cpp` / builder | 残影调查的形态键清除对照 |
| `compile_consumer_delta.cpp` / builder | 单消费者离线追加证明 |

移动后 `.cmd` 从三级父目录定位仓库，C++ 相对 include 指向当前 `src/`。保存源码不保证旧实验夹具与当前协议兼容；这些手工工具不会由部署脚本运行。历史方案见 [旧适配记录](../../../docs/archive/compiled-pipeline/game-update-prefab-adaptation-plan.md)。

仍在用的 `compiled_mod_set_smoke`、`inspect_compiled_resource_payloads`、旧格式迁移器和被当前诊断 import 的 `audit_deployed_skinning.py` 保留原处。未移动生产 `src/` 文件、release worktree、用户 Mod、Blender 工程或捕获数据。
