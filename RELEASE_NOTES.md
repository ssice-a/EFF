# EFF v1.3.2

配套：[EFF Blender v0.40.1](https://github.com/ssice-a/EFF-blender/releases/tag/v0.40.1)、[AnimeStudio v1.3.1](https://github.com/ssice-a/AnimeStudio/releases/tag/v1.3.1)。

- UIModelLoader 完成加载后接入与大世界共用的 Mod 控制值；Ins 选择控制目标，小键盘切换沿用作者配置。
- UI F10 使用游戏异步加载与页面项 BindGameObject 更新模型，保留页面和编队缓存的项目身份，以及当前层、位置、显隐、动画参数与播放状态。
- 候选完成校验和页面接管后才释放旧人物；取消、关闭页面、新请求覆盖或候选失败时保留原显示，部分提交失败执行回滚。
- 保留多角色 Mod、完整目录增删改、空集合恢复、静态 Mesh、材质贴图及形态控制；DLL 继续读取 Blender 生成的独立编译结果。
- 一个正式 DLL 安装包，只包含运行 DLL、代理加载器、设置模板、结构声明和文档。

下载 **EFF_v1.3.2_dll.zip**，退出游戏后解压到 Endfield.exe 同级目录，保留自己的配置与 Mod。已有 v1.3.1 的 compiled.bin 无需重导出。旧 Mod 请用配套 Blender 重新导出以生成 compiled.bin。旧 plugin/tools 编译器与来源运行库不再使用。F10 更新 Mod 资源；DLL 更新需要重启游戏。

生产构建、原生事务与生产 Lua 页面回归通过。实机连续四次 F10 提交世界/UI 资源与页面引用，耗时约 97–100 ms；最终 UI 画面、动作与显隐仍待确认。无法确认实际页面项归属的 UI 实例在修改前拒绝原地 F10。独立人群 NPC/场景物件原地刷新、多代资源退休、20–30 个 Mod 压力及新增 Physics 模拟尚未完整验收。
