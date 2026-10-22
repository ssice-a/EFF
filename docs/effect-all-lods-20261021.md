# 特效覆盖全部 LOD

15:20 后续补充：all-LOD 的隔离运行副本也对属于本特效层级的 ParticleSystem 使用 AlwaysSimulate，解决屏外暂停造成UI粒子时间不推进的候选原因；作者原始值保持。已从安装资源只改两个粒子的cullingMode并发布，其他粒子字段/材质/贴图保持。画面尚待验收，详情见[后续记录](effect-recovery-20261021.md)。

2026-10-21 14:35 后续已从实际安装包构建并发布只改 EffectSetting 的全 LOD 副本，保留安装材质、贴图和粒子参数，解决下文暂存贴图不一致导致的未发布状态。部署与待验收边界见[特效跟进](effect-recovery-20261021.md)。下文 05:01 的未发布记录保留为历史。

本轮复用人物模型的 Replace/Part、owner、骨骼和显隐链路，不新增特效 DLL，也不在游戏内按 Mesh 名猜测 LOD 关系。

## 模型 LOD

`RuntimeControls::Prepare` 已经对 Replace 规则中的每个 target 生成包含精确源身份与作者资源摘要的原生 Mesh 标记。因此，同一个 Part 的 LOD0～3 可以直接通过现有 Match 识别。当前 lx 的衣服规则包含 0、1、2、3 四级目标；不需要额外给每个 LOD 导出一份特效。

实测回归发现，`ModelReady` 在后续登记更多 LOD Renderer 时，会因为 Renderer 列表变化把同骨骼的特效当成新配置，重新创建粒子实例。本轮分离了渲染器集合变化和特效内容变化：只改变集合时更新显隐/渲染层来源，保留已有模拟、资源代际与引用计数。全部目标隐藏仍停止；不同 owner 或实际骨架副本各有独立实例。

没有放宽作者 Mesh 的有序骨骼 palette 校验。模型的各级目标都使用对应作者 Part 重建，应该保持该 Part 的骨骼契约；简单忽略低 LOD 骨骼数或按骨骼名兜底会掩盖错误归属。本轮也未引入这样的旁路。

## 特效自身的 LOD

当前花瓣的原生 EffectSetting 启用了 `isUseDistanceLod`，最后一个 `_distanceLodInfos` 档位会关闭 internal 节点及花瓣发射器。这是独立于人物模型 LOD 的限制。

Blender 根对象新增默认开启的“覆盖全部 LOD”。作为离线构建选项 `lodCoverage=all/source` 传给 AnimeStudio，已有附件二进制格式与 package-protocol.json 不变。原始作者参数和来源快照保留；仅对导出运行副本中、属于当前特效层级的 EffectSetting 应用：

- 关闭距离与 guard 降级，启用禁用距离剔除的原生标记。
- 画质掩码包括 Low/Mid/High/Highest，保持平台与目标层掩码。
- 各距离档位保持 active、速度倍率 1 和更新比例 1。
- 原生命名空间 key 包含该策略，F9 不会尝试在旧 CAB 身份上覆盖不同内容。

离线工具未指定该选项的旧编辑文档仍保留来源策略。Blender 新旧工程通过 RNA 默认值开启，新导出会重建；关闭开关可恢复来源策略。

独立包在没有角色 Mod/Replace 映射时，仍只能使用导出的精确 Renderer 证明；本轮没有扩展独立包的来源证明协议来枚举未声明的原版 LOD。当前追加到 lx 的花瓣已有完整四级 Replace 映射，覆盖此使用场景。

## 验证与验收边界

- 先增加实际 `RuntimePartName` 生成的不同 LOD 标记，复现晚登记重启；修复后通过同实例保持、LOD1/2/3 单独可见、只有 LOD3 Renderer、全部隐藏、返回 LOD0、骨架副本、NPC/UI、F9 和最终 GC handle 清零回归。
- 实际花瓣来源分别以 source/all 构建并重新解析原生归档，检查 EffectSetting 改动范围、源文件保持和确定性，47 项断言通过。
- 28 个粒子、28 个 Renderer、60 个脚本的既有真实来源回归通过，共 444 项断言。
- 独立后台 Blender 验证根 RNA 默认值、开关切换、导出输入和作者原始参数保持。
- 当前 Blender 会话已加入开关，未保存 blend；新的配套 AnimeStudio 构建入口已备份更新。
- 普通 v35 DLL 构建、PE 静态运行库检查及真实 DLL 在游戏运行库下的隔离加载通过。

证据目录：`bin/effect-all-lods-20261021/`，运行时测试日志位于 `bin/effects-debug-tests/lod-before.log`、`lod-after.log`。构建/隔离测试不能证明实机远近 LOD、NPC/UI 的画面已验收；原地面碰撞和 TAA 残影没有由此得到修复结论。

## 当前本地安装

05:01 已在游戏未运行状态备份并安装普通 `plugin/eff.dll`，SHA256 `1b2b169da05b0427f62ffae124b5892d1c1c5c365cd0ac39af377124a14b0e87`；336 个 Mod、配置、协议和代理受检文件保持。备份与回退依据位于 `bin/effect-all-lods-20261021/runtime-deployment.json`，未启动或停止游戏。

配套 AnimeStudio 的 `dist/net9.0-windows/bin/AnimeStudio.GUI.dll` 已备份更新；其余核心库摘要与测试构建相同。当前 Blender 编译器路径指向这份配套入口，根开关及实际面板已就地验证，选择/帧/挂载保持，未保存工程。下次正常启动加载新 DLL；只选特效导出后 F9 应用原生 LOD 策略。

当前工程在隔离 lx 副本中完成统一特效导出，耗时约 3.50 秒。冷读、F9 磁盘快照和 37 份人物原生输入保持验证通过。暂存包重新解析后发现花瓣主图 `flower_D.png` 的 RGB 与现有游戏包不同，Alpha 相同；变化来源未在本轮定位。因此**没有发布这个暂存特效包到游戏**，角色与现有特效参数保持安装前状态。该观察不影响独立 source/all 原生构建测试，但不能将当前工程导出误称为“仅改变 LOD 的游戏资源更新”。
