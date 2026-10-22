# Endfield 特效装配与属性块调研

日期：2026-10-19。本文记录实际资源调查和离线编解码结果，以及由此提出的编辑模型；不代表特效导入、导出、播放或 F10 已实现。

## 范围和证据

使用当前安装的游戏基础与 Persistent VFS 合并视图，并核对其指纹与 `E:\EIEM_Workspace\index\endfield_assets.eidx` 一致。没有启动或修改游戏，也没有修改 Blender 工程、作者 Mod、DLL 或已安装工具。

从索引中按逻辑路径和命名筛选出 12,424 个特效 Prefab 候选。这个数字是名称筛选结果，不是全部特效的完整普查。当前索引没有独立的 ParticleSystem/MonoScript 资产行，不能将零行解释为游戏没有粒子或脚本；本轮从 Prefab 所在 Bundle 的对象表继续读取这些组件。

抽取 12 个不同来源的 PFB，包含提丰命中/烟雾、庄方宜攻击拖尾/VAT、篝火、火花、火焰、烟雾、两种花瓣、技能灯光和 UI 效果。样本包含 28 个 `ParticleSystem`（classID 198）及 28 个 `ParticleSystemRenderer`（199）；这些粒子组件的 TypeTree 结构一致，渲染器的 TypeTree 结构也一致。不能由这组抽样断言所有游戏特效都使用同一实现。

本地证据目录：`E:\vscode\EIEM\bin\effects-research-20261019`。

- `selected-paths.txt`：精确选取的 Prefab 路径。
- `sample-results.json`：当前资源指纹、Bundle 来源、CAB、PathID、类编号、组件字段和引用类型表。
- `script-names.json`：根据实际 MonoScript 引用解析的类、命名空间和程序集。
- 每个对象的 JSON/RAW：原始 TypeTree、解析值和粒子/脚本原字节。
- `module-research-summary.json`：模块统计和 5 项离线修改探针的结果。
- `codec-roundtrip.json`：逐对象无修改还原结果。
- `EffectsAudit/`、`probe_module_edits.py`：可复用的只读调查和隔离对象修改脚本。

## 普通粒子确实是模块结构

样本中每个 ParticleSystem 都包含以下 23 个顶层模块结构。关闭的模块仍保存字段、曲线和渐变，不是从文件中删除。

| 原生结构 | 对应功能 | 28 个样本中启用数 |
| --- | --- | ---: |
| InitialModule | 初始寿命、速度、颜色、大小等基础设置 | 28 |
| EmissionModule | 按时间/距离发射、爆发条目 | 28 |
| ShapeModule | 发射形状、位置、方向、Mesh/纹理引用 | 12 |
| SizeModule | 随寿命变化的大小 | 19 |
| RotationModule | 随寿命变化的旋转 | 5 |
| ColorModule | 随寿命变化的颜色/透明度 | 23 |
| UVModule | 粒子纹理序列帧 | 4 |
| VelocityModule | 随寿命变化的速度 | 5 |
| InheritVelocityModule | 继承发射器速度 | 0 |
| LifetimeByEmitterSpeedModule | 按发射器速度调整寿命 | 0 |
| ForceModule | 力随寿命变化 | 1 |
| ExternalForcesModule | 外力设置 | 0 |
| ClampVelocityModule | 速度限制与阻尼 | 5 |
| NoiseModule | 噪声运动 | 8 |
| SizeBySpeedModule | 按速度变化的大小 | 0 |
| RotationBySpeedModule | 按速度变化的旋转 | 0 |
| ColorBySpeedModule | 按速度变化的颜色 | 0 |
| CollisionModule | 碰撞参数及引用 | 0 |
| TriggerModule | 触发器参数及引用 | 0 |
| SubModule | 子发射器、事件类型、继承选项和概率 | 0 |
| LightsModule | 粒子灯光参数及引用 | 0 |
| TrailModule | 粒子拖尾、宽度、颜色、空间和生命周期 | 0 |
| CustomDataModule | 自定义向量/颜色数据 | 10 |

“0”仅说明这些抽取对象没有启用该模块。其结构确实存在，但当前不能据此声称游戏内功能已经验证。

### 添加属性块的实际含义

可以在 UI 提供“添加模块 → 噪声/颜色/速度/拖尾/子发射器……”。底层启用原生对应结构并写入完整设置，不要求原模板已经启用此模块。关闭/移除 UI 模块应保留作者参数，导出时写 `enabled=false`，不能从固定 TypeTree 中删除字段。基础设置作为常驻模块。

