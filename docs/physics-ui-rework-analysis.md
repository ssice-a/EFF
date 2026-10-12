# 物理骨骼 UI 重构分析

日期：2026-10-09；2026-10-10 补充参数操作规范并实现第一版统一编辑器。第 11 节记录实际实现范围，其余章节是完整目标设计。

10 月 9 日的分析读取插件源码、五个角色解包包、当前游戏元数据和公开工具；10 月 10 日继续核对插件、61 个源物理组的参数分布及官方参数说明。用户随后授权推进实现，第一版已修改 Blender 插件及回归测试；没有保存用户工程、重导出 Mod，也没有进行新的游戏内碰撞实验。此前 DLL 通用资源接入是另一项代码工作，范围见 [DLL 接入说明](compiled-resource-graph.md)。下面区分源数据事实、公开后端语义参照、目标交互和实际完成范围。

## 1. 结论

重做物理编辑 UI 是合理的。目标是一个编辑位置、一份可编辑参数、独立的视口显示规则，以及可以批量执行的创建和参数复制操作。

可以参考 RE Chain Editor 的骨骼选择、链创建、碰撞体手柄、参数预设和多选粘贴体验。数据仍应按终末地 BeyondDynamicBone 表达，不能把 RE 的 Header / Settings / Group / Node 对象层级及逐节点参数原样套过来。

用户允许舍弃旧 UI；不需要同时维护两套编辑入口。原生读取器、序列化身份、未编辑源字段及引用关系仍有用途，重做 UI 不意味着丢弃这些数据。

## 2. 当前数据证据

样本来自 `E:/EIEM_Workspace/assets/beyond/dynamicassets/gameplay/actors/postmodels/characters`。

| 角色包 | BeyondBoneCloth | 胶囊 | 球 | 平面 | 合计 |
|---|---:|---:|---:|---:|---:|
| pelica | 7 | 12 | 4 | 0 | 23 |
| wulfa | 11 | 18 | 6 | 0 | 35 |
| zhuangfy | 13 | 25 | 1 | 3 | 42 |
| lizhiyan | 19 | 44 | 0 | 3 | 66 |
| typhoea | 11 | 25 | 1 | 1 | 38 |
| 合计 | 61 | 124 | 12 | 7 | 204 |

204 个组件的记录均声明完整 TypeTree 解码；本轮另行核对原始文件长度、解析消耗长度和 SHA256，全部匹配。引用记录中非空但未解析的引用为 0。这证明这些样本的数据覆盖，不代表所有角色、其他物理后端及所有参数的实机行为已经验证。

61 个物理组的 `clothType` 全为 BoneCloth，`connectionMode` 全为 Line，`colliderCollisionConstraint.mode` 全为 Point。35 个组有多个根骨，50 个组引用了碰撞体，11 个组没有碰撞体列表。UI 因此必须支持一个物理组包含多条根链及分支，不能把每个组件强行显示成一条链。

当前游戏元数据可确认以下枚举：

| 字段 | 当前游戏定义 |
|---|---|
| ClothType | MeshCloth=0、BoneCloth=1、BoneSpring=10 |
| BoneConnectionMode | Line=0、AutomaticMesh=1、SequentialLoopMesh=2、SequentialNonLoopMesh=3 |
| 碰撞模式 | None=0、Point=1、Edge=2 |
| 胶囊方向 | X=0、Y=1、Z=2 |
| UpdateMode | Normal=0、UnityPhysics=1、Unscaled=2、AnimatorLinkage=10 |
| TeleportMode | None=0、Reset=1、Keep=2 |
| 相机剔除 | Off=0、Reset=10、Keep=20、AnimatorLinkage=30 |
| 相机 Renderer 选择 | AutomaticRenderer=0、ManualRenderer=10 |
| SelfCollisionMode | None=0、FullMesh=2 |

枚举存在表示代码和格式有对应定义，不等于这些选项已在本项目中逐项验收。样本里未启用的功能应保留字段并标明验证状态，不凭字段名编造效果。

## 3. 旧 UI 的具体问题

| 位置 | 当前行为 | 重构处理 |
|---|---|---|
| `eiem_physics_authoring.py` 的 `EFF_PT_physics` | 侧栏选当前组、创建、复制、选择骨骼、切显示，再让用户去对象属性调参数 | 编辑集中在侧栏，同一对象不再另开一套属性编辑面板 |
| `EFF_PT_physics_object`、`eiem_physics_native.draw` | 作者组与源组分支绘制，参数、结构、引用分散 | 源/新增只是来源属性，使用同一编辑流程；能力差异在具体字段上表达 |
| `draw_group_parameter_controls` | 多种曲线参数被塞进一个下拉框，切换后才知道其关联开关、强度在哪里 | 参数归回自己的功能区，每行就地展开曲线 |
| F-Curve、CurveMapping、作者直接属性、原生字段集合、快照 | 多种投影和同步路径同时存在 | 保留一份可编辑参数和曲线；控件、手柄与导出读同一份数据 |
| `draw_parameter_fields` | 当前已经是只读表，但仍重复展示常用值及曲线内部字段，并分页 | 只保留一个默认关闭的源数据检查入口，参数搜索跳到唯一编辑位置 |
| 原生碰撞体 `location` 与 `center.*` | 两者都能影响中心；导出还会把物体位移叠加进源中心 | 一个中心值，移动手柄直接改该值，不再叠加一个隐含偏移 |
| `group_of`、`active_object_updated`、`current_group` | 当前对象与 Scene 当前组共同决定编辑和显示目标 | 明确活动组、活动条目和多选集合；选共享碰撞体不暗中换组 |
| `apply_visibility` | 根据 Scene 当前组遍历改写全部物理对象显隐 | 显示过滤由一个入口管理，不与编辑选择或游戏启用状态互相覆盖 |
| `preview_style_updated` | 切线框/实体时重建场景中的各类物理预览 | 样式改变只改显示；数值变更只更新受影响的图形 |
| `copy_group_parameters` / `paste_group_parameters` | 剪贴板同时带参数和碰撞体引用，源/作者路径各自处理 | 默认只复制物理参数及曲线；引用复制作为明确的独立选项；一次粘贴给全部所选组 |

这部分是源码审查。尚未对用户旧工程逐步复现每一次显隐覆盖；不能把静态调用关系写成已经确认的全部现场原因。

## 4. 参数应该怎样分类

`serializeData` 中按语义折叠后有 91 项：曲线整体算一项，向量整体算一项，引用/列表整体算一项，带启用开关的数值整体算一项。它们不是 91 个都应默认展示的滑条。完整字段、类型和源示例见附录。

| 编辑区 | 参数与结构 | 含义和显示方式 |
|---|---|---|
| 结构 | clothType、rootBones、ignoreFromRootBones、connectionMode | 类型、根链、排除的子树和连接方式。用骨骼列表/树，不输入 PathID |
| 基础动力 | gravity、gravityDirection、gravityFalloff、damping、stablizationTimeAfterReset | 重力、方向、重力的姿态衰减、运动阻尼、初始化/重置稳定时间 |
| 姿态与混合 | animationPoseRatio、blendWeight、clothSimulateWeight、rotationalInterpolation、rootRotation | 恢复基准及物理/动画混合、旋转控制；不同字段不合并成同一个“强度” |
| 形状恢复 | tetherConstraint.distanceCompression、distanceConstraint.stiffness、triangleBendingConstraint.stiffness | 压缩约束、长度/距离恢复、三角弯曲恢复；是否适用取决于拓扑 |
| 角度恢复 | angleRestorationConstraint 的启用、stiffness、velocityAttenuation、gravityFalloff | 回到基准形状的恢复力及恢复过程的速度衰减。这里的 gravityFalloff 与整体重力衰减是两项 |
| 角度限制 | angleLimitConstraint 的启用、limitAngle、stiffness | 相邻基线段允许弯折的角度和限制强度；角度以度显示 |
| 惯性 | inertiaConstraint 的 anchor、anchorInertia、worldInertia、movementInertiaSmoothing、localInertia、depthInertia、centrifualAcceleration | 挂点、整体移动和动画局部运动对物理的影响及平滑 |
| 速度与传送 | 世界/局部移动与转速限制、particleSpeedLimit、teleportMode/Distance/Rotation | 每个速度限制的开关与值放同一行；传送行为单独折叠 |
| 碰撞 | radius、colliderCollisionConstraint 的 mode、friction、colliderList、collisionBones、limitDistance | 模拟节点/边的厚度、检测模式、摩擦及明确引用的碰撞体；BoneSpring 的额外字段按类型显示 |
| 运动范围 | motionConstraint 的 useMaxDistance、maxDistance、useBackstop、backstopRadius/Distance、stiffness | 允许离开基准位置的距离和沿法线的回挡约束；它不是身体胶囊 |
| 自碰撞/组间碰撞 | selfMode、surfaceThickness、syncMode、syncPartner、clothMass | 自身或另一物理组之间的检测、厚度及相对质量；默认折叠并注明当前样本未启用 |
| 风 | influence、frequency、turbulence、blend、synchronization、depthWeight、movingWind | 风影响和扰动设置；独立折叠，不与基础阻尼混放 |
| 弹簧 | useSpring、springPower、limitDistance、normalLimitRatio、springNoise | BoneSpring 相关配置，不能因 BoneCloth 样本存有这些默认字段就认定它们正在生效 |
| 运行/LOD | updateMode、clothAnimatorAbilityLODThreshold、clothAnimatorLODThreshold、clothLodFadeTime、低权重重置开关及阈值、cullingSettings | 更新与剔除策略，放高级区；保留游戏专属 LOD 字段，不按官方现版文档猜阈值意义 |
| 蒙皮/法线/构建 | sourceRenderers、meshWriteMode、paintMode/Maps、reductionSetting、customSkinningSetting、normalAlignmentSetting、normalAxis | 依赖资源与构建设置，按实际物理类型显示；骨链常用流程不要求填写无关网格配置 |

