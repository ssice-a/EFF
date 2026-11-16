# 面板、资源检查与 Blender 架构修复

日期：2026-11-16。接续 [修复前审查](architecture-review-20261116.md)。修改位于现有四仓库的 `main` 工作树，包含未提交内容；本轮没有推送、发布或修改用户工程和 Mod。

## 实现状态

| 项目 | 本轮修复 | 验证范围 |
| --- | --- | --- |
| 面板图形失败 | 原 ImGui 后端报告阶段、HRESULT、适配器和设备移除原因；硬件 DirectComposition → 硬件 HWND → WARP HWND 降级，Wine 跳过 DirectComposition；图形失败后保留窗口开关和控制快照，3 秒后重建 | Windows/NVIDIA 真实 D3D11 与故障注入通过；尚未在 AMD＋RTSS 或 Proton 验收 |
| Issue 7 多目标检查 | 已有 `Inspection` 在单次请求内共享 Layout、逻辑路径容器、owner 预载区间和 Mesh 名称；未命中路径不读取预载表和对象名 | 真实 3 MB 游戏 Bundle 的 325 次未命中全部完成；正匹配、重复匹配、跨输入同名及预载所有权回归通过 |
| 准备失败策略 | 预算限制统一类型化；发现、完整准备和等待者保留失败类型；确定性失败随 manifest 缓存，暂态失败可重试，F10 新代失效 | 完整资源重载回归通过，包括跨调用者重复准备、保留原资源、不伪造未命中、新代恢复 |
| 材质动画 | 每份贡献保存源 Action 和 MATERIAL slot；当前激活 Action 不再决定已登记通道；导入的形态、骨骼、材质保持同组播放设置，采样率统一整数，保留原 INI 变量名 | 真实 Blender 多 Action 独立样本、切换/清空激活 Action、删除曲线、混合通道和 INI 往返通过 |
| 特效 DDS 往返 | 当前 INI 可达关系决定资源，旧作者快照只提供原生结构模板；新增和共享的 Texture 声明采用当前 DDS 像素、采样及 Material 引用 | 真实 `lizi` 隔离副本的新增 DDS、共享引用、重绑、移除不再使用的旧声明、外部修改和精确像素往返通过 |

面板继续使用现有 Snapshot / Command 队列与独立 UI 线程，没有新增游戏 Present 钩子或另一套控制逻辑。ImGui DX11 的 GPU 对象创建和 Map 失败改为带阶段的异常，清理部分对象并恢复渲染状态，避免已复现的断言终止宿主路径。第三方钩子自身阻塞或原生访问异常仍需对应机器证据，不能仅凭本机通过认定 AMD/Proton Issue 已解决。

资源检查只覆盖游戏实际请求的原生输入，没有新增离线索引或导出扫描，没有提高 8 MiB 候选检查 / 32 MiB 依赖准备预算。请求内解析结果复用与跨代资源常驻是不同的生命周期；前者随请求释放。确定性准备失败缓存随 manifest/F10 失效，不将预算错误解释成可靠未命中。

特效导回不再用旧 `resourceBindings` 作为新增 DDS 的隐藏白名单。共享 Texture ID 对应同一作者身份；DDS 不存在或资源声明类型不兼容时报告具体错误，不能静默回退到游戏贴图。本轮未验证任意新增 Mesh/Material 原生槽重映射，不能将 Texture 回归扩大解释为全部资源往返已验收。

## 关键实测

输入 `bundles/windows/main/194592d13240f45fcbc6e9fc.ab`，文件 3,052,638 B、36 个块；仍使用 8 MiB 检查预算。

| 指标 | 修复前 | 修复后 |
| --- | ---: | ---: |
| 查询结果 | 第 4 个未命中查询耗尽预算 | 325 个查询全部完成 |
| 累计编码读取 | 8,374,782 B | 155,651 B |
| 累计解压 | 12,595,936 B | 262,144 B |
| 修复后实际读取 / 块解压次数 | — | 4 / 2 |

修复前另一次 32 MiB 预算测试在第 16 个查询失败。统计是累计 I/O，不是 Bundle 文件大小或常驻内存。修复后记录见 真实输入日志 (local: `diagnostics/architecture-repairs-20261116/native-inspection-fixed.log`)。

原生测试：`native_random_access_smoke` 与 `full_resource_reload_smoke` 全套通过。面板窗口布局、交互、Resize、控制队列、跨线程关闭以及 图形恢复 (local: `diagnostics/architecture-repairs-20261116/panel-graphics-recovery.log`) 通过；真实 Shader、Texture、SRV、顶点 Map、常量缓冲 Map 五类失败均被捕获，移除故障后重建并成功绘制，见 注入日志 (local: `diagnostics/architecture-repairs-20261116/imgui-failures.log`)。

Blender 作者/更新 Python 回归 243 项，242 通过、1 跳过。真实 Blender 的材质 Action、统一动画 UI、混合 Action 和形态通道回归通过；最终包内的 INI 控制往返 7 项 (local: `diagnostics/architecture-repairs-20261116/packaged-controls-import.log`) 及 真实特效 DDS 往返 (local: `diagnostics/architecture-repairs-20261116/packaged-effect-roundtrip.log`) 通过。后台 Blender 均使用 `--python-exit-code 1`，测试失败不会以正常退出码掩盖。

## 本机部署

游戏生产 DLL 已通过生产检测并安装到 `D:\Hypergryph Launcher\games\Endfield Game\plugin\eff.dll`，无 RenderDoc 或动画探针。大小 3,724,800 B，SHA256：

```text
E8ACD2B5B7D61DD6DA17839190189E9CA30EEEC577867615BDD155CE93B133B9
```

安装保留原 `eff.ini` 与用户 Mod，旧 DLL 在部署备份中。回执：Dll.json (local: `bin/local-deployments/20261116-142322-398/Dll.json`)。

Blender 版本为 0.50.36，使用现有事务安装器部署至 `C:\Users\25487\AppData\Roaming\Blender Foundation\Blender\5.2\scripts\addons\EFF_Blender`。已校验全部 72 个包内文件和游戏 DLL 的安装摘要，见 安装核验 (local: `diagnostics/architecture-repairs-20261116/installed-verification.json`)、插件安装及备份回执 (local: `diagnostics/architecture-repairs-20261116/Blender-install.json`)；随后从真实安装目录加载插件的 7 项 INI 往返测试 (local: `diagnostics/architecture-repairs-20261116/installed-controls-import.log`) 通过。共享作者 DLL 本轮未改，已核对工作区选中版本与本机作者构建摘要一致。用户确认 Blender 未运行；不结束无窗口的残留进程，下次正常启动加载新版。

尚未执行 AMD＋RTSS、Proton、游戏画面与新 DLL 的 F10 实机验收。这里记录实现及自动回归完成，不将其当作用户机器表现或 GitHub Issue 关闭依据。