每种模块在一个 ParticleSystem 中有一个对应字段。不能把 UI 当成任意数量、任意执行顺序的计算节点：同一个粒子系统里没有第二个 `ColorModule` 字段。需要独立运动/渲染行为的另一层粒子时，可以新增发射器；其视觉结果不应被承诺与任意两个计算模块的叠加完全等价。

模块中的列表则可以真正增删条目，例如 `EmissionModule.m_Bursts`、曲线的 `m_Curve` 和 `SubModule.subEmitters`。新增时必须同步实际计数字段、完整元素结构和引用。

曲线保存 `minMaxState`、`scalar`、`minScalar`、两条曲线及关键帧时间/值/切线/权重；渐变保存模式、颜色/透明度关键点及其时间和数量。不能将其降为一个浮点数或一个颜色。显示“常数/曲线/范围”等模式需要按实际字段和值验证。

## PFB 还包含其他层次

### 节点、组件、资源

GameObject/Transform 组成层级；ParticleSystem 管行为；ParticleSystemRenderer 管材质、渲染形式、Mesh、顶点流和排序。需要支持新增分组节点、新建完整发射器、复制子树、改父级及挂点，不只修改原有节点。

提丰 `p_fxbat_typhoea_attack_01_hit.prefab` 的 8 个粒子分别挂在 `p_flash`、`lizi001`、`p_glow`、`trail`、`p_flash (1)`、`star`、`QL_Smoke (1)`、`P_ring` 上。它们启用的模块和时长不同，说明一个效果是多层发射器的组合。

渲染器具有 `m_Materials`、`m_RenderMode`、`m_Mesh`、`m_UseCustomVertexStreams`、`m_VertexStreams` 等字段。28 个样本中 12 个启用了自定义顶点流。因此自定义粒子数据的设置需要与渲染器流布局及实际 Shader 读取方式一起验证；不能只增加一个数值输入块就宣称渲染生效。

### 游戏自定义组件

根据真实脚本 PPtr 解析出以下组件：

- `Beyond.Gameplay.EffectSetting`：`effectLogicCfg` 中有循环、时长、延迟、淡出、时间缩放等设置；另有包围盒、LOD/裁剪、动态形状粒子和 `effectFunc`。
- `Beyond.Gameplay.EffectAnimation`：开始、循环、结束动画引用。
- `HG.Rendering.Runtime.VFXTrailRenderer`：拖尾时长、宽度曲线、颜色渐变、UV 和刀刃锚点等字段。
- `HG.Rendering.Runtime.VFXTrailPointsTool`：点列表、世界空间等设置。
- `HG.Rendering.Runtime.VFXAutoEmissiveRate`：发射率与缩放相关设置。
- 灯光和后处理样本还包含额外灯光、Volume、Bloom、Vignette 等组件。

庄方宜攻击拖尾样本同时包含 6 个普通 MeshRenderer、1 个粒子系统、多个 VFXTrailRenderer/PointsTool 和动画。提丰技能灯光样本没有粒子系统，却有 7 个 Light 和多种控制组件。名称是“特效”的 PFB 不一定是粒子，不能全部转换为一个通用粒子模块列表。

粒子内部的 `TrailModule` 和游戏的 `VFXTrailRenderer` 是不同实现，应在 UI/数据模型中明确区分。

### 多态功能块需要额外编解码

火焰样本的 `EffectSetting.effectFunc.data` 有一个 `rid`，对应 `references.RefIds` 中 `Beyond.Gameplay.PlaySound`。SerializedFile 的引用类型表提供 `PlaySound` 的结构，包含 `soundName` 和 `largeType`。

这是真正的多态功能列表，与粒子内固定模块不同。当前通用 Python codec 对这个对象报 `unknown native leaf: ReferencedObjectData`。C# 普通 ToType 调用没有抛出异常，但其该块 `data` 为空，不能将此视为完整解析成功。读取和写回此类数据必须解析引用类型表和实际载荷，并正确维护 managed reference ID、类型和完整内容；没有支持时不能以空字典覆盖它。

未来若开放此类“添加功能”，必须从游戏已实现且字段结构已验证的类型库选取。用户新写一个类名或新增未知字段，并不会给现有游戏增加可执行逻辑。

## 离线写回验证及其边界

使用仓库现有 `tools/nativepack/type_tree.py` 严格 codec，基于提取的当前原生 TypeTree：

