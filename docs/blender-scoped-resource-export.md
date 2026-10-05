# Blender 分资源导出

Blender 内的“导出所选 Mod”默认导出全部资源。选择已有 Mod 目录时可选择以下范围：

| 范围 | 写出内容 |
| --- | --- |
| 全部资源 | 所选网格、材质、贴图，以及切换、形态控制与选定 LOD 配置 |
| 仅 Mesh | 已有对应 Mesh 的几何、索引、骨骼路径、bindpose 和形态通道 |
| 仅材质与贴图 | 所选网格引用的材质增量、贴图，以及必要的 INI 声明 |
| 仅贴图 | 所选网格材质引用的 TEX（像素、mip 和规格）及必要的绑定声明 |

部分导出必须有已有的完整 Mod。未选中的资源、按键、形态控制、替换规则及 LOD 映射保留。新增或重命名部件、变更材质槽布局、删除受控形态通道需要全部导出。“全部资源”仍以选择内容为完整导出范围。

无需手动执行 PNG 转换器。Blender 自动读取图片并生成 `.tex`，随后调用共享原生资源编译器准备当前完整版本。游戏 DLL 不解码 PNG。

## 同路径贴图测试

1. 在游戏中观察当前模型。
2. 修改 Blender 材质引用的 PNG 内容，例如将 `G:\zmd\lzy\body.png` 改为同尺寸白色图片，保持路径不变。
3. 选中身体 Mesh，使用“导出所选 Mod”，选择“仅贴图”，输出父目录为 `D:\Hypergryph Launcher\games\Endfield Game\plugin\mods`，Mod 名为 `LZY`。
4. 等待 Blender 导出完成后，在游戏中按 F10，观察身体贴图。

未绘制修改的文件图片直接读取最新磁盘字节，避免 Blender 已加载图片的旧像素缓存。正在 Blender 中绘制的 dirty 图片仍导出当前绘制缓冲。

新 Texture 声明不写 `source.*`，只写 `path` 和非默认的颜色解释/采样设置。TEX 内保存实际尺寸、RGBA32 格式和 mip 数；换用不同尺寸的 TEX 不需要修改 INI。首次更新旧格式会迁移这几个字段，此后只改像素或尺寸时，原来的 `.tex` 路径和 `mod.ini` 字节保持不变。

TEX 始终使用当前图片的文件名主体：`body.png` 写成 `textures/body.tex`。更新已有 Mod 时也会迁移旧哈希文件名、更新 INI 引用并清理无引用的旧 TEX，不再优先保留旧路径。不同文件名的图片即使像素一样也保留各自的 TEX 名。不同内容的图片若同名则报告命名冲突，需修改图片文件名；不会自动添加哈希或声明后缀。颜色解释、采样参数或材质绑定变化时会更新 INI。

## 游戏运行时导出

作者目录保持原位置，先在同盘临时目录准备变更文件及回滚副本，再逐文件原子替换，INI 最后发布。未变 Mesh、材质和 INI 不重写；state.ini、PNG/PSD 等作者辅助文件保留。贴图改名时删除旧的未引用 TEX，发布失败恢复原文件。完整原生候选仍由共享编译器完成后发布。

旧版整体重命名 `LZY → LZY.backup-*` 在运行中实测报 `WinError 5`。修复通过 Windows 目录拒绝删除/重命名、未变 Mesh 只读占用、贴图改名、INI 写入失败回滚及首次导出测试。当前 Blender 已加载修复，并在 PID 38328 的游戏运行期间完成实际仅贴图导出：只变更 `textures/body.tex`，INI、Mesh、材质、选择及活动对象保持；耗时约 29.1 秒，生成候选已由 DLL 记录 `FULL-RELOAD-PREPARED`。这不等于 F10 模型迁移验收。证据为 `diagnostics/blender-live-export-denied-20261005/live-export.json`。

## 性能及验收边界

贴图与材质模式不调用网格导出。当前原生编译器仍会准备整组加载输入；单张贴图更新尚未实现按依赖局部发布，因此 Blender 导出准备仍可能需要数十秒，首次导出或系统繁忙时更长。这个时间发生在导出期间，不能等同于 F10 的游戏内提交耗时，也不声称已经达到毫秒级。

目录测试检查同路径更新、共享载荷拆分、材质增量、Mesh 及控制保留。实时 Blender 测试在隔离 LZY 副本中对比白色与原图的全部 mip，并调用同构建的共享编译器。实际 LZY 的 2K TEX 换成 1K、INI 字节不变也已通过原生载荷检查；11 个原生 Texture2D 对象及其像素保持不变。

当前 v35 已同步部署到游戏 DLL、游戏编译器和 Blender 编译器。安装后的启动/F10 磁盘路径验证通过；这个测试不调用 Unity 的模型迁移 API。游戏中 F10 的完整资源迁移仍需实机视觉验收。证据：`diagnostics/source-free-textures-20261005/v35/verification.json`、`installed-loader.txt`、`installed-native.txt`；实时导出结果见 `diagnostics/blender-partial-export-20261005/source-free-v35/verification.json`。