基础语义参照 Magica Soft 的 [Force](https://magicasoft.jp/en/mc2_magicacloth_force/)、[Angle Restoration](https://magicasoft.jp/en/mc2_magicacloth_anglerestoration/)、[Angle Limit](https://magicasoft.jp/en/mc2_magicacloth_anglelimit/)、[Shape Restoration](https://magicasoft.jp/en/mc2_magicacloth_shaperestoration/)、[Inertia](https://magicasoft.jp/en/mc2_magicacloth_inertia/) 和 [Movement Limit](https://magicasoft.jp/en/mc2_magicacloth_movementlimit/)。这些是后端概念参照；终末地的实际字段以本机 TypeTree/元数据为准，逐参数响应仍需游戏内验证。

### 4.1 九种曲线，采用一种操作

以下九项都使用 `CurveSerializeData`：阻尼、模拟点半径、距离恢复强度、角度恢复强度、限制角度、最大移动距离、回挡距离、BoneSpring 碰撞限制距离、自碰撞厚度。

统一一行显示“名称 / 基础值 / 沿链变化按钮”。点击变化按钮，只在该行下面展开图形编辑和当前关键点的数值，不另开一套参数页。

曲线横轴是基线归一化深度 0～1，不是动画时间，也不是骨骼数组索引。计算形式为基础值乘该深度的曲线值；关闭曲线时使用基础值。例如角度基础值为 60°、某节点深度处倍率为 0.5，该处有效角度为 30°。[参数与基线说明](https://magicasoft.jp/en/mc2_baseline/)

归一化参照是该组代理拓扑中的最长基线；较短分支的末端不一定到 1。不能给每条短链各自拉伸到 0～1，或把深度当作“第几根骨骼 / 总骨数”。原生构建结果是最终依据；现有 Blender 按 Transform 和 Selection 点计算的是编辑预览，不是已经证明与所有原生代理拓扑完全一致的映射。

选择骨骼时只显示“来自哪条组曲线、深度和有效值”，并提供“定位到曲线”按钮。第一版不添加独立的逐骨骼半径/角度副本。同一深度可能对应多条分支；编辑曲线会影响这些节点，需要高亮影响范围。

原关键点、切线、权重和外插设置未编辑时原样保留。图形和数值控件编辑同一组关键点；不再维持 F-Curve、CurveMapping、原生字段三份可以独立改写的曲线。未掌握的曲线类型只读保留，不静默改成线性。

### 4.2 不应当变成日常控件的数据

- `serializeData2.selectionData` 是模拟点选择数据，不可按数组序号直接绑定骨骼；固定/运动/忽略需有真实映射。
- `serializeData2.preBuildData` 包括代理拓扑、绑定姿态、Transform 引用和约束数组，是构建数据，不是让用户逐项编辑的参数。
- 组件顶层还有 gravityProperty、dampingProperty 等动画驱动属性，以及 relativeTransformPos/Rot。它们与 serializeData 配置并存，不能把同名字段显示成第二套“重力/阻尼”；具体覆盖时机要继续核对游戏调用链。
- 原始字节、TypeTree、CAB/PathID、未理解字段保留为来源证据，不在日常面板分页刷屏。

## 5. 碰撞到底是什么

需要区分三个量：

| 项目 | 归属 | 含义 |
|---|---|---|
| 模拟点/边半径 `radius` | 物理组及深度曲线 | 衣服、头发等代理节点或连线的检测厚度 |
| 球/胶囊半径 | 某个共享碰撞体 | 身体阻挡体的外形 |
| 自碰撞厚度 `surfaceThickness` | 物理组 | 代理面之间自碰撞/组间碰撞的厚度，不能由上面两个半径替代 |

当前游戏存在 Point 和 Edge 两类求解 Job。Point 将代理节点作为带半径的球进行检测；Edge 检测节点之间的边，能覆盖点间空隙，处理成本也更高。官方接口说明也是这一区分。[Collider Collision](https://magicasoft.jp/en/mc2_magicacloth_collidercollision/)

身体碰撞体必须被物理组的 `colliderList` 引用；仅仅把一个球显示在 Blender 里，或创建在角色骨架下，不等于它已经加入某个组。一个碰撞体可被多个组共享；“从组移除引用”和“删除碰撞体”是两种操作。Mesh 通过蒙皮跟随模拟骨骼，不能把显示出来的衣服三角面直接当作已设置好的物理检测面。

### 5.1 当前源碰撞形状

| 形状 | 源字段 | 新 UI 的主控件 |
|---|---|---|
| 球 | center、size.x | 绑定骨骼、局部中心、半径 |
| 胶囊 | center、size.x/y/z、direction、reverseDirection、radiusSeparation、alignedOnCenter | 绑定骨骼、局部中心、轴向、反向、总长、起始半径、可选末端半径、中心/起点对齐 |
| 平面 | center 和所属 Transform | 绑定骨骼、局部中心、法线显示；预览方块仅帮助观察，不是有限边界 |

胶囊 `size.x` 是起始半径，分离半径开启时 `size.y` 是末端半径，`size.z` 是包含两端圆头的总长，不能当作 XYZ 缩放。现有解析器还按居中/起点对齐换算两球心位置。新 UI 选总长作为唯一可编辑长度；球心距作为只读结果，不同时提供两个互相换算的长度输入。[CapsuleCollider](https://magicasoft.jp/en/mc2_capsulecollidercomponent/)

平面按无限平面理解，局部 +Y 为推出方向；有限方块只是编辑显示。终末地对应的真实碰撞方向还应纳入最小游戏实验。[PlaneCollider](https://magicasoft.jp/mc2_planecollidercomponent/)

源球/胶囊组件没有独立的任意旋转字段。不能为了一个旋转手柄而改动原动画骨骼的静止旋转。已有组件先提供其真实的中心和轴向；新增碰撞体若需要自由方向，可在导出 Prefab 中拥有独立的附属 Transform，该路径需与游戏接入一起验证。

自碰撞与组间碰撞使用代理拓扑及单独厚度，不是把所有碰撞体互相加入列表。现有 61 个组未启用这些模式，UI 不应暗示已在本项目验收。[Self Collision](https://magicasoft.jp/en/mc2_magicacloth_selfcollision/)

## 6. 拟定面板

以 3D 视图侧栏 `EFF → 物理` 为唯一参数编辑位置。对象属性中的旧 EFF 物理面板退出；选中骨骼、物理组或碰撞体后都进入这一个编辑器。

| 从上到下 | 内容 |
|---|---|
| 角色/骨架 | 当前 Rig、参数搜索、创建菜单 |
| 条目列表 | “物理组 / 碰撞体”切换；支持搜索、多选、来源标记、游戏启用和视口眼睛；两类开关明确分开 |
| 操作栏 | 添加、复制参数、粘贴到所选、预设、定位；直接写明目标数量 |
| 内容标签 | 结构、动力、碰撞、高级；同一字段只能属于其中一个位置 |
| 视口显示栏 | 范围、骨链、节点半径、角度锥、碰撞体、穿透、隔离/固定显示 |

“动力”按基础、恢复与角度、惯性、风、运动范围组织折叠区；“碰撞”放模式、节点半径、摩擦、碰撞体引用。半径不再在动力或骨骼面板出现可编辑副本。高级区放运行/LOD、蒙皮/构建及只读源检查，不重复收录常用参数。

参数搜索定位并展开字段所在位置，不生成另一套可以修改同一参数的搜索结果面板。选中多个组时，相同值正常显示，不同值显示混合状态；用户改哪个字段，只批量改变哪个字段。

源组件与新增组件使用相同的主控件。未知能力、引用或字段显示具体限制，不让用户先在“源参数”和“作者参数”两套 UI 中猜当前编辑模式。

### 6.1 选择与显示互不抢控制权

- 活动组负责正在编辑哪个原生物理组；活动条目可为该组的骨骼或某个共享碰撞体；多选集合负责批量操作。
- 选共享碰撞体不会任选它的某个引用组作为活动组。面板列出所有引用组，用户可以主动跳转。
- 显示范围只从一个显示栏设置。固定显示的碰撞体在切组后继续显示；取消隔离恢复之前显示状态。
- 单独隐藏优先；分类开关和范围过滤只作用于 EFF 辅助显示，不改用户 Mesh/Armature 的隐藏状态。
- 点击定位可临时显露并高亮指定条目，结束后恢复原过滤；不执行全场景 Unhide All。
- 隐藏辅助图形不会禁用游戏物理、删除导出资源或改变 Mesh 按键显隐。
- 骨链、半径球、角度锥和碰撞体独立勾选；仅活动对象强调，其余低对比显示。共享碰撞体只绘制一份。

### 6.2 创建流程

1. 选中链头骨骼，点击“从骨骼创建物理组”。默认沿子层级建链，根为固定、其余为运动；排除的分支在结构树中修改。允许多根建立一个组。
2. 选择骨骼，点击“添加球 / 胶囊 / 平面”。创建时绑定当前骨骼；若已有活动组，同时加入该组的碰撞列表，列表立即显示新条目。
3. 胶囊根据真实父子关节点生成初始长度和方向，多子骨骼时明确选择方向。不要拿导入时用于显示的 bone tail 当真实子节点位置。生成的尺寸是初始建议，可直接调整。
4. 手柄移动直接改中心；半径、长度手柄直接改对应字段。没有另一个 Object.location/scale 偏移等着在导出时相加。
5. 支持多选骨骼批量添加和明确的左右骨骼映射后镜像；已有碰撞体通过列表多选加入组，不逐个搜索下拉框。

每次创建、镜像、批量粘贴作为一个 Undo 操作。创建新物理组不自动重分配 Mesh 权重；复制骨链与复制参数也是不同操作。

### 6.3 参数复制与预设

默认“复制参数”包含动力参数和曲线，不包含根骨、忽略骨骼、碰撞体引用、源身份、启用状态、运行缓存或对象句柄。粘贴一次应用到全部所选兼容组。

支持“全部参数 / 仅当前功能区 / 仅当前参数”，都在同一个粘贴操作中选择。单字段粘贴沿用该字段的唯一编辑位置，不另建参数副本。

“同时复制碰撞引用”作为明确的可选项：同一 Rig 可共享已有碰撞体；跨 Rig 必须重新绑定，不能把另一角色的引用直接粘过去。预设保存参数值和曲线，不保存 CAB/PathID 或运行时身份。应用预设复制值，不制造一个同时控制多个组的隐藏共享 Settings 对象。

## 7. 与 Chain2 的对应关系

| RE Chain Editor 的体验 | 本项目采用方式 |
|---|---|
| 选链头创建 Chain | 选链头创建原生物理组，支持多根及分支 |
| 从骨骼创建 Collision | 一步绑定并加入活动组，带中心/尺寸手柄 |
| 节点球和角度锥 | 显示原生组曲线作用后的有效半径和角度 |
| 多选粘贴及预设 | 一次应用到所有选中目标，明确复制范围 |
| 节点/角度/碰撞显示过滤 | 使用一个显示控制入口，增加固定显示与恢复隔离 |
| Header / Settings / Group / Node 多级对象 | 不照搬；终末地物理组自身已有参数和多根列表 |
| 每个节点独立角度、半径和方向 | 不直接照搬；原生数据是组级曲线时按组级曲线编辑 |
| Hide Non Nodes / Unhide All | 不照搬全场景隐藏，避免影响用户场景和多个 Rig |

参考 [RE Chain Editor 工作流](https://github.com/NSACloud/RE-Chain-Editor)、[操作实现](https://github.com/NSACloud/RE-Chain-Editor/blob/54ed5d41a6360511b4b314c80e9b459032b32688/modules/re_chain_operators.py)、[Chain2 节点结构](https://github.com/NSACloud/RE-Chain-Editor/blob/54ed5d41a6360511b4b314c80e9b459032b32688/modules/file_re_chain2.py) 和 [可视化构造](https://github.com/NSACloud/RE-Chain-Editor/blob/54ed5d41a6360511b4b314c80e9b459032b32688/modules/re_chain_geoNodes.py)。这里参考编辑体验，不进行 Chain2 格式转换。

## 8. 实现时保留与舍弃

保留：TypeTree/原始组件读取、精确资源与骨骼身份、原字段保留、坐标换算、已验证胶囊几何、必要的导出验证。当前正式导出仍未接通新增 Physics，不能因新 UI 可以编辑便声称游戏支持。

舍弃：旧两处参数面板、源/新增两套交互、曲线参数下拉总页、日常参数分页表、多套可独立修改的曲线投影、碰撞中心的隐含叠加偏移、多个路径改写同一显示状态。

一个有类型的作者参数模型作为可编辑真值，原始源快照用于保留和比较；显示图形只读取参数，不持有第二份求解参数。不要借重构 UI 引入独立物理求解器、额外版本门槛或人为骨骼/曲线容量上限。

普通重力、阻尼数值改变不重建整套图形；半径/角度变化只更新相关预览；样式开关不重建物理数据。此处是性能要求，尚未做新实现的性能测量。

## 9. 后续验收

UI 可以先做交互原型，同时保留最小原生游戏接入实验；两者不互相代替。建议验收至少覆盖：

1. 一个参数可通过搜索定位，只有一个正式编辑位置；保存重开与导出值一致。
2. 九类曲线原样往返保留关键点及切线；基础值、曲线和选中节点的有效值一致。
3. 多根/分支组可正确选择与排除；不靠数组序号猜节点。
4. 一次多选粘贴可撤销，不改变骨骼、碰撞引用或游戏启用，除非明确选中对应操作。
5. 同一碰撞体被两个组引用时只显示一份，尺寸修改的影响范围明确；移除引用不删除共享体。
6. 切组、选碰撞体、固定显示、隔离与退出隔离不会互相覆盖，也不更改用户 Mesh 显隐。
7. 球/胶囊/平面的显示尺寸、局部坐标和导出字段一致；非均匀缩放需要单独验证，不能假定表现与均匀缩放相同。
8. 新三骨骼链、新碰撞体进入游戏后确实模拟和驱动 Mesh，Point/Edge 及碰撞方向有可观察结果。
9. 大世界、UI、多实例、连续 F10、卸载及切角色后仍正常注册/注销，无原物理被意外覆盖。

以上为目标验收；第一版实际实现与回归见第 11 节，不能将目标全部视作已实现。

## 10. 参数操作规范（2026-10-10）

以下是下一版编辑器的具体设计，不是当前插件已具备的功能承诺。常用骨链先围绕 BoneCloth 实现；其他原生类型保留源字段，按实际类型和已验证能力显示相关控件。

重构前 `COMMON_PARAMETERS` 只有 5 个常用值，`CURVE_PARAMETERS` 有 9 种曲线及部分关联字段，`draw_parameter_fields` 是只读保留表；许多惯性、风和运行设置没有日常编辑控件。第一版补齐了第 11 节所述字段绑定。再次核对发现旧作者验证器已接受多个固定根；旧 `add_selected_nodes` 的“第二根无效”注释过时，现已更正，不应将旧注释当作实际数据限制。

### 10.1 每个控件改什么

下表字段均省略 `serializeData.` 前缀。常用项在对应折叠区展开，其余折叠，但不另造一套“简易参数”覆盖它们。

| 唯一位置 | 中文控件 → 原生字段 | 操作规则 |
|---|---|---|
| 结构 / 骨链 | 根骨 → `rootBones`；排除子树 → `ignoreFromRootBones`；连接方式 → `connectionMode` | 从当前 Rig 选择骨骼，树中预览实际影响范围；根列表支持多根；切换连接方式属于结构变更 |
| 结构 / 模拟点 | 固定、运动、忽略 → 原生 Selection 属性与节点映射 | 选择骨骼批量设置；只有映射可信时可写；不是给骨骼加 Blender 刚体 |
| 动力 / 基础 | 重力 → `gravity`；世界方向 → `gravityDirection`；姿态重力衰减 → `gravityFalloff`；阻尼 → `damping` | 重力是加速度，阻尼是无量纲系数；阻尼带沿链曲线；不把重力衰减解释为沿链曲线 |
| 动力 / 基础 | 重置稳定时间 → `stablizationTimeAfterReset` | 以秒显示；用于初始化/重置，不当作持续阻尼的替代品 |
| 动力 / 姿态 | 输出物理混合 → `blendWeight`；恢复参考姿态 → `animationPoseRatio` | 前者控制物理结果参与混合，后者控制约束恢复参照；两者不能合成一个“物理强度” |
| 动力 / 角度恢复 | 启用 → `angleRestorationConstraint.useAngleRestoration`；恢复强度 → `.stiffness`；恢复速度衰减 → `.velocityAttenuation`；姿态重力衰减 → `.gravityFalloff` | 启用开关在区标题；强度带曲线；区内的重力衰减不与基础重力衰减互相同步 |
| 动力 / 角度限制 | 启用 → `angleLimitConstraint.useAngleLimit`；允许角度 → `.limitAngle`；限制强度 → `.stiffness` | 角度显示度数并带曲线，限制强度为独立数值；视口角度锥只读该结果 |
| 动力 / 形状保持 | 长度恢复 → `distanceConstraint.stiffness`；允许向根压缩比例 → `tetherConstraint.distanceCompression`；三角弯曲恢复 → `triangleBendingConstraint.stiffness` | 长度恢复带曲线；压缩比例越大表示允许缩得更近，不能误标成“保持强度”；没有代理三角形时三角弯曲项不适用 |
| 动力 / 惯性 | 整体移动影响 → `inertiaConstraint.worldInertia`；动画局部影响 → `.localInertia`；整体惯性平滑 → `.movementInertiaSmoothing`；根部局部惯性抑制 → `.depthInertia` | 分别控制不同来源的运动影响，不能并成一个摇摆滑条 |
| 动力 / 惯性（折叠） | 参考物体/影响 → `.anchor` / `.anchorInertia`；离心增强 → `.centrifualAcceleration`；五项移动/转动/粒子速度限制 | 引用用对象选择；每项限制均为“启用 + 数值”；移动速度与转速采用不同单位 |
| 动力 / 运动范围（折叠） | 最大偏移 → `motionConstraint.useMaxDistance` / `.maxDistance`；回挡 → `.useBackstop` / `.backstopRadius` / `.backstopDistance`；约束强度 → `.stiffness` | 最大偏移、回挡距离各带曲线；回挡依赖代理法线，不能用身体胶囊替代 |
| 动力 / 风（折叠） | `wind.influence`、`frequency`、`turbulence`、`blend`、`synchronization`、`depthWeight`、`movingWind` | 各字段保留原义；风区可单独复制，不能承诺 Blender 视口已有同款风模拟 |
| 碰撞 / 身体碰撞 | 模式 → `colliderCollisionConstraint.mode`；模拟厚度 → `radius`；摩擦 → `.friction`；碰撞体集合 → `.colliderList` | 厚度带曲线，模式下拉显示原生枚举；列表支持多选加入/移除/定位；来源默认保留 |
| 碰撞 / 自碰撞与组间碰撞（折叠） | `selfCollisionConstraint.selfMode`、`surfaceThickness`、`syncMode`、`syncPartner`、`clothMass` | 单独厚度曲线及组引用；样本中未启用，启用后的原生构建/模拟须单独验收 |
| 高级 / 运行与构建 | `updateMode`、`clothSimulateWeight`、游戏 LOD、剔除、传送、旋转控制、网格/蒙皮设置 | 已有组原样继承；语义未确认项保留原字段名与说明；不作为调软硬的日常入口 |

上述力、约束、惯性和混合语义参考 [Force](https://magicasoft.jp/en/mc2_magicacloth_force/)、[Shape Restoration](https://magicasoft.jp/en/mc2_magicacloth_shaperestoration/)、[Inertia](https://magicasoft.jp/en/mc2_magicacloth_inertia/)、[Animation Pose Ratio](https://magicasoft.jp/en/mc2_magicacloth_animationposeratio/)；这些是公开后端的解释，终末地的覆写、钳制和运行时响应仍须验证。特别是 `clothSimulateWeight` 与组件顶层动画驱动属性，不按名字推定等同 `blendWeight`。

### 10.2 怎样输入、恢复与批量修改

- 数值支持直接输入与拖动，显示单位；有曲线的参数统一为“基础值 + 沿链变化开关 + 就地曲线”。功能启用开关与曲线开关分开：关闭曲线表示均匀值，不表示关闭该约束。
- 每项有“恢复来源值”；新建组则恢复所选模板的初值。未编辑字段保留原始表示，不因 UI 打开、重绘或 float32 投影就重新写一遍。
- 常见系数可给 0～1 的拖动建议范围，角度用度数；这不等于确认所有原生字段的合法范围。不能用样本最小/最大值作为硬限制，也不能静默钳制已有源值。有限数值、合法枚举/引用等真实结构约束照常检查，未知范围先说明。
- 长度、速度、加速度采用资源约定的单位，并在导入/导出边界做统一坐标及比例换算；重力方向为世界空间，碰撞中心为绑定 Transform 的局部空间。UI 显示 Blender 轴向，原始游戏分量只在源检查中只读显示，不能出现两套可编辑坐标。不能仅修改 Blender 的单位显示便改变导出尺寸。
- 角度以度数、转速以度/秒、稳定时间以秒显示。拖动手柄与输入数字修改同一个字段，不在 Object.scale、中心偏移和字段之间暗中相加。
- 多选组显示共同值或“混合”；只修改操作的字段。复制可选全部参数、当前区或单项，曲线随对应参数复制；不复制运行状态、源身份或骨骼数字索引。
- 复制骨链、复制参数、复制碰撞引用是三个明确操作。跨 Rig 的碰撞引用必须重绑定；镜像必须有明确左右骨骼映射并正确转换局部中心/方向，不靠碰撞体名字猜目标。

### 10.3 曲线与选中骨骼如何配合

示例仅解释编辑方式，不作为全角色默认：角度基础值为 60°；深度 0、0.5、1 的倍率分别为 0.2、0.6、1，对应 12°、36°、60°。曲线图横轴标“组内基线深度”，纵轴标“倍率”；骨骼上高亮实际采样位置及有效角度。[官方曲线示例](https://magicasoft.jp/en/mc2_baseline/)

选中骨骼后显示“组、固定/运动状态、深度、有效半径/角度”。“在当前深度加点”直接给组曲线加关键点，不生成独立逐骨参数；若多条链在相同深度，提示并高亮共同影响的节点。短链末端的深度若为 0.6，必须在 0.6 调整，不能一律跳到 1。

同组曲线不能表达“相同深度的两个节点使用不同值”。确有此需求时，显式拆分组并配置各自参数，检查骨骼归属与可能的重复写入；不暗中增加后端不支持的逐骨覆盖层。原生构建可能改变代理节点、基线和深度，结构改动后应重新核对预览采样。

现有 `group_local_selection_graph` 按位置容差匹配 Selection 点，并在候选重合时按顺序挑一个，适合现阶段辅助预览，不能升级成可靠的结构写回契约。新编辑器应使用原生构建映射或可消歧的显式引用；共点、零长骨等无法消歧时保留源选择数据，显示具体限制，不随便给另一根骨骼写属性。修改已知角色位时仍保留其他未理解的属性标志位。

### 10.4 碰撞体怎样设置才会生效

1. 在当前 Rig 选骨骼，创建球、胶囊或平面；若当前有活动组，创建界面显示并勾选“加入此组”，一步建立引用。没有活动组时明确显示“尚未分配”，不让用户把可见辅助物误认为已参与碰撞。
2. 球只调绑定骨骼、局部中心、半径。胶囊再调轴向、反向、总长、起始半径、可选末端半径及对齐方式；生成初值以真实关节点为准。平面显示无限碰撞方向，预览方块大小仅属于显示设置。
3. 将碰撞体贴合需要阻挡的身体区域，避免静止姿态与模拟点严重重叠；界面同时显示组的模拟厚度。身体胶囊半径与模拟厚度分别编辑，不能为了修穿模同时任意放大两者。
4. 在组的碰撞列表确认引用、碰撞模式及组启用状态。一个碰撞体可供多组使用，尺寸调整前显示“被 N 个组使用”；从当前组移除只删除这一条引用。
5. Point 适合作为现有来源组的起点；确认穿过发生在节点之间后才试 Edge，不能默认把全部组改 Edge。摩擦越高越不易沿表面滑动，但不能替代尺寸/绑定/模式设置。[官方碰撞说明](https://magicasoft.jp/en/mc2_magicacloth_collidercollision/)

源胶囊没有任意旋转字段，旋转手柄不得修改原动画骨骼。需要自由方向的新碰撞体，显式生成附属 Transform；这个 Transform 及原生组件必须由作者编译器一并输出并验证。身体碰撞、自碰撞、Mesh 蒙皮权重是不同层次，碰撞体本身不自动给 Mesh 增加权重。

### 10.5 参数怎么调：一次解决一种现象

以下是依据公开后端说明制定的调节顺序，属于接入完成后的工作流；不是声称这些调整已经在终末地逐项验证。

| 观察到的现象 | 优先检查/调整 | 避免的误操作 |
|---|---|---|
| 完全不动 | 游戏启用、固定/运动映射、Mesh 权重是否指向模拟骨骼、物理输出/运行权重、实际注册 | 不先把重力和刚度全部拉满 |
| 长时间来回弹、停不住 | 逐步增加阻尼；区分角度恢复的速度衰减，观察恢复过程 | 不把阻尼当作角度范围，不同时改数项 |
| 太硬、几乎不弯 | 降低角度恢复强度；确认角度限制是否过小、最大偏移是否过紧 | 不首先降低长度保持以制造“软”，否则可能只是被拉长 |
| 太软、一直塌下 | 增加角度恢复，核对重力、参考姿态；拉长时再核对距离恢复 | 不用大幅增加阻尼替代形状恢复 |
| 转身/起停时甩得过猛 | 整体/局部惯性来源、惯性平滑及相应速度限制；仅快动作异常时优先限速 | 不默认将整体惯性清零，导致慢动作也完全不响应 |
| 根部乱动但末端应自由 | 先确认固定根，再调恢复/角度曲线或根部局部惯性抑制 | 不修改原动画骨骼的静止姿态来补偿 |
| 身体穿模 | 碰撞引用、绑定空间、尺寸、组厚度；再判断 Point 间隙与 Edge 需求 | 不用摩擦代替碰撞，不把自碰撞厚度当身体碰撞半径 |
| 衣服浮起/被顶飞 | 静止姿态的初始交叠、身体碰撞尺寸、组厚度；重置时再查稳定时间 | 不将每个碰撞体持续放大 |
| 动作中恢复到不合适姿态 | 核对 `animationPoseRatio` 及实际动画输入 | 不把参考姿态比例当作输出物理强度 |

调节时先固定一套可重复动作：静止、起步/停止、转身，再看碰撞及重置。复制一份参数预设保存起点；每次只改一个字段或一条曲线，再对照。仅做简单静态骨链、半径和角度预览不能证明动态碰撞正确；第一阶段游戏才是运动效果验收环境，不额外实现近似求解器误导用户。

### 10.6 新组的初值从哪里来

已有组保留来源值，不因换 UI 重置。新组优先从用户选择的同类来源组复制纯参数，再重新指定骨链、所属组件挂点和碰撞引用。模板按实际选择/资产结构生成，不在 DLL 或插件里硬编码“某角色衣服使用某组”。没有模板时明确给出工具初值及尚未游戏验证的状态，不能把它叫作已验收原生预设。

本轮只读汇总五角色 61 个 BeyondBoneCloth，得到下列**基础值**范围，未包含曲线倍率，未区分每个约束是否启用；这些数据用于理解差异，不能作为默认值或合法上下限：

| 字段 | 样本最小～最大 |
|---|---:|
| 重力 | 0～10 |
| 阻尼基础值 | 0.01～0.8 |
| 模拟厚度基础值 | 0.006～0.13 |
| 距离恢复基础值 | 0.242～1 |
| 角度恢复基础值 | 0.03～1 |
| 角度恢复速度衰减 | 0.08～0.95 |
| 角度限制基础值 | 8°～140° |
| 角度限制强度 | 0.046～1 |
| 恢复参考姿态比例 | 0～1 |
| 输出混合 `blendWeight` | 全部为 1 |
| 整体惯性 / 局部惯性 | 各为 0～1 |
| 整体惯性平滑 | 0.1～0.8 |

### 10.7 编辑、导出与性能边界

编辑器保存一份有类型的作者参数及结构，原始组件快照用于保留未知字段和来源比较。数值控件、曲线图、视口手柄和导出读取同一份值；Undo/Redo 也以这份数据为准。参数与根骨/碰撞引用各有类型，不把引用塞进随意可写的数字路径。

改变重力、阻尼等参数不重建 Blender 骨架或全部辅助物；半径/角度只更新对应可见预览。拖动时合并刷新，松开完成一次 Undo；隐藏组不持续重建图形，不因选择变化扫描所有场景对象。结构变更才标记相应构建数据需要更新，不能每次面板重绘重新解析源 JSON。

参数变更也可能影响原生约束预计算，不能假定“参数没改结构就一定不用构建”。作者编译器需核对真实消费路径，必要时重新生成对应约束/预构建数据；骨骼、连接、点选择、碰撞绑定变化则须重新验证相关结构。未编辑的游戏原始数据继续保留，不能无条件复制旧 `preBuildData` 给新链。

导出所选 Mesh 时，物理仍须显式选择对应物理组/导出选项；仅沿已确认引用补齐所需骨架节点、碰撞体及组件，不把整个 Rig 的全部物理或全场景资源默认打进去。参数文件不保存 team、粒子槽位或当前游戏对象句柄。编译资源由 Blender 自己的构建器完成，DLL 使用通用资源规则加载；新增组完整进入原生构建/注册及 F10 安全退出仍是待完成接入项。

旧作者编解码仍有 `MAX_CURVE_KEYS=64` / `MAX_NATIVE_PARAMETERS=4096` 等工具限制；单根限制的旧注释与当前多根验证器不符。统一编辑器没有新增数量上限，但本轮未改交换资源的编解码范围或 wire 格式。后续应按真实结构、文件剩余长度及格式可表示范围校验，不能仅改 UI 就声称已去掉所有旧协议限制或支持任意字段。

界面给出具体状态：“可编辑，待导出”“结构改动，需重新构建”“映射未确认，仅保留来源”。可见、可编辑、已编译、游戏已验证是不同事实；不能用一个绿色勾选混淆这些状态。第一批验收应先做已存在组参数往返，再做新增碰撞体，再做三骨骼新链及连续 F10，最后扩展拓扑、自碰撞和其他类型。

这里显示的有效半径/角度是根据编辑配置与当前映射计算的预览值，不是从游戏实时读回的最终求解值。顶层动画驱动属性与 `serializeData` 的覆写关系未查明前，不提供第二套同名滑条，也不保证修改配置就一定越过游戏控制器生效。

## 11. 第一版统一编辑器的实际范围

2026-10-10。实现位于 `tools/Blender/eiem_physics_editor.py`，从 `eiem_physics_authoring.register` 接入。旧侧栏和对象属性参数面板的实现已删除，正式入口为 3D View → EFF → 物理。

已完成：

- 按当前 Rig 的物理集合列出物理组/碰撞体，支持搜索、批量勾选、来源标记、单独隐藏及固定显示；选共享碰撞体不自动挑选某个引用组。列表显示与游戏禁用是不同控件。
- 参数集中在结构、动力、碰撞、高级；数值、开关、枚举和重力向量直接通过 get/set 读写现有字段，没有保存第二份 UI 参数值。未知枚举保留来源值；原作者紧凑属性继续作为旧工程与导出的适配层。
- 九类曲线在各自参数行就地展开；图形控件是现有原生关键点的投影，保留未编辑的切线、权重和源数据。短链不一律归一化到 1；骨骼信息标明是配置预览。
- 复制全部已建控件参数或当前功能区，批量粘贴到勾选组；默认不复制骨骼、碰撞引用、来源身份、启用状态或预构建数据。变长关键点数组完整替换，所有目标先验证；意外应用失败恢复字段元数据、曲线和作者适配值。
- 在当前工程保存/更新命名参数预设，应用时复制值到所选组，不创建跨组共享 Settings。修改一个数值时，若活动组在批量勾选集合内，只改变所选组的该字段；不同值显示“混合”。曲线图直接修改当前组，批量曲线通过复制/粘贴完成，界面明确说明。
- 来源参数可恢复来源值。源球/胶囊中心只显示一个 Blender 坐标输入；编辑时把旧 Empty 的附加平移合入序列化中心，避免再次相加。源胶囊按总长、真实半径和轴向编辑；新作者胶囊以总长作为编辑输入，旧 span 继续作为交换格式适配值。
- 保留新作者链的节点增删、角色设置、碰撞体创建/重绑/删除、碰撞引用及独立作者文件导出操作。源根链与排除结构显示但未开放未经证明的结构写回。
- 普通力参数不重建辅助图形；阻尼等不影响外形的曲线编辑也不再触发整组预览重建；中心向量输入合并为一次碰撞体预览重建。

实际回归在独立 Blender 5.0.1 进程运行，临时目录由测试自动回收，用户工程未保存：

1. Typhoea 11 个源组、27 个碰撞体导入，源字段无修改读取、参数/曲线/中心写入及独立物理资源往返。
2. 全部/局部参数计划、不同关键点数量、元数据保留、多目标预验证和故障注入回滚；不改变骨骼、碰撞引用或组件启用。
3. 新作者组应用完整模板、参数进入已有作者交换资源、预设随测试工程保存重开；固定显示/隐藏不改变导出字段。
4. 四个标签与源组/作者组/碰撞体的参数绘制通过验证布局检查；正式插件连续三次注册/卸载、场景对象不变。
5. 普通 Typhoea 包的 Mesh、共享 Rig 和物理一起导入；旧作者链、多根分支、共享碰撞体与工程往返回归通过。

不是完成范围：新增物理进入游戏的编译、原生预构建/注册、原生点映射写回、骨链自动延伸/镜像、碰撞 Gizmo 拖动、任意附属 Transform、新平面创建、自碰撞实机响应及物理退出/F10 验收。列表/参数布局已有 Blender 回归，但尚未进行用户角色上的完整动态操作验收。完整参数一份 typed model 的进一步收敛仍应保留旧工程适配，不能随便抛弃原始证据或数据。

本地插件目录是源码目录的 Junction，文件已同步；当前打开的 Blender 也已仅重载物理模块，新编辑器存在、旧两个面板不存在。重载前后核对当前工程路径、模式、对象清单、活动对象、选择与全部 Mesh 的顶点坐标 SHA，完全一致；没有保存用户工程。当前工程的 EFF 物理记录为 0，具体参数需要导入相应源物理后查看。没有更改 DLL、Mod 或用户工程文件。

### 11.1 源 Selection 预览与导入解耦

2026-10-10。庄方宜真实解包包也已回归。`MC_pendant`、`MC_pantsL`、`MC_pantsR`、`MC_sideHair` 保存的 Selection 与当前 Transform 姿态存在约 1.5 毫米的位置偏差，旧版将辅助预览的 `1e-4` 坐标匹配失败当成组件导入失败。独立 Blender 可稳定复现同一报错，不依赖作者 Mesh 或权重。

现在先检查可唯一匹配的源 Transform；不匹配时，使用原包预构建代理点、单骨骼蒙皮槽与显式 Transform 引用恢复只读预览。Selection 和代理点按几何匹配，不按数组顺序直接配对；允许整组共同平移，但点集合、属性、引用、父链和深度仍需核验。引用字段与解包引用记录也必须一致。不会重写源 Selection、骨架或预构建数据，不将该预览映射升级为结构写回/游戏装配契约。

共点、过时或不一致的预构建不能证明唯一绑定时，保留完整源组件与参数，显示原始 Selection 标记及 Transform 层级，并在结构页说明该组深度/半径/角度预览不可用。仅明确的预览映射错误允许退化；解码、源节点、骨架和引用校验仍正常拒绝无效输入。也不再按源数组或 Transform 排序随意选一个共点骨骼。

新增真实来源回归覆盖庄方宜完整导入/原字段往返、Selection 重排仍映射到相同骨骼与深度、共点拒绝猜测、无法证明的映射保留可编辑源组件、故障状态不重试每个骨骼查询，以及预览恢复后清理失效状态。原 Typhoea 参数/曲线/作者组/工程往返、普通包导入、统一编辑器、插件注册回归同时通过；旧测试不再要求对无法消歧的共点生成虚假的半径和角度预览。

现场已仅重载物理模块，在用户刚选中的 `Skeletonchr_0030_zhuangfy_postmodel_0.001` 补齐庄方宜的 13 个源组、29 个碰撞体；只有 `MC_ClothR_Ornament` 保留受限预览，其参数仍可编辑。原 Mesh 数据、顶点坐标和权重摘要前后相等，活动对象与选择恢复，未保存用户工程。新增物理游戏编译与 F10 接入的未完成边界不变。

## 附录：serializeData 完整语义字段清单

以下清单从 LZY 首个 BeyondBoneCloth 的真实 TypeTree 自动折叠。曲线、向量、引用、列表及带开关数值各计为一项。源示例来自该组件，不是默认值，也不是合法范围；不同角色可有不同取值。


| 原生字段（省略 serializeData.） | 数据形式 | 此源组示例 |
|---|---|---|
| `clothType` | 枚举/整数 | 1 |
| `sourceRenderers` | 引用/资源列表 | 0 项 |
| `meshWriteMode` | 枚举/整数 | 0 |
| `paintMode` | 枚举/整数 | 0 |
| `paintMaps` | 引用/资源列表 | 0 项 |
| `rootBones` | 引用/资源列表 | 1 项 |
| `ignoreFromRootBones` | 引用/资源列表 | 7 项 |
| `connectionMode` | 枚举/整数 | 0 |
| `rotationalInterpolation` | 浮点数 | 0.5 |
| `rootRotation` | 浮点数 | 0 |
| `updateMode` | 枚举/整数 | 10 |
| `clothAnimatorAbilityLODThreshold` | 枚举/整数 | 2 |
| `clothAnimatorLODThreshold` | 枚举/整数 | 9 |
| `clothLodFadeTime` | 浮点数 | 2 |
| `clothSimulateWeight` | 浮点数 | 1 |
| `resetSimulationToAnimationPoseWhenWeightLow` | 开关 | 开 |
| `resetSimulationToAnimationPoseWeightThreshold` | 浮点数 | 0.03 |
| `animationPoseRatio` | 浮点数 | 0 |
| `reductionSetting.simpleDistance` | 浮点数 | 0 |
| `reductionSetting.shapeDistance` | 浮点数 | 0 |
| `customSkinningSetting.enable` | 开关 | 关 |
| `customSkinningSetting.skinningBones` | 引用/资源列表 | 0 项 |
| `normalAlignmentSetting.alignmentMode` | 枚举/整数 | 0 |
| `normalAlignmentSetting.adjustmentTransform` | 对象引用 | 空引用 |
| `cullingSettings.cameraCullingMode` | 枚举/整数 | 30 |
| `cullingSettings.cameraCullingMethod` | 枚举/整数 | 0 |
| `cullingSettings.cameraCullingRenderers` | 引用/资源列表 | 0 项 |
| `cullingSettings.distanceCullingLength` | 启用开关＋数值 | 关；30 |
| `cullingSettings.distanceCullingFadeRatio` | 浮点数 | 0.2 |
| `cullingSettings.distanceCullingReferenceObject` | 对象引用 | 空引用 |
| `normalAxis` | 枚举/整数 | 1 |
| `gravity` | 浮点数 | 6 |
| `gravityDirection` | 三维向量 | 0, -1, 0 |
| `gravityFalloff` | 浮点数 | 0 |
| `stablizationTimeAfterReset` | 浮点数 | 0.1 |
| `blendWeight` | 浮点数 | 1 |
| `damping` | 基础值＋开关＋曲线 | 基础 0.5；曲线开；2 个关键点 |
| `radius` | 基础值＋开关＋曲线 | 基础 0.077；曲线开；2 个关键点 |
| `inertiaConstraint.anchor` | 对象引用 | 空引用 |
| `inertiaConstraint.anchorInertia` | 浮点数 | 0 |
| `inertiaConstraint.worldInertia` | 浮点数 | 0.5 |
| `inertiaConstraint.movementInertiaSmoothing` | 浮点数 | 0.734 |
| `inertiaConstraint.movementSpeedLimit` | 启用开关＋数值 | 开；5 |
| `inertiaConstraint.rotationSpeedLimit` | 启用开关＋数值 | 开；720 |
| `inertiaConstraint.localInertia` | 浮点数 | 0.5 |
| `inertiaConstraint.localMovementSpeedLimit` | 启用开关＋数值 | 关；5 |
| `inertiaConstraint.localRotationSpeedLimit` | 启用开关＋数值 | 关；720 |
| `inertiaConstraint.depthInertia` | 浮点数 | 0.7 |
| `inertiaConstraint.centrifualAcceleration` | 浮点数 | 0 |
| `inertiaConstraint.particleSpeedLimit` | 启用开关＋数值 | 开；4 |
| `inertiaConstraint.teleportMode` | 枚举/整数 | 0 |
| `inertiaConstraint.teleportDistance` | 浮点数 | 0.5 |
| `inertiaConstraint.teleportRotation` | 浮点数 | 90 |
| `tetherConstraint.distanceCompression` | 浮点数 | 0.5 |
| `distanceConstraint.stiffness` | 基础值＋开关＋曲线 | 基础 1；曲线关；2 个关键点 |
| `triangleBendingConstraint.stiffness` | 浮点数 | 1 |
| `angleRestorationConstraint.useAngleRestoration` | 开关 | 开 |
| `angleRestorationConstraint.stiffness` | 基础值＋开关＋曲线 | 基础 0.547；曲线开；2 个关键点 |
| `angleRestorationConstraint.velocityAttenuation` | 浮点数 | 0.241 |
| `angleRestorationConstraint.gravityFalloff` | 浮点数 | 0 |
| `angleLimitConstraint.useAngleLimit` | 开关 | 开 |
| `angleLimitConstraint.limitAngle` | 基础值＋开关＋曲线 | 基础 30；曲线开；2 个关键点 |
| `angleLimitConstraint.stiffness` | 浮点数 | 1 |
| `motionConstraint.useMaxDistance` | 开关 | 关 |
| `motionConstraint.maxDistance` | 基础值＋开关＋曲线 | 基础 5；曲线关；2 个关键点 |
| `motionConstraint.useBackstop` | 开关 | 关 |
| `motionConstraint.backstopRadius` | 浮点数 | 0.1 |
| `motionConstraint.backstopDistance` | 基础值＋开关＋曲线 | 基础 0；曲线关；2 个关键点 |
| `motionConstraint.stiffness` | 浮点数 | 1 |
| `colliderCollisionConstraint.mode` | 枚举/整数 | 1 |
| `colliderCollisionConstraint.friction` | 浮点数 | 0 |
| `colliderCollisionConstraint.colliderList` | 引用/资源列表 | 1 项 |
| `colliderCollisionConstraint.collisionBones` | 引用/资源列表 | 0 项 |
| `colliderCollisionConstraint.limitDistance` | 基础值＋开关＋曲线 | 基础 0.05；曲线关；2 个关键点 |
| `selfCollisionConstraint.selfMode` | 枚举/整数 | 0 |
| `selfCollisionConstraint.surfaceThickness` | 基础值＋开关＋曲线 | 基础 0.005；曲线关；2 个关键点 |
| `selfCollisionConstraint.syncMode` | 枚举/整数 | 0 |
| `selfCollisionConstraint.syncPartner` | 对象引用 | 空引用 |
| `selfCollisionConstraint.clothMass` | 浮点数 | 0 |
| `wind.influence` | 浮点数 | 1 |
| `wind.frequency` | 浮点数 | 1 |
| `wind.turbulence` | 浮点数 | 1 |
| `wind.blend` | 浮点数 | 0.7 |
| `wind.synchronization` | 浮点数 | 0.7 |
| `wind.depthWeight` | 浮点数 | 0 |
| `wind.movingWind` | 浮点数 | 0 |
| `springConstraint.useSpring` | 开关 | 开 |
| `springConstraint.springPower` | 浮点数 | 0.04 |
| `springConstraint.limitDistance` | 浮点数 | 0.1 |
| `springConstraint.normalLimitRatio` | 浮点数 | 1 |
| `springConstraint.springNoise` | 浮点数 | 0 |

### 11.2 新骨链作者编译（2026-10-10）

本节更新上述“待接入”的边界：新骨链和球/胶囊碰撞体已能通过“全部资源”导出生成原生 Prefab 修改，作者数据仍由 INI 声明。以实际正权重对应的来源 Mesh 找全部消费者，按真实骨架去重，离线写入新节点、组件、类型/脚本注册、Selection、碰撞引用和预加载列表。平面、已有组修改、不同物理类型、自碰撞及游戏原生工作退出/F10 均不得据此标为已完成。

当前 ZFY 两组 Silkskirt 在 21 个 Prefab 中通过引用及原对象保留检查；每个模型的两个组共用对应新骨架节点，多个 Renderer 不重复生成组。新物理文件约 281 KB，compiled.bin 不包含原包的完整副本。游戏画面及物理生命周期还需要实机验收，离线验证不替代该步骤。

### 11.3 ZFY 小骨骼的压缩动画核查（2026-10-10，只读）

不能依据 PostModel 的物理组件列表没有某条骨骼，就推断它没有驱动、只能手工 K 帧或必须新增物理。动画在独立资源包内，可能与运行时物理叠加。

本轮从本地 `endfield_assets.eidx` 定位并仅提取 `Bundles/Windows/main/139c9cff3063dc0b2ca537e9.ab`：

| 动画 | Path ID | 来源 Container |
|---|---|---|
| `A_actor_zhuangfy_cloth_ik_l` | -2369741714076334036 | `assets/beyond/arts/entity/actor/lady/zhuangfy/animations/a_actor_zhuangfy_cloth_ik_highres.fbx` |
| `A_actor_zhuangfy_cloth_ik_r` | -2916405244026222548 | 同上 |

提取文件 SHA256 为 `EB40A860574221FD2E3169430F4FBD08AB7F3CBE779A1919B25093EC580EA62C`。当前 AnimeStudio 能读出两个 AnimationClip 对象，各有 455 个 genericBindings，但转换得到的 `.anim` 普通位置、旋转、缩放曲线为空。`AnimationClip.cs` 读取了 `m_AclCompressedBuffer`，而 `AnimationClipConverter.ProcessInner()` 未消费它；仅导出绑定表不代表已还原动画。

实际 Transform 缓冲标记为 `0xAC11AC11`、ACL 格式 10，每个动画含 224 条 Transform 轨道，解码时长约 0.166667 秒。使用 [Ruri.RipperHook 的 Ruri.ACL 解码器](https://github.com/FractalTools/Ruri.RipperHook/blob/ed62f85af4e5da83dabd514b29ee93ef7d34e86b/Source/Ruri.ACL/AclCompressedTracks.cs)（提交 `ed62f85af4e5da83dabd514b29ee93ef7d34e86b`）在临时工具中采样成功：12 个时间点均为有限值，左右各有 8 条变化轨道。工具尚未集成到本项目。

以本地来源 Skeleton 的完整路径 CRC32 对 genericBindings 精确匹配，找到下列左侧骨骼的位置和旋转绑定。224 个 Transform 位置绑定与 224 条解码轨道对应；源骨架能匹配其中 163 条路径，左/右动画分别有 149/152 条初始局部姿态与源骨架吻合。左侧动画中，下列轨道的局部位置也与源骨架吻合，旋转随时间变化：

| 骨骼 | 完整路径 CRC32 | 轨道索引 | 相对首样本的最大旋转差，约 |
|---|---:|---:|---:|
| `clothes_ribbon_L_b_01_jnt` | 603359091 | 116 | 58.5° |
| `clothes_ribbon_L_b_02_jnt` | 2283762242 | 117 | 30.1° |
| `clothes_ribbon_L_b_03_jnt` | 1851515774 | 118 | 11.4° |

因此，这三条骨骼确有压缩动画轨道；仅据模型包的物理清单断言其运动完全来自实时物理或全部手工 K 帧，都不成立。本轮未验证游戏何时、按什么权重混合此短片段，也未证明它是否由离线物理烘焙、程序生成或手工制作。`cloth_tilt`、`wind_loop_additive_cloth` 等片段已在索引中找到，但未在本轮解码验证。完整动画导出仍需处理掩码、默认值/常量、根运动、浮点曲线、骨架映射及实际播放验证。

仅修改本分析记录；未修改、部署 AnimeStudio、Blender 或游戏 DLL，未改用户工程和 Mod。临时提取包、导出、试读工具及外部源码克隆位于 `E:\EIEM_Workspace\tmp_anim_probe`；清理请求被自动审批以 `blocked by policy` 拒绝，目录暂留，未改用其他方式删除。

### 11.4 jnt 的物理引用与动画轨道交叉核对（2026-10-11，只读）

本轮复用上一节已有的两个压缩动画采样，重新核对原始 `.ab` 的 SHA256，与上一节完全一致。将当前 ZFY `physics/components.json` 的完整 Transform 路径去掉模型根前缀后计算 CRC32，匹配采样绑定表；同时按 Transform 的 CAB/PathID 身份检查全部 BeyondBoneCloth 引用，未依赖骨骼名后缀分类。

| 骨骼 | 当前模型物理组件中的证据 | 两段 cloth_ik 中的动画证据 |
|---|---|---|
| `R_trousersDn_A_02_jnt` | `MC_pantsR` 的代理 `transformArray[10]` 引用该 Transform | 两段均有轨道，12 个采样中的局部旋转恒定 |
| `R_trousersUp_05_jnt` | 未找到 BeyondBoneCloth 对它的直接引用 | 两段均有轨道（197），采样局部旋转恒定 |
| `sleeve_base_L_b_01_jnt` | 未找到 BeyondBoneCloth 对它的直接引用 | 两段均有轨道（88），采样局部旋转恒定 |
| `clothes_ribbon_L_b_01/02/03_jnt` | 三条均未找到 BeyondBoneCloth 的直接引用 | 左侧轨道 116/117/118 确认旋转变化，最大差 58.522°/30.094°/11.372°；右侧采样恒定 |

`MC_pantsR` 的四个根为 `R_trousersDn_A/B/C/D_01_jnt`，不是 `R_trousersUp_*`；左侧对应 `L_trousersDn_A/B/C/D_01_jnt`。该物理组的名字不能当作整条裤装骨架的驱动分类。

恒定局部旋转仍可通过父骨骼姿态产生世界空间运动；在这两个短片段中恒定也不证明在其他动作、叠加动画或程序驱动中恒定。仅凭模型包没有 BeyondBoneCloth 引用不能排除其他后端或运行时驱动；有压缩动画轨道也不能判断创作时采用手工关键帧、IK 或离线物理烘焙，更不能断言与实时物理互斥。本轮没有重新解包、修改用户资源或运行会写采样文件的 Inspector。

### 11.5 动画是否实际接入物理结果（2026-10-11，只读调查）

本轮区分三个层次：原动画是否包含物理骨链的姿态、游戏是否有实际调用动画数据的物理控制链、当前具体动作的最终混合量。前两项有资源及原生代码证据，第三项未进行实机采样，不能以字段名或某个动画的骨骼角度代替最终物理结果。

仅额外提取三个相关动画包，共 232,189 字节，没有解包全游戏：

| 来源包 | 实际 AnimationClip |
|---|---|
| `Bundles/Windows/main/0c3a12afa2b36bbf371286dc.ab` | `A_actor_zhuangfy_wind_loop_additive_cloth`、`A_actor_zhuangfy_sprint_loop_additive` |
| `Bundles/Windows/main/1c6f11b5ee7af914609e62fe.ab` | `A_actor_zhuangfy_cloth_tilt_fb1/fb2/rl` |
| `Bundles/Windows/main/57b59a72327236b6f411cdda.ab` | `A_actor_zhuangfy_cloth_openclose`、`A_actor_zhuangfy_cloth_leanopenclose` |

用原物理组件代理 `proxyMesh.transformData.transformArray` 的 CAB/PathID 身份取得完整骨骼路径，再以路径 CRC32 匹配动画绑定。上述动画各能匹配 `MC_pantsR` 的 12 条链骨骼。ACL Transform 位置绑定数与解码轨道数逐片段一致；12 个时间点相对首样本的局部旋转差如下。它们是原动画姿态变化，**不是游戏最终物理输出角度**。

| 物理链骨骼 | wind / sprint additive | cloth_tilt_fb1 | cloth_openclose |
|---|---:|---:|---:|
| `R_trousersDn_A_01_jnt` | 25.387° | 9.452° | 6.000° |
| `R_trousersDn_A_02_jnt` | 21.392° | 1.255° | 恒定 |
| `R_trousersDn_D_03_jnt` | 69.337° | 恒定 | 恒定 |

七个新查片段及先前两个 cloth_ik 的非 Transform 绑定均为 Animator 的 `customType=8` 绑定，没有 BeyondBoneCloth 脚本属性绑定。因此不能宣称这些片段直接用曲线写 `blendWeightProperty` 或 `animationPoseRatioProperty`，也不能外推其它未查片段同样没有。

对当前 GameAssembly 和元数据的只读核查得到以下实际调用链。RVA 仅记录本次被检查的文件，不用于产品寻址或版本限制。

1. `CharacterAnimationBlackboard._UpdateMagicaCloth`（`0x2fb6e30`）读取字符串键 `MagicaClothWeight`、`MagicaClothWeightAdd`、`MagicaClothWeightForce`。按元数据字符串字面量索引解出三个键，不是根据名称猜测。读取接口的原生路径最终解析并调用 `UnityEngine.Animator::GetFloatID(System.Int32)`；缺失曲线时还有移动/战斗、IK 等分支及显式覆盖分支，因此不能保证每个角色每个动作都提供这三条曲线。
2. 该方法计算并平滑 `m_physicsClothWeight`、`m_physicsClothStaticWeight`，遍历模型物理组件；在 `0x2fb7e23`、`0x2fb7ee4` 实际调用 `BeyondBoneCloth.SetClothSimulateWeight`（`0x2fb59b0`）。敌人 `EnemyAnimationBlackboard.OnUpdate` 也有同类调用。
3. `SetClothSimulateWeight` 将传入值写入组件动画属性、缓存值及 `serializeData.clothSimulateWeight`。它不是与 `blendWeight` 同一字段；本轮不将其简化为通用线性混合滑条。
4. `InitAnimationProperty`（`0x343a0a0`）将 `serializeData` 中的初始值复制到顶层 `...Property` 和缓存字段。这解释了源 JSON 的顶层属性为 0，而 `serializeData.clothSimulateWeight/blendWeight` 为 1：不能直接把顶层序列化 0 当作运行时权重。
5. `OnDidApplyAnimationProperties`（`0x59dcf38`）比较动画属性与缓存；变化时直接覆写 `serializeData.animationPoseRatio/clothSimulateWeight/blendWeight` 等对应值并调用 `SetParameterChange`。这一机制已确认，但新查九个片段没有对该组件的属性绑定，不能据此宣称它们触发了所有这些覆写。
6. `TeamManager.ApplyWeightDrivenSimulationPosePolicy` / `ShouldResetSimulationToAnimationPose` 有低模拟权重时切回动画姿态的条件分支。当前 `MC_pantsR` 和 `MC_ClothL` 的 `resetSimulationToAnimationPoseWhenWeightLow=0`，阈值 0.01；故不能把该可选分支当作它们当前已启用的行为。

字段身份通过当前 PE 的字段偏移表与元数据交叉验证，未单靠指令偏移猜测：组件 `serializeData` 为 0x98、`clothSimulateWeightProperty` 为 0x40、`animationPoseRatioProperty` 为 0x38、`blendWeightProperty` 为 0x70；serializeData 对应偏移为 0x5c、0x68、0xac。值类型 TeamData 的偏移包含装箱头，不能直接照搬为原生结构偏移。

由此能确认原物理骨链也存在动画姿态轨道，游戏动画更新流程确实把 Animator 曲线及运动状态接入模拟权重控制。尚未确认 ZFY 当前状态机对三条权重曲线的具体播放与数值，也未测到最终骨骼姿态；不得声称所有物理骨骼在所有动作中都按固定比例混合，或将动画轨道当作模拟结果录制。

本轮未修改、部署 DLL、Blender 或 AnimeStudio 产品代码，未修改工程、Mod 和游戏原包。新增只读证据工具 `tools/diagnostics/inspect_animation_bindings.cs`：依赖目录、原 physics/components.json 和已提取包作为输入，打印绑定摘要及物理链动画采样；ACL 解码器复用 11.3 的已有诊断程序集。工具及小包位于忽略的 `bin/physics-animation-probe` 构建目录。

### 11.6 动画影响量与最终位置混合（2026-10-11，只读）

继续核查原生求解代码得到最终位置混合的定量边界，不能把 11.5 的骨骼轨道角度、`clothSimulateWeight` 或 `animationPoseRatio` 当作同一个输出百分比。

`SimulationStepTeamUpdateKernel$BurstManaged`（`0x5a39fa0`）把 `TeamData.blendWeight` 计算为 `clamp01(ClothParameters.blendWeight * TeamData.velocityWeight * TeamData.distanceWeight)`。在当前文件 `0x5a3a9dc` 到 `0x5a3aa28` 可见连续两次乘法、0/1 钳制及写回。TeamData 的原生偏移为 velocityWeight=0xfc、distanceWeight=0x100、blendWeight=0x104；ClothParameters.blendWeight=0x18，均用元数据与字段表核对，值类型偏移扣除装箱头。

`CalcDisplayPositionKernel$BurstManaged`（`0x5a6da04`）在 `0x5a6deb6` 读取该 blendWeight；在 `0x5a6df05` 调用有 0/1 边界处理的 double3 插值，再写入 positions 数组。调用前的两端为原基础姿态位置与经过显示插值处理的模拟位置。相应标量插值辅助路径 `0x59e89e4` / `0x59eac0c` 为 `a + (b-a)*w`。

因此，**在这一最终位置混合步骤内**：w=0 采用基础姿态位置，w=0.5 位于基础与模拟位置中间，w=1 采用模拟位置。该比例不是整个骨骼运动的“动画占比”，也不是旋转角度、约束力度或模拟停止开关。模拟结果本身仍可能受动画驱动的根、固定点及恢复参考影响。

当前源 MC_pantsR 和 MC_ClothL 均为 animationPoseRatio=0、clothSimulateWeight=1、blendWeight=1。若 velocityWeight、distanceWeight 也为 1，最终位置混合 w=1；不能据此把具体动作的运行时值写成恒定 1。animationPoseRatio=0 对应恢复参考采用初始姿态，不能解读成所有动画输入被关闭；`clothSimulateWeight` 是另一运行时控制量，本轮未完成其所有消费者的定量追踪。

当前没有 Endfield 进程，没有取得具体状态中的权重、骨骼写回结果或开关对照。可以确认原生混合公式和来源初值，尚不能给出 ZFY 当前待机/跑动“动画占 20% / 50%”之类实测结论。本轮仅追加分析记录，未修改资源、工程或产品代码。

### 11.7 具体动作的物理控制曲线与骨链动画（2026-10-11，离线资源核对）

继续读取 ZFY 的实际待机、移动、跳跃、战斗和 UI 动画。本轮诊断缓存共 20 个包、6,986,895 字节，未解包全游戏；28 个 AnimationClip 中 27 个含可读取的 ACL 标量数据，另一个是 NPC curve_collection。对 27 个片段检查标量绑定数与 ACL 轨道数完全一致。按 `MagicaClothWeight` 的路径属性 CRC32=3423551091 匹配 Animator、customType=0 的绑定，解出 15 条控制曲线；其中 13 条恒定曲线还与原序列化常量表逐 float32 值交叉核对一致，另外两条随时间变化。Animator、customType=8 的连续小整数属性是肌肉轨道编号，不能当作命名的物理权重曲线。

| 原始 AnimationClip（均省略 `A_actor_zhuangfy_` 前缀） | 动画中的 `MagicaClothWeight` |
|---|---|
| `battle_attack_01/02/03` | 各自恒定 0.01 |
| `battle_loop`、`battle_power_attack` | 各自恒定 0.2 |
| `battle_normal_skill`、`battle_normal_skill_loop` | 各自恒定 0.2 |
| `relax_loop`、`idle_jump_start_r` | 各自恒定 0.2 |
| `uiteam_idle01`、`ui_overview_loop_01/start_01`、`gacha` | 各自恒定 0.01 |
| `run_jump_land_r` | 从 1 过渡到 0.35 |
| `idle_to_relax` | 从 1 过渡到 0.2 |
| `idle_loop_additive`、`walk/run/sprint_loop_additive` | 当前片段没有该命名绑定；不据此把整套播放图的权重认定为 0 |

已查片段没有匹配到 `MagicaClothWeightAdd`、`MagicaClothWeightForce` 或三项顶层 BeyondBoneCloth Property 的直接绑定。这只描述本轮片段，不外推全部游戏动画。

骨链身份从当前 physics/components.json 的 BeyondBoneCloth 实际代理 Transform 引用取得，不用 `jnt` 后缀猜测。对可确认位置绑定数与 ACL Transform 轨道数一致的片段，读取全部原采样帧。例如 MC_pantsR 的 `R_trousersDn_A_02_jnt` 相对首帧局部旋转变化最大值：`battle_attack_02` 为 63.684°（641 帧），`walk_loop_additive` 为 24.989°（65 帧），`run_loop_additive` 为 37.474°（41 帧），`sprint_loop_additive` 为 22.743°（33 帧），`idle_loop_additive` 为 2.746°（121 帧）。因此实际物理骨链上确实存在变化的动画姿态轨道。部分其它片段的位置绑定不覆盖全部 ACL Transform 轨道，工具明确报告映射未确认，不输出这些片段的骨骼幅度；它们的标量权重有独立绑定及常量表证据。

结论是动作之间可以设置不同的物理控制曲线，但不是每个动作都有独立且互不相同的权重；同一动作内也可以过渡。这里的数值是资源中的 Animator 控制输入，不能直接等同于最终 `TeamData.blendWeight` 或动画/物理输出百分比。骨骼动画幅度同样不是最终游戏模拟幅度。当前参数复制只复制配置和参数曲线，不复制这些动作资源或运行时状态，不能保证不同骨链仅靠相同静态参数得到完全相同的表现。

证据工具仍为 `tools/diagnostics/inspect_animation_bindings.cs`；本轮增加标量解码、绑定/常量交叉检查和全帧骨骼采样。原始 JSON 摘要位于忽略的 `bin/physics-animation-probe/action-evidence.jsonl`。未修改或部署 DLL/Blender/AnimeStudio 产品、用户工程、Mod 或原游戏资源。
