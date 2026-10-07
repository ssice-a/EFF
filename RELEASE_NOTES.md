# EFF v1.3.0

配套：[EFF Blender v0.40.0](https://github.com/ssice-a/EFF-blender/releases/tag/v0.40.0)、[AnimeStudio v1.3.0](https://github.com/ssice-a/AnimeStudio/releases/tag/v1.3.0)。

- 统一静态来源与小型替换规则：INI 指定源 Mesh，离线确定其全部 Prefab/Renderer 消费者；游戏加载资源前应用规则，未声明内容保留原版。
- F10 读取整个 mods 集合，支持多角色 Mod 增删及 Mesh、材质、贴图更新；准备或提交失败保留当前代际。
- 修复 LoadModel 内部清理误删新请求归属造成的重复 F10 拒绝，并沿游戏模型生命周期重新绑定动画、武器和角色音频。
- 修复默认隐藏部件污染游戏 Renderer 初始状态，恢复袖子三状态和其他部件显隐；保留游戏真实禁用意图。
- Ins 默认选择首个可控制 Mod，物理按键只作用于所选 Mod；玩家数值保存在各 Mod 的 state.ini。
- 允许从同一游戏材质生成多个独立作者副本，各自保持参数与贴图槽。
- 复用已准备候选，避免无改动 F10 重复启动编译器；未访问 Prefab 延后创建模板。

## 下载与安装

- **EFF_v1.3.0_dll.zip**：默认 v35，使用本轮实机验证的布局。
- **EFF_v1.3.0_dll_v36.zip**：v36 构建，原生事务回归通过，尚未实机验证；两者择一安装。

退出游戏，将 ZIP 解压到 Endfield.exe 同级目录。包含两种代理加载器、plugin/eff.dll、eff.ini 模板、协议、配套编译器、来源工具及空 mods 目录。升级时保留自己的 eff.ini、Mod 和 state.ini。完整 Mod 文件夹直接放入 plugin/mods/；导出或增删完成后按 F10，Ins 选择按键目标。更新 DLL 需要重启游戏，F10 只更新资源与规则。

## 验证边界

用户确认当前 v35 袖子及部件切换恢复正常；世界热重载此前已有实机反馈。v35/v36 回归覆盖连续世界/展示重载、动画消费者、失败回滚及集合组合。其他角色/NPC 的完整实机覆盖、20–30 个 Mod 的帧率/内存压力及完整多代资源退休仍待验证。Physics/Skeleton 作者工具尚未接入完整游戏模拟链。
