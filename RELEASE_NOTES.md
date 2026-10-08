# EFF v1.3.3

配套：[EFF Blender v0.40.2](https://github.com/ssice-a/EFF-blender/releases/tag/v0.40.2)、[AnimeStudio v1.3.1](https://github.com/ssice-a/AnimeStudio/releases/tag/v1.3.1)。制作流程见[简易教程](https://github.com/ssice-a/EFF/blob/v1.3.3/Mod%20Tutorial.md)。

- 首次加载和 F10 使用一致的编译规则，补齐借用材质与新增资源的依赖映射。
- 世界模型重载恢复游戏配置的待机及特殊冲刺资源预加载，保留动画、武器、音频和未替换附件的生命周期。
- Ins 使用半透明面板，独立勾选每个 Mod 的按键开关；默认全部启用，同键可同时控制多个 Mod。开关与 mod.ini 的 key_switch_enabled 同步。
- 改善共享资源的独立 Mod 合并和来源注册；继续支持静态网格、多个作者部件、形态控制与分资源导出。

只有一个 DLL 安装包：EFF_v1.3.3_dll.zip。根目录仅 d3dcompiler_47.dll、vulkan-1.dll；plugin 内是 eff.dll、默认配置、数据结构协议、许可及空 mods 目录。包内没有作者编译器、AnimeStudio 运行库、日志、调试符号或游戏资源。

退出游戏后解压到 Endfield.exe 所在目录，更新时保留自己的 eff.ini、mods 和 state.ini。既有 Mod 继续携带 compiled.bin；需要新制作或更新资源时安装配套 Blender 插件。F10 更新 Mod 数据，DLL 文件更新需要重启游戏。

验证：连续世界/UI 重载及回滚回归；两角色最新实机 F10 请求到提交约 107 ms，待机附件正常。这是已准备候选的样本，不代表所有 NPC、场景物件或二三十个 Mod 的压力验证。全 LOD 导出不自动减面，Physics 尚未接通完整游戏模拟链。
