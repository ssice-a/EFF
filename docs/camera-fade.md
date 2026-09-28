# 相机反虚化：CameraMono 调用契约

## 当前方案

只改变相机渐隐，不做 UI 绘制、景深处理或模型资源替换。
旧 Renderer setter hook 在真实日志中未命中，已删除；失败证据保存在

运行时从 IL2CPP 元数据查找 `Beyond.Gameplay.View.CameraMono` 的两个零参数实例方法：
`_ProcessDitherByPitch()` 与 `ForceClearDither()`。不硬编码地址、偏移或角色名字。

每次游戏调用 `_ProcessDitherByPitch`：

1. 先调用原函数，原样传递当前实例与 MethodInfo。
2. 插件启用且 `disable_camera_fade=true` 时，在同一个实例调用 `ForceClearDither`，传入它自己的 MethodInfo。
3. 关闭功能时只执行原函数。原函数失败不吞异常，也不继续清理。

**纠正旧说法：不能把 `ForceClearDither` 理解为一次调用即可永久关闭渐隐。**
下一次相机评估可以重新施加渐隐，因此采用每次原评估之后清理，而非只在初始化或 F10 时调用。
不持有 CameraMono/Renderer 对象，不建弱引用表，不扫描场景，也不增加相机创建/销毁 hook。

## 配置与刷新

唯一全局文件仍为游戏目录下 `plugin/eff.ini`：

```ini
[Hotkeys]
reload=F10
gui=INSERT

[Graphics]
disable_camera_fade=true
```

新安装模板默认 true。修改后按配置的 reload 键发布设置，下一次相机评估读取新值。
F10 不重装 hook，也不在按键线程直接调用游戏相机方法；它不具有 DLL 热替换能力。
更换 DLL 首次需重启游戏。关闭功能后由后续游戏评估恢复原行为；如果相机暂停评估，
不能承诺设置发布的一刻就恢复画面。

非法配置不发布，保留上一份有效设置并报错。Mod 原有 F10 重载、mesh/skip 语义不变。
管理器开关与配置共同决定是否清理，不需要逐对象恢复调度。

## 依据与边界

参考项目在固定提交中采用原评估之后清理的调用顺序：
[Endfield-Uncensored CameraMono hook](https://github.com/DynamiByte/Endfield-Uncensored/blob/509d1b8b6ad0cec9787af7b055984d955c1f0461/src/dll.zig#L275-L293)。
仅参考游戏方法与调用契约；不复制其手写跳转补丁、快捷键轮询或 UI，使用本项目 MinHook 与全局配置。

本机 `eff_il2cpp_classes.txt` 列出 Gameplay.Beyond.dll 下的 CameraMono；
当前 global-metadata.dat 含 `_ProcessDitherByPitch`、`ForceClearDither` 名称。
资源详情 dump 因资源类过滤不列出 CameraMono，不能据此推断相机类不存在。
元数据名称存在只证明可继续解析，不证明运行时一定经过该入口。

本方案不直接写 Renderer.enabled、Mod handling 或材质槽。
但游戏原生 ForceClearDither 对遮挡、技能或不同 UI 相机的作用范围仍需画面核验，
不能从方法名或模拟测试推断所有场景都覆盖且完全无副作用。景深、运动模糊、TAA 不属于此方案。

## 清理与测试

删除 v34 的 Renderer enableCameraDither setter hook、逐 Renderer 弱引用恢复表及专用 weakref API 绑定。
移除 F10/管理器对旧恢复表的调度，仅保留配置发布与状态日志。

`tests/test_camera_fade_hook.py` 使用真实 MinHook、生产 adapter 与模拟 CameraMono 方法验证：

- 一次清理后再评估会重新渐隐的反例。
- 两个相机连续评估，同实例 original → clear 顺序及各自 MethodInfo；地址复用无旧对象记录。
- 配置反复开关、非法配置保留、管理器开关。
- 原评估失败不会吞异常或继续清理。
- 类、方法、参数个数或地址缺失时拒绝安装，不猜地址、不转旧实现。

历史回归曾在旧 v34 adapter 上复现错误；当前测试覆盖 CameraMono 入口。
模拟对象里的 skip/manual 字段不变，只验证 adapter 未直接修改它们，不证明真实游戏副作用。
`tests/test_camera_fade.py` 保留真实全局配置解析与发布测试。

### 日志验收

- `[BUILD]`：核对实际加载版本；以当前部署 DLL 的标识为准，不固定要求 v35。
- `Installed`：MinHook 安装完成，不等于画面成功。
- `observed`：真实原评估返回，首次记录。
- `cleared`：真实 ForceClearDither 返回，首次记录。
- `status ... evaluations=... clears=...`：初始化、F10、管理器切换时报告累计次数。
- `Unavailable`：元数据或 hook 安装失败。

游戏画面需要核验拉近镜头、开关往返、换地图、角色 UI，并与日志一起判断。
编译及模拟 MinHook 测试不能替代该验收。历史自动验证与部署结果见版本记录。
