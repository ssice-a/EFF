# 特效封包加载失败（2026-10-20）

23:27 会话 PID 6052 已通过精确 Part / 骨骼绑定。23:28 的首次准备中，3 个原生 Bundle 请求全部返回空，Player.log 同时记录 `Unable to read header from archive file` 和 `Failed to read data for the AssetBundle 'IO.Stream'`，尚未进入 READY / PLAY。后续的“namespace reused with incompatible content”是失败代际不能重试造成的次生错误。

当前 lx 花瓣包内的 3 个输入是通用 UnityFS v7，摘要和目录完整。游戏普通 Mod 使用 Endfield 原生 VFS 封包。直接将实际输入交给独立的 Endfield `VFSFile` 读取器会报 `Not a VFS file / VFS version mismatch`；之前仅使用通用 `BundleFile` 往返，未覆盖游戏实际封包要求。

修正：

- AnimeStudio `EiemEffectNativeArchive` 离线输出游戏原生封包，保留全部序列化成员和 resS 数据，并使用 VFSFile 独立读回。通用 UnityFS 只用于离线中间产物。
- 原生运行时在提交请求前校验封包；旧作者输入可以读取以完成迁移，但不会传给游戏加载器，也不在帧循环转换封包。
- 内容命名空间纳入新封包语义，构建缓存随实际编译器摘要失效。Blender 使用中的 Debug 编译器、发行目录 bin、作者更新工具和 Python 校验器配套更新。
- F9 可重试相同内容的失败代际。先等待已提交请求完成并清理已证实归属的容器，再重新准备；等待期间保留上一可见实例。内容冲突和未知容器归属仍拒绝。

验证记录位于 `bin/effect-bundle-fix-20261020`：

- `native-before.log`：实际旧包被游戏格式读取器拒绝。
- `native-crosscheck.log`：实际 3 包改用原生封包后，C# / 既有生产 C++ 写出结果逐字节一致；全部原始成员与贴图流逐字节保留。
- `source-regression.log`：实际花瓣来源编辑、确定性重建、未修改输入、失败候选拒绝及原生根检查，56 项断言通过。
- `runtime-before.log` / `runtime-final.log`：首次 Bundle 失败且另一个请求未完成时，F9 原先无法恢复；修正后等待完成再重试通过。错误格式在原生提交前拒绝，原有多实例、可见性、层、独立回收和材质回归通过。
- `author-update-check.log`：仅替换现有特效封包及控制索引，32 份人物加载输入和原生重放结果保持一致。
- `python-tests.log`：使用新原生构建运行作者编码、追加、独立发布和损坏拒绝，共 5 项测试通过。
- `deployment.json`：安装与备份记录。仅修改游戏 DLL、lx 特效资源和 compiled.bin；玩家参数、挂点、人物资源、配置、其他 Mod 保持。

源码/离线回归/安装通过不代表实际画面已验证。安装后仍需新游戏会话核对 BIND → READY → PLAY，以及世界 / NPC / UI 各自的实际显示。未确认之前不标记 `game_visual_verified`。