- 28/28 ParticleSystem 和 28/28 ParticleSystemRenderer 解码后逐字节还原成功。
- 60 个脚本对象中 59 个逐字节还原成功；1 个包含上述 ReferencedObjectData，明确失败。
- 在研究目录的对象副本中启用噪声并设置强度，启用原来关闭的颜色模块，新增爆发条目，新增大小曲线关键帧，及连接同 CAB 内现有的另一粒子组件作为子发射器，共 5 项均成功编码再解码。
- 每项仅目标顶层模块字节发生变化，其他顶层字段逐字节保持。新增列表元素时对象长度相应变化。

这些验证证明数据层的模块和列表修改可实现；未将候选对象写回完整 PFB、未实例化游戏，也未验证任何新模块的可见效果、播放行为和生命周期。

## 对编辑模块的建议

编辑模型应包含四类操作，名字和数据保持区分：

1. **添加节点/发射器**：空间层级及完整组件的新增、复制、删除与挂载；内部引用按作者稳定 ID 表达，导出时统一分配原生引用。
2. **添加粒子模块**：从当前已验证 TypeTree 的模块目录选择；启停保留数据，每种模块遵守原生单实例字段结构。
3. **添加条目**：爆发、曲线点、子发射器关系等实际列表的增删；处理引用、计数、曲线和渐变语义。
4. **添加游戏组件/多态功能**：单独的已适配组件类型目录，保留脚本实现身份；不与普通粒子模块混成任意属性列表。

UI 使用特效节点树、组件列表和“添加模块”菜单即可，不需要 Blender 模拟预览。块的界面排列不能被解释为改变引擎计算顺序。原始来源、完整未知字段及未编辑组件保留，友好控件按验证过的功能开放。

现有 AnimeStudio EFF 导出入口只开放 Mesh/Material/Texture 和部分相关组件，纯粒子 PFB 可能因没有可读 MeshRenderer/SkinnedMeshRenderer 被拒绝；现有 Blender 导出也以所选 Mesh 为入口。因此后续需要单独的特效作者源导出/导入和 PFB 编译适配，不能把本轮直接读取 Bundle 的成功称为现有产品已经支持特效导出。

现有离线编译器的新增 GameObject/Transform、类型/脚本表、对象 PPtr/依赖重定位能力可作为基础。特效适配还要处理粒子组件与 Renderer 配对、材质/Mesh/顶点流、总时长/包围盒/LOD、动画和脚本引用，以及实际游戏的创建、停止、回收和 F10 退出。对根设置具体如何影响运行时，应在下一阶段结合代码/实机验证，不能只凭字段名推断完整行为。

后续先选普通粒子完成“新增发射器 + 添加原模板未启用的模块 + 增加列表项 + 骨骼挂载 + 游戏播放与移除”的完整验证，再逐类适配自定义拖尾和多态功能块。本轮只做研究，没有开始上述产品实现。

## 独立特效资源与运行时挂载的追加调查

当前游戏 IL2CPP 元数据中的 `Beyond.Gameplay.EffectManager` 声明包含：

- `CreateEffectOnTransform(name, target, followRot, followScaleTrans, timeScaleSource)`。
- `CreateEffectAttachToEntity` 的实体/跟随目标重载。
- `_CreateInstance` 的资源路径、跟随 Transform 和创建上下文参数。
- `_DoAsyncLoadEffect`、`_LoadCallBack`、特效预加载及 active/pool 容器。
- `_GetInstanceFromPool`、`DestroyFromPool`、`DestroyImmediate`。
- `RegisterClothEffect`、`AddUnmanagedEffect`、`CreateInstanceWithExistEffect` 等既有特效登记入口。

这些是独立资源加载、实例挂载和池管理的静态接口证据，不证明任意作者路径可直接传给这些接口，也没有验证其运行时签名、具体调用顺序和清理契约。证据见研究目录 `EffectManager-metadata.json` 和 `effect-loading-entry-evidence.json`。

现有提丰人物来源主 SerializedFile（`CAB-f859e4fe0ae8ddbe9e3aac716c534e11`，418,191 字节）有 GameObject、Transform、脚本、SkinnedMeshRenderer、Animator 等对象，没有 classID 198/199。其攻击命中和烟雾是不同来源 Bundle 的独立 PFB。这不能推出所有人物/场景 PFB 都没有内嵌特效；应按实际资源关系判断。

样本中提丰命中特效的 8 对粒子组件/渲染器共 63,880 字节，烟雾的 1 对共 7,900 字节；数字不含其余对象、材质、贴图、Mesh、外部依赖和 Bundle 开销，不能作为完整特效包大小或加载耗时。

