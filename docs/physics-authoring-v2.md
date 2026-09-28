# Physics 源数据作者格式 v2（离线）

状态：工作区实验格式，Blender 插件源码 0.30.2。新增组使用作者 v5，原生源图仍使用独立的 v2 格式，
用于原始 Prefab 物理数据的保留和编辑；整份原生 v2 图不是可直接加载的游戏 Physics Mod。

## 输入与编辑

AnimeStudio 的正常 **Export Prefab as EFF mod package** 现在会在所选 Prefab 含有受支持组件时，
把完整源图写入包内 `physics/components.json` 及同目录的原始字节、TypeTree 和解码字段文件。
Blender 勾选“导入物理骨骼与碰撞体”即可在同一次包导入中载入 Mesh、共享 Rig 和原生物理。
导入器只把组件 owner、显式 Transform 引用、`rootBones - ignoreFromRootBones` 展开的物理节点及其祖先
合并进共享 Rig；Prefab 中无关的 IK、VFX 和渲染容器 Transform 不进入骨架。

- 输入为解包器的 `components.json` 及相邻 `.bin`、`.schema.json`、`.data.json`，
  或此前导出的 `.physics` 及其 `.skeleton` 依赖。
- 以 CAB + PathID 保留源组件和 Transform 身份；包含完整解码字段、TypeTree、引用图、原始字节及 SHA256。
  复制到作者 v5 时携带可编辑的数值、布尔/枚举与曲线；引用、预构建字节和 64 位 PathID 继续只由原生 v2 保留。
  带完整模板的作者组与原生组使用同一套常用参数、角度限制、九条 Blender F-Curve 与完整字段表，
  支持短名、筛选和分页；Empty 的自定义属性中显示九个中文曲线属性。隐藏快照仅用于旧 `.blend` 迁移与兼容保存。
- Blender 用一个共享 Text 数据块保存源快照，多个模拟组共享同一碰撞体对象。
  重用现有 Rig 时校验源节点路径、父级和 TRS；不匹配则报错，不猜测同名骨骼。
- 源快照在第一次读取后缓存；面板不再在每次重绘时反复解析 556 Transform 的 JSON。
  面板先显示五个常用参数；九类常用曲线统一在 Graph Editor 中编辑，避免再维护一份逐关键帧数值界面；
  完整源字段按 32 项分页并可筛选。普通求解参数修改不重建几何；碰撞体 `center/size/direction` 等外形字段
  刷新碰撞几何；节点半径、角度限制开关、基础角及对应曲线只刷新该组的链预览。
- 参数存在物理组或碰撞体 Empty 的 RNA 属性组中，是导出的唯一源数据；选中 Empty 后从对象属性中的
  **EFF 物理参数**编辑。九类常用曲线另投影到同一 Empty 的 ID 自定义属性和 Action/F-Curve，供 Blender
  Graph Editor 直观编辑；导出时自动写回 RNA 源字段，不建立第二份独立物理配置。
  JSON 中看起来是整数的源 float，依据 TypeTree 保持浮点编辑。
  未编辑字段保留源十进制值，不因为 Blender RNA 的 float32 显示精度重写源数据。
- 侧栏使用两个直接按钮：**复制参数**和**粘贴参数**。原生组之间复制全部匹配的可编辑数值/曲线字段；
  身份、根骨、选择点和链结构不进入参数剪贴板。复制时会先把 Blender Graph Editor 中的九类
  曲线写回源字段。粘贴到新增作者组时，常用参数、角度限制及全部九条曲线进入与原生组相同的编辑器，
  包括关键帧、切线、加权模式、权重、Infinity/RotationOrder 元数据和 useCurve 开关；其余原生字段继续
  保存在目标 Empty 的高级字段中。操作同时按源身份复制该组使用的碰撞集合；目标 Empty 可从同一 Rig
  手动加入或移除作者/游戏源碰撞体。原生组之间粘贴仍只改参数，不改原生源图的引用关系。
  作者 v5 导出把字段写入 `nativeParameters`，并在导出前用 Empty 上当前的
  五项常用值和节点半径 F-Curve 覆盖快照中的对应字段。DLL 按元数据字段名写入普通标量、布尔/枚举、
  `gravityDirection` 和九类曲线，并逐项回读后才发布配置草稿。该链路已完成离线/宿主验证，尚未完成游戏内逐参数响应验证。
- 可以记录显式禁用源组件；这是作者操作标记，DLL 尚未执行它。
  v2 尚未提供源拓扑重建、完整引用图编辑、原生模拟或游戏预构建缓存再生成。

