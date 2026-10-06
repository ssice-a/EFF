# EFF：终末地资源替换框架

[English](README_EN.md)

EFF（Endfield Framework）是《明日方舟：终末地》的游戏内资源替换框架。它将格式 2 作者声明编译为原生加载输入，按源资源身份替换网格、材质、贴图及材质参数。

当前源码已整理为单一资源入口，默认构建 v36；2026-10-06 游戏目录已部署当前源码的 v35 配套 DLL/编译器，F10 重载、Insert 控制。历史 v35c 世界实例贴图正向更新已通过；修正版、完整装配、反向恢复及池生命周期仍待实机验收。版本与下一步见[文档索引](docs/README.md)和[热重载测试](docs/hot-reload-testing.md)。

## 功能

- 支持多材质、多 submesh 网格，以及网格、材质、贴图和形态键。
- 按键切换、按住形态键与玩家数值持久化；Renderer 保留游戏原生显隐仲裁。
- 当前源码支持 Ins 控制面板：选择角色对应的 Mod、查看资源目标、按键按钮及形态滑块；物理按键只控制所选 Mod。
- 离线候选校验完成后请求游戏加载与已适配的 owner 重建；准备失败保留当前代际。
- 支持多角色各自加载 Mod，没有对应 Mod 的角色保持原版；F10 统一提交 Mod 增删与资源修改。需要对应角色基线，已部署 v35，仍待实机验收。
- 可修改重载快捷键和镜头淡出行为；Lua UI 适配器尚未接入当前宿主。

每个 Mod 自动带有 `source-inputs.bin`；编译器从 INI 发布小型替换规则，不再写完整 PFB 或原资源副本。DLL 按已知范围读取游戏原字节，在实例创建前应用规则；目标离线确定，见[命中与加载流程](docs/runtime-resource-discovery.md)。

## 安装

从 [EFF Releases](https://github.com/ssice-a/EFF/releases) 下载 DLL 包。退出游戏，将 ZIP 解压到 `Endfield.exe` 所在目录：

```text
d3dcompiler_47.dll         # DirectX 代理加载器
vulkan-1.dll               # Vulkan 代理加载器
└─ plugin/
   ├─ eff.dll
   ├─ eff.ini              # 全局设置模板
   ├─ package-protocol.json # DLL/编译器协议
   ├─ tools/
   │  └─ eff_resource_pack.exe # 配套离线编译器
   └─ mods/                 # 放置 Mod 文件夹
      └─ ExampleMod/
         ├─ mod.ini
         └─ ...
```

把各角色**包含 `mod.ini` 的文件夹**直接放入 `plugin/mods/`，不要多套一层目录。配套编译器将全部 Mod 组合成同一候选；原生编译合并各 Mod 自动生成的 `source-inputs.bin`，首次作者准备需要已有 AnimeStudio 索引，见[离线包契约](docs/offline-resource-package-format.md)。代理加载器可能与其他插件冲突；如果已有同名加载器，请先确认当前图形后端和插件加载方式。

## 游戏内使用

1. 用配套编译器准备完整集合的替换规则后，启动游戏进入目标角色场景，替换沿资源加载发生。
2. 按 **Ins** 打开控制面板，选择要控制的 Mod；关闭面板后使用其声明的按键切换款式和形态。默认选中首个可控制 Mod，选择不影响其他角色的资源。当前安装的 v35 已接入此面板，游戏内交互仍待验收。
3. 新增、删除 Mod 文件夹，或从 Blender 导出修改后，按 **F10** 请求完整集合热重载。离线构建失败保留当前画面；删除最后一个时恢复原版。已安装 DLL 的实际按键仍由游戏 `plugin/eff.ini` 决定。

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
4. 导出到 `plugin/mods/` 后等待配套离线编译完成，进游戏按配置的重载键检查结果。

制作时请配套使用 AnimeStudio、Blender 插件和 EFF 的格式/ABI。当前生产作者包使用独立 Mesh、Material、Texture；Skeleton/Physics 编解码保留为作者支持，尚未接入当前游戏模拟链。分资源导出见[Blender 导出](docs/blender-scoped-resource-export.md)。

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