建议把作者特效定义、材质/贴图/Mesh 依赖与角色挂载声明分开表达，仍可随同一个 Mod 分发。调参编译应按实际改变的特效输入失效；挂载点及父级未变时复用它们的编译结果。若采用运行时挂载，必须新增通过游戏原生特效生命周期创建、停止、移除和更新实例的适配，不能只保存一个 Prefab 引用就期待自动创建。

首次增加挂载关系和后续修改特效定义应分别计量。完整人物 PFB 重建是否昂贵，取决于实际受影响的来源数量、载荷是否重复及现有编译分支；没有“新增特效必然完整重编人物贴图”的必然关系，也不能在实现测量前承诺毫秒级更新。

特效热重载需要让后续新实例使用新资源，并处理旧活跃实例、旧预加载和池实例。开发中的单独测试实例可以优先建立停止后重播的较小闭环；战斗特效和完整游戏对象池的适配不能凭该测试实例成功宣称已经完成。当前 F10 尚无上述通用独立特效热重载能力。

## 调用配置和实例状态：完整系统尚未调研完成

追加只读检查当前游戏 IL2CPP 元数据，记录在 `effect-lifecycle-types-metadata.json`、`effect-trigger-config-metadata.json`。以下是声明的实际字段/方法，不等于执行流程、枚举语义或运行时调用已经验证。

`Beyond.Gameplay.EffectActionCfg` 包含 `effectName`、`sourceTag`、`releaseByAction`、`ignoreOwnerTimeScale`、数量限制、位置/旋转参考、普通/武器/衣物挂点、位置偏移、黑板值、随目标大小缩放、随实体/武器显隐、镜头视口锚定、释放时脱离跟随等配置。例如字段有 `mountPoint`、`isClothMountPoint`、`weaponMountPoint`、`positionOffset`、`rotMountPoint`、`visibleWithEntity`、`unpackFollowTargetOnRelease`。因此创建效果时的挂载、控制和参数不必全部包含在粒子 PFB 中。

`Beyond.Gameplay.EffectInstance` 保存加载句柄、原生对象、owner、效果根、当前位置/旋转/缩放、跟随目标、延迟、剩余时间、循环、Animator、透明度、时间缩放、预加载、池和回收状态。存在 `LoadFinish`、`OnEffectInit`、`SetFollowTransform`、`FollowTick`、`ResetForPoolReuse`、`Finish`、`OnStop`、`OnDestroy` 等入口。不能将创建实例理解为只有 Instantiate/SetParent/Play 三个操作。

`Beyond.Gameplay.View.Animation.PlayEffectHandler` 有 `OnFastAnimationEvent`，模型状态侧 `EffectHandler` 有持续特效创建、透明度过渡、挂起释放和重置入口。这是动画事件和状态控制参与特效的线索，尚未追到样本技能或实际事件数据，不能据此宣布完整触发链已打通。

当前确认的是：普通粒子资源的字段组织；若干自定义效果组件；部分调用配置及管理器/实例接口声明。还没有完成：

1. 从一条真实技能/常驻效果配置追踪到特效资源名、解析路径、加载、挂点计算、播放和回收的完整链路。
2. 动画是否覆盖粒子、Transform 或材质参数，以及开始/循环/结束阶段的实际控制关系。
3. 自定义粒子顶点流、材质 Shader 和具体 CustomData 通道之间的对应。
4. 世界、NPC、UI、武器/衣物和场景实例各自的创建、显隐、时间、相机、释放与池复用行为。
5. 多态 ReferencedObjectData 的完整产品编辑和原生包写回（下文已有 version 2 的严格对象编解码）；GPU/VFX Graph 等其他效果类型是否实际被游戏使用。
6. 作者新特效如何接入原生资源名称/配置解析，以及共享依赖、原生类型和引用的完整合法性。
7. 完整 PFB 重建后的实机首次播放、连续重播、重载、移除和内存退休。

前文的编辑器组织和独立挂载方案是候选设计，不能视为完整架构已经确定。先保留整份原生数据和原触发关系，建立一个既有普通粒子的完整往返验证；再分别选择自定义拖尾和持续跟随效果追踪运行链，最后决定首版开放哪些新增组件、挂载和控制能力。禁止用局部字节往返或简单测试实例代替对完整系统的理解与验收。

## 用户补充约束：毫秒级调试与骨架节点范围

