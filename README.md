# EFF：终末地资源替换框架

[English](README_EN.md)

EFF 在《明日方舟：终末地》创建资源和实例前应用 Mod。AnimeStudio 导出来源，Blender 制作并编译独立 Mod，EFF DLL 读取结果并完成游戏内加载与热重载。

配套发布：**EFF v1.10.2、EFF Blender v0.49.2、AnimeStudio v1.8.1**。DLL 只提供一个正式安装包；兼容性依据数据结构、字段、来源摘要和引用关系判断。

首次制作 Mod 可先阅读带截图的简易教程：[中文](Mod%20Tutorial.md) · [English](Mod%20Tutorial_EN.md)。

## 功能

- 多角色各自加载 Mod，没有对应 Mod 的角色保持原版。
- 蒙皮 Mesh 和静态 MeshFilter＋MeshRenderer 使用同一 Replace、材质、贴图及显隐规则；静态 Renderer 不支持运行时形态键。
- 一个原 Renderer 可对应多个作者部件；未声明替换的目标保留原内容。材质和贴图可以借用游戏资源或由作者新建。
- 按键切换、形态键与玩家数值持久化；Ins 面板独立勾选各 Mod 是否接收按键，默认全部启用，可同时启用多个 Mod。
- Shape Keys 动画支持多网格同步播放、多个组共享频道；冲突组切换播放，Ins 支持暂停和实时倍率，调速保持进度。
- 特效可作为独立 Mod 或挂载角色 Mesh 骨骼；支持按键显隐、F9 热重载和覆盖全部 LOD，并随所属模型的生命周期加载。
- 材质动态参数和特效资源参与同一 Mod 导出与热重载流程；支持递归发现 `plugin/mods` 下的独立 Mod 目录。
- F10 读取整个 mods 目录，统一处理增删改和空集合；沿已适配的游戏世界/展示模型生命周期重载，重新绑定动画、武器和角色音频。准备或提交失败保留当前代际。

## 安装