“导入物理骨骼与碰撞体”是 Mesh 包导入的可选项，默认关闭；也可以单独“导入源物理”。
每次 Mesh package 导入建立独立的 package 集合，其下包含 Meshes、Skeletons 和可选 Physics；不同 package
不再共用全局 LOD 集合。Physics 下固定分为 Groups、Colliders、Visuals，便于单独隐藏或选择。
旧 `.blend` 可用“整理当前 Rig 的物理集合”迁移已有平铺辅助对象；迁移不改源图或导出数据。
源物理导入自身失败会回滚本次新建辅助对象；Mesh 包的可选物理阶段失败时，先前导入的 Mesh 可能已存在，
目前没有把整个 Mesh 包导入扩展成一个事务。

## 显示的含义

源选择点位置和属性保持原数组对应关系；层级连线显示源静止姿态中的真实父子节点方向。Blender bone tail
保留游戏 Transform 的局部 `+Y` 轴，不保证指向子节点；它不能替代层级连线，也不能据此判断物理拓扑。
不将源选择点序号当成 Mesh palette、setupIndex 或 Animator 写回槽，也没有 Blender 物理求解。

原生碰撞辅助体按源组件类型显示球、胶囊和无限平面。球使用 `center` 与 `size.x`；胶囊使用
`direction`、`reverseDirection`、`alignedOnCenter`、`radiusSeparation`、`size.x/y/z`；平面显示局部 `+Y`
法线和有限网格标记，但语义仍标为无限平面。这些解释来自 MagicaCloth 的公开组件/API 合同，尚未等同于
终末地运行时已验证的碰撞响应。[胶囊 API](https://magicasoft.jp/en/mc2_api_magicacapsulecollider/)、
[胶囊组件](https://magicasoft.jp/en/mc2_capsulecollidercomponent/)、[碰撞设置](https://magicasoft.jp/en/mc2_collision_setup/)。
v1 的胶囊是作者层自行定义的几何，不能作为原生 `SetSize` 转换依据。

视图默认只显示“当前物理组 + 它引用的共享碰撞体”，并默认关闭穿透 Mesh 显示；还可切换为全部物理组、
全部碰撞体、全部或隐藏。默认显示为实体节点、实体连接体和半透明碰撞体，也可切换为诊断线框。
物理组 Empty 表示一个 BeyondBoneCloth 配置，它可以包含多个根和分支，不能一律解释成一条线性骨链；
碰撞体是独立组件，可被多个物理组共享，因此不放进某一个组的专属集合。
组面板分别给出根节点数、物理 Transform 数、Selection 点数，逐条列出并可选择每个根链，同时按源引用列出
本组使用的碰撞体。角度锥预览用组局部静止位置在 Selection 点与根展开 Transform 之间做一对一几何匹配；
Typhoea 样本全部点可匹配。该匹配只用于视图，不把 Selection 数组序号声明成 Mesh palette、setupIndex、
Animator 槽或通用骨骼身份，也不据此改写源拓扑。

九个源字段组提供 Blender F-Curve：阻尼、节点半径、距离约束强度、角度恢复强度、角度限制、最大运动距离、
回挡距离、碰撞限制距离和自碰撞表面厚度。横轴 0～100 映射源链累计长度 0～1，纵轴为倍率，曲线静音映射
`useCurve=false`。曲线 Action 归属于物理组 Empty；若同一原生组有多个根链，它们共享这一组曲线与碰撞体引用。
需要不同曲线或碰撞集合时，应建立不同物理组。当前版本允许移动关键帧和 Bezier 手柄，但保持源关键帧数量；
新增或删除点会在写回时拒绝。切线、`weightedMode` 和权重随手柄写回；它没有把 Selection 点顺序猜成骨骼顺序。

原生组的 FIXED 与 MOVE Selection 点按实际节点碰撞半径显示。每点半径为
`serializeData.radius.value × radius.curve(depth)`；关闭 `radius.useCurve` 时直接使用基础值。实体模式绘制真实尺寸的
低面数球，线框模式绘制三个正交圆环。IGNORE 点不参与模拟点半径，只显示小型灰色拓扑标记。基础半径和
Graph Editor 中的半径曲线都会触发当前组预览刷新。这里可视化的是 BoneCloth 节点自身的半径字段；单独的球、
胶囊、平面 Collider 仍按各自组件显示和引用。0.30.1 可将源球体/胶囊转换成作者 v5 的骨骼、局部姿态、两端半径、
球心距离及中心对齐记录；无限平面没有进入作者格式。

新增作者组也使用同一半径含义：组 Empty 保存基础半径，Blender F-Curve 保存根部到末端的倍率；允许 2～64 个
关键帧并保留切线、加权模式和权重。FIXED/MOVE 点以 `基础半径 × 曲线(depth)` 显示，Graph Editor 改动后自动刷新。
这使从原生组复制的节点半径不再退化成固定装饰点。

原生 `useAngleLimit=true` 时，每条 MOVE 父子边显示一个黄色半透明角度锥。锥轴取静止 baseline 的父点→子点，
半角为 `limitAngle.value × curve(depth)`。游戏的 depth 生成已由本机方法体确认：对 MOVE 点沿父链累计静止局部
位置距离，走到首个非 MOVE 父点后停止，再除以组内最大累计长度并钳制到 0～1。因此多根和分叉共用同一个
归一化尺度，而不是按层数取值。对象属性把 `useAngleLimit`、基础角和 `stiffness` 放在同一常用区；
`limitAngle.useCurve` 仍是独立开关。锥体随 Empty 字段和 Graph Editor 曲线改动自动刷新。它表达静止作者基准，
不是 Blender 仿真；运行时动画基准会继续受到 `animationPoseRatio` 影响。

Typhoea 实际样本包含 27 个原生碰撞体：25 个胶囊、1 个球、1 个平面。19 个位于 Pelvis、Spine、
Spine1/2、胸部、Neck、Head、左右锁骨/上臂/前臂和大腿等身体层级；8 个位于头发、腰包、装置、裙摆和尾部
附件层级。11 个物理组共引用其中 25 个，Neck 与 Spine1 两个胶囊未被当前组引用；同一碰撞体最多被 4 组共享。
面板直接显示绑定骨骼、引用次数和引用组，未引用的原生碰撞体也保留，便于检查躯干配置和复制参数。

## 导出边界

独立导出选中组件及其共享碰撞依赖，输出 `.physics` 和内容哈希命名的 `.skeleton`。
未选中且未被引用的组件不输出；依赖辅助对象缺失时报错，不静默丢弃。
支持 `.blend` 保存/重开和可携带作者包重导入。新增作者组与 v2 源组暂不能混在一次物理导出中。

原生 v2 组仍不进入 Mesh Mod；显式传入 v2 组时在改动目标目录前报错。
组合 Mod 接受新增作者组及其作者/源球体和胶囊。旧 v1 创建/运行路径已经完成 `Render.physics` 实机验证；
作者 v5 在同一路径上增加完整参数模板与碰撞体记录。选中作者组和同 Rig Mesh 时，导出器生成
`[Physics…]`、共享 `[Skeleton…]` 及每个同 Rig Render 的 `physics=`；未选作者组时保持 Mesh-only。
DLL v72 的碰撞组件创建、共享组引用及退休代码已通过宿主测试和本地构建，但首轮作者 v5 实机在配置阶段失败；
v73 已部署，尚未由新进程验证游戏内碰撞响应。
引用无限平面的作者组会在写目标前拒绝。
源物理辅助体的眼睛、相机和选择状态不会自动生成 Mesh/skip 动作。

## 编码与校验

头为 `EFFPHYS\0` + little-endian u32 `2`，后接带类型标签的树：
null/false/true/int64/float64/string/array/object 分别为 0～7。
字符串使用 u32 UTF-8 字节长度；集合使用 u32 数量，object 键为字符串。
限制包括 16 MiB 文件、64 层嵌套、262144 单集合元素及 524288 树节点。
资源带 `native-authoring` 用途、坐标系、后端、作者身份、Skeleton 相对路径、源组件和 Transform 图。
这是一份作者交换格式，不是 Unity 序列化文件或可直接提交原生注册的内存结构。

Python 输入核对原始字节长度/哈希、引用闭包、层级及数值。
C++ 读取器检查树编码、资源用途、身份、引用、层级、TRS 和 Skeleton 节点依赖；
**C++ 尚未解码 `raw` 字段的 Base64 并重算原始哈希，也不把原始字节反序列化为 Unity 对象。**
不得据“C++ 已能读取”声称原生创建或原配置深复制已完成。

实现：[源图编解码](https://github.com/ssice-a/EIEM-blender/blob/main/eiem_physics_source.py)、[Blender 源作者工具](https://github.com/ssice-a/EIEM-blender/blob/main/eiem_physics_native.py)、
[C++ 树读取](../src/eiem_physics_native_document.h)。
真实 Typhoea 样本覆盖 11 组、27 碰撞体、556 个 Transform 的离线往返；
测试也覆盖曲线编辑与加权手柄、角度总开关、累计深度采样和锥体刷新、共享引用、物理 Transform 根/忽略闭包、
三种碰撞体几何、完整参数模板复制、
碰撞集合复制/加入/移除、旧作者快照迁移、复制新增骨链、保存/重开、可携带导出、Mesh-only、
v1 无碰撞体组合输出，以及 v2/碰撞体组合拒绝。
这些结果不涵盖原生绑定、仿真、取消、卸载或游戏画面。
