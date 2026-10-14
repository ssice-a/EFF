# EFF v1.6.0

配套：[EFF Blender v0.43.0](https://github.com/ssice-a/EFF-blender/releases/tag/v0.43.0)、[AnimeStudio v1.5.0](https://github.com/ssice-a/AnimeStudio/releases/tag/v1.5.0)。

- 形态动画支持多网格共用一个播放组、时钟与速度控制。同一网格和频道可以用于多个组；启动共享频道的新组会停止冲突组，包括暂停的组。不同频道仍可同时播放。
- 修复连续 F10 因重复加载内容相同的 AssetBundle 被 Unity 拒绝的问题；已完成且验证过的容器在代际间共享持有，最后一个持有者退出时释放。
- 修复热重载后部分 Mod 未准备形态动画状态而导致的控制更新异常。
- 配套编译器适配原 PFB 骨骼引用数量与源 Mesh 不一致的情况，使用已有可靠映射和完整骨架路径解析；保留缺骨、歧义及骨架归属检查。
- 正式包使用静态 C/C++ 运行实现，排除性能测点和资源调查探针。

退出游戏后安装 `EFF_v1.6.0_dll.zip`，保留自己的 eff.ini、Mods 和 state.ini。制作形态动画请同时更新 Blender v0.43.0；新动画导出要求配套两端声明共享频道播放能力。当前代理支持 Windows 系统目录为 C:\Windows 的环境。

回归覆盖多组播放仲裁、连续资源重载、共享 Bundle 退出、控制准备、实际 PFB 骨骼引用及 Blender 完整隔离导出。新动画与连续 F10 的最终游戏画面仍需验收；游戏更新自动补齐新命中 PFB 尚未接入完整生产运行链。

## English

Shape animation groups now support multiple meshes and shared channels. Starting a group replaces conflicting playing or paused groups; disjoint channels can play together. Repeated F10 reuses verified, completed bundles with shared lifetime ownership and prepares animation state for every Mod.

The paired compiler handles mismatched native Renderer/Mesh bone-palette counts using verified bindings and exact hierarchy paths, retaining missing/ambiguous/foreign-skeleton rejection. The production archive excludes performance and resource probes.

Quit the game before installing; preserve configuration and Mods. Use Blender v0.43.0 for new animation exports. Regression and isolated real-resource checks passed; final in-game visual validation remains pending. Automatic discovery and adaptation of new matching Prefabs after game updates is not included.