用户要求频繁特效热重载达到毫秒级；若全局 F10 无法满足，应提供独立的特效更新通道。验收必须计量从提交编辑到实际画面变化的端到端延迟，分别记录编辑数据生成、传输、队列等待、游戏更新和下一帧显示；不能仅报主线程某个函数耗时。首次加载和大纹理上传等不同工作量应单独报告，不能在没有实测的情况下承诺所有操作具有相同延迟。

候选实现需要将调试参数更新与完整发布解耦。已验证存在且可调用的粒子 API 可以作为部分参数的直接更新通道；结构/引用修改可针对受影响特效独立编译与刷新，既有角色 Mesh 和未变依赖应复用。关闭调试或正式导出时仍需要保证作者数据与离线包一致。以上只是待验证方向，没有接入生产 setter、运行时构建器或新的热重载通道。

只读检查当前元数据可见 ParticleSystem 的播放/停止/清理、MainModule 部分参数、EmissionModule 的启用/发射率等方法，但不能假设完整 Unity API 都存在：本次查看的 ColorOverLifetimeModule 和 NoiseModule 仅有构造方法声明。元数据可见不等于真实调用绑定已验证；缺失 API 不能使用猜测私有偏移补齐。证据为 `particle-live-api-metadata.json`。

关于“网格未用某骨骼时是否存在该 Transform”，需要区分 Blender 顶点组、原生 Renderer 的 `m_Bones` 蒙皮槽，以及模型 GameObject/Transform 层级。离线检查提丰人物主 SerializedFile：

- 模型含 556 个 Transform；这是所有空间节点的数量，不等于 556 根蒙皮骨骼。
- `S_actor_typhoea_cloth_06_lod0` 的 `m_Bones` 只有 `acc_R_Gadget_a_03_jnt` 与 `acc_R_Gadget_a_02_jnt` 两项。
- 同一模型文件仍有 `Bip001_L_Hand` 与 `Bip001_R_Hand` 的 Transform，不在该 Renderer 的两项槽表中。
- Animator 的 `m_HasTransformHierarchy` 为 1，同时也有启用的优化相关设置。因此不能由单个优化标志推断所有未加权骨骼都被删除。

相关数量和例子记录在 `renderer-vs-model-bones.json`；本轮没有验证当前游戏活实例的骨骼可访问性。原始层级完整的 Prefab 通常不因某一个 Renderer 未使用节点就自动删除该节点；优化骨架、精简 NPC 或 LOD 模型仍可能缺失/隐藏 Transform。不能承诺任何 PFB 都保留全部 Blender 骨骼。新增特效挂点必须逐来源确认并在目标实例创建后校验可访问性与所属作用域；缺失时列出准确目标，不能偷偷绑到其他同名骨骼。

## 第一阶段实现：独立特效来源解析

2026-10-19 用户授权后，已加入 AnimeStudio 独立入口 `--export-eff-effects` 和 EFF 菜单 `Checked Prefabs as EFF effect sources...`。输出 `.efffx`，包含完整原生依赖快照、按实际身份验证的节点/组件、结构化字段、原生 TypeTree/managed 类型表、粒子模块摘要、资源引用与未解析诊断。纯粒子不依赖人物 Mesh；批量选择共用一次相关索引读取。

新增 `TypeTreeDocument` 严格编解码，不替换展示用途的宽松解析器。真实样本 28/28 粒子、28/28 粒子 Renderer、60/60 MonoBehaviour 均通过未修改字节往返；此前丢失的 `PlaySound.soundName` 现在准确读取为 `au_amb_emitter_damagefire_small_01`，修改后的变长字符串也能重新解析。当前支持 managed registry version 2，未知 schema/版本明确拒绝严格解析，由来源导出层保留 opaque 底稿。

启用 Noise、追加 burst、追加曲线键和修改 sub-emitter PPtr 已通过对象级测试，并核对其余顶层字段字节不变。含真实样本的测试通过 317 项断言；12 个实际特效 PFB 完整来源导出/模块/快照 SHA256 测试通过 434 项断言，0 个解析或引用告警。索引回归通过。独立 net9 Release 构建及真实 EXE CLI 导出成功；证据见 `bin/effects-tests.log`、`bin/effects-export-tests.log`、`bin/effects-index-regression.log`、`bin/effects-release-build.log`。

测试程序位于 `bin/AnimeStudio-effects-parser-20261019/AnimeStudio.GUI.exe`，未覆盖现有安装、游戏 DLL 或 Mod，未发布 Release。格式与使用说明见 `tools/AnimeStudio/docs/endfield-effects-source.md`。