从 [EFF Releases](https://github.com/ssice-a/EFF/releases) 下载 `EFF_v1.10.2_dll.zip`。退出游戏，将 ZIP 解压到 `Endfield.exe` 所在目录。当前代理转发支持 Windows 系统目录为 `C:\Windows` 的安装；其他系统目录尚不支持。更新时保留自己的 `plugin/eff.ini`、`plugin/mods/` 和各 Mod 的 `state.ini`。

```text
d3dcompiler_47.dll
vulkan-1.dll
plugin/
  eff.dll
  eff.ini
  package-protocol.json
  mods/
    YourMod/
      mod.ini
      compiled.bin
      source-inputs.bin
      meshes/
      materials/
      textures/
```

正式 DLL 与代理静态链接自身的 C/C++ 运行实现，不依赖游戏目录中较旧的 MSVCP/VCRUNTIME；无需修改游戏运行库。DLL 包不包含编译器、AnimeStudio 运行库或来源工具。已有安装的 `plugin/tools/eff_resource_pack.exe` 和 `plugin/tools/static-sources/` 可删除；新版 DLL 不调用它们。若已有同名代理加载器，请先确认使用的图形后端与插件加载方式。

把包含 `mod.ini` 的完整 Mod 文件夹直接放入 `plugin/mods/`。`compiled.bin` 必须随作者资源一起分发。文件夹可移动到其他安装，来源路径相对游戏目录。游戏更新后，玩家可使用 EFF Mod Manager 中的 Mod 修复功能；修复前会匹配当前游戏资源并备份原包，不要求 Blender 或手动解包。

作者也可用 AnimeStudio 更新原解包资源，再从原 Blender 工程重新导出。修复使用当前游戏的来源和消费者；资产删除、来源歧义或不受支持的结构会报告失败，保留原包。

## 游戏内使用

1. 放入完整 Mod 后启动游戏，资源请求使用当前替换规则。
2. 按 **Ins** 勾选需要响应按键的 Mod，关闭面板后使用按键切换款式或形态。同一个键会触发所有已启用且绑定该键的 Mod；下拉选择只决定面板中手动调整哪个 Mod。面板可拖动窗口边缘或右下角手柄调整大小，也可最大化；内容超出时滚动查看。
3. 从 Blender 导出修改、添加或删除 Mod 后，按 **F10** 提交当前完整集合。空集合恢复原版；模型尚在异步加载时请待加载完成后重试。

同一 Mod 的 UI 与大世界模型共用切换值。UI 原地刷新通过游戏加载器和页面项的模型绑定流程完成，保留旧人物直到候选校验、页面绑定和控制发布完成；无法确认页面归属的实例会在修改前拒绝刷新。

各 `mod.ini` 的 `[Mod]` 中用 `key_switch_enabled=1` 启用按键，`0` 停用；未填写时默认启用。手改后按 F10 生效，Ins 勾选立即生效并保存到同一字段。停用按键保留 Mod 资源与当前数值。

F10 更新 Mod 资源和规则；DLL 代码更新需要退出并重启游戏。全局设置位于 `plugin/eff.ini`：

```ini
[Hotkeys]
reload=F10
gui=INSERT

[Graphics]
disable_camera_fade=true
```

## 制作与三端职责

1. 使用 [AnimeStudio](https://github.com/ssice-a/AnimeStudio) 打开 VFS，选择 Prefab 并导出 EFF 源包。保留源包的字段模板、manifest 和作者索引。
2. 安装 [EFF Blender 插件](https://github.com/ssice-a/EFF-blender)，导入源包 `mod.ini`，编辑并导出所选网格。首次导出选择全部资源；后续可仅更新 Mesh、材质与贴图或贴图。
3. Blender 使用插件自带的作者核心、编译器和小型来源工具，按 INI 源 Mesh 离线找到全部 Prefab/Renderer 消费者，编译当前这一个 Mod。暂存结果完整验证后才发布，失败保留此前输出。
4. DLL 读取各 Mod 的编译结果，合并共用资源上的已编译修改，在游戏加载前应用字节规则；F10 使用同一规则并调用游戏模型生命周期。DLL 不编译作者资源，不查询全游戏索引，不全场景扫描 Renderer。

导出到游戏 `plugin/mods/` 会自动识别安装路径；导出到其他目录请设置 `EFF_RELOAD_GAME`。玩家不需要作者索引、原游戏包快照或 Blender 工具。`mod.ini` 仍是唯一作者规则入口，`compiled.bin` 是它的派生编译结果。新贴图短名称与旧哈希名称都依各自 INI/manifest 解析。

完整原包不会写入 Mod。编译结果包含资源输入视图、依赖与实例绑定索引，大小随实际作者资源和共享修改变化。规则文件大小与 Unity 加载时的内存占用不同。详见[命中与加载流程](docs/runtime-resource-discovery.md)、[输出契约](docs/offline-resource-package-format.md)。

## 验证边界

本次更新的 BLC v5、公共索引、来源事务更新、旧工程隔离导出及旧 Mod 修复回归已通过。DLL 不承担游戏更新后的后台发现、编译或自动回写；更新适配在游戏外完成。全部角色画面、活跃动画性能和骨架实质变化仍需独立验收，见[本轮实现与验证](docs/update-workflows-20261015.md)。

离线回归覆盖同 PFB 多 Mod、增删恢复、流偏移、静态网格、连续世界/展示重载、动画消费者与回滚。最新两角色实机 F10 请求到提交约 107 ms，待机附件已获视觉确认；这是已准备候选的样本，不能代表所有场景的耗时。独立人群 NPC/场景物件的原地 F10、完整多代资源退休和 20–30 个 Mod 的压力尚未验证；数量不等于同时渲染数量。全 LOD 导出不会自动减面。新增 BoneCloth 骨链及球/胶囊碰撞体已接入离线编译；不同动作的混合、完整物理退出和 F10 仍需实机验收。当前发布构建通过自动回归，尚无本轮新二进制的实机验收。详细边界见[热重载验收](docs/hot-reload-testing.md)。

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
