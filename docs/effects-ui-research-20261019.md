# 特效编辑器：模块、界面与性能

## 2026-10-20：模块入口与结构导航修复

真实 Blender 复现：将 `UVModule.enabled` 从 true 取消为 false，参数仍在，但 `Draw.particle` 仅绘制已启用模块，导致“贴图序列帧”标题和开关一起消失。修复为绘制来源已有的全部模块标题；关闭仅将参数区置灰，标题显示“未启用”，可在原位重新勾选。保留“启用模块…”选择器，枚举读取当前控件状态，启用不重置数据。未新增游戏模块或组件拓扑。

Empty 数据页新增当前编辑对象类型、返回根按钮、真实来源节点/材质导航和花瓣参数位置速查。节点按来源 Prefab 层级显示；材质 Empty 是资源编辑容器，实际由渲染器引用。根可查看全部发射器，子发射器单独编辑，材质页可直接返回发射器。

`test_eiem_effect_module_navigation_blender.py` 先在修复前稳定失败于“关闭序列帧后标题消失”，修复后通过三份真实来源的所有模块开关、保留参数、启用选择器、根/发射器/材质导航、原生数据恢复无差异以及保存重开检查。既有 `test_eiem_effect_objects_blender.py` 也通过。检查使用独立后台 Blender 5.0.1，未接管或保存用户工程；本轮没有游戏运行时变更或实机验收。下文性能数据为 2026-10-19 版本的历史测量，不代表增加模块标题后的界面耗时。

## 2026-10-19：研究与实现记录

本轮依据用户对原始字段堆砌和拖参卡顿的反馈，重做作者视图。没有把 MHW 的二进制结构套用到终末地，也没有改游戏 DLL。

## 研究依据

1. 本地 `G:/插件和工具/MHW-EFX-Template-master/MHW-EFX-Template-master/EFX_Subtypes.bt`：`Spawn` 组织发射数量、间隔和延迟，`Life` 组织寿命与淡入淡出，`EmitterShape3D` 组织出生形状，`Velocity3D` / `Transform3D` 组织运动和变换。借鉴的是按作用分块、字段有名字与类型的编辑方式；它属于 MHW 的格式。
2. Unity 官方 UnityCsReference：`Modules/ParticleSystemEditor/ParticleSystemModules/InitialModuleUI.cs`、`EmissionModuleUI.cs`、`ShapeModuleUI.cs`、Velocity / Force / Noise / Size / Rotation / Color / ClampVelocity / InheritVelocity / LifetimeByEmitterSpeed / ExternalForces / UV / Collision / Trigger / Sub / Lights / Trail / CustomData 模块编辑器，以及 `Modules/ParticleSystem/Managed/ParticleSystemEnums.cs`。本次联网读取官方原始源文件；本地快照在 `bin/effect-ui-research/`。
3. 三份真实 `.efffx` 的 ParticleSystem（198）/ ParticleSystemRenderer（199）TypeTree、EffectSetting 和 VFXAutoEmissiveRate。更广泛的 28 个粒子抽样见 `effects-authoring-research-20261019.md`。

官方源码位置：https://github.com/Unity-Technologies/UnityCsReference/tree/master/Modules/ParticleSystemEditor/ParticleSystemModules 。枚举：https://github.com/Unity-Technologies/UnityCsReference/blob/master/Modules/ParticleSystem/Managed/ParticleSystemEnums.cs 。这些是编辑语义依据，不代表游戏一定采用最新 Unity 的所有功能。实际写回仍由来源 TypeTree 限定。

## 原游戏的分层

- GameObject / Transform：一个特效的节点层级和挂点偏移；不是一个粒子的生命属性。
- ParticleSystem：每个发射器的基础设置和 23 个固定模块槽。UI 可以启停模块，但同一模块不能任意叠加两次或改变执行顺序。
- ParticleSystemRenderer：选择 Billboard / Mesh、来源 Mesh、材质、排序、顶点流。渲染 Mesh 与 ShapeModule 中用于采样发射位置的 Mesh 是两个不同引用。
- Material / Texture / Shader：来源材质保存参数和贴图绑定；Shader 如何消费自定义数据需要与 Renderer 顶点流一起核对。
- EffectSetting：游戏脚本对整个效果的循环、延迟、时长、淡出、裁剪和 LOD 控制。与粒子自身 Start Lifetime 分开显示，离线不冒充执行脚本。
- VFXTrailRenderer 等：游戏自定义拖尾不是粒子 TrailModule，保持独立组件身份；未适配组件保留来源数据。

## 23 个粒子模块的划分

