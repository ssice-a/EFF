# EFF v1.3.4

配套：[EFF Blender v0.40.3](https://github.com/ssice-a/EFF-blender/releases/tag/v0.40.3)、[AnimeStudio v1.3.1](https://github.com/ssice-a/AnimeStudio/releases/tag/v1.3.1)。简易教程：[中文](https://github.com/ssice-a/EFF/blob/v1.3.4/Mod%20Tutorial.md) · [English](https://github.com/ssice-a/EFF/blob/v1.3.4/Mod%20Tutorial_EN.md)。

- 正式 DLL 排除临时动画、骨骼、材质和资源卸载探针，不抓取原包，不进行持续采样或创建会话诊断目录；保留单份 `plugin/eff_log.txt` 的加载及 F10 结果。
- 保留世界/UI 模型重载、动画恢复、武器及音频绑定，以及游戏配置的待机附件资源预加载与失败回滚。
- 未修改的共享依赖复用游戏原资源身份，减少重复资源准备。多 Mod、按键与形态控制继续使用现有规则。
- 新增配图英文简易教程，并同步 Blender 导出性能优化。

仅一个安装包 `EFF_v1.3.4_dll.zip`：根目录为 `d3dcompiler_47.dll`、`vulkan-1.dll`；`plugin/` 内为 `eff.dll`、默认配置、数据结构协议、许可和空 `mods/`。不包含作者编译器、AnimeStudio 运行库、日志、PDB 或游戏资产。

退出游戏后解压到 `Endfield.exe` 所在目录，保留自己的配置和 Mods。F10 更新 Mod 数据；DLL 更新需要重启。现有完整 Mod 无需因诊断清理重新导出。

验证：正式二进制诊断隔离检查、完整世界/UI 事务模拟、待机资源预加载与回滚、游戏 UI 页面绑定回归通过。此轮未新增实机画面验收。

## English

- The production DLL excludes temporary animation, skinning, material and resource-unload probes. It performs no bundle capture or continuous sampling and creates no per-session diagnostic folders. Essential loading and F10 results remain in `plugin/eff_log.txt`.
- World/UI reload, animation resume, weapon/audio rebinding, game-owned idle attachment preloads and rollback remain intact.
- Unmodified shared dependencies reuse original game resource identities, reducing duplicate preparation.
- An illustrated English quick tutorial is now available. The paired Blender release improves export compilation and supports independent material/texture variants.

There is one DLL archive containing the two proxy DLLs and the `plugin/` installation files. Quit the game before updating; preserve your configuration and Mods. Existing complete Mods do not need re-exporting for this diagnostic cleanup.

Validation includes production-binary isolation, mocked world/UI transactions, idle preload/rollback and game UI page binding. No new in-game visual acceptance is claimed for this cleanup.
