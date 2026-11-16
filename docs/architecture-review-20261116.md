# 面板兼容、Issue 7 与资源架构审查

核对日期：2026-11-16（Asia/Shanghai）。对象为四个 `main` 工作树的当前源码，包含未提交修改；不是仅核对远程 HEAD，也不将旧报告中的成功记录作为本轮验收。

本文保留修复前的审查与复现结论。后续实现、回归、本机部署及尚未完成的实机验收见 [逐项修复记录](architecture-repairs-20261116.md)，不要将下表的旧状态当作当前实现状态。

初始审查仅完成只读源码审查、真实 Bundle 检查、后台 Blender 复现及独立图形测试，没有修改生产实现或安装程序。

## 结论

| 问题 | 当前判断 | 证据与限制 |
| --- | --- | --- |
| AMD + MSI Afterburner 打开 Ins 卡死/闪退 | 已找到可导致进程退出的图形错误处理路径，但未证明它就是反馈中的根因 | 独立进程注入真实 D3D11 Shader 创建失败后触发 ImGui 断言，退出码 3；本机 NVIDIA，没有 AMD + RTSS 实机复现 |
| [Linux / Proton 面板 #2](https://github.com/ssice-a/EFF/issues/2) | 仍未修复；已经使用 ImGui，缺口在窗口/图形后端兼容与失败恢复 | 当前只支持硬件 D3D11 + DirectComposition；Issue 只有通用 `unavailable/restartRequired`，不能定位具体失败步骤 |
| [最新 Issue #7](https://github.com/ssice-a/EFF/issues/7) | 仅缓解部分重复失败，完整问题仍存在 | 相同约 3 MB 游戏 Bundle 在多目标查询下累计读取到约 33 MB 后失败；单请求复用容器语义的对照消除该读取放大 |
| 架构一致性 | 有两项新增实证缺陷，触及 Action 来源及 INI 权威 | 同材质多 Action 串组；INI 新增 DDS 的特效导回静默退回游戏贴图 |

## P1：面板图形失败可以退出宿主，且没有兼容呈现路径

面板已使用 ImGui 的 Win32、DX11 后端，见 [面板实现](../src/eiem_control_panel_win32.h#L3)。ImGui 控件可复用，并不意味着 DXGI、DirectComposition、DWM 和 Windows 输入已经在 Proton 下适配。

当前 [Graphics](../src/eiem_control_panel_win32.h#L100) 固定创建默认硬件 D3D11 设备，随后必须成功创建 composition swap chain、DirectComposition device、HWND target、visual 并提交。没有普通 HWND swap chain 路径，也没有软件设备降级。Backdrop 的 DWM 属性失败可以忽略，但图形呈现链路失败会直接关闭面板；这两者不能混为一谈。

首次初始化 [ImGui_ImplDX11_Init](../src/eiem_control_panel_win32.h#L274) 不创建全部 GPU 对象。[首次 Tick](../src/eiem_control_panel_win32.h#L284) 调用后端 NewFrame，若创建 Shader 等设备对象失败，[后端直接断言](../deps/imgui/imgui_impl_dx11.cpp#L687)。纹理创建和 ShaderResourceView 创建失败也有类似断言。当前 [ImGui 构建](../tools/build_imgui.cmd#L26) 保留断言；`catch(std::exception)` 接不住 CRT abort。仅关闭断言并不会建立正确的失败处理。

本轮隔离测试使用真实 D3D11 device、真实 ImGui DX11 后端，通过 MinHook 仅将 `CreateVertexShader` 改为返回 `E_FAIL`：

```text
BACKEND_INIT_SUCCESS
INJECTED CreateVertexShader E_FAIL
Assertion failed: 0 && "ImGui_ImplDX11_CreateDeviceObjects() failed!"
```

子进程退出码为 3，没有执行异常处理。证据：夹具 (local: `diagnostics/panel-compat-audit-20261116/first_frame_failure.cpp`)、输出 (local: `diagnostics/panel-compat-audit-20261116/first-frame-shader-failure.log`)。测试宿主使用 `/MD`，与生产共享同一后端源码和启用断言的构建规则；它证明该错误路径能终止进程，不证明 AMD、Afterburner 或 RTSS 必然令 Shader 创建失败。

`Graphics`、`ResizeBuffers`、`Present` 当前均未保留具体 HRESULT、失败阶段、适配器或 `GetDeviceRemovedReason`。[Run](../src/eiem_control_panel_win32.h#L306) 将失败转换为永久 `store.failed`，随后 [Ins 处理](../src/eiem_resource_entry.cpp#L1662) 仅报告需要重启。Issue #2 的日志只能证明进入了此状态，不能确认 DirectComposition 的哪一步不受支持。第三方钩子造成的阻塞或 native 异常也不受普通 C++ catch 的隔离。

收敛方向：将现有窗口/图形模块集中负责后端选择、阶段诊断、GPU 对象失败、设备恢复和呈现降级；共用现有 ImGui 内容。先消除可处理图形失败导致进程退出的路径，再提供普通 HWND 呈现及明确恢复策略。AMD + RTSS 和 Proton 需要分别验收；不将 Windows/NVIDIA 通过写成两者通过。

保留 [Snapshot / Command](../src/eiem_control_panel.h#L15) 队列与 Unity 执行分离，保留独立 UI 线程和不在游戏 Present 上新增钩子的设计。它们解决控制与实例线程归属，无须再建立另一套 Mod 控制程序。

## P1：Issue 7 的多目标读取放大仍在

Issue #7 指向的显式限制仍存在：[候选检查 8 MiB](../src/eiem_entry_demand_loading_probe.h#L541)、[依赖准备 32 MiB](../src/eiem_entry_demand_shared_inputs.h#L839)。修改 `Limits` 默认值不会覆盖这两处设置。

但预算耗尽不能一概解释成 Bundle 本身或 metadata 超大。[Reader 的预算](../src/eiem_native_random_access.h#L199) 按累计编码读取量计算；同一范围被重复读取也重复计费。Reader 本来已按范围读取，不是每次完整解压整个文件。

[Inspection](../src/eiem_demand_target_lookup.h#L13) 只缓存 SerializedLayout。[Find](../src/eiem_demand_target_lookup.h#L31) 对每个 unresolved target 再次调用 `Assets` 和 `ContainerPreloads`，重复读取容器、预载关系和对象名；有限解压缓存发生淘汰时会重复读块和解压。

真实游戏 `main/194592d13240f45fcbc6e9fc.ab`，文件 3,052,638 B、36 块，查询 325 个未命中目标：

| 检查方式 | 结果 | 累计编码读取 | 累计解压 |
| --- | --- | ---: | ---: |
| 当前实现，32 MiB 预算 | 第 16 个目标失败 | 33,489,217 B | 50,252,672 B，387 次块解压 |
| 当前实现，8 MiB 预算 | 第 4 个目标失败 | 8,374,782 B | 12,595,936 B |
| 诊断对照，单请求复用容器语义，仍为 8 MiB | 325 个查询完成 | 2,426,901 B | 3,640,504 B，28 次块解压 |

对照仅证实未命中查询的重复读取可避免，不代表已实现正匹配、跨 Bundle PPtr 或完整运行时修复。Issue 中另外两个输入在同样 325 查询下并未耗尽预算，不能笼统归因于所有大 Bundle。完整命令、日志和限制见 真实 Bundle 诊断 (local: `diagnostics/issue7-native-budget-audit-20261116/README.md`)。

已有 [InspectTargetInput](../src/eiem_entry_demand_shared_inputs.h#L61) 能按 manifest 缓存累计读取预算失败，并与可靠未命中区分。历史同 Bundle 由重复报错降到一次，确实缓解卡顿；失败的 Bundle 仍未成功完成资源发现。完整准备中的 blocked token 和 [回滚失败路径](../src/eiem_entry_demand_shared_inputs.h#L1322) 没有完全采用同样的分类，仍有级联重试风险。字段、metadata、block 预算也并非全部使用类型化失败。

收敛方向：把容器、预载区间和逻辑路径查询结果收进已有 `Inspection` 的单请求生命周期；多个目标共享一次读取，正匹配才继续读必要对象和外部引用。无需增加离线索引、导出扫描或永久消费者清单。发现和完整准备共用“命中 / 可靠未命中 / 确定性失败 / 可重试失败”，失败状态随输入与代际失效。预算失败不能伪装成无目标。

## P1：材质动画来源依赖当前激活 Action

[材质动画记录](../tools/Blender/eiem_material_animation.py#L133) 有组身份和 slot handle，却没有形态/骨骼通道持有的源 Action 引用。[action_tracks](../tools/Blender/eiem_material_animation.py#L222) 读取当前 `material.animation_data.action`，再按属性取首个匹配元数据；[source_actions](../tools/Blender/eiem_shape_animation.py#L80) 也将该材质的所有组指向当前 Action。

后台真实 Blender 构造同一材质两个 Action：A 的亮度 1→2，B 的亮度 8→10，两组均注册。实际只导出 A，样本却是 B 的 8→10；两组显示的源 Action 都是 B。证据：复现和 JSON (local: `diagnostics/architecture-review-20261116/README.md`)。

这与 0.50.35 的 Mesh 范围修复不同：该修复正确删除了“所有成员必须同时导出”的限制，但没有补齐多 Action 的稳定来源身份。单个 Action 多通道的通过记录不能覆盖同材质多个 Action。

收敛方向：材质贡献像形态/骨骼一样保存源 Action 与 slot，按该 slot 的实际曲线生成通道；共享组仅持有播放设置。切换活动 Action 只影响编辑器，不应重新解释已注册动画。继续支持直接删除/新增曲线，无需用户维护第二份通道白名单。

## P1：特效导回的旧快照名单覆盖当前 INI

[overlay_resources](../tools/Blender/eiem_effect_author_resources.py#L381) 仅从旧作者快照的 `resourceBindings` 建立内部身份映射。材质引用当前 INI 中新增的 Texture ID 时，不在旧名单内，就在 [回退分支](../tools/Blender/eiem_effect_author_resources.py#L480) 被解释为纯游戏逻辑资源，忽略其 `dds`。

本轮把真实 `lizi` 复制到隔离目录，新增 1×1 DDS，RGBA 为 `[17,34,51,85]`，新增 Texture 声明并改绑材质。真实 Format 3 reader 校验新 Mod 有效；经过 `overlay → externalize` 后新像素丢失，材质引用变回游戏 `t_fx_flow_111_m.tga`。用户 Mod 未改。证据：脚本、像素与结果 (local: `diagnostics/architecture-review-20261116/README.md`)。

这直接违背 INI 资源关系权威。原生快照提供编辑结构/类型模板，不应该成为作者资源的隐藏白名单。收敛方向：按当前 INI 关系遍历实际可达资源，新引用根据真实声明区分作者 DDS 与纯游戏引用；快照身份只用于把编辑结构连接到当前资源，不决定资源能否存在。通用 Mesh、Material、DDS 和特效引用应复用这一规则。

## 架构保留与实施顺序

现有 Format 3 方向可以保留：INI 装配、普通独立 Mesh/Material/DDS、原生特效逻辑、统一控制队列、共享动画时钟和 Owner/代际退休。不同资源具有各自读取适配器不等于另一套框架；问题在于来源身份和权威规则没有在所有入口一致执行。

1. 面板：补阶段/HRESULT/适配器诊断，处理 GPU 对象创建失败与设备丢失，加入兼容呈现和失败恢复的测试接口。保留统一控制与 ImGui 内容。
2. Issue 7：集中单请求资源检查，消除多目标读取放大，统一失败分类；验证正负混合、外部 PPtr、多 owner、失败保留和 F10 新代恢复。
3. 动画：统一贡献的 Action/slot 来源；回归同材质多 Action、活动 Action 切换、删除曲线和部分 Mesh 导出。
4. 资源往返：以当前 INI 可达关系恢复资源；回归新增 DDS/材质/网格、资源改名、重绑和导回再导出，证明新像素与关系保持。

这些修改应落在现有模块的统一入口，而不是新增第二份控制清单、资源管理器或动画时钟。

## 本轮验证与尚缺证据

- 重新构建并运行当前真实 Windows 面板 smoke：布局、输入、Resize、控制与关闭通过。
- 跨线程关闭 smoke 通过，测试中游戏窗口暂停消息泵，关闭约 3.6 ms。
- Shader 创建失败注入：按预期复现进程断言退出；这是负例证据，并非修复后通过。
- 完整资源热重载模拟通过，包括预算失败跨调用者缓存、不伪造未命中、新代和暂态重试；该夹具不覆盖本次真实多目标读取放大。
- 真实游戏 Bundle 的累计读取问题、真实 Blender 的多 Action 错组、有效 Mod 新增 DDS 的往返丢失均已复现。
- 未做 AMD + Afterburner/RTSS、Linux Proton、游戏画面或修复后 F10 验收；本轮没有修复这些生产行为，也没有发布所谓兼容修复版。
