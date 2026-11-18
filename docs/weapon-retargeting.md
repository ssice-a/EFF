# 武器 Mod 的 INI 目标重定向

`mod.ini` 中的 Replace 负责指定游戏目标。作者 Mesh、材质、DDS、动画采样和特效资源是可复用的输入，不因更换命中目标而重新编码。

## 管理器选择目标

Manager 0.2.6 在武器 Mod 和武器挂载特效的详情中为每个绑定提供一个“替换目标”下拉框，默认选中现有目标。武器名单以匹配当前游戏的本地索引为来源。中文和英文名称由游戏 VFS 的 `WeaponBasicTable`、`ItemTable`、`I18nTextTable_CN/EN` 关联得到，随管理器语言设置显示。目录缓存会跨进程复用，手动刷新或游戏、索引、工作区变更时更新。作者的 Mod 显示名称保存在 `[Mod] name`。

武器替换按整把武器组织：保留作者 Part，覆盖目标模型逻辑路径内的所有 Mesh 与已知 LOD；没有承载作者 Part 的原版辅助网格和 VFX 生成 `handling=skip` 规则，避免残留原模型或重复显示作者模型。同一 Mod 的其他武器目标不会随之更改。找不到唯一对应关系时，在发布 INI 前报告具体规则。

挂载特效使用所选真实 Prefab 的 Renderer 层级和跨 LOD 稳定挂点，保留作者局部偏移与网格坐标桥接。Prefab 引用的共享 Mesh（例如渲染管线的 Quad）通过该武器的逻辑 Prefab 路径与 preload 身份定位，不能生成指向全局共享 Quad 的替换规则。借用资源的索引元数据通过现有 `.eidx.bundles` 中经过校验的范围读取，不重新扫描完整索引。

候选 INI 先经过随管理器安装的 Format 3 声明校验器，再检查外部文件变更、备份至 Mod 内 `.effmm-backups`，最后原子更新 INI。不会重写 EFFRES、DDS、动画或控制资源。应用后在游戏按 F10 重载。

本机已对当前索引的 112 个武器 Prefab，分别用 `langzhifei` 网格 Mod 和 `lizi` 挂载特效的声明副本进行解析与重定向，224 份候选声明全部通过；中英文 WPF 界面与完整 Mesh/VFX/LOD 覆盖、未知字段保留、改名和发布失败回滚也有回归验证。此结果不代替跨类型握持、尺寸和游戏画面的实测。

## 改哪几项

管理器的武器下拉框按类型分组，再按当前语言的名称排序，同类武器连续显示。

人物 Mod 的详情另设“目标角色（仅查看）”：从 INI 的 Replace / AttachEffect 关系识别当前角色，显示游戏中英文名称，并可展开查看具体 Mesh、Prefab 和 LOD 的逻辑路径。借用的材质、贴图等来源不会被当成替换目标。名称直接读取游戏 `CharacterTable` 与 `I18nTextTable_CN/EN`，不为查看人物再次扫描资产索引。人物涉及骨骼、蒙皮、物理和动画绑定，此处不提供一键更换角色，也不改写人物 INI。

```ini
[MeshBlade]
path=meshes/Blade.effres

[MaterialBlade]
path=materials/Blade.effres
source.path=assets/example/author_material.mat
source.asset=AuthorMaterial

[PartBlade]
mesh=MeshBlade
material=MaterialBlade
visible=1

[ReplaceWeapon]
target.path=assets/example/other_weapon.fbx
target.asset=OtherWeapon_LOD0
parts=PartBlade
lod.1.path=assets/example/other_weapon.fbx
lod.1.asset=OtherWeapon_LOD1
```

上例路径和资产名是示意，须使用实际游戏身份。命中条件是规范化逻辑路径 **加 Mesh 资产名**，不是作者文件名、Replace 段名或整个武器 Prefab 的名字。更换武器时修改 `target.path`、`target.asset`，并更新或删除旧武器的 `lod.N.path/asset`。删除 LOD 声明表示不再命中该 LOD，不表示自动找到新武器的其余 LOD。

不需要修改 `Mesh.path`、`Part.mesh`、材质和贴图的 `source.*`。材质的 `source.*` 是作者材质的游戏模板来源，不能随新武器目标一起改。保留 `Part.material` 就继续使用作者声明的材质；省略它则通过 `material_slot` 使用当前命中 Renderer 的原版材质。

