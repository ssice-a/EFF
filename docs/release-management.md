# 本地构建与发布

本地源码和本地构建结果是发布依据。构建、回归、打包、安装都在开发机完成；远端只接收已核验的提交和产物，不重复编译或跑回归。EFF 主 Release 只包含游戏安装包、共享运行工具包和工具清单，不手工上传源码压缩包。Blender 不按每次提交创建稳定版 Release，作者插件更新进入 `continuous` 热更新通道。

## 本地部署

从 EFF 仓库运行组件构建与部署脚本：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/release/build_deploy_local.ps1 -Component Dll
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/release/build_deploy_local.ps1 -Component Blender
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/release/build_deploy_local.ps1 -Component AnimeStudio
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/release/build_deploy_local.ps1 -Component Manager
```

`-Component All` 依次构建并部署四端。每个组件只运行自己的本机验证一次；EFF 发布准备会再运行一套 EFF Release 回归，然后上传阶段不重建、不重测。

若 AnimeStudio 回归已对同一源码提交通过、但随后的构建或部署失败，可用 `-Component AnimeStudio -SkipAnimeStudioTests` 只重试构建与部署；正常构建不要跳过测试。

| 组件 | 本机安装目标 | 部署前要求 |
| --- | --- | --- |
| Format 3 DLL 与协议声明 | `D:\Hypergryph Launcher\games\Endfield Game\plugin` | 游戏正常退出；只替换 `eff.dll` 和 `package-protocol.json`，保留用户的 `eff.ini` 与 Mods |
| Blender 插件 | `%APPDATA%\Blender Foundation\Blender\5.2\scripts\addons\EFF_Blender` | Blender 5.2 正常退出；使用插件现有事务更新器，保留完整回滚副本和用户文件；不安装 Blender 5.0 |
| AnimeStudio | `E:\AnimeStudio-net10-1ccfbc16bf7fe625e8295bac8074ac3b1b9a065b` | 使用 net9.0-windows 构建；GUI/CLI 正常退出；保留 `CNKeys.json` 和日志 |
| Mod Manager | `E:\EFF-Mod-Manager` | Manager 正常退出；旧目录保存在 E 盘回滚目录 |

脚本不会强制关闭进程。目标程序运行时会中止对应部署；游戏进程还会按可执行文件实际路径核对，避免把目录外同名的 `NTEGame.exe` 当成游戏。重新正常关闭后重跑即可。构建回执位于 `bin/local-deployments/<时间戳>/`，记录源码提交、目标位置、备份位置和部署文件 SHA256。

Blender 组件命令会生成完整 continuous 更新包和 feed；安装完成后可将该目录直接发布到已有的热更新通道：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/release/publish_blender_continuous.ps1 `
  -BuildDirectory bin/local-deployments/<时间戳>/blender-build
```

该命令只推送 Blender 本地 `HEAD` 到 `main` 并上传已构建的 ZIP/feed，不创建版本化稳定 Release，也不运行云端构建或测试。应先在本地确认构建部署成功；Git 和上传失败时不会覆盖稳定版。

## EFF 稳定版

先在 EFF 和三个子模块分别提交准备发布的源码，并将准确的子模块提交记录进 EFF gitlink。更新 `src/eiem_version.h` 与 `RELEASE_NOTES.md`，确保工作区干净；版本标签必须在本地指向当前 `HEAD`。然后运行：

```powershell
git tag -a vX.Y.Z -m "EFF vX.Y.Z"
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/release/prepare_local_release.ps1 -Tag vX.Y.Z
```

只发布、不更新本机游戏安装时，给准备命令添加 `-SkipDeployment`。该模式仍完成全部构建、回归和资产核验，不要求关闭正在运行的游戏；回执记录跳过部署。

准备脚本按固定顺序构建生产 DLL、迁移工具、来源导出器、Format 3 作者工具和双 Python ABI Pillow；运行 Release 回归一次；生成全新的三文件 assets 目录；验证 ZIP 成员、协议、版本、工具清单、SHA256 和源码排除规则。全部通过后，DLL 与协议声明会部署到游戏插件目录。

产物和证据保存在：

- `bin/local-releases/vX.Y.Z/assets/`：只有 `EFF_vX.Y.Z_dll.zip`、`EFF_Shared_Native_Tools_vX.Y.Z.zip`、`eff-native-tools.json`
- `bin/local-releases/vX.Y.Z/preparation.json`：源码/子模块提交、产物大小和 SHA256
- `bin/local-release-receipts/`：完整本机构建与回归日志

先检查这些本地产物和回执，再运行上传：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/release/publish_local_release.ps1 -Tag vX.Y.Z
```

上传脚本再次核对 tag、提交、子模块 gitlink 和资产；要求本地提交能快进到远端 `main`。它把本地 `HEAD` 推到 `main`，推送本地标签，将三个本地产物上传为 Draft，再下载回本机核对资产清单、内容和 SHA256，最后才公开 Release。已有的公开标签或 Release 不会被移动或覆盖；失败时保留 Draft 以便检查和重试。此阶段不编译、不运行回归。

## 远端行为

EFF 的标签构建 workflow、Blender 的自动 continuous 打包和 stable-tag workflow 已移除。AnimeStudio 与 Manager 的云端构建/发布仅保留手动触发，普通推送和版本标签不会重复构建本地已验证的产物；它们的版本化 Release 上传本机生成的安装包。历史发布凭证保留在 [2026-11-02 coordinated receipt](releases/2026-11-02-coordinated.md)，仅作为历史记录。
