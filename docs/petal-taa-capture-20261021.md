# 花瓣 TAA 残影抓帧部署

2026-10-21 02:13（北京时间）。用户反馈将花瓣 `m_MotionVectors` 从 2 改为 1 后仍有残影，授权部署对应抓帧 DLL。本轮没有继续修改粒子、材质、Shader 或用户 Blender 工程，也没有启动/停止游戏。

## 依据与选择

- 项目 `src/README.md`、`src/proxy_vulkan_full.cpp` 和 `tools/diagnostics/build_resource_capture_proxy.cmd` 保留抓帧入口：诊断 Vulkan 代理在创建 VkInstance 前启用注册的 GfxTrace 隐式层，继续加载 plugin DLL。
- 配套本机 `E:/renderdoc/vulkanproxy/README.md`、`Manage_Endfield_Proxy.ps1` 要求进程停止、先备份、按 SHA256 核对，并说明 F12 手动抓帧。其单独代理不负责 EFF 插件加载，因此本次构建本仓库代理，沿用当前注册的配套 GfxTrace。
- 现行 `eff.dll` 无需切换成 shape-capture 构建。该构建包含额外形态诊断与特定快照后的自动抓帧，本轮需要的是大世界花瓣出现后手动抓一帧。

构建命令：

```powershell
cmd /c tools\diagnostics\build_resource_capture_proxy.cmd bin\petal-taa-capture-20261021\proxy_with_path.cpp
```

`proxy_with_path.cpp` 仅定义本轮绝对输出前缀，再包含当前仓库代理源码。没有更换插件加载实现。

## 已部署

- 目标：`D:/Hypergryph Launcher/games/Endfield Game/vulkan-1.dll`。
- 抓帧库：`E:/renderdoc/x64/Release/gfxtrace.dll`，1.46，沿用已存在的 HKLM Vulkan ImplicitLayers 注册；没有写注册表或复制额外抓帧库到游戏。
- 保存前缀：`E:/vscode/EIEM/diagnostics/petal-taa-capture-20261021/petals`。
- 手动按键：F12；默认关闭启动自动抓帧。
- 备份：`bin/petal-taa-capture-20261021/backup/vulkan-1.dll`。
- 部署记录：`bin/petal-taa-capture-20261021/deployment.json`，包含新旧 SHA256、抓帧库实际 SHA256 和检查文件快照。

构建成功，x64 PE 导出名、序号及系统 Vulkan 转发目标与原代理完全一致，导入的运行库可加载。确认 Endfield 和 Launcher 均未运行后替换，再次校验 SHA256。334 个检查文件保持，包括 `plugin/eff.dll`、全部 Mod、协议、配置和 DirectX 代理。当前 GfxTrace 的旧 stable_manifest.json 中 DLL 哈希与本机现有文件不同，本轮按实际文件哈希记录，不将旧清单当作当前二进制验收。

## 用户操作与后续检查

1. 正常启动游戏，进入能看到花瓣残影的大世界场景。
2. 等花瓣持续发射，保持镜头静止，让移动花瓣及拖尾留在画面中。
3. 按 F12 一次，等待抓帧写盘完成。文件位于上述输出目录，文件名以 `petals` 开头，扩展名 `.rdc`。
4. 不必额外用 RenderDoc 启动游戏，不必填写 PID。若没有文件，先核对本次日志、加载的 GfxTrace 模块和抓帧层，而非继续改材质参数。

部署并不证明抓帧成功，也不证明残影修复。拿到帧后应检查花瓣真实 Draw 的顶点输入、当前/上一帧位置常量、HGBuffer 运动输出、深度/模板与 TAA 输入，区别缺少每粒子运动、标记不符和抖动覆盖造成的历史残留。

诊断完并退出游戏/启动器后，可恢复原代理：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File E:\vscode\EIEM\bin\petal-taa-capture-20261021\restore_capture.ps1
```

恢复脚本先校验当前代理和备份哈希；不匹配则拒绝覆盖，不修改 Mod 或其他 DLL。
