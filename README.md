# EFF：终末地资源替换框架

[English](README_EN.md)

EFF（Endfield Framework）是《明日方舟：终末地》的资源替换框架。AnimeStudio 解包、Blender 制作、EFF 在游戏加载资源前应用替换规则，组成完整制作流程。

本次配套发布：**EFF v1.3.0、EFF Blender v0.40.0、AnimeStudio v1.3.0**。默认 DLL 包采用已实机验证的 v35 布局；另提供 v36 构建供验证。构建布局不是作者资源的版本准入条件，兼容性以实际数据结构和配套工具协议为准。

## 功能

- 支持多材质、多 submesh 网格，以及网格、材质、贴图和形态键。
- 按键切换、按住形态键与玩家数值持久化；Renderer 保留游戏原生显隐仲裁。
- Ins 控制面板：选择角色对应的 Mod、查看资源目标、按键按钮及形态滑块；物理按键只控制所选 Mod，默认选择首个可控制 Mod。
- 离线候选校验完成后请求游戏加载与已适配的 owner 重建；准备失败保留当前代际。
- 支持多角色各自加载 Mod，没有对应 Mod 的角色保持原版；F10 统一读取整个 mods 目录，提交新增、删除和资源修改。
- 可修改重载快捷键和镜头淡出行为；Lua UI 适配器尚未接入当前宿主。

`mod.ini` 是唯一作者规则入口。离线工具依据指定的源 Mesh 找到使用它的全部 Prefab/Renderer，每个 Mod 自动携带 `source-inputs.bin`。编译器发布引用作者资源的替换清单，DLL 在 Unity 创建资源/实例前应用规则；未声明的资源保留原版。无需解包整个游戏供玩家使用，也没有运行时全场景 Renderer 扫描。见[命中与加载流程](docs/runtime-resource-discovery.md)。

## 安装

从 [EFF Releases](https://github.com/ssice-a/EFF/releases) 下载 `EFF_v1.3.0_dll.zip`。退出游戏，将 ZIP 解压到 `Endfield.exe` 所在目录。更新时保留自己的 `plugin/eff.ini`、`plugin/mods/` 和 Mod 内的 `state.ini`：

```text
d3dcompiler_47.dll         # DirectX 代理加载器
vulkan-1.dll               # Vulkan 代理加载器
└─ plugin/
   ├─ eff.dll
   ├─ eff.ini              # 全局设置模板
   ├─ package-protocol.json # DLL/编译器协议
   ├─ tools/
   │  ├─ eff_resource_pack.exe # 配套离线编译器
   │  └─ static-sources/    # 配套来源描述工具及运行库
   └─ mods/                 # 放置 Mod 文件夹
      └─ ExampleMod/
         ├─ mod.ini
         └─ ...
```

把各角色**包含 `mod.ini` 的文件夹**直接放入 `plugin/mods/`，不要多套一层目录。完整 Mod 包含作者 Mesh/Material/Texture 及 `source-inputs.bin`，不需要分发作者机器的全局索引或游戏原包。配套编译器将全部 Mod 组合成同一候选，见[离线包契约](docs/offline-resource-package-format.md)。若已有同名代理加载器，请先确认当前图形后端和插件加载方式。

## 游戏内使用

1. 放入完整 Mod 后启动游戏，替换沿原游戏资源加载发生；首次准备需等待完成。
2. 按 **Ins** 打开控制面板，选择要控制的 Mod；关闭面板后使用其声明的按键切换款式和形态。选择只限制按键控制，不影响其他角色的资源。
3. 新增、删除 Mod 文件夹，或从 Blender 导出修改后，按 **F10** 请求当前完整集合热重载。失败保留当前画面；空集合自然恢复原版。实际按键由游戏 `plugin/eff.ini` 决定。

F10 更新 Mod 资源和规则；更新 DLL 代码必须退出并重启游戏。模型仍在异步加载时，受影响的重载会被拒绝，请待加载完成后重试。

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

初次制作需保留 AnimeStudio 源包和来源索引；Blender 会自动为 Mod 生成来源描述。新版解包贴图使用短名称，旧包带哈希的名称仍以自身 INI/manifest 为准，无需手动删除哈希。当前生产 Mod 使用独立 Mesh、Material、Texture；Skeleton/Physics 作者工具仍未接入完整游戏模拟链。分资源导出见[Blender 导出](docs/blender-scoped-resource-export.md)。

## 验证与性能

2026-10-07，用户确认当前 v35 构建的袖子三状态切换、另一部件显隐及本轮验证正常。连续世界/展示热重载、动画重绑定、多角色集合组合及失败回滚已有原生回归。其他角色/NPC 的完整实机覆盖、20–30 个 Mod 的帧率与内存压力尚未验证；安装数量不等于同时渲染数量。当前全 LOD 导出可以将一套作者几何映射到多个目标，不会自动生成低面数模型。详细验收边界见[热重载测试](docs/hot-reload-testing.md)。

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
