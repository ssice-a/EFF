# EFF Mod 修复工具

游戏外修复工具：识别/选择游戏与 Mod 目录，检查工具和完整索引更新，下载或导入完整索引，并重编译旧 Mod。不修改 DLL、游戏原资源、作者源包或 Blender 工程；复用工作区索引时，位置重绑定和索引导入可能更新其 `index/` 文件。

作者可以直接在 AnimeStudio 重新解包，让已有解包来源更新，再从原 Blender 工程重新导出 Mod；这条流程不需要本工具。

## 使用

运行 `EFF.Repair.exe`，确认游戏与 Mod 目录，下载匹配索引或导入维护者提供的 `.effindex.gz`。退出游戏后勾选 Mod 并修复。原包保存在 Mod 目录旁的 `.eff-repair-backups`；失败不提交暂存结果。重复修复同一内容会跳过。

默认检查 `ssice-a/EFF` 最新 Release 中的 `eff-index-catalog.json`，下载适合本机游戏内容的完整索引。也可在“更新源”填写独立索引 Release 的 GitHub API 地址或 catalog HTTPS 地址，或使用 `EFF_INDEX_CATALOG` 环境变量；索引可独立于 DLL 发布。没有发布目录、没有对应游戏内容的索引、网络检查失败会分别显示原因。EFF v1.9.0 Release 已提供完整索引附件与下载目录；匹配依据为本机游戏内容。

本机数据和任务日志保存在 `%LOCALAPPDATA%/EFF/Repair`。程序包含配套离线编译器；运行需要 .NET 9 Windows Desktop Runtime。

索引位置与 AnimeStudio、Blender 共享登记在 `%LOCALAPPDATA%/EFF/Resources/registry.json`，可用 `EFF_RESOURCE_ROOT` 更换公共根。先验证并复用同一游戏安装已有的工作区，也兼容旧 Repair 私有 `workspace`；已有索引时无需重复下载。旧工作区登记原位置，不复制索引。新工作区默认在公共根的 `installations/<安装路径摘要>/workspace`，下载包缓存在 `downloads/<游戏内容摘要>/<包SHA256>.effindex.gz`。不同安装分别验证和绑定，登记不代替内容校验。

重复修复记录保存在上述工具目录的 `receipts`，不会向 Mod 中添加额外 JSON 文件，保持生产读取器的严格文件校验。

CHK 名称、位置或范围变化而逻辑 Bundle 内容未变时，工具可校验已有完整索引并重新绑定本机位置，无需下载相同内容的索引。收据按逻辑 Bundle 内容判断，索引文件时间变化不会单独触发重编译。实际内容变化仍需要匹配当前内容的完整索引；修复编译保留必要依赖，只读末端继续使用游戏原资源。

## 构建

```powershell
dotnet publish tools/EFF.Repair/EFF.Repair.csproj -c Release -o bin/eff-repair/portable
```

## 维护端索引发布

使用当前游戏和完整 AnimeStudio 工作区生成可跨机器核验的索引包，再生成下载目录。将包和目录附到准备发布的 EFF Release 或独立索引 Release；这些命令本身不上传任何文件。独立发布时将客户端更新源指向该 Release 的 API 地址或 catalog 地址。

```powershell
dotnet AnimeStudio.GUI.dll --pack-eff-index GAME WORKSPACE current.effindex.gz GAME_RESOURCE_VERSION
dotnet EFF.Repair.dll --make-catalog current.effindex.gz.json HTTPS_PACKAGE_URL eff-index-catalog.json
```

公共包包含完整资产/依赖索引和 Bundle 内容证明，不包含游戏 Mesh、贴图或作者 Mod。首次用户全量下载，不依赖旧分块。导入时对照本机实际内容，重建本地 eidx/辅助记录，供来源编译器及 AnimeStudio 使用；绝对目录和文件时间不参与公共内容匹配。

诊断命令：`--repair MOD GAME WORKSPACE RESULT_FILE`。修复仍执行相同协议核验、作者文件保持、备份及提交检查。
