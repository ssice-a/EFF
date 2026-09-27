# EIEM v1.2.2

- 按 `plugin/eiem.ini` 中的 `disable_camera_fade=true` 真正启用反虚化 hook；关闭该项时不安装对应 hook。
- 保持每个 Mod 独立解析：单个 `mod.ini` 出错时跳过该 Mod，其他 Mod 继续加载。
- 保留当前网格、材质、贴图、形态键和按键切换运行时行为。

**安装：**下载 `EIEM_v1.2.2_dll.zip`，退出游戏后解压到 `Endfield.exe` 所在目录。更新时保留自己的 `plugin/eiem.ini` 与 `plugin/mods/`。制作工具请搭配 [EIEM Blender v0.37.0](https://github.com/ssice-a/EIEM-blender/releases/tag/v0.37.0) 与 [AnimeStudio v1.2.1](https://github.com/ssice-a/AnimeStudio/releases/tag/v1.2.1)。