| 编辑块 | 原生路径 / 主要参数 | 影响与当前预览 |
| --- | --- | --- |
| 发射器基础 | lengthInSec、looping、prewarm、startDelay、simulationSpeed、moveWithTransform、scalingMode；InitialModule | 系统周期、出生寿命/速度/颜色/大小/旋转、重力和存活上限；基础预览支持 |
| 发射数量 | EmissionModule.rateOverTime / rateOverDistance / m_Bursts | 每秒连续发射、随移动发射、定时爆发；按距离暂未还原 |
| 发射形状与范围 | ShapeModule.type / radius / arc / angle / length / m_Position / m_Rotation / m_Scale | 出生位置和初始方向；简单形状与线框支持，Mesh/Sprite 采样未还原 |
| 速度随生命周期 | VelocityModule.x/y/z / orbitalX/Y/Z / radial / speedModifier | 附加直线/轨道/径向运动；当前只预览直线部分 |
| 继承发射器速度 | InheritVelocityModule.m_Mode / m_Curve | 使用出生时或当前挂点速度；未还原 |
| 寿命随发射器速度 | LifetimeByEmitterSpeedModule.m_Curve / m_Range | 根据挂点速度缩放寿命；未还原 |
| 力随生命周期 | ForceModule.x/y/z / inWorldSpace | 持续加速度；未还原 |
| 外部力场 | ExternalForcesModule.multiplierCurve / influence* | 场景力场过滤与影响；未还原 |
| 速度限制与阻力 | ClampVelocityModule.magnitude / x/y/z / dampen / drag | 超速阻尼与阻力；未还原 |
| 噪声扰动 | NoiseModule.strength / frequency / octaves / positionAmount 等 | 位置、旋转、大小扰动；当前仅近似位置摆动 |
| 颜色与淡入淡出 | ColorModule.gradient | 粒子寿命 0→1 的颜色和 Alpha；预览支持；不是合成独立 fade 字段 |
| 大小随生命周期 | SizeModule.curve / y / z / separateAxes | 乘出生大小；预览支持 |
| 旋转随生命周期 | RotationModule.x / y / curve / separateAxes | 角速度积分；预览支持，UI 标明原生 rad/s |
| 颜色随速度 | ColorBySpeedModule.gradient / range | 速度映射颜色；未还原 |
| 大小随速度 | SizeBySpeedModule.curve / y / z / range | 速度映射大小；未还原 |
| 旋转随速度 | RotationBySpeedModule.x / y / curve / range | 速度映射角速度；未还原 |
| 贴图序列帧 | UVModule.tilesX/Y / frameOverTime / startFrame / cycles / rowMode | 网格图集预览；Sprite 和按速度推进未还原 |
| 粒子碰撞 | CollisionModule.type / collisionMode / m_Dampen / m_Bounce / m_EnergyLossOnCollision | 平面/世界碰撞和寿命损失；未还原 |
| 粒子触发器 | TriggerModule.inside / outside / enter / exit / primitives | 粒子触发体响应；未还原；与 Mod 的事件触发挂载不同 |
| 子发射器 | SubModule.subEmitters | 出生/碰撞/死亡/触发/手动事件、引用与继承；未还原 |
| 粒子灯光 | LightsModule.light / ratio / rangeCurve / intensityCurve | 复用灯光来源；未还原 |
| 粒子拖尾 | TrailModule.lifetime / widthOverTrail / colorOverTrail | 带状几何、宽度、颜色和寿命；未还原 |
| 着色器自定义数据 | CustomDataModule.mode0/1 / vector* / color0/1 | 两组向量/颜色数据；需匹配 Shader/顶点流；未还原 |

原生模拟空间枚举已核对：Local=0、World=1、Custom=2；同时修正旧预览中 Local/World 颠倒。自定义空间目前明确标记为未还原。旋转数值保留原生弧度，不把角度显示直接写进弧度字段。

## 界面实现

选特效 Empty → 数据属性（空物体坐标轴图标）→ EFF 特效 · 发射器。基础、发射、形状默认展开；已启用的运动/外观/交互模块按功能出现，点击标题展开全部相关控件。渲染单独成块；技术字段进入高级区。根物体可查看所有发射器，选子 Empty 可单独编辑。

- 模式是真正的中文下拉框，不是整数输入或输入框后面的类型标签。常量一行、随机范围显示最小/最大，曲线模式仅展开当前使用的关键帧；切线/权重进入高级折叠。
- 颜色使用 Blender 颜色控件；透明度独立编辑。渐变位置用 0..1，严格映射回原生 UInt16 的 0..65535，颜色点与 Alpha 点的时间分开保存。
- 定时爆发显示时间、数量、重复次数、间隔和概率；按钮叫“添加定时爆发”“添加关键帧”，不再以“编辑列表”作为编辑入口。
- float 仍是浮点、int32 仍是整数、bool 仍是开关；精确 UInt32/int64 保持十进制文本，原生路径与类型在 tooltip。未知字段保留，未命名字段显示 unknown。
- 打开页面不删除关闭模块、曲线的另一侧、未使用颜色点或脚本数据。旧工程在内存中升级控件，先同步尚未保存的原生字段，不覆盖原来源文件。

## 性能反馈环与结果

