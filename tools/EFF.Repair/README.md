# EFF Mod 修复工具

游戏外修复工具：识别/选择游戏与 Mod 目录，检查工具和完整索引更新，下载或导入完整索引，并重编译旧 Mod。只处理 Mod，不修改 DLL、游戏原资源、解包工作区或 Blender 工程。

作者可以直接在 AnimeStudio 重新解包，让已有解包来源更新，再从原 Blender 工程重新导出 Mod；这条流程不需要本工具。

## 使用

运行 `EFF.Repair.exe`，确认游戏与 Mod 目录，下载匹配索引或导入维护者提供的 `.effindex.gz`。退出游戏后勾选 Mod 并修复。原包保存在 Mod 目录旁的 `.eff-repair-backups`；失败不提交暂存结果。重复修复同一内容会跳过。

默认检查 `ssice-a/EFF` 最新 Release 中的 `eff-index-catalog.json`，下载适合本机游戏内容的完整索引。修复工具、完整索引包和下载目录随 EFF Release 一起发布。没有发布目录、没有对应游戏内容的索引、网络检查失败会分别显示原因。

本机数据和任务日志保存在 `%LOCALAPPDATA%/EFF/Repair`。程序包含配套离线编译器；运行需要 .NET 9 Windows Desktop Runtime。

重复修复记录保存在上述工具目录的 `receipts`，不会向 Mod 中添加额外 JSON 文件，保持生产读取器的严格文件校验。

## 构建

```powershell
dotnet publish tools/EFF.Repair/EFF.Repair.csproj -c Release -o bin/eff-repair/portable
```

## 维护端索引发布

使用当前游戏和完整 AnimeStudio 工作区生成可跨机器核验的索引包，再生成下载目录。将包和目录附到准备发布的 EFF Release；这些命令本身不上传任何文件。

```powershell
dotnet AnimeStudio.GUI.dll --pack-eff-index GAME WORKSPACE current.effindex.gz GAME_RESOURCE_VERSION
dotnet EFF.Repair.dll --make-catalog current.effindex.gz.json HTTPS_PACKAGE_URL eff-index-catalog.json
```

公共包包含完整资产/依赖索引和 Bundle 内容证明，不包含游戏 Mesh、贴图或作者 Mod。首次用户全量下载，不依赖旧分块。导入时对照本机实际内容，重建本地 eidx/辅助记录，供来源编译器及 AnimeStudio 使用；绝对目录和文件时间不参与公共内容匹配。

诊断命令：`--repair MOD GAME WORKSPACE RESULT_FILE`。修复仍执行相同协议核验、作者文件保持、备份及提交检查。