此阶段还没有 Blender 编辑 UI、编辑 JSON 的产品导入、完整 PFB/Bundle 重建、新增发射器的发布、骨骼挂载、游戏播放或毫秒级热重载。大资源与未知组件使用完整原生快照，不能把完整底稿保留理解成全部类型已可编辑，也不能把对象级往返理解成游戏内验收。

## 第二阶段实现：Blender 既有特效编辑与离线原生重建

同日继续授权后新增独立 `3D View → EFF → 特效` 面板，可导入 `.efffx`，按节点/组件或资源选择编辑，保留关闭的模块，支持原生标量、bool、引用、曲线/发射列表增删。作者状态保存在 `.blend`，编辑输出 `EFF_EFFECT_EDIT` v1，只包含改变的既有对象及来源摘要。没有 Blender 粒子模拟。

AnimeStudio 新增 `--build-eff-effects`：核对来源快照/编辑摘要，严格导入 JSON，验证引用类型、受保护归属、曲线顺序和 burst 数量，重建受影响 SerializedFile/UnityFS，并重新读取、逐对象及逐成员比较。保留原生类型/外部/managed 表与未改对象；构建输出原子发布，重复构建内容一致。普通参数编辑不用重新扫描全游戏索引或重新导出人物几何。

本轮发现并修复两处数据接口问题：JSON 数字的 Double/Single 表示不能直接作为来源不一致判据，需转回原生精度并核对字节；vector.Array 在 JSON 中折叠后，引用扫描路径也必须折叠，才能正确查验脚本列表中的引用。116 个对象 JSON 导入和字节往返通过 439 项断言。10 个样本的两次确定性原生构建、无修改输出、PlaySound 变长修改及错误编辑拒绝通过 104 项断言；Blender 隔离导入、修改、列表增加、导出、保存/重开和三轮注册卸载通过，索引回归通过。

测试程序为 `bin/AnimeStudio-effects-editor-20261019/AnimeStudio.GUI.exe`；Blender 当前安装以 junction 指向 `tools/Blender`，下次加载读取本次代码，无需覆盖用户安装。未修改作者当前工程、游戏 DLL/Mod 或发布 Release。证据为 `bin/effects-edit-blender.log`、`bin/effects-blender-registration.log`、`bin/effects-edit-codec-tests.log`、`bin/effects-native-tests.log`、`bin/effects-edit-index-regression.log`。

本轮尚未开放完整发射器/节点/组件新增、添加外部贴图或 Mesh 依赖、managed 类型登记变化，也没有接入 Mod 发布协议、游戏挂载、原生预加载/池刷新与热重载。`.efffxbuild` 是独立离线验证产物，标记 `gameVerified=false` 和 `installed=false`，现有 F10 不读取它；原生重建通过不等于可用的游戏特效 Mod。下一阶段仍需选一个原生特效打通准确资源路由和播放生命周期，再扩展新增发射器与独立调试通道。

## 第三阶段候选：DLL 独立调试状态机

2026-10-19 继续授权后新增 `effects-debug` 专用构建。作者构建另外输出隔离 CAB/stream/external 命名空间的 UnityFS 和 `runtime.bin`，根在原生字节中设为 inactive；不改变原始来源，未修改来源也能生成首次调试输入。10 个实际来源的确定性构建、完整成员/原生根重新读取、清单 SHA 与粒子计数通过 191 项断言。

DLL 的本地命名管道只传递命令和纯数据。后台核验离线文件；原生 ResourceManager 来源租约、Bundle/Prefab 异步加载、精确已登记模型实例/Transform 地址、unmanaged 登记与粒子播放均由 Unity 主线程执行。准备和播放分开，新候选失败保留旧实例；取消等待真实请求结束，owner 释放前清理，延迟 Destroy 后才退休资源，未知所有权保留隔离。当前是独立测试实例，未接管技能/动画事件或战斗池。

真实生产代码隔离状态机和 Windows named pipe/Python 客户端均已回归，包括 Unicode 路径、命令 ticket、损坏包拒绝、失败保持、重播、停止、清理、旧容器缓存碰撞与 idle 零 Unity 调用；调试 DLL 使用静态 CRT 并完成 PE/旧宿主运行库隔离加载验证。这些不是游戏调用签名、显示效果或性能验收。安装 DLL 与生产 Mod 未修改，没有发布。具体协议、测点和未实现范围见 [DLL 特效调试](effects-runtime-debug.md)。
