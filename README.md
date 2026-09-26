# EIEM：终末地资源替换

[English](README_EN.md) | 中文

EIEM 是《明日方舟：终末地》的游戏内资源替换插件。它按原资源身份替换模型、材质和贴图；Mod 作者可用 [AnimeStudio](https://github.com/ssice-a/AnimeStudio) 导出源资源，再用 [EIEM Blender 插件](https://github.com/ssice-a/EIEM-blender) 编辑并导出 Mod。只使用现成 Mod 时，无需安装这两个制作工具。

## 功能

- 替换 Mesh、材质、贴图及声明的材质参数；一个 Mesh 可包含多个 submesh 和材质槽。
- 相同资源规则应用于大世界角色、角色 UI 和 NPC，并支持作者导出的 LOD 规则。
- 每个 Mod 有独立的款式按键和形态键状态；内置管理页提供按键按钮与形态键滑块。
- 按 F10 重新读取 Mod 配置与资源；配置有误时保留上次成功加载的效果。
- 可在全局配置中修改管理页和刷新快捷键，也可配置反虚化。
- 打开 Mod 管理页时检查本仓库的新 Release；可稍后再说或忽略指定版本。

## 下载与安装

从 **[EIEM Releases](https://github.com/ssice-a/EIEM/releases)** 下载 DLL 发布包。关闭游戏后，将 ZIP 解压到 `Endfield.exe` 所在目录：

```text
游戏目录/
├─ d3dcompiler_47.dll      # DirectX 代理加载器
├─ vulkan-1.dll            # Vulkan 代理加载器
└─ plugin/
   ├─ eiem.dll
   ├─ eiem.ini             # 全局设置；缺失时插件会生成默认文件
   └─ mods/
      └─ 某个Mod/
         ├─ mod.ini
         └─ ...            # 保持 Mod 包内的资源目录结构
```

可按实际图形环境放置一个或两个代理加载器。已有其他插件提供兼容的同名加载器时，先核对加载方式，不要直接覆盖。`plugin`、`mods` 目录不存在时可自行创建。[Applepie Manager](https://github.com/Sasye/ApplepieManager) 是可选的插件管理工具。

安装现成 Mod 时，将**包含 `mod.ini` 的文件夹**放入 `plugin/mods/`；确认没有多解压一层目录。更新 EIEM 时先退出游戏，替换 DLL 和需要更新的加载器，保留自己的 `plugin/eiem.ini` 与 `plugin/mods/`。

## 游戏内使用

1. 启动游戏并进入包含目标角色的场景。符合资源规则的 Mod 会自动应用。
2. 按 **Insert** 打开 Mod 管理页，选择要控制的 Mod，使用页面上的款式按钮或形态键滑块。静态 Mod 会自动生效。
3. 修改或新增 Mod 文件后按 **F10** 热重载。

`plugin/eiem.ini` 中的快捷键可修改，例如：

```ini
[Hotkeys]
reload=F10
gui=INSERT

[Graphics]
disable_camera_fade=true
```

修改快捷键后，先按一次**旧的刷新快捷键**加载新设置。更多按键与 Mod 规则见[配置说明](docs/conditional-keys.md)。

## 制作自己的 Mod

1. 使用 AnimeStudio 打开游戏 VFS，选择 Prefab，导出 EIEM 源包。
2. 在 Blender 中安装并启用 EIEM Blender 插件，导入源包的 `mod.ini`。
3. 编辑网格、材质、贴图、款式或形态键，选择目标网格，导出 EIEM Mod 包。导出时可以关闭按键切换，保留默认款式和形态键滑块。
4. 将导出文件夹放入 `plugin/mods/`，进游戏按 F10 检查效果。

具体步骤见 [AnimeStudio 说明](tools/README.md)与 [Blender 插件说明](https://github.com/ssice-a/EIEM-blender#readme)。

制作 Mod 时请配套使用当前版本的三款工具。以前导出的 EIEM 模型、骨架和物理文件需要重新导出。

## TODO

- 完善新增骨骼、物理骨骼和碰撞体支持。
- 扩展角色与游戏版本的兼容范围。

## 鸣谢

- [AnimeStudio](https://github.com/Escartem/AnimeStudio) 及其贡献者提供资源浏览、解包与导出基础；EIEM 使用其独立维护的 fork。
- [MinHook](https://github.com/TsudaKageyu/minhook)、[Dear ImGui](https://github.com/ocornut/imgui) 及其他依赖的版权与许可证见 [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES)。

## 用户协议与免责声明

<details>
<summary>在下载、安装或使用本插件（EIEM）之前，请您仔细阅读本协议。<b>使用本插件即代表您已完整阅读、充分理解并同意遵守以下所有条款。</b></summary>

### 1. 开源许可与最终用户权利
- 本插件基于 **AGPL-3.0** 许可证在 GitHub 平台完全开源。用户可在遵守该许可证的前提下自由使用、修改和分发本插件的源代码。
- 最终用户（End User）在不对本插件进行修改的前提下，使用和分发本插件**不受任何限制**。此权利不因用户是否违反本协议而改变。

### 2. 反欺诈声明
- 您**不得**在网络销售平台公然售卖本插件**软件本体**且未提供 GitHub 仓库地址与售后服务。
- 本插件在 GitHub 平台完全免费开源，如果您是付费购买获取的，请知悉本插件可从 GitHub 免费获取。

### 3. 内容合规与行为约束
- 本插件本身不包含任何游戏美术资产。用户知悉并同意，《明日方舟：终末地》游戏内置的官方动画、场景、模型等资产其版权完全隶属于鹰角网络，并不适用 AGPL-3.0 协议。您**不应该且不得**利用本插件，或利用游戏内置的官方动画、场景、模型等游戏资产，制作、播放或传播任何不合适的动作/动画（包括但不限于色情、暴力、政治敏感等违反法律法规或引起社区不适的内容）。

### 4. 风险与免责声明
- 本项目仅供学习、技术研究和交流目的。本插件中使用的明日方舟游戏数据资产版权均隶属于鹰角网络。使用本工具可能违反游戏服务条款，存在账号封禁的风险。因使用本插件而直接或间接导致的任何损失（包括但不限于账号封禁、游戏数据损坏等），**本项目不承担任何法律或经济责任**。用户需自行承担所有风险，强烈建议您在测试账号上运行。

</details>