`tools/Blender/benchmark_eiem_effect_editor.py` 在独立 Blender 5.0.1 factory 进程导入真实三份来源、帧 151、每项 7 次，cProfile 同时开启；统计是 Python 回调耗时，不是 GPU 帧率或拖动端到端延迟。根 UI 默认展开状态由 recorder 模拟，另用独立带窗口进程实际截图检查。

先记录 baseline，再验证三个可证伪假设：

1. 若发射曲线重复积分主导卡顿，精确分段积分后同样来源的 preview 回调应显著下降。profile 显示积分/采样占主要耗时；已验证。
2. 若 UI 递归绘制无用结构导致打开慢，按语义只绘制使用模式后字段数和 draw 时间应同步下降。已验证。
3. 若全量几何/其他发射器重建贡献额外开销，局部失效与拓扑复用应减少非目标更新。由真实对象回归核对；仍保留整根强制刷新供手工复算。

| 来源 | 根 UI 绘制，中位 ms | 预览回调，中位 ms | 拖参+timer，中位 ms | 默认控件数 |
| --- | ---: | ---: | ---: | ---: |
| 梨花瓣 | 94.738 → 1.943 | 114.074 → 5.463 | 129.204 → 23.954 | 2152 → 83 |
| 粉花瓣 | 116.354 → 4.201 | 655.090 → 20.730 | 672.065 → 32.595 | 2771 → 167 |
| 小光点 | 76.846 → 1.394 | 16.984 → 14.935 | 32.717 → 32.260 | 1728 → 56 |

上述记录来自 `baseline.json` 与 `after-verified.json`；运行前后导入分配的 effect_id/随机种子不同，粒子数会有轻微差异（粉花瓣同为 93），不能据此声称逐粒子视觉完全相同。重构后另外用固定输入单测验证任意时间跳转、出生 ordinal、缓存失效与曲线数值。根界面默认字段数减少包含将已启用可选模块折叠的效果，展开所有模块时仍有更多控件。

优化内容：Hermite 曲线分段解析积分替代每次 32 点 Simpson；按发射曲线、周期、循环和 ordinal 缓存出生时间；常量分支不采样无用曲线；字段路径缓存；只规范化修改对象；按发射器局部失效；发射范围缓存；相同拓扑批量更新坐标/UV/颜色。拖参轮询从 250ms 改成 100ms。

未宣称所有负载都变快：三根空闲轮询的 profile 中位从 7.543ms 增至 11.022ms（新增颜色/渐变控件也需检测），导入增加约 8–19ms；小光点拖参基本相同。动画世界空间需要采样出生时挂点，复杂骨架/约束和数万粒子仍可能慢，未取得通用实时保证。

## 检查与交付

- 45 项 Python 测试，44 通过，1 项既有可选 C++ fixture 跳过。新测分段积分与数值积分对照、带曲线发射任意跳转与缓存失效。
- 真实 Blender：三份来源导入不改原生值、全部 23 模块 draw、命名枚举双向修改、颜色控件/原生字段双向同步、爆发增长、世界空间出生挂点、保存重开、三轮注册卸载、材质动画回归。
- 保存重开核查额外发现并修复：根移动/时间线后，`matrix_local` 的父级依赖图尚未刷新可能被误判为对子节点的作者编辑；改用 `matrix_parent_inverse @ matrix_basis` 判断和写回本地变换，新增“移动根后原生对象仍无变化”的回归。旧示例已保存的细小变换差异保持，不擅自修成原值；新示例重新从来源生成。
- 新干净工程：`E:/EIEM_Workspace/effects/petals-lightpoints-20261019/rework-review/module-ui/effects-module-editor.blend`。独立 factory 场景生成，三份来源保留原值。
- UI 实际截图：同目录 `effect-module-ui.png`。
- 新示例已独立保存重开，三份特效 `export().objects` 均为空。可选第二视图截图尝试有一次 NVIDIA `nvoglv64.dll` 驱动访问异常；不将该次失败视为通过，最终干净工程使用后台 Blender 重新生成并重开校验，前面已成功取得的主界面截图保留。没有据此宣称用户当前复杂场景的 GPU 预览性能已经验收。
- Blender 安装目录是源码目录 Junction，磁盘已更新；当前连接的角色工程没有特效根，不在该工程导入测试特效或自动重存。
- 8/8 真实粒子/Renderer 原生字节严格解码再编码保持一致；在真实 Blender 修改随机寿命、RGBA 和归一化 Alpha 渐变时间后，配套 AnimeStudio 原生构建成功（`bin/effect-ui-research/native-roundtrip/`）。
- 已在当前连接的 Blender 中只重载特效编辑器相关模块；当前文件路径、对象总数和时间帧保持，不保存用户工程。其他 Blender 进程仍需重新加载插件。
- 最后补充根变换误差修正后的会话同步返回 `Not connected to Blender`；因此最终源码和安装目录已更新，但不能声称最终全部补丁已进入用户仍打开的会话。重新加载插件或重启 Blender 即使用磁盘最终版本。
