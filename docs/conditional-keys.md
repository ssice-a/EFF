# 条件与按键配置

状态：已实现 DLL 解析、求值、按 Mod 隔离的按键调度、内置 Mod 管理器与同一 Mesh 上的 submesh 索引更新。当前重构候选仍需世界、角色 UI、NPC 三端实机验收。
作者端操作见[Blender 切换流程](blender-switches.md)，当前能力和验证等级见[文档索引](README.md)。

## 固定的全局配置

游戏根目录下 **`plugin/eff.ini`**，唯一入口，不再读取 `eiem_config.txt` 或搜索备用路径。
新 DLL 启动时若文件不存在，会创建默认配置。发布模板位于 `config/eff.ini` 和构建后的 `bin/eff.ini`。

```ini
[Hotkeys]
reload=F10
gui=INSERT

[Graphics]
disable_camera_fade=true
```

`reload` 重读全局设置及所有 mod；普通变量使用 `[Constants]` 初始值，`persist` 变量恢复该 Mod 保存的玩家值。
v47 的保存文件和默认值恢复方式见 [作者状态与材质导入](author-state-materials.md)。
将其改成 `reload=Ctrl+F8` 后，按一次**旧快捷键**加载配置，随后使用新快捷键。
`gui` 打开内置 Mod 管理器。两者不得相同；mod 按键不能占用这两个全局组合。
ApplePie 的配置入口指向 `eff.ini`，刷新接口同样只向 Unity 线程提交 Reload。

管理器列出声明了 `[Key...]` 或有 `shape.*` 控制变量的 Mod。启动时默认选中第一个带 `[Key...]` 的 Mod；若只有形态控制，则选中第一个有滑块的 Mod。可以从下拉列表改选；F10 后保留仍存在的选择，所选 Mod 删除时自动回退到可切换 Mod。所选 Mod 的 Key 按钮只触发对应 section；按住 `type=hold` 按钮持续推进到目标。形态键即使没有按键或 Lua UI 也有内置滑块，范围来自 `[ShapeControl...]`，旧包可用默认范围。世界、角色 UI、NPC 的同一 Mod 实例共享变量。内置管理器获得焦点时，所选 Mod 的普通 `scope=game` 快捷键仍生效；作者 Lua UI 单独获得焦点时继续使用 `scope=ui`。Lua UI 的变量事务不受管理器选择限制。选择只保留在本次进程。

`disable_camera_fade` 控制相机渐隐，详见 [相机反虚化](camera-fade.md)，不改变 Mod 显隐规则。

DLL 将 Mod 文件夹名、`mod.ini`、资源相对路径和内置段名按 UTF-8 处理。可以使用中文文件夹、Mesh/Material/Texture/Skeleton/Physics/Lua 文件名，以及 `[Mesh衣服]`、`[Render衣服]` 等带类型前缀的中文段名；引用名称必须与声明一致。`mod.ini` 建议保存为 UTF-8（可带 BOM），变量名及段类型前缀仍遵守下文语法。资源 `path=` 仍须是 Mod 内的相对路径，不能用 `..` 跳出目录。文件访问、缓存时间戳、状态保存和 F10 重载使用同一 UTF-8 路径约定。

全局配置写错时明确记录错误，保持最后有效的设置（含快捷键与相机反虚化），防止失去修正配置的入口；
不尝试别的文件，也不使用另一套解析路径。任一已发现的 mod 文件非法时，本次 F10 候选代际整体拒绝，现有规则和实例保持最后一次有效状态。

## 一个按键切换原版、修改版、隐藏

以下内容写入该 mod 的 `mod.ini`，资源路径需指向实际已导出的文件：

```ini
[Constants]
$outfit=1

[KeyOutfit]
key=F6
type=cycle
$outfit=0,1,2

[MeshCloth]
path=meshes/cloth.mesh

[MaterialCloth]
path=materials/cloth.mat

[RenderCloth]
asset=S_actor_typhoea_cloth_01_lod0
if $outfit == 1
    mesh=MeshCloth
    material.0=MaterialCloth
else if $outfit == 2
    handling=skip
endif
```

- 0：本规则不修改，撤销此前由该规则施加的修改，回到游戏原状态。
- 1：给原 Renderer 更换 Mesh 与材质槽 0；不新增 Renderer。
- 2：只隐藏原 Renderer。`skip` 不自动创建或删除 Mesh。
- `0,1` 就是普通开关；`0,1,2,...` 为多状态循环，不另加 toggle 别名。

