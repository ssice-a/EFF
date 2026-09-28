# EFF：终末地资源替换框架

[English](README_EN.md)

EFF（Endfield Framework）是《明日方舟：终末地》的游戏内资源替换框架。它按源资源身份替换网格、材质、贴图和声明的材质参数，并为世界角色、角色 UI、NPC 及 LOD 提供统一的 Render 规则。

## 功能

- 支持多材质、多 submesh 网格，以及网格、材质、贴图和形态键。
- 每个 Mod 独立保存按键切换和形态键状态。
- Insert 打开内置管理器，F10 热重载 Mod；无效 Mod 不会破坏上一次成功加载的结果。
- 支持冲刺期间的原生材质生命周期和完整 submesh 恢复。
- 可在全局配置中修改管理器、热重载快捷键和镜头淡出行为。

## 安装

从 [EFF Releases](https://github.com/ssice-a/EFF/releases) 下载 DLL 包。退出游戏，将 ZIP 解压到 `Endfield.exe` 所在目录：

```text
d3dcompiler_47.dll         # DirectX 代理加载器
vulkan-1.dll               # Vulkan 代理加载器
└─ plugin/
   ├─ eff.dll
   ├─ eff.ini              # 全局设置模板
   └─ mods/                 # 放置 Mod 文件夹
      └─ ExampleMod/
         ├─ mod.ini
         └─ ...
```

把每个**包含 `mod.ini` 的文件夹**直接放入 `plugin/mods/`，不要多套一层目录。代理加载器可能与其他插件冲突；如果已有同名加载器，请先确认当前图形后端和插件加载方式。

## 游戏内使用

1. 进入包含目标角色的场景，匹配的 Mod 会自动应用。
2. 按 **Insert** 打开 EFF 管理器，选择 Mod 后使用款式按钮和形态键滑块。
3. 添加或编辑 Mod 后按 **F10** 热重载。

全局设置位于 `plugin/eff.ini`：

```ini
[Hotkeys]
reload=F10
gui=INSERT

[Graphics]
disable_camera_fade=true
```

## 制作 Mod

1. 使用 [AnimeStudio](https://github.com/ssice-a/AnimeStudio) 打开游戏 VFS，选择 Prefab 并导出 EFF 源包。
2. 在 Blender 安装 [EFF Blender 插件](https://github.com/ssice-a/EFF-blender)，导入源包中的 `mod.ini`。
3. 编辑网格、材质、贴图、款式和形态键，然后导出 Mod 文件夹。
4. 将导出的文件夹放入 `plugin/mods/`，进游戏按 F10 检查结果。

制作时请配套使用当前版本的 AnimeStudio、Blender 插件和 EFF。网格、骨架、材质和物理文件都使用 EFF 格式；导出失败时请重新导出源包和 Mod。

## 致谢与许可

AnimeStudio 及其贡献者提供资源浏览、解包和导出基础。第三方依赖的许可见 [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES)。EFF 源码使用 AGPL-3.0；项目不包含游戏资源，游戏资产版权归鹰角网络所有。

## 用户协议与免责声明

<details>
<summary>在下载、安装或使用 EFF 之前，请仔细阅读本协议。使用 EFF 即代表你已阅读、理解并同意以下条款。</summary>

### 1. 开源许可与最终用户权利

- EFF 基于 **AGPL-3.0** 许可证开源。用户可以在遵守该许可证的前提下使用、修改和分发源代码。
- 最终用户可以在不修改 EFF 的前提下使用和分发软件本体。

### 2. 反欺诈声明

- 不得在网络销售平台单独售卖 EFF 软件本体而不提供本仓库地址和必要的售后服务。
- EFF 在 GitHub 免费开源。如果通过付费购买获得软件本体，请知悉它可以从 GitHub 免费获取。

### 3. 内容合规与行为约束

- EFF 不包含任何游戏美术资产。《明日方舟：终末地》的官方动画、场景、模型和其他游戏资产归鹰角网络所有，不适用 AGPL-3.0。
- 不得利用 EFF 或游戏官方资产制作、播放或传播违反法律法规、违反游戏规则或引起社区不适的内容。

### 4. 风险与免责声明

- EFF 仅供学习、技术研究和交流使用。使用本工具可能违反游戏服务条款并产生账号封禁、数据损坏或其他风险。
- 因使用 EFF 造成的直接或间接损失由使用者自行承担，项目不承担法律或经济责任。建议使用测试账号进行验证。

</details>