一个 Part 可被多个 Replace 规则引用。如果希望同一套控制同时控制多个目标，可在一个 Mod 里复用 Part；如果希望各自独立控制，可复制整个 Mod 目录，各改自己的 Replace。两个副本不要求修改 `[Mod] id`，也不必改作者资源里的名称。

## 游戏内如何定位

DLL 首先通过游戏当前 Manifest 查询逻辑路径和资产名。`target.bundle` 与 `lod.N.bundle` 只提供旧包兼容的定位提示，不能覆盖当前逻辑查询结果；只改目标后残留旧提示不会使成功的当前逻辑查询继续指向旧武器。

部分 Mesh 只有 Prefab 的 preload 所有权，没有独立公开索引入口。这些目标在游戏实际请求的 native 输入及其依赖中确认精确逻辑身份；跨 CAB 的 preload 引用也回到对应请求依赖验证。不会仅凭同名 Mesh 匹配，不扫描整个游戏或预先枚举所有使用者。

这类后备查找在真实资源请求范围内进行，首次需要读取有限的原生目录和名称元数据。同一次请求内复用已解析的类型元数据，避免按每个 preload 引用重复读取；请求结束即释放。负结果按输入和 Manifest 代缓存；它不是零成本路径。没有真实请求时不打开作者 Mesh、材质或特效载荷。定位或资源准备失败时保留该次游戏原输入。

## 控制、动画和挂载归属

运行实例由作者 ID 和规范化安装目录共同确定，增删另一个副本不改变已有实例的 ID。变量、动画播放器和效果 owner 使用各自实例的命名空间。`state.ini` 和 Ins 的快捷键开关仍以 INI 中声明的作者 ID 校验，并保存到各自 Mod 目录；生成的实例 ID 不写回作者 INI。

原生 Mesh 和 Renderer 的运行标记由 **当前安装实例、Part 和当前目标** 生成，不继续使用导出时旧目标的标记。Part 显隐、形态键、材质动画与常态挂载特效因此仍能找到当前 owner。

`attach_mesh` 特效声明了 `part=PartBlade` 时，原有挂载流程本来就跟随这个 Part，无需另建目标映射格式。只有独立指定游戏 Prefab 的特效挂载、`replace_game_prefab` 模式等明确目标仍按其自身声明命中，不随武器 Replace 被隐式改写。

作者骨架和作者动画可复用，但本功能不负责自动重定向游戏骨骼名称、修复不兼容的蒙皮调色盘，或适配新武器的握持点、尺寸和游戏动作。更换目标后的这些表现仍须实机检查。

## 物理的来源绑定

新导出器为物理资源自动生成来源与 Replace 的关系，例如：

```ini
[ResourcePhysicsRibbon]
type=physics
path=physics/Ribbon.effres
targets=ReplaceWeapon
bind.0.rule=ReplaceWeapon
bind.0.lod=0
bind.0.source.path=assets/example/original_weapon.fbx
bind.0.source.asset=OriginalWeapon_LOD0
```

`bind.N.source.*` 描述不可变物理资源中的原始身份，`rule/lod` 指向可编辑的当前目标。更换武器只改 Replace，不改这些来源字段。DLL 先验证资源本身的完整性和原始组归属，再绑定当前目标；不同物理组不会因为共享一个资源文件而串到其他规则。

旧包没有这些字段时，目标未变可继续使用；只有一条 Replace 规则时可无歧义跟随该规则。旧物理资源涉及多条规则且目标已改时，不能猜测各组归属，会报告需要显式绑定或重新导出。Blender 新导出自动生成，无需作者手写。

## F10 与验证边界

改目标后 F10 同时记录旧目标和新目标，清理旧 owner 并让新的请求使用新代资源。旧目标恢复的定位同样优先查询当前逻辑身份，不因残留的旧 Bundle 提示漏掉真实输入。

本轮通过生产安装代构建、同 ID 副本、运行标记、物理分组、持久化、快捷键、请求入口和完整 F10 模拟原生 API 回归。Issue #4 的真实镜像 Prefab 也已在 Blender 后台导入并成功选中导出。这些是离线验证；跨武器换装后的画面、动画适配和场景生命周期仍需游戏实测。