合并 Mesh 时可以在条件分支里控制单个 submesh 的显隐：

```ini
[KeyOutfit]
key=F6
type=cycle
$outfit=0,1

[RenderCloth]
asset=S_actor_cloth_lod0
mesh=MeshClothMerged
if $outfit == 0
    submesh_visible.2=true
else
    submesh_visible.2=false
endif
```

`submesh_visible.N=false` 在已经装配到 Renderer 的同一个 Mesh 对象上，把第 N 个 submesh 的索引缓冲置空；`true` 从 Mesh 资源缓存的原始索引恢复该 submesh。按键不会更换 `sharedMesh`，也不会创建或销毁 Renderer，`bones[]`、`rootBone`、Animator、LOD 和 Physics 均保持不变。N 从 0 开始，最多 32 个 submesh。

## 语法约定

- `[Constants]` 的变量名为 `$` 加英文字母/下划线开头的名称，**区分大小写**，当前 mod 内共享。
  进入 UI、换图、新实例出现不会重置。全局刷新只重置普通变量；`persist $name=初始值` 保留玩家调整。
- 数值为有限数值，支持负数、小数及 `true/false`（对应 1/0）。
- `[Key...]` 必须有 `key`、`type=cycle`、至少一项已声明变量的值列表。
  多个变量可同一按键同步切换，列表长度必须相同且至少有两项。
  根据当前变量组合选择列表中的下一组；不在列表中则选择第一组。
  列表允许重复值；Blender 形态键作者端用 `最大值,最大值` / `最小值,最小值` 生成方向明确、重复按下也不反转的增大/减小动作。
- 目前按键支持 F1–F24、A–Z、0–9、INSERT/DELETE/HOME/END/PAGEUP/PAGEDOWN、方向键，以及
  `NUMPAD0`–`NUMPAD9`、`NUMPADPLUS`、`NUMPADMINUS`、`NUMPADMULTIPLY`、`NUMPADDIVIDE`、
  `NUMPADDECIMAL`。支持 `Ctrl+Shift+F6`、`Ctrl+Alt+NUMPAD7` 这类 `Ctrl/Shift/Alt + 一个主键`
  的组合；不支持两个普通主键组成一个快捷键。小键盘 Enter 与普通 Enter 统一为 `ENTER`。
  支持 `type=cycle` 与用于形态键连续变化的 `type=hold`；不支持手柄。持久化使用 Constants 的 `persist` 声明。
- `if / else if / else / endif` 允许嵌套，不能跨 section；不同时提供 elif 等别名。
- 表达式支持 `== != < <= > >= && || ! ()` 和一元正负号。
  比较优先于 `&&`，`&&` 优先于 `||`；逻辑操作短路求值。不支持四则运算、函数调用或赋值表达式。
- 条件只出现在 Render 动作里。资源声明和 `asset/path/match.*` 选择器必须保持静态。
- 同一 Render 内按顺序应用成立的赋值，同一字段最后一次有效赋值生效。
  `mesh=`、`skeleton=`、`handling=`、`material.N=`、`shape.名称=` 可显式清除此轮此前对该字段的设置。
  `shape_speed.名称=正数` 让同名 `shape` 赋值成为平滑目标，单位为 Blender 权重/秒；省略则立即写入。
- 每次求值从该 Render 的静态选择器和空动作开始，不继承上一状态的动作。
- 当前规则集仍为**选择器第一条命中**，条件不成立不意味着自动尝试第二条规则。
- 整行 `;` / `#` 注释有效；不支持行尾注释。

未知变量、错误表达式、缺失 endif、无效资源引用在加载时统一报错，包括当前不会执行的分支。
资源引用只能指向本 mod；所有条件分支都会在加载时静态校验。`partner.N` 已停止支持，
多部件必须由 Blender 合并为同一 Mesh 的 submesh，再通过 `submesh_visible.N` 控制显隐。
单 Render 最多 128 个条件节点，单表达式最多 256 个节点，超限报错而非截断执行。

## 执行与生命周期

1. 加载：解析有序条件树、校验全部分支、建立静态作用域索引，以初始变量求值得到 Render 动作。
2. 模型完成：先登记已观察模型的弱引用，再匹配选择器。v40 起，即使整个 INI 注释，也保留实例观察信息，
   后续 F10 启用才能找到已经存在的模型；条件关闭与整份配置不存在均不删除观察信息。
