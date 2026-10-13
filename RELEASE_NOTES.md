# EFF v1.5.0

配套：[EFF Blender v0.42.0](https://github.com/ssice-a/EFF-blender/releases/tag/v0.42.0)、[AnimeStudio v1.5.0](https://github.com/ssice-a/AnimeStudio/releases/tag/v1.5.0)。

- 新增 Shape Keys 动画播放：独立组支持快捷键播放/停止、循环和初始自动播放；Ins 提供播放、暂停、停止、直接输入倍率及恢复作者默认。
- 调整倍率不跳进度，玩家倍率保存；手动形态滑块或快捷键先停止关联动画。同 Mod 的多个实例共享播放时间，播放不逐帧重建 Mesh/PFB。
- 配套编译器减少重复来源分析、骨架查询与装配编码开销；重新导出保留已提交的额外 PFB 命中记录，并用本次作者内容重新构建。
- 保留静态 C/C++ 运行实现和游戏原生模型生命周期重载，正式包不包含性能测量或资源调查探针。

退出游戏后安装 `EFF_v1.5.0_dll.zip`，保留自己的 eff.ini、Mods 和 state.ini。制作动画需同时更新配套 Blender 插件；形态动画包要求两端声明 sampled-shape-weight-clips/le32/1。未包含动画且结构仍兼容的当前 Mod 可继续使用；更旧 compiled.bin 需要配套工具重新导出。当前代理支持 Windows 系统目录为 C:\Windows 的环境。

作者采样、原生播放器、偏好、Python/C++ 往返和真实资源隔离编译已回归；新形态动画的实机显示及活跃动画性能仍待验收。新增物理生命周期和大量 Mod 压力也未全面实测。游戏更新自动补齐旧 Mod 的新命中 PFB 尚未接入完整生产运行链，本次不提供该能力；游戏来源变化仍需作者刷新来源并重新导出。

## English

Independent Shape Keys clips support shortcuts, looping and autoplay. Ins exposes play, pause, stop and persistent live speed controls without resetting phase. Manual shape controls stop their associated clip; instances of one Mod share playback time. Playback does not rebuild Meshes or Prefabs per frame.

The paired compiler reduces repeated source, skeleton and assembly work and preserves committed additional Prefab matches across re-export. The production archive excludes temporary performance and resource probes and retains the self-contained C/C++ runtime.

Quit the game before installing and preserve configuration, Mods and player state. Animation exports require the paired Blender add-on and runtime. Authoring, native controls, persistence and isolated real-resource compilation have regression coverage; new animation rendering and active-playback performance remain unverified in-game. Automatic adaptation of old Mods to new matching Prefabs after game updates remains under development and is not included.
