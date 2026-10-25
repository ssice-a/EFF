# EFF v1.10.1

配套版本：[EFF Blender v0.49.0](https://github.com/ssice-a/EFF-blender/releases/tag/v0.49.0)、[AnimeStudio v1.8.1](https://github.com/ssice-a/AnimeStudio/releases/tag/v1.8.1)。

- 修复多部件静态 Mesh 装配的 provenance 校验，使额外 Renderer 与对应作者部件的稳定资源标识一致。
- 延续 v1.10.0 的资源增量更新和命名按键切换组协议；保留既有 Mod 格式兼容读取。
- EFF Release 工作流构建并上传配套 Blender native base；Blender 插件包校验基础包摘要与 DLL/编译器共同协议。
- DLL 仍须在目标游戏版本进行冷启动、F10、UI、NPC、多 Mod 与特效实机回归后再从 draft 发布。

## 下载与安装

- `EFF_v1.10.1_dll.zip`：退出游戏后解压到游戏根目录，保留个人配置和 Mods。
- `EFF_Blender_Native_v0.49.0.zip`：Blender Release 工作流使用的配套基础包，不需要单独安装。

## English

Fixes static multi-part Mesh provenance validation while retaining the v1.10.0 resource update and named key-switch protocols. The release workflow builds and uploads the paired Blender native base. Mod repair is distributed through EFF Mod Manager. Validate the DLL in the target game before publishing the draft.

## Previous release

## EFF v1.9.0

配套版本：[EFF Blender v0.48.0](https://github.com/ssice-a/EFF-blender/releases/tag/v0.48.0)、[AnimeStudio v1.8.1](https://github.com/ssice-a/AnimeStudio/releases/tag/v1.8.1)。

- 接入 ParticleSystem 特效 Mod：可独立安装，也可挂载角色 Mesh 骨骼；默认随模型生命周期加载，支持快捷键显隐及 F9 热重载。
- 特效挂载覆盖全部角色 LOD，并与多 Mod 资源准备、提交、回滚和场景更新共用现有运行时流程；不增加独立特效 DLL。
- 支持独立特效包和角色包同时导出；Mod 目录递归扫描，允许在现有 Mod 文件夹内再放置含独立 `mod.ini` 的子包。
- 增加材质动态参数播放控制，并保留现有 Shape Keys 动画组与玩家按键控制。
- 收紧热重载资源所有权和代际事务处理，保留已验证的末端依赖裁剪、增量 Mesh 编译和多 Mod 事务组合。

## 下载与安装

- `EFF_v1.9.0_dll.zip`：退出游戏后解压到游戏根目录，保留个人 `plugin/eff.ini`、Mods 和 `state.ini`。只含生产 DLL、代理、协议声明和默认配置。
- `EFF-Repair-v0.3.0-win-x64.zip`：独立游戏外修复工具，需要 .NET 9 Windows Desktop Runtime；修复前会备份原 Mod。
- `EFF-Endfield-Index-20261022.effindex.gz` 与 `eff-index-catalog.json`：当前游戏内容的完整索引及 Repair 下载目录。索引压缩包 97,429,663 字节；工具会按游戏 Bundle 内容摘要选择并校验下载。
- `EFF_Blender_Native_v0.47.0.zip`：配套 Blender Release 使用的完整固定原生基础包，包含本次编译器、来源索引器、作者核心及插件 Python 文件。普通 DLL 用户不需要安装它。

旧 Mod 不需要仅为升级 DLL 重新导出。若来源内容发生变化，作者可用配套 Blender 更新原工程并重新导出；玩家可使用修复工具。游戏内表现仍取决于来源 Prefab、Renderer 和材质配置，离线预览不等同于游戏渲染。

## English

Adds ParticleSystem effect Mods that can run independently or attach to a character Mesh bone. Effects follow the model lifecycle, cover every character LOD, support key toggles and F9 reload, and use the existing Mod transaction path without a separate DLL. Mod discovery now recurses through nested package folders. Adds animated material-property controls and retains the existing Shape Keys playback groups.

Install `EFF_v1.9.0_dll.zip` beside `Endfield.exe` after closing the game. Preserve personal configuration and Mods. The optional Repair tool downloads a content-matched full index from the release catalog. `EFF_Blender_Native_v0.47.0.zip` is a pinned build base for the paired Blender package, not a game installation component.

## 2026-10-23 工具更新

DLL 继续使用 v1.9.0，本次未修改或重新发布 DLL。新增 EFF Repair v0.3.0，配合 AnimeStudio v1.8.1 与 Blender v0.48.0 共享本机完整索引位置和下载缓存，避免为每个工程复制全量索引；完整索引包内容保持不变。旧源包不会仅因全局指纹、应用版本或索引路径变化而自动重解包；缺少可靠逐 Bundle 记录时保留原文件，仍可显式刷新。
