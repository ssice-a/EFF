# Mod Lua UI

现行边界：**DLL 是运行宿主，不定义 Mod UI 的打开方式、窗口数量或业务布局。**
用户写 INI 变量/按键和 Lua 绘制；Blender 只可选地生成一个简单模板，不代表 DLL 固定界面。
没有默认 Mod UI 快捷键。F8 只是旧模板的预填值，现已移除。
此前答复“关闭窗口销毁变量并恢复模型”是错误描述；关闭 UI 与模型状态独立，除非用户脚本显式修改模型变量。

本轮只接入独立 Mod UI、Lua 与现有变量系统；不修改用户模型、现有 Mod 配置或游戏资源替换算法。
`plugin/eff.ini` 的 `gui` 键控制内置管理器。管理器提供当前所选 Mod 的按键按钮与形态键滑块；Lua UI 仍可由 Mod 独立声明窗口。旧 `[Slider...]` 语法不再解析，Blender 形态键控制会导出 `[ShapeControl...]` 供内置管理器使用。

## 文件与语法

```ini
[Constants]
$size=0
$window=0

; 用户自行选择此键，F6 仅是这个例子的值，不是 DLL 默认值
[KeyWindow]
key=F6
scope=both
type=cycle
$window=0,1

[UICloth]
path=ui.lua
```

一个 UI section 只声明脚本文件，对应一个隔离的 Lua 环境；脚本可以绘制零个、一个或多个窗口。
UI 块没有 key/condition 字段，打开条件、关闭行为及多个窗口的协调全部由 Lua 决定。
普通 Key 块更新变量，不知道变量会被 UI 还是 Render 使用。可配置 scope=game（缺省）、ui 或 both，
决定游戏窗口、插件/Mod 窗口或两者有焦点时响应；不会在其他应用前台注册这些快捷键。
与用户全局 gui/reload 键冲突时报告并不注册重复快捷键，不暗中改为 F8。
没有自动轮询磁盘。默认 F10 重读 INI 与 Lua，关闭 UI 只隐藏窗口，不改变模型变量。
相对 Lua 路径限制在 Mod 目录内；不允许绝对路径、上级目录或重解析点越出 Mod 目录。

```lua
return function()
    if mod.get("$window") == 0 then return end
    imgui.SetNextWindowBgAlpha(0.8)
    local visible, open = imgui.Begin("衣服", true)
    if not open then mod.set("$window", 0) end
    if visible then
        local changed, value = imgui.SliderFloat("鼓起", mod.get("$size"), 0, 1)
        if changed then mod.set("$size", value) end
    end
    imgui.End()
end
```

Lua 返回每帧绘制函数，即使上一帧没有窗口也继续调用，以便脚本响应变量决定显示。
常显窗口可以不声明任何 Key，也不用 `$window`。按钮、滑条、窗口之间可用 Lua 局部状态或者本 Mod 变量协调。
窗口名与控件 ID 按 Mod/section 隔离，保留原生 `###` 稳定 ID 语义；同脚本的多个窗口使用不同名称/ID。
绑定常用公共 ImGui 接口，保持原生名称：Begin/End、Button、Checkbox、SliderFloat/DragFloat、
InputFloat、Text、布局、子窗口、表格、颜色/样式等。未绑定接口报错，不假装暴露完整 C++ API。

### 当前绑定签名

参数遵循 Lua 数值/字符串传值，不传 C++ 指针。控件 label 支持原生 `##id` 隐藏 ID。
Begin/End、BeginChild/EndChild 必须配对，即使 Begin 返回 false；BeginTable 返回 true 才 EndTable。
`Begin(title)` 不显示 ×；`Begin(title, open, flags)` 显示 × 并返回新的 open，是否保存这个值由脚本决定。
DLL 不擅自选择窗口开关的保存策略、快捷键或关闭行为；用户声明 persist 的变量由统一状态存储保存，
脚本通过同一变量事务控制模型，不另设窗口专用的模型恢复流程。

| 接口 | 参数及返回值 |
| --- | --- |
| `Begin` / `End` | `Begin(title, open=nil, flags=0) → visible, newOpen`；多个根窗口分别配对 |
| `BeginChild` / `EndChild` | `BeginChild(id, width=0, height=0, border=false) → visible` |
| `Button` / `RadioButton` | `(label, width=0, height=0) → clicked` / `(label, active) → clicked` |
| `Checkbox` | `(label, value) → changed, newValue`，value 是布尔值 |
| `SliderFloat` | `(label, value, min=0, max=1) → changed, newValue`，min < max |
| `DragFloat` / `InputFloat` | `(label, value, speed=.01, min=0, max=0)` / `(label, value, step=0)`，均返回 changed, newValue |
| `ColorEdit4` | `(label, r, g, b, a=1) → changed, r, g, b, a` |
| `Text` / `SetTooltip` | `(text)`，按原样文本绘制，不解析 printf 格式 |
| `SameLine` / `Dummy` | `(offset=0, spacing=-1)` / `(width=0, height=0)` |
| `Separator` / `Spacing` / `NewLine` | 无参数 |
| `SetNextWindowSize` / `SetNextWindowPos` | `(x, y, condition=0)`，条件由用户选择，不强制 FirstUseEver |
| `SetNextWindowBgAlpha` / `SetNextItemWidth` | `(alpha=1)` / `(width)` |
| `PushStyleColor` / `PopStyleColor` | `(imgui.Col.xxx, r, g, b, a=1)` / `(count=1)` |
| `PushStyleVar` / `PopStyleVar` | `(imgui.StyleVar.xxx, value[, value2])` / `(count=1)` |
| `PushID` / `PopID` | `(string)` / 无参数，作用域内配对 |
| `BeginDisabled` / `EndDisabled` | `(disabled=true)` / 无参数，作用域内配对 |
| `BeginGroup` / `EndGroup` | 无参数，作用域内配对 |
| `BeginTable` / `EndTable` | `(id, columns)`，1–64 列 / 无参数 |
| `TableNextRow` / `TableNextColumn` | 无参数；后者返回列是否可见 |
| `CollapsingHeader` | `(label) → open`，不需要 TreePop |
| `GetItemRectMin` / `GetItemRectMax` | 无参数 → x, y |
| `IsItemHovered` | 无参数 → hovered |

