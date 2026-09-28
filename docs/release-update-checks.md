# 三端 Release 检查

三端各自使用所属仓库的 GitHub `releases/latest`，只比较正式版 `vMAJOR.MINOR.PATCH`。
检查只提供浏览器下载入口，不下载或覆盖正在使用的 DLL、Blender 插件、AnimeStudio
程序。网络不可用不影响运行或编辑。GitHub Release 未建立时，检查失败属于正常状态。

| 产品 | 来源与当前版本 | 触发和提示 | 忽略状态 |
|---|---|---|---|
| 游戏内 EFF | `ssice-a/EFF`；`src/globals.h` | 打开 Mod 管理页时异步检查；发现新版在该页弹窗；手动重查按钮 | 游戏 `plugin/eff-update-state.txt` 中的版本号 |
| Blender | `ssice-a/EFF-blender`；`bl_info.version` | 插件设置中的“检查更新”，不自动弹窗 | Blender 插件设置 `ignored_release_tag` |
| AnimeStudio | `ssice-a/AnimeStudio`；程序集版本 | GUI 显示后异步检查并提示；About 页可手动检查 | 用户设置 `ignoredReleaseTag` |

三个产品独立比较，不因为 EFF DLL 的版本变化就提示更新 Blender 或 AnimeStudio。
“稍后”只关闭本次提示；再次进入检查入口可再提示。“忽略此版本”只屏蔽当前 tag，
更高版本仍提示；手动检查可查看已忽略的版本。Blender 手动检查不会主动弹窗。
检查结果只在内存或用户本地设置中保存，不写入 Mod 包。DLL 在 15 分钟内复用查询
结果，避免频繁打开管理页时重复请求；手动检查始终重新查询。

## 发布约束

三个仓库均要求 Release tag 与程序显示的版本一致：DLL 使用 `src/globals.h`，
Blender 使用 `__init__.py` 的 `bl_info.version`，AnimeStudio 使用
`Directory.Build.props`。各自的 Release workflow 在打包前检查该对应关系。
AnimeStudio 的 Release workflow 打包 .NET 9 与 .NET 10 两个 GUI/CLI 压缩包。
三个产品分别发布到自己的仓库。EFF Release 仅附带 DLL 安装包，Blender 插件和
AnimeStudio 工具包不再附加到 EFF Release。推送版本 tag 会触发各仓库发布工作流；
修改 workflow 本身不会自行发布。

## 验证范围

DLL 和 AnimeStudio 已完成构建；Blender 的离线比较覆盖新版、最新、忽略与无效元数据。
最终还需分别在
游戏 Mod 管理页、Blender 插件设置和 AnimeStudio GUI 验证网络成功/失败时的
界面表现，以及发布下一版后真实 Release 链接是否正确。
