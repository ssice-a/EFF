# 项目管理与发布边界（2026-09-26）

## 三个工作树

| 产品 | 工作目录 | 分支 | 仓库 | 本轮版本 |
|---|---|---|---|---|
| 游戏内 DLL、协议、集成测试 | 仓库根目录 | `main` | `ssice-a/EFF` | `v1.1.0` |
| Blender 作者插件 | `tools/Blender` | `main` | `ssice-a/EFF-blender` | `v0.33.0` |
| AnimeStudio 解包工具 | `tools/AnimeStudio` | `master` | `ssice-a/AnimeStudio` | `v1.1.0` |

两个工具为 Git submodule，各自维护源码和提交历史。先提交、推送工具，再在 EFF
更新 gitlink 并推送主仓库。日常只在这三个工作树开发；工作区本地 VS Code 设置只显示这三个仓库。

## Release 内容

- EFF：只发布 `EFF_v<版本>_dll.zip`，按游戏目录布局包含加载器、`plugin/eff.dll`、
  `plugin/eff.ini` 模板和许可证。更新时保留用户配置与 Mod。
- Blender：只在 EFF-blender 仓库发布可直接从磁盘安装的插件 ZIP。
- AnimeStudio：只在 AnimeStudio 仓库发布 .NET 9、.NET 10 两种 Windows GUI/CLI 包，
  用户选择与本机 .NET Desktop Runtime 匹配的一种。
- tag 必须与程序版本一致。新版本默认由远程工作流构建；使用 CI 产物补发时必须核对构建提交与 tag。
- GitHub 自动提供的源代码 ZIP/TAR 是平台功能，不是额外的产品安装包。

## 工作区卫生

`.gitignore` 排除本地 `analysis/`、备份、IDE 设置、外部调查工程和一次性诊断脚本。
研究记录保留在本地，不随发布包上传。正式运行时源码和回归测试依赖的可复现辅助代码继续版本管理。
旧 DLL 备份不是发布输入；清理必须核对具体路径，不删除当前构建 DLL、第三方依赖或研究日志。

## 本轮验证记录

- Blender 优化提交 `1d00061`：固定宽度数据批量读写、顶点与面角批量读取、法线 CRC
  和切线校验优化。三个 Mesh 导入约 2.55→1.21 秒，网格包导出约 4.66→2.68 秒，
  导出文件逐字节一致。作者工程后台测试和当前 Blender 5.0.1 只读校验均通过。
- Blender 的格式、蒙皮、面角、切线、合并网格和形态键测试通过；主仓库带 Blender
  环境的测试 140 项通过、42 项因可选环境缺失跳过。
- AnimeStudio 提交 `821167c`：导出器、更新检查、发布流程，MessagePack 3.1.10。
  本地 .NET 9 GUI 重建通过，远程 .NET 9/10 构建通过。
- DLL 和两个加载器本地构建通过；远程 Release 使用 tag 对应源码重新构建。

## 历史迁移记录

原 `E:\vscode\AnimeStudio` 的较新源码已合并到 `tools/AnimeStudio`，原重复目录由用户删除。
Git 引用备份在本地忽略目录 `backups/AnimeStudio-old-checkout-20260926.bundle`。
先前 `/PURGE` 曾误删未跟踪的 `EndfieldMeshObjectDump.cs`，并覆盖两处本地诊断改动；
该文件尚无可恢复副本，不能将那次迁移描述为无损。后续同步须先比较再逐项合并，禁止用 `/PURGE` 覆盖本地改动。