枚举：`imgui.WindowFlags` 提供 NoTitleBar、NoResize、NoMove、NoBackground、AlwaysAutoResize，可用 Lua `|` 组合。
`imgui.Cond` 提供 Always、Once、FirstUseEver、Appearing。
`imgui.Col` 提供 Text、WindowBg、Button、ButtonHovered、ButtonActive、FrameBg；
`imgui.StyleVar` 提供 Alpha、WindowPadding、WindowRounding、FrameRounding、FramePadding、ItemSpacing。
未提供 Image/外部纹理句柄、FFI 或所有 ImGui 重载；需要新绑定时增加这一个边界，不修改 Render 语义。

## 状态与执行

- UI 通过 `mod.get("$变量")` / `mod.set("$变量", 数值)` 访问本 Mod 已声明变量；
  `mod.default("$变量")` 只读该变量的作者默认值。恢复默认值使用普通 set 事务，不另建替换通道。
- 一帧脚本写入先暂存，绘制成功才通过统一输入队列提交；多个变量作为一批提交。
- ImGui 线程不调用 Unity；现有 Unity 线程求值 Render，保留形态键纯权重更新路径。
- 独立透明原生宿主承载所有 Mod ImGui 窗口，不嵌入插件面板，不 hook 游戏图形管线。
  在现有 GUI 线程调度，使用独立 ImGui context 和绘制资源；鼠标不在 Mod 窗口时穿透到游戏。
- 全局刷新在帧边界销毁旧 Lua 状态、清除旧交互；事件带配置代号，旧代事件不可落入新配置。
  Lua 局部状态重新初始化；普通变量使用 Constants 默认值，persist 变量恢复玩家值。
  之后是否画窗口由新脚本决定，DLL 不持久化 Lua 局部状态。详见 [作者状态](author-state-materials.md)。
  这与“关闭 UI”不同：关闭 UI 本身既不卸载脚本，也不重置 Mod 变量。

## 错误与权限

每 UI 独立 Lua 状态，限定内存/每帧指令预算。仅开放基础运算、math/string/table/utf8；
不开放 io/os/package/debug、文件加载、动态库或 Unity 指针。不把它宣传成进程级安全沙箱。
Lua 错误或 ImGui 栈不平衡会停止该脚本并记录 `[UI]` 文件/行号；清理当前帧栈，不执行半帧变量写入。
不擅自生成一个 DLL 固定布局的 Mod 错误窗口。其他脚本继续工作，全局刷新修正并重载，不静默回退。
当前 Lua 5.4.9：脚本文件最多 256 KiB、Lua 分配预算 16 MiB、每次执行约 200,000 条 VM 指令。
不开放 Lua 侧 pcall/xpcall/coroutine，避免脚本反复捕获指令预算错误。宿主用受保护调用和 C++ 异常展开。
指令限额不是原生库函数的硬实时中断；不承诺恶意脚本在同进程绝对安全或任意复杂 UI 都无帧耗。

## Blender

保留现有网格切换组和形态键控制面板。N → EFF 可勾选“生成简单 UI”（默认不勾选）。
勾选后选择标题及用户自定开关键，生成 `ui.lua`、UI section；按钮控制切换组，滑条控制形态键。
快捷键留空时，模板生成不含按键/关闭按钮的常显 UI；填写时，模板生成普通 Key、`$ui_open` 及 Lua 开关逻辑。
不勾选时仍导出网格/形态键及其变量绑定，但不生成 UI 声明或脚本。模板尺寸/排版在 Lua 文件中可修改。
旧 .blend 无需修改格式；生成脚本属于导出产物，再导出会更新生成脚本。

## 验收

真实解析器与 Lua/ImGui 运行测试：变量隔离、滑块/拖动、批量提交、关闭/独立开关、条件、
全局刷新代际、语法错误/超预算/不平衡栈/路径拒绝、另一个 UI 不受影响。
后台 Blender：作者数据保存重开、生成 Lua、INI 由实际解析器读取、脚本由真实 Lua VM 执行。
原生宿主进行独立隐藏窗口测试，不启动或修改用户游戏；真实游戏输入/透明显示结果另行记录。
