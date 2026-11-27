# Mod 自定义面板

Lua 5.4 编写布局和交互，EFF 用 Dear ImGui 绘制控件。作者面板和原有 Ins 面板共用同一个可调整大小的窗口。各个 Mod 安装实例拥有自己的 Lua 状态、图片和控制值。

在已有 `mod.ini` 的 `[Mod]` 节加上以下字段。

```ini
gui_script=ui/panel.lua
gui_key=F8
gui_enabled=1
```

把脚本和图片放在 Mod 目录内。路径以 `mod.ini` 所在目录为起点。`gui_key` 可省略，也可写 `Ctrl+F8` 等组合键。打开 Ins，在「手动调整」中选择 Mod，即可打开面板或更改「允许作者面板」。该开关即时生效并写入 `gui_enabled`。Ins 的 Mod 按键开关也控制 `gui_key`，仍可用 Ins 手动打开已允许的面板。修改脚本、图片或 INI 后按 F10。对同一 Mod 输出目录重新导出时，Blender 会保留这三个字段和作者的 GUI 文件。

## 示例

下面的脚本使用已经绑定到材质的 `$opacity`、作者动作 `ActionOutfit` 和动画 ID `Idle`。替换成你的 INI 使用的名称，并把预览图存为 `ui/outfit.png`。

```lua
return function()
  imgui.SetNextWindowSize(440, 360, imgui.Cond.FirstUseEver)
  local visible = imgui.Begin("服装设置", true)
  if visible and imgui.BeginTabBar("设置") then
    if imgui.BeginTabItem("服装") then
      if imgui.ImageButton("outfit", "ui/outfit.png", 180, 100) then
        mod.invoke("ActionOutfit")
      end
      local changed, value = imgui.SliderFloat("透明度", mod.get("$opacity"), 0, 1)
      if changed then mod.set("$opacity", value) end
      if imgui.Button("恢复默认") then
        mod.set("$opacity", mod.default("$opacity"))
      end
      imgui.EndTabItem()
    end
    if imgui.BeginTabItem("动画") then
      if imgui.Button("播放") then mod.play("Idle") end
      imgui.SameLine()
      if imgui.Button("停止") then mod.stop("Idle") end
      imgui.EndTabItem()
    end
    imgui.EndTabBar()
  end
  imgui.End()
end
```

## 接口

| 接口 | 用法 |
| --- | --- |
| `mod.api_version` | 当前为 `1` |
| `mod.get(name)`、`mod.default(name)` | 读取当前生效值和作者默认值。同一帧的 `mod.set` 可立即读回 |
| `mod.set(name, value)` | 修改已声明的数值变量，布尔变量传 `0` 或 `1` |
| `mod.invoke(actionId)` | 执行 INI 中的 `[Action…]`，参数为完整节名 |
| `mod.trigger(keyId)` | 执行旧式 `[Key…]` 的 `press` 或 `cycle` 行为，参数为完整节名 |
| `mod.play(clip)`、`mod.pause(clip)`、`mod.stop(clip)` | 控制当前 Mod 的动画，参数为动画 ID |
| `mod.animation_state(clip)` | 返回 `"stopped"`、`"playing"` 或 `"paused"`，第二个返回值为当前播放时间（秒） |
| `imgui.Image(path, width, height [, u0, v0, u1, v1])` | 显示 PNG、JPEG 或 DDS BC7。加载后，省略宽高时使用原始尺寸，只指定一边时保持比例 |
| `imgui.ImageButton(id, path, width, height [, u0, v0, u1, v1])` | 绘制图片按钮并返回是否点击。每个按钮使用独立的 `id` |
| `imgui.DrawList.AddImage(path, x0, y0, x1, y1 [, u0, v0, u1, v1, alpha])` | 在当前窗口的屏幕坐标中绘制图片。可自定坐标、裁切和透明度。它不占布局空间，也不接收点击。返回是否已绘制，读图失败时第二个返回值为错误原因 |
| `imgui.GetWindowContentRect()` | 返回当前窗口可用于画背景的内部矩形 `x0, y0, x1, y1`。坐标不随内容滚动 |
| `imgui.Checkbox(label, value)` | 返回 `changed, value`，值为 Lua 布尔值 |
| `imgui.SliderFloat(label, value, min, max)`、`DragFloat`、`InputFloat` | 返回 `changed, value` |
| `imgui.ColorEdit4(label, r, g, b, a)` | 返回 `changed, r, g, b, a` |
| `imgui.BeginCombo(label, preview)`、`Selectable(label, selected)` | 下拉选择，展开时调用 `EndCombo()` |
| `imgui.BeginTabBar(id)`、`BeginTabItem(label)` | 分页，返回 true 时调用对应的 `EndTabBar()` 或 `EndTabItem()` |

