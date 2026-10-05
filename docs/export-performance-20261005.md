# 导出性能优化（2026-10-05）

当前 Blender 分资源导出已针对 Mesh、材质和贴图减少重复工作；共享 C++ 编译器的发布阶段也改为按输入受控并行。当前本地实测如下，Blender 部分不含后续离线编译器耗时：

| 类型 | 之前 | 现在 |
|---|---:|---:|
| Blender 仅贴图 | 约 11.1 秒 | 约 0.99 秒 |
| Blender 仅材质 | 约 9.9 秒 | 约 0.91 秒 |
| Blender 仅 Mesh | 约 10.8 秒 | 约 2.27 秒 |
| Blender 同一 Mesh 重复导出 | — | 约 1.16 秒 |
| C++ 命中几何缓存的完整构建 | 约 21.1 秒 | 约 12.4 秒 |
| C++ 冷构建 | 约 30 秒 | 约 22.4 秒 |

Blender 使用资源解码缓存、可信 staging 副本缓存、CNG SHA-256、一次材质分拆、字段直接比较与按 Mesh 内容/修订号失效的快照缓存。不会因为仅改贴图而重新计算 Mesh。

C++ 发布阶段在所有依赖和命名映射发现完成后，以最多 8 个 worker 完成各输入的命名替换、编码、完整解码一致性校验、hash 和写盘。结果依原始输入顺序合并；所有线程结束、错误检查通过后才写 `ready.tsv`，编译器仍在完整清单和作者一致性校验后原子更新 `current.tsv`。新版本已同步到 `tools/Blender/nativepack` 和游戏 `plugin/tools`。

真实 LZY 136 个 startup/隔离输入与优化前逐字节一致。监视器+单次导出并发、重复 watcher 排除、state.ini 变化忽略、损坏作者保留 current、关联父进程退出回归通过。证据为 `diagnostics/compiler-concurrency-20261005d/result.json`；Blender 分资源完整回归为 `diagnostics/blender-export-optimization-20261005/native-partial/verification.json`。

剩余主要耗时是冷构建的 Renderer 装配，以及每次全代际命名隔离。资源级持久缓存和更细的 Renderer 缓存可继续优化；当前仍保留完整 payload 与失败回滚。
