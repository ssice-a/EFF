# EFF v1.4.0

配套：[EFF Blender v0.41.0](https://github.com/ssice-a/EFF-blender/releases/tag/v0.41.0)、[AnimeStudio v1.4.0](https://github.com/ssice-a/AnimeStudio/releases/tag/v1.4.0)。教程：[中文](https://github.com/ssice-a/EFF/blob/v1.4.0/Mod%20Tutorial.md) · [English](https://github.com/ssice-a/EFF/blob/v1.4.0/Mod%20Tutorial_EN.md)。

- Blender 离线生成原生输入视图、依赖、装配与控制索引；DLL 读取预编译结果，组合共享修改并在游戏加载前应用。游戏端不运行作者编译器。
- F10 统一提交 mods 目录的增删改，保留世界/UI 重载、动画恢复、武器/音频及待机附件绑定和失败回滚。
- Ins 独立启用多个 Mod 的按键，默认状态、NPC 完成登记及玩家控制值持久化使用同一规则。
- 通用编译资源支持对象增改删、类型/脚本注册和引用重定位；新增 BoneCloth 链和球/胶囊碰撞体由作者端编译。

仅一个 `EFF_v1.4.0_dll.zip`：两个根目录代理 DLL 与 plugin 下的运行 DLL、默认配置、结构协议、许可及空 mods 目录。没有编译器、解包器运行库、PDB、日志或游戏资产。

更新：退出游戏后安装，保留配置、Mods 和 state.ini。**旧 compiled.bin 必须用配套 Blender 插件重新导出。** 数据兼容性按结构判断，不按产品版本号限制。当前代理支持 Windows 系统目录为 `C:\Windows` 的安装；其他目录尚未支持。

验证：生产二进制诊断隔离、连续八次世界重载、异步 UI、动画/武器/音频与回滚、多 Mod 组合、静态网格及控制持久化通过自动回归。四 Mod 编译集合独立读取实测约 0.99 秒，不含游戏加载；不是 F10 总耗时或磁盘冷读。**本轮新二进制尚未完成实机验收**；20–30 Mod 压力和完整新增物理生命周期仍待验证。

## English

The author tool now emits native input views, dependency, assembly and control indices offline. The runtime reads compiled outputs and combines shared edits without an author compiler. F10 commits the complete current Mods directory, preserving world/UI reload, animation, weapon/audio and idle attachment binding, and rollback. Independent key routing and persistent controls also cover registered NPC instances.

One DLL archive contains the two game-root proxy DLLs and plugin installation files. Quit the game before updating; preserve configuration and player state. **Re-export old compiled Mods using the paired Blender release.** The current proxy forwarders support Windows at `C:\Windows`.

Automated production, reload, UI, composition, static Mesh and control regressions passed. Reading the four-Mod set took about 0.99 seconds outside Unity; this is not total game loading or F10 latency. New in-game acceptance, large Mod-set stress and complete physics lifecycle validation remain outstanding.
