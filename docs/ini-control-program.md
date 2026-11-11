# INI 参数、执行逻辑与 Ins 面板

`mod.ini` 是资源装配与控制逻辑的入口。DLL 编译其中的声明，按键和 Ins 面板调用同一个执行器；材质、显隐、形态键和已有动画播放器使用同一套 Mod 变量。Blender 导出、导入和再次导出保留这些声明，不生成另一份 `controls.effres`。

本轮提供的是通用执行层。强度、UV 速度等是作者选用的参数名称，不是独立的控制器类型。需要随时间变化的数值交给已有动画时间轴；按键动作适合切换、赋值、运算及播放控制。

## Blender 作者操作

作者继续使用已有可视控件，不需要创建变量、填写内部参数名或编辑文本：

- 网格显隐和款式：在 3D 视图侧栏的 EFF 网格切换面板中设置分组、款式和按键。
- 材质切换：在对象的材质切换控件中设置槽位、选项和按键。
- 形态键：在形态键控制面板中接管当前形态键，设置显示名称、默认值、范围和按键；内部通道标识由插件维护。
- 动画：继续使用统一动画面板与 Blender 时间轴，设置片段、采样和播放方式。
- 特效：继续使用特效根对象的挂载、默认播放和快捷键控件。

导出时插件自动生成相应的 INI 声明，与资源装配一起校验；导入时恢复对应的可视控件，不能由已有控件表达的自定义逻辑在包内部保留。修改已恢复控件的默认值、名称或范围后再次导出，这些作者设置更新对应声明，不会被旧的导入值覆盖。这里没有单独的参数变量管理页面，也没有场景属性或 Text 数据块编辑入口。

以下文法用于说明运行时协议和开发适配器，并非 Blender 作者的操作步骤。任意 INI 动作或 Ins 布局并未自动转换成一个可视化脚本/布局设计器；现有面板负责其已支持的操作。

## 执行层最小例子

下面展示自动装配层可使用的声明结构。实际输出在 `[Mod]` 中使用 `controls=ini`，同一变量只有一份默认值声明。

```ini
[ParameterEnabled]
variable=$enabled
type=bool
default=true
public=true
persist=true
label=显示流光

[ParameterSpeed]
variable=$speed
type=float
default=1
min=0
max=10
ui.min=0
ui.max=3
public=true
label=流动速度
description=此变量由下面的绑定写入作者选择的材质参数。

[ParameterEffectiveSpeed]
variable=$effective_speed
value=$enabled * $speed
public=true
min=0
max=10
label=当前速度

[ActionToggle]
public=true
label=切换流光
code=
    if $enabled
        $enabled = false
    else
        $enabled = true
    endif

[EventToggle]
key=F7
trigger=press
action=ActionToggle

[UISettings]
type=group
label=流光设置
expanded=true

[UIEnabled]
type=checkbox
parent=UISettings
variable=$enabled
label=显示流光
order=1

[UISpeed]
type=slider
parent=UISettings
variable=$speed
label=流动速度
order=2

[UIEffective]
type=text
parent=UISettings
variable=$effective_speed
label=当前速度
order=3
```

参数声明和 UI 不会自动改变 Shader。作者需要绑定到实际资源，例如：

```ini
[BindingFlowSpeed]
target=material.float
material=MaterialFlow
property=_ExampleSpeed
value=$effective_speed

[BindingFlowVisibility]
target=part.visible
part=PartFlow
value=$enabled
```

这里的 `MaterialFlow`、`PartFlow` 必须是同一 INI 已声明的资源标识；`_ExampleSpeed` 是占位参数名，须改为该 Shader 实际支持的 float 属性。DLL 不猜测哪个 Shader 字段代表速度。材质绑定通过已有的实例属性块适配器应用，保留原游戏属性和共享材质身份。`part.visible` 与 Part 已有 `visible=` 条件同时成立时才显示。

## 参数与实际值

`[Parameter…]` 以节 ID 和 `variable=$名称` 声明参数。支持 `float`、`int`、`bool`；默认类型为 `float`。

| 字段 | 含义 |
| --- | --- |
| `default` | 作者默认值；缺省为兼容 Constants 值或 0 |
| `min` / `max` | 硬范围；动作、UI 与状态恢复都必须满足 |
| `ui.min` / `ui.max` | 滑块软范围；必须在硬范围之内 |
| `public` | 是否允许面板编辑；默认 false |
| `persist` | 是否跨进程保存；默认 false，兼容已有 Constants 持久化声明 |
| `reload` | `preserve`（默认）保留兼容的运行值；`reset` 在 F10 后恢复作者默认 |
| `value` | 只读派生表达式；不能被动作、滑块或动画直接写入，也不能持久化 |
| `label` / `description` | 显示名称与说明 |

硬范围可省略；UI 可用软范围缩小操作区间。整数必须是整数，布尔值必须为 0 或 1。没有指定 UI 软范围时优先使用硬范围，未指定的下界默认为 0，上界默认为 `max(1, default)`；负数或大范围参数应显式设置合适的 `ui.min/ui.max`。与旧 Constants/Control 声明不一致的重复定义会报错。

运行时区分两种值：

- **设定值**：按键、动作、面板或持久化保存的基础值。
- **生效值**：叠加当前动画采样，并计算派生表达式后的值。属性绑定和显隐读取它。

面板滑块编辑设定值；被动画驱动时另显示生效值，悬停可查看变量名、两种值以及驱动动画。派生参数只读。动作表达式从当时的生效值读取，只提交动作显式赋值的变量，避免把其他动画通道全部写回设定值。手动写入某个动画通道会通过原有播放器的通道接管规则停止冲突片段。

