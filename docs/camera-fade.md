# 相机反虚化：CameraMono 调用契约

## 当前方案

资源入口通过 `eiem_entry_camera_fade.h` 与纯核心 `eiem_camera_fade_core.h` 接入相机渐隐控制。
运行时从 IL2CPP 元数据精确查找 `Beyond.Gameplay.View.CameraMono` 的两个零参数实例方法：
`_ProcessDitherByPitch()` 与 `ForceClearDither()`，返回类型均为 `System.Void`。
不硬编码地址、偏移或角色名。缺失、签名歧义、相同地址或 hook 失败均拒绝接入，
相机模块失败不阻断独立的资源输入和控制模块。

每次游戏调用 `_ProcessDitherByPitch`：

1. 调用原函数，原样传递当前实例与 MethodInfo。
2. 插件启用且 `disable_camera_fade=true` 时，在同一实例调用 `ForceClearDither`，
   传入它自己的 MethodInfo。
3. 关闭功能时只调用原函数。原函数失败不吞异常，也不继续清理。

下一次游戏评估可以重新施加渐隐，因此每次原评估之后都需要清理。
模块不持有 CameraMono/Renderer 对象，不建立逐对象恢复表，不扫描场景。

## 配置与刷新

唯一全局文件为游戏目录下 `plugin/eff.ini`，当前模板为：

```ini
[Hotkeys]
reload=F10
gui=INSERT

[Graphics]
disable_camera_fade=true
```

修改后按当前生效的重载键请求设置；Unity 帧提交成功后发布，新评估读取新值。
非法候选保留此前设置。管理器插件启用状态与配置共同决定是否清理。
重载不重装 hook，不在按键线程调用相机方法，也不热替换 DLL。
更换 DLL 需要重启游戏。关闭后由游戏后续评估恢复原行为；
相机暂停评估时，不能承诺发布设置的一刻就恢复画面。
`gui` 的配置保留不代表当前宿主已提供 Mod Lua 窗口。

## 依据与边界

参考项目在固定提交中采用原评估之后清理的顺序：
[Endfield-Uncensored CameraMono hook](https://github.com/DynamiByte/Endfield-Uncensored/blob/509d1b8b6ad0cec9787af7b055984d955c1f0461/src/dll.zig#L275-L293)。
只参考游戏方法与调用契约，本项目使用 MinHook 和严格全局配置。

元数据名称存在只证明可继续解析，不证明实际经过入口。
本模块不直接写 Renderer.enabled 或材质槽；游戏原生 ForceClearDither
对遮挡、技能和 UI 相机的真实作用仍需画面核验。景深、运动模糊和 TAA 不在范围内。

## 当前检查与实机验收

`tools/diagnostics/build_entry_camera_smoke.cmd` 构建当前入口的 CameraMono fixture，
产物为 `bin/resource-directory-entry/entry_camera_smoke.exe`。它使用真实 MinHook、
生产 Exact adapter 与模拟方法检查同实例 original → clear、MethodInfo、配置/插件开关、
重复评估、异常传播和缺失方法拒绝。模拟成功不能代替实机画面验收。

日志核对：

- `Installed`：hook 安装完成。
- `observed`：真实原评估返回。
- `cleared`：真实 ForceClearDither 返回。
- `status ... evaluations=... clears=...`：累计调用计数。
- `Unavailable`：元数据或 hook 安装失败。

游戏内需要检查拉近镜头、开关往返、换地图和角色展示，并结合对应 DLL 的日志。
当前版本及重载验收边界见[热重载测试](hot-reload-testing.md)，模块关系见[代码架构](code-architecture.md)。