3. 输入：前台窗口的真实按下事件与 ImGui 控件共用有序队列，送到 Unity 窗口线程；
   只合并相邻的同一滑条采样，不跨越按键事件。
4. 重应用：先在独立数据中求得新状态；发布新动作、应用已登记实例。
   不重新解析磁盘 INI；资源仍由同一后端按需构造并复用缓存。
   v36 增加纯形态键权重更新路径：通道集合和装配不变时只更新权重，不撤销并重建 Renderer。
   纯 submesh 显隐使用索引更新快速路径，不重放 Mesh、骨骼、材质或 Physics。其他装配变化仍走完整恢复。
   平滑目标使用游戏主窗口线程的 16ms 计时消息继续推进；到达目标后停止计时，不反复重建资源。
5. 退出/换实例：沿用实例所有权清理；新实例使用当前变量值。
6. 全局刷新：提交待保存数值 → 撤销旧效果 → 重读 eff.ini/mod.ini → 初始化变量并恢复仍声明的持久化数值 → 应用已登记实例。
   同批次若有刷新，旧代按键及滑条事件丢弃，避免旧操作落到新配置。

静态资源基线使用只处理 Mod 按键、F10 和事务计时器的最小窗口过程，并运行独立的最小 Mod 管理器宿主；不启用旧动画、旧诊断 GUI、相机或更新工作线程。键位随配置版本、当前所选 Mod 和前台窗口变化重新注册；离开游戏注销占用，不在桌面切换 Mod。
注册失败明确记录键码和错误，不回退到另一套轮询输入逻辑。
循环按键按真实按下事件切换；hold 按键只在按住期间按时间推进，不伪造松开事件。

恢复缩小为本次真正修改的字段：

- 未进入 Mesh setter 就不拥有 Mesh 写入；进入 setter 后，即使返回失败但字段已变空，也保留恢复责任。
  纯材质/skip 不捕获或恢复骨架。恢复失败不提前删除快照，不把残留状态作为下一次源模型。
- 材质只恢复被写入的槽位，保留其他槽的当前游戏值；插件扩展的尾部槽会撤销。
- Mesh 恢复时，骨骼数组/根骨骼与保存值相同则不重复调用 setter。
- 对不同 mod 的按键更新保持原有第一条命中次序，不先过滤规则再把次优规则提升成命中者。

## 验证与未完成项

`tests/test_mod_program.py` 执行真实解析/表达式/循环状态代码；`test_mod_controls.py` 执行全局配置、
按键队列发布、材质槽恢复以及真实模型登记函数；`test_hotkey_worker.py` 将实际快捷键线程放在模拟 OS
消息队列中，验证改键、连续按键、离开/返回前台和注销。未向真实桌面注入按键。


已知边界不能隐去：

- 新语法需要 v32 或更新 DLL；此前的旧 DLL 不会识别这些条件。
- 更大规模多 Mod 场景的长期内存曲线和更多游戏版本仍需回归。
- 尚未识别游戏内聊天框等文本输入状态；游戏窗口在前台时 mod 快捷键仍会触发，建议使用功能键/组合键。
- 多条源 Render 对同一 Mesh 消费者的冲突仍按配置顺序取第一条；PFB 关系不构成运行时作用域。
  原生资源最终 Destroy 的完整所有权仍未完成。
  v40 支持启用规则后重应用生命周期入口已经观察过的模型；不保证找到从未经过受支持入口的陌生实例。
  不要配置选择同一 Mesh、动作又互相矛盾的多条源 Render。
- 同一受控材质槽后来被游戏改写时，恢复基线的持续同步尚未完整解决；本轮保证未受控槽不被旧快照覆盖。
- Blender 0.5 已提供切换组/状态 UI 与 INI 自动生成；不是任意表达式的可视化编辑器。
  Mesh/材质/贴图格式没有更改。

v36 / Blender 0.6 曾增加固定 Slider UI；v37 / Blender 0.7 改为 `[UI...]` + Lua，移除旧 Slider 解析。
v38 / Blender 0.8 再将 UI 块收敛为只声明脚本：不存放 key/condition。普通 Key 新增
`scope=game|ui|both`（缺省 game），控制其响应焦点；UI 是否显示及布局由用户 Lua 决定。
`shape.名称=表达式` 不变，Lua、快捷键继续共用上述变量与输入顺序，见 [Lua UI](lua-ui.md)。
详见 [形态键与 ImGui 控制](shape-controls.md)，该文单独记录 API 验证与游戏 GPU 验收边界。