## 表达式和动作

表达式支持数值、`true/false`、`$变量`、括号、取负、`!`、`+ - * / %`、比较、`&&` 和 `||`。逻辑运算短路。除零、非有限结果和参数越界均报错。派生参数按依赖顺序计算，循环依赖在加载时拒绝。

`[Action…]` 使用 `code=`，后续指令缩进写入；支持赋值、嵌套 `if/else/endif`、`call ActionId`，以及 `play/pause/stop ClipId`。Clip ID 是 `[Animation…]` 节名去掉 `Animation` 前缀后的 ID。不要求给动作公开 UI；`public=true` 才能被面板按钮调用。

```ini
[ActionStart]
code=
    $enabled = true
    play Flow

[ActionStop]
code=
    stop Flow
    $enabled = false
```

上例要求实际存在 `[AnimationFlow]`。动画的数值轨道、骨骼轨道、时间轴和采样文件继续使用现有格式；本轮未增加另一套动画系统。动画速度变量可以声明为公开参数，仍受现有播放器“速度必须大于 0”等校验约束。

动作先在临时变量集执行；全部成功才提交变量与播放器命令。失败不会只完成前半段赋值。调用环在编译时拒绝，每次执行最多 65,536 条指令；无任意脚本、文件访问或网络操作。

`[Event…]` 用 `key`、`trigger`、`action` 绑定输入。触发方式为 `press`（默认）、`release`、`hold`。只有动作表达式可以读取 `$dt`：hold 为本次输入处理的秒数，press/release 和面板按钮为 0。hold 是底层输入事件；连续动画通常直接使用时间轴。打开面板或游戏失焦时停止物理按键动作，但继续观察按键状态，恢复焦点不会凭空产生一次按下事件。Ins、F10 等全局保留键不能被 Mod 占用。

## 属性绑定

本轮提供三个适配器：

| `target` | 其他字段 | 用途 |
| --- | --- | --- |
| `material.float` | `material`、`property`、`value` | 写入一个材质 float 属性 |
| `part.visible` | `part`、`value` | 为已有 Part 增加显隐条件 |
| `shape.weight` | `part`、`channel`、`value` | 驱动一个形态键通道 |

绑定的 `value` 可以是变量或表达式。同一个属性不能同时声明多个写入者。内部合成通道由编译器生成，不是作者要维护的变量，也不会再导出为另一套声明。

新增适配器应扩展绑定目标解析、目标校验和现有运行时写入接口，复用同一套表达式、事件与 UI 执行逻辑。当前没有宣称已经支持任意 Unity 字段、向量/颜色参数、材质纹理切换或特效组件全部属性。已有 EffectAttachment 的启用变量继续读统一生效值；特效时序仍用原有挂载与动画机制。

## 声明式 UI

没有 `[UI…]` 时，Ins 自动列出公开参数和公开动作，并兼容旧控件。有 UI 声明时使用作者布局；需要的参数、动作与动画按钮由作者显式列出。

支持 `group`（折叠组）、`row`（等宽列）、`tabs/tab`、`slider`、`number`、`checkbox`、`button`、`text`。使用 `parent` 组织层级，`order` 指定同级整数顺序，同序按节 ID 排列；`visible` 和 `enabled` 可以写表达式。slider/number/checkbox 引用公开参数，checkbox 要求 bool，button 引用公开动作。text 可显示变量的生效值。

UI 按钮产生与按键相同的动作命令，UI 线程不直接修改游戏对象。父子环、缺失参数/动作、非法控件组合在加载时拒绝；布局最多嵌套 64 层，单 row 最多 64 列。运行期 UI 表达式出错会显示诊断，不破坏其他控件的绘制。

## F10 与作者往返

只修改控制/UI/绑定等声明、且资源文件与资源/目标声明未变化时，F10 发布控制代际，避免重建模型资源。参数节 ID、变量名、类型与新范围兼容时，默认保留运行值；`reload=reset` 重置。兼容的整个动画集合保留播放相位及暂停/停止状态；轨道内容或播放定义改变时重新准备播放器，尚未实现逐片段迁移。

加载出错时保留该 Mod 上一次有效程序，面板显示原因；修正 INI 后再按 F10。若同时改动了不安全的资源载荷，候选代际会被拒绝。真正删除 `mod.ini` 仍表示移除 Mod。旧面板排队的命令不能跨代际写入新程序。

Blender 将导入的执行层声明保存在包内部；选择 LOD 子集合内的网格仍可找到所属包。不会额外创建控制文本。新建工程从正常可视控件生成声明，作者不手工添加参数变量。多个包合并导出时仍校验相同声明 ID、默认值和按键设置是否冲突。

导入的 Part ID 保留，不因改对象名而改变；复制对象产生重复 Part ID 时导出会明确拒绝，需要独立标识。INI 材质绑定会保留被驱动的材质资源，即使材质静态参数没有编辑。被 INI 绑定接管的形态通道不会再生成自动控制。可视控件只更新它实际负责的参数字段，其他动作、事件、表达式和 Ins 布局保留。

## 验证边界

覆盖共享 C++ 编译/执行器、Python 调用同一原生编译器、运行时按键/面板/动画/材质适配器、F10 保留与恢复、真实 ImGui/D3D 控件输入，以及 Blender 5.2 的导入/编辑/再导出。自动化使用受控运行时接口，不等于已验证实际游戏 Shader 画面；本轮没有实机画面验收。
