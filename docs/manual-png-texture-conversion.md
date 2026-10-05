# 手动 PNG 贴图转换

此工作流已由 [Blender 分资源导出](blender-scoped-resource-export.md) 取代。用户无需拖 PNG 到 EXE；转换器保留为已有辅助工具，下面记录其历史行为。

用户手动把 Photoshop 导出的 PNG 拖到 `eff_texture_convert.exe`，转换器读取目标 Mod 的 `mod.ini`，输出与 PNG 同名的 `.tex`，并更新对应的 `Texture.path`。F10 不执行 PNG 解码或 mip 生成。

本机入口：

```text
D:\Hypergryph Launcher\games\Endfield Game\plugin\tools\eff_texture_convert.exe
```

把一个或多个 PNG 拖到 EXE。PNG 可以放在 Mod 的 `textures` 目录中，EXE 也可以复制到这里使用。文件名可以使用现有 `.tex` 的文件名（仅改扩展名），也可以省略导出器生成的最后 12 位 hash 来自动匹配 INI 设置。例如：

| PNG 文件名 | LZY 的输出 |
|---|---|
| `body.png` | `textures/body.tex` |
| `body_010de4985b31.png` | `textures/body_010de4985b31.tex` |
| `body_NM.png` | `textures/body_NM.tex` |
| `clothes_D.png` | `textures/clothes_D.tex` |
| `clothes_N.png` | `textures/clothes_N.tex` |
| `clothes_E.png` | `textures/clothes_E.tex` |
| `clothes_P.png` | `textures/clothes_P.tex` |
| `clothes_M.png` | `textures/clothes_M.tex` |
| `WHITE.png`，选择目标贴图声明 | `textures/WHITE.tex` |

复制到 Mod 内的 EXE 优先读取最近的 `mod.ini`；常规安装读取 `plugin/mods/*/mod.ini`。有多个 Mod 使用同名贴图时，把目标 `mod.ini` 和 PNG 一起拖到 EXE。未知文件名（例如 `WHITE.png`）在拖入窗口中列出已有贴图供选择，也可使用 `--texture <TextureSection>` 明确指定。INI 已声明贴图的尺寸和参数，但任意 PNG 名称本身无法确定应替换哪个槽，因而需要这次选择。

若多个 Texture 声明共享同一个像素文件，转换器更新全部相关 `path=`。材质的 Texture ID、`source.path/source.asset` 和原版/自定义槽选择保留。改名后的旧 `.tex` 与原 INI 归档到 Mod 目录外，避免留下未引用原生资源。`.png/.psd` 和 `eff_texture_convert.exe` 是辅助文件，不要求在 INI 声明，也不参与离线资源摘要或触发重新构建。

PNG 的宽高必须与 INI 相符，工具不改尺寸和贴图参数，只在需要改名时更新 `Texture.path`。当前仅支持 `format=RGBA32`：Windows WIC 解码为 8 位 RGBA，生成 INI 指定数量的 BOX mip，每层上下翻转，输出规范 `EFFRES2` kind 4 的 `pixels` 块。透明度保留，mip 生成与 Blender 的 Pillow RGBA/BOX 流程一致；不执行 ICC/gamma 变换。`linear/filter/wrap/aniso/mip_bias` 保留在 INI，由原生资源打包器应用。

转换成功显示输出文件、尺寸、mip 数和备份路径；从资源管理器拖入时窗口等待 Enter 关闭。旧 `.tex` 和 INI 备份位于 `plugin/texture-convert-backups/<Mod>/<本次操作>/`，临时文件也在这里。整批先解码，随后发布像素、更新 INI、移走旧文件；提交失败恢复已提交的输出和 INI。

命令行：

```text
eff_texture_convert --mod "<Mod 目录或 mod.ini>" "<PNG>" ...
eff_texture_convert --no-pause --mod "<Mod>" "<PNG>"
eff_texture_convert --no-pause --mod "<Mod>" --texture TextureT_actor_lizhiyan_body_01_D "<WHITE.png>"
```

构建：`tools/build_texture_convert.cmd`，工作区产物为 `bin/texture-convert/eff_texture_convert.exe`。仅依赖 Windows WIC/COM，静态链接 C++ 运行库，不需要 Python 或 Blender。转换不重建原生包、不改 DLL 或 F10；当前完整资源 F10 实机提交仍未通过验收，得到新 `.tex` 不代表游戏已热重载成功。

独立验证入口 `tools/diagnostics/verify_texture_convert.py` 在隔离 Mod 检查真实 LZY 批量转换、透明度、非正方/奇数尺寸、RGB/灰度/调色板 PNG、错误输入保留原文件、多 Mod 消歧、共享 INI 更新、同名输出及原文件备份。输出与规范 Python `encode_resource` 和 Blender/Pillow 流程逐字节对比，不修改已安装 Mod。`verify_texture_rename_reload.py` 进一步验证旧 TEX 删除后生产 F10 磁盘入口能调用外部编译器并选择新候选；实机装配由用户验收。
