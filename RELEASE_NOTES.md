# EFF v1.4.1

配套：[EFF Blender v0.41.0](https://github.com/ssice-a/EFF-blender/releases/tag/v0.41.0)、[AnimeStudio v1.4.0](https://github.com/ssice-a/AnimeStudio/releases/tag/v1.4.0)。

- 正式运行 DLL、ImGui 和两个代理统一静态链接 C/C++ 运行实现，避免新版编译器生成的互斥锁与游戏附带旧 MSVCP140 冲突。
- 保留静态 CRT 所需的线程通知；生产二进制检查拒绝依赖宿主 C++ 运行库的产物，并增加真实 DLL 加载、启动日志和控制锁回归。
- Mod 数据结构、热重载、控制和三端职责不变。已经用 v0.41.0 导出的 Mod 无需重新导出；更旧的 compiled.bin 仍需使用配套插件重新导出。

仍只有一个 `EFF_v1.4.1_dll.zip`：两个根目录代理 DLL 和 plugin 的运行文件，无外置编译器、额外运行库、PDB 或游戏资产。退出游戏后安装，保留 eff.ini、Mods 和 state.ini。无需替换游戏自带的 MSVCP140/VCRUNTIME。代理当前仍支持 Windows 系统目录为 C:\Windows 的环境。

本地隔离加载已使用游戏实际的 14.29.30139 运行库复现 v1.4.0 的同址崩溃，修正版在同一条件及系统新版运行库下通过启动日志/控制锁检查；用户已确认本地修正版能正常进入场景。新增检查随正式发布运行。此项不代表所有场景、20–30 Mod 压力和新增物理生命周期已完成实机验收。

## English

The runtime, ImGui implementation and both proxies now statically link their C/C++ runtime, preventing a startup mutex crash when the host preloads an older app-local MSVCP140. Static CRT thread notifications remain enabled. Release validation rejects host CRT imports and loads the actual production DLL to verify startup logging and control locks.

The Mod structure is unchanged. Mods exported with Blender v0.41.0 do not need re-exporting. Quit the game before installing the single DLL archive and preserve configuration, Mods and player state. Do not replace the game’s MSVCP/VCRUNTIME files. The current proxies support Windows at C:\Windows.

The previous release reproduced the exact reported fault with the game’s runtime in isolation; the corrected local binary passes under that same runtime and a newer system runtime. The user also confirmed successful scene entry with the local startup fix. General in-game, large Mod-set and complete physics lifecycle acceptance are separate from this startup check.
