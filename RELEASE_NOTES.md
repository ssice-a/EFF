# EFF v1.7.0

配套：[EFF Blender v0.44.0](https://github.com/ssice-a/EFF-blender/releases/tag/v0.44.0)、[AnimeStudio v1.6.0](https://github.com/ssice-a/AnimeStudio/releases/tag/v1.6.0)。

- 适配本次游戏更新后的模型池释放接口，保留游戏资源持有关系。修复旧 Mod 的更新流程可重新定位当前来源、退休旧来源并补全当前命中的 PFB。
- 新增独立 **EFF Repair v0.1.0**：自动识别或选择游戏及 Mod 目录，检查工具/索引更新，在软件内下载完整索引并在游戏外批量修复旧 Mod。修复保留作者资源、玩家设置和原包备份；相同内容再次修复会跳过。
- 提供当前游戏完整公共索引。按实际 Bundle 内容匹配，可跨安装目录和文件时间导入，不要求玩家自行解包或拥有旧索引。
- 作者可使用新版 AnimeStudio 更新原解包资源，再从原 Blender 工程重新导出，无需经过修复工具。DLL 不后台发现新 PFB、不补编译或自动回写 Mod；手动 F10 和已有作者控制保持。

下载选择：

- `EFF_v1.7.0_dll.zip`：退出游戏后解压到游戏根目录，保留自己的 `plugin/eff.ini`、Mods 和 `state.ini`。使用静态 C/C++ 运行实现；当前代理支持 Windows 系统目录为 `C:\Windows`。
- `EFF-Repair-v0.1.0-win-x64.zip`：独立修复工具，需要 .NET 9 Windows Desktop Runtime。解压到独立目录运行，选择游戏和 Mod，下载对应索引后退出游戏并修复。
- `EFF-Endfield-Index-20261015.effindex.gz`：约 97 MB 的完整索引，由修复工具自动下载，也可手动导入。不含游戏 Mesh/贴图或作者 Mod；解压后的索引约 831 MB。
- `eff-index-catalog.json`：供工具检查更新的发布目录，无需用户编辑。

本轮验证包含新旧模型池签名、静态 CRT 的真实 DLL 隔离加载、离线来源和编译规则回归、公共索引跨路径/时间导入、旧庄方宜工程隔离导出，以及旧 ZFY Mod 的 45 输入/21 根编译与生产读取。实际游戏新会话已确认五个 Mod 进入资源加载链；全部角色画面、骨架实质变化及连续 F10 仍需独立验收。索引无法代替不受支持的游戏接口/资源结构适配。

## English

Updates the model-pool binding for the current game and adds EFF Repair v0.1.0, a standalone offline repair tool. It discovers or accepts game/Mod folders, checks releases, downloads the matching full content-verified index and recompiles old Mods with backups. Authors can instead refresh extracted sources in AnimeStudio and export their existing Blender projects again.

Install the DLL archive with the game closed and preserve configuration and Mods. The optional repair tool needs the .NET 9 Windows Desktop Runtime. The full index is a separate download and contains asset/dependency metadata, not game assets. The DLL performs no background update discovery or compilation. Automated and isolated real-resource checks passed; broad in-game visual coverage remains separate.