其余已开放控件包括文字、换行、分隔线、同一行布局、子窗口、组、表格、折叠标题、单选按钮、进度条、悬停提示和禁用区域。通过 `SetNextWindowSize`、`SetNextWindowPos`、`PushStyleColor`、`PushStyleVar` 等设置布局及样式。每次调用 `Begin` 和 `BeginChild` 都要调用对应的 `End` 和 `EndChild`。`Begin(title, true)` 显示关闭按钮。

可用 Lua 的函数、条件、表、字符串和数学运算组合控件。同一帧的修改按调用顺序交给游戏线程，复用 INI 的变量范围、显隐、材质和动画逻辑。执行结果在后续帧反映到读取值。Lua 可用基础函数、`math`、`string`、`table` 和 `utf8`，文件读取限于 EFF 提供的脚本与图片加载接口。脚本出错会关闭该作者面板，错误显示在相应 Mod 的 Ins 页面和 `eff_log.txt`。

关闭作者面板或按 F10 会回收旧面板的 ImGui 状态。面板打开时，EFF 也会在控件记录增多时回收近期未绘制的状态。给窗口、表格和分页使用稳定 ID，可以让它们在打开期间持续保留状态。标题中的 `###` 后缀可为改名的窗口或控件保留相同的 ID。

游戏线程忙时，作者面板的待处理操作有上限，Ins 保留自己的队列空间。队列满时会提示该帧的操作未提交，等游戏恢复后可重试。脚本可在按钮触发或控件值变化时提交操作。

## 大量图片按钮

每套外观可以用独立缩略图，点击后调用相应的 `mod.invoke` 或 `mod.set`。给按钮指定宽高可保持布局稳定，并让滚动区外的图片推迟到可见时加载。后台线程读图并解码，面板线程分帧上传。等待时按钮仍可点击，读图失败时可悬停查看原因。

缓存按 GPU 纹理的数据量计费。每个 Mod 面板最多驻留 512 张纹理和 64 MiB，所有打开的面板共享 2,048 张和 128 MiB。单帧可显示的纹理额度按打开的面板数均分，每个面板仍适用自己的上限。缓存满时先释放近期未用且本帧还未提交的纹理。关闭面板释放对应缓存，F10 刷新会释放旧缓存并重新加载可见图片。

PNG 和 JPEG 上传为 RGBA8，256 × 256 一张占 256 KiB。批量缩略图可以用 **DDS BC7**，以直通方式上传压缩块，同样尺寸一张占 64 KiB，完整 mip 链约 85 KiB。DDS 使用普通二维 BC7 贴图、直通 Alpha，宽高为 4 的倍数。PNG/JPEG 每边最多 2048 像素、每文件最多 8 MiB。DDS 每边最多 4096 像素、每文件最多 24 MiB。

大图可以带完整 mip 链。EFF 根据绘制尺寸和 UV 区域选择上传层级，`Image`、`ImageButton` 和 `DrawList.AddImage` 共用这一机制。比如 3840 × 2160 的 BC7 图，完整链约 10.55 MiB，选用 960 × 540 起始层时约 0.66 MiB。作者可以先画背景，再放按钮或其他控件。以下写法会把整图铺到窗口内容区，UV、尺寸与透明度都可以由脚本调整。

```lua
if imgui.Begin("服装选择", true) then
  local x0, y0, x1, y1 = imgui.GetWindowContentRect()
  imgui.DrawList.AddImage("ui/background.dds", x0, y0, x1, y1, 0, 0, 1, 1, 0.85)
  if imgui.ImageButton("outfit", "ui/outfit.dds", 128, 128) then
    mod.invoke("ActionOutfit")
  end
end
imgui.End()
```

多张缩略图也可以放进同一张图集，所有按钮共用一份纹理。最后四个参数是归一化 UV 坐标，左上角为 `0, 0`，右下角为 `1, 1`。例如在一张左右两格的图集中，下面两个按钮分别使用左半和右半。

```lua
if imgui.ImageButton("outfit_a", "ui/outfits.png", 128, 128, 0, 0, 0.5, 1) then
  mod.invoke("ActionOutfitA")
end
imgui.SameLine()
if imgui.ImageButton("outfit_b", "ui/outfits.png", 128, 128, 0.5, 0, 1, 1) then
  mod.invoke("ActionOutfitB")
end
```

每个面板脚本限 256 KiB。
