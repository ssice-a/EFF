# Format 3：INI 装配与资源复用

`mod.ini` 管理目标、资源位置、绑定和控制逻辑。DDS 保存图像，Mesh 保存几何和作者快照，动画文件保存编号采样通道，原生特效包保存游戏需要的特效对象。修改目标、按键或绑定无需重新编码这些资源。

通用变量、算术/if 动作、输入事件、属性绑定、Ins 声明式 UI 与 F10 状态规则见 [INI 控制程序](ini-control-program.md)。下面的 Constants/Key/Control 形式继续兼容；它们与新声明使用同一运行状态和播放器。

## 更换武器目标

更换 `Replace` 的 `target.path`、`target.asset` 和相应 `lod.N.path/asset`，即可继续使用原作者资源命中另一把武器。材质、贴图的 `source.*` 不随目标更改。相同 Mod ID 的不同安装目录有独立运行状态，按 Part 挂载的常态特效跟随当前 Part；新物理资源自动保存原始来源与 Replace/LOD 的绑定。详细示例、旧物理资源的限制及实测边界见 [武器目标重定向](weapon-retargeting.md)。

## DDS 与采样设置

新导出不为每张 DDS 再生成 Texture `.effres`。例如：

```ini
[TextureFlow]
dds=textures/dao_Roll.dds
source.path=assets/example/source_texture.png
source.asset=SourceTexture
linear=false
filter=1
wrap=0
aniso=1
mip_bias=0

[MaterialFlow]
path=materials/Flow.effres
source.path=assets/example/source_material.mat
source.asset=SourceMaterial
texture._MainTex=TextureFlow
```

示例中的游戏来源路径、资源名和 Shader 贴图槽应替换为实际来源。`source.path` 和 `source.asset` 用来取得当前游戏的 Texture2D 序列化结构；DDS 是唯一像素来源，尺寸、压缩格式和 mip 数据从 DDS 读取。`filter` 为 0 点采样、1 双线性、2 三线性；`wrap` 为 0 重复、1 钳制、2 镜像、3 单次镜像。各向异性和 mip 偏移在 INI 声明。

可以用 Photoshop 导出的受支持标准二维 DDS 覆盖该文件，也可以修改 `dds=` 指向另一张 Mod 内的贴图。无需同步更新旁边的 `.effres`。旧包已有的 Texture scaffold 仍兼容；旧文件缺失或旧 DDS 损坏，不应阻止 Blender 将完整的新导出事务式覆盖到该输出目录。

## 按键、变量和显隐

新导出声明 `controls=ini`，不生成 `controls.effres`：

```ini
[Mod]
format=3
id=Example
controls=ini
key_switch_enabled=1

[Constants]
persist $style=0
$enabled=1

[KeyStyle]
key=F7
type=cycle
$style=0,1

[PartFlow]
mesh=MeshFlow
material=MaterialFlow
visible=($style == 1) && ($enabled != 0)
```

`MeshFlow`、`MaterialFlow` 必须是同一 INI 中的资源声明。`cycle` 在多个值之间切换，`press` 写入一个目标值，`hold` 按住时以 `speed` 指定的速度接近目标值。显隐条件写在 `visible=`，支持算术、比较、`&&`、`||`、`!` 和括号；命令式 `if/else/endif` 放在 `[Action…] code=` 中，由 `[Event…]` 或面板按钮调用。

形态键通过 `Part` 中的 `shape.<通道名>=$变量` 绑定；材质动画通过 `[MaterialTrack…]` 的变量、材质、Shader 参数、类型和分量绑定。采样资源只保存数值通道，`[Animation…]` 在 INI 中指定通道对应的变量或骨骼路径，以及播放速度、循环、自动播放和快捷键。形态键、材质和骨骼轨道可使用同一个动画 ID 和时间轴。

特效的 `EffectAttachment` 同样在 INI 声明模式、目标、挂载位置、变量和原生包依赖；原生包仍是独立资源。`attach_mesh` 是为命中的网格增加常态特效，`replace_game_prefab` 是替换游戏原特效的逻辑 Prefab，沿用游戏原来的调用时机。

## Blender 往返与兼容

导入新的 INI 控制包时，形态键控制和快捷键恢复为 Blender 控件，形态键、材质和骨骼采样恢复为可编辑 Action。原变量名、动画 ID、显隐表达式和自定义按键关系随导入包保留；修改控件后再次导出会使用修改后的数值和按键。骨骼导出逐片段求值各自 Action，不把当前激活的一个 Action 当作全部动画。

一般显隐表达式和额外自定义按键关系由 INI 编辑，不承诺将任意表达式反推为 Blender 款式集合。资源位置由 INI 决定，文件名使用可读名称，重名采用数字后缀，内部资源 ID 不要求与文件名相同。

旧包显式声明 `controls=resource`，或保留 `ResourceControls` 而未声明控制模式时，DLL 仍读取旧控制资源；已有用户包不自动批量改写。运行时加载、游戏内 F10 和画面表现需要另行实测，离线往返测试不代替游戏验收。

设计依据见 [ADR-0002](adr/0002-runtime-resource-files.md)。

## 本轮本机验证（2026-11-12）

- DLL 已安装到 `D:\Hypergryph Launcher\games\Endfield Game\plugin\eff.dll`，3,169,792 字节，SHA256 `E8AAF988400BC2CDCF23A2B6B02B65A99ABC2B3B0222DE42CFB1B07217F7116B`，与正式构建产物相同。部署回执：`bin/local-deployments/20261112-012850-345/Dll.json`。
- Blender 已事务式安装到固定的 Blender 5.2 插件目录，59 个文件与当前源码和安装清单逐一校验一致。最终部署回执：`bin/local-deployments/20261112-013103-754/Blender.json`。回执 revision 是本地 `main` 的 HEAD；本轮修改仍在工作区，文件哈希反映实际安装内容，尚未提交、推送或发布。
- 作者/更新回归运行 136 项，1 项旧 Format 2 发布测试按既有规则跳过；其余通过。另有 21 项贴图/特效导出测试及 40 项 nativepack 目录测试通过。
- 已安装插件在 Blender 5.2.3 后台实例中通过 4 项控制往返测试和 8 项材质导入测试；材质编辑器检查通过。控制测试覆盖共享形态变量、改键后旧绑定移除、默认值修改、原材质变量保持、父子骨骼及同骨架多个 Action 分别导出。
- 原生 manifest、catalog、DDS 与完整 F10 模拟 API 测试通过；catalog 覆盖无 Texture scaffold 的 DDS 装配。未执行本轮游戏画面验收，也未批量改写用户已有 Mods。
