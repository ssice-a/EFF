# EIEM v1.2.1

- 每个 Mod 独立解析：单个 `mod.ini` 出错时跳过该 Mod，其他 Mod 继续加载。
- 清理没有对应 `shape.*` 绑定的旧形态键控制声明，旧导出包不再因为悬空控制项整包失效。
- 保留当前网格、材质、贴图和按键切换运行时行为。

**安装：**下载 `EIEM_v1.2.1_dll.zip`，退出游戏后解压到 `Endfield.exe` 所在目录。更新时保留自己的 `plugin/eiem.ini` 与 `plugin/mods/`。制作工具请搭配 [EIEM Blender v0.35.0](https://github.com/ssice-a/EIEM-blender/releases/tag/v0.35.0) 与 [AnimeStudio v1.2.1](https://github.com/ssice-a/AnimeStudio/releases/tag/v1.2.1)。
