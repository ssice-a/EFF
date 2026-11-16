# 作者 Empty 特效挂点

2026-11-13：Blender 作者操作、INI 装配、原生 Clip 构建、生产目录解析和 DLL 挂点生命周期已接通。真实 lino `.efffx` 已完成离线导出/导回；新挂点的游戏内视觉效果仍待实机验收。此前混合 AnimationClip 和 ZFY 形态键的实机结果不能替代本功能验收。

## 作者操作

1. 新建 Empty，作为武器/角色的作者挂点；可以继续创建 Empty 子级。最上级 Empty 可无父级，也可直接以目标 Mesh 为父级。其局部变换相对运行时命中的 Renderer，或特效设置中指定的基础骨骼。
2. 在特效根的“用途与挂载”选择“挂载目标 Mesh”和“挂载 Empty”。骨骼为空时以 Renderer 为基础；选择骨骼后，作者 Empty 层级建立在该骨骼下。特效根自身 TRS 是相对 Empty 的额外偏移。
3. 没有动画的 Empty 保持作者默认姿态。有动画时，给 Empty 的位置、欧拉/四元数旋转、缩放插关键帧，选中 Empty，在“统一动画组”创建或加入播放组。Action、插槽、范围、采样率、循环、默认播放、速度与快捷键仍属于同一个播放组。
4. 导出 Mesh 时自动包含它启用的附属特效；只导出特效时也会包含挂点和必要父级。不需要额外选中 Empty。多个特效选择同一个 Empty 时，共享同一个运行时挂点。
5. 导回 Format 3 时恢复可编辑 Empty、父子关系和采样关键帧。INI 的节点装配和播放设置优先于快照中的旧声明。作者快照保存必要的编辑数据，不要求用户手写 INI。

当前拒绝约束、未烘焙驱动器/NLA、非 Empty 的中间父级、骨骼父级方式、非正缩放和损坏路径；骨骼基础应在特效挂载栏明确选择。动画组还有其他 Mesh/骨架成员时，完整模型导出须包含这些成员，不能在独立特效包里悄悄丢掉它们。

## 装配与执行

```ini
[HierarchyOrbit]
node.0.path=Pivot
node.0.label=刀光中心
node.0.position=0,0,0
node.0.rotation=0,0,0,1
node.0.scale=1,1,1
node.1.path=Pivot/Tip
node.1.label=粒子挂点
node.1.position=0,1,0

[ResourceOrbitClip]
type=animation-clip
path=animations/刀光旋转.ab

[AnimationSpin]
label=刀光旋转
duration=2
speed_variable=$spin_speed
speed=1
loop=1
autoplay=1
native.0.hierarchy=Orbit
native.0.resource=ResourceOrbitClip
native.0.asset=assets/eff/animations/orbit.anim

; 现有 EffectAttachment 段另外声明：
; hierarchy=Orbit
; node=Pivot/Tip
```

示例只展示新增装配；完整 Constants、EffectAttachment 的 Mesh 来源证明、原生特效 Bundle 等由原导出器生成。层级内部使用稳定节点身份，标签供作者阅读；不扫描游戏场景按名称猜挂点。原游戏 Prefab 替换模式由游戏调用者管理挂点，不允许声明作者 Mesh 挂点。

层级由 `(真实 owner, 安装实例, 基础 Transform, 层级/动画资源代际)` 持有，世界/UI 等实例各自装配。Animator 只添加到新建的作者根，不修改游戏原 Animator、骨架或动画。多个粒子实例共享层级；旧粒子退休期间继续持有层级，最后一个使用者释放后才销毁作者根并释放 Clip。

勾选“覆盖全部 LOD”时，来源清单中的精确 LOD 消费者共用挂点。无骨骼挂载在 LOD 分支外的公共祖先建立稳定基准链，保留原 Renderer 的局部 TRS；Empty 与原生 Clip 仍使用原来的层级/采样路径，不挂在可能被游戏关闭的 LOD0 分支下。骨骼挂点继续使用经过调色板验证的骨架。具体协议、测试和实机边界见 [特效依赖与 LOD 挂点](effect-game-dependencies-20261114.md)。

EFF 的现有播放组负责时钟、Ins、快捷键、速度、暂停/停止和冲突。同一个组可同时有已有形态键/材质/骨骼输出及新挂点输出；新挂点的 TRS 使用原生 `AnimationClip.SampleAnimation` 对同一播放时间求值，不建立第二个时钟。暂停保留姿态，停止恢复作者基准 TRS；争用同一作者层级的组互斥。

纯挂点动画只声明 `.ab` 原生 Clip，不伪造空 `.effanim`。混合组保留需要的数值样本文件。Clip 构建模板只含 TypeTree 元数据，输出只有 AnimationClip 和 AssetBundle 两个对象、零外部资源引用；不重打包游戏 Mesh/Texture。数值轨道仍存在于原有生产链路，整套动画迁移到原生 Clip 的更大工作不由此宣称完成。

资源按命中/播放延后打开、冻结并共享；F10 按节点装配及 Clip 文件代际重建相关层级。无效输入记录错误，不逐帧重试；F10 可重试。旧二进制控制序列化显式拒绝新装配，避免静默丢数据。

## 验证

- `tools/diagnostics/build_author_transform_smoke.cmd`：真实 INI 解析、路径/TRS/引用拒绝、纯原生/混合组统一时钟、循环/暂停/冲突、二进制拒绝。
- `tools/diagnostics/build_author_transform_runtime_smoke.cmd`：实际生产模块加 mock Unity API，验证共享树/Animator、原生采样时间、停止复位、代际变化及最后特效使用者退休。
- `tools/Blender/test_eiem_effect_mounts_blender.py`：Blender 5.2.3 后台实测 Empty 父级、采样、共用组、配套原生构建器和可编辑往返。
- `tools/Blender/test_eiem_effect_mount_package_blender.py`：真实 `p_fxbat_liino_relax_react_03.efffx` → 独立特效 Mod → Blender 作者导入；产物在 `diagnostics/author-transform-schema/real-effect-mod`，使用合成 Mesh 来源，未安装到游戏 Mods。
- `bin/author-transforms/smoke.exe <上述实际包目录>`：生产 Catalog/作者装配读取真实导出包，按需打开不可变原生动画输入。本测试修复了旧目录加载器对原生动画空 `sampleSource` 的错误假设。
- AnimeStudio `--native-animation-clip` 测试同时独立反序列化正式 `--build-eff-animation` 的产物，验证单成员、两个原生对象、零源资源依赖和确定性输出。

待实机验收：真实武器静态挂点、自动循环和 Ins/按键暂停停止，两个特效共享挂点，世界/UI 双实例，连续 F10、换武器/换场景后的退休与恢复。mock、离线包、构建成功不代表这些场景已通过。
