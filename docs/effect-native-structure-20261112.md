# 原生特效结构面板与 lino 模板

## 本轮范围

按实际 `.efffx` 来源结构整理 Blender 编辑面板，并解包 lino 的待机相关特效作为剑特效的作者素材。没有更改用户正在编辑的场景、剑 Mod 或原游戏资源，没有提交、推送或发布 Release。

本轮沿用原生特效构建和运行时挂载/逻辑 Prefab 替换链路。游戏资源引用走现有 `gameReferences` 管线：AnimeStudio 按逻辑路径解析索引并补原生外部依赖，运行时由游戏资源管理器提供按需租约；Blender UI 本身不复制游戏资源载荷。

## 编辑结构

面板依据来源 Prefab 的 GameObject 层级、组件表和 TypeTree：

```text
原 GameObject 节点
  Transform
  ParticleSystem
    系统字段
    InitialModule
    …来源中实际存在的 Module，保持来源顺序…
  ParticleSystemRenderer
    m_RenderMode
    m_Materials[index] → Material
      m_SavedProperties
        m_TexEnvs[index]
          first（Shader 属性名）
          second.m_Texture → Texture2D
    m_Mesh → Mesh
  游戏原 MonoBehaviour / 其他原组件
```

- 节点、组件和模块在当前位置展开，默认折叠。折叠不改变模块启用状态，不创建或删除原生字段。
- 保留中文说明及原组件、Module、字段名；不再用“运动/外观/交互”等人工分类决定模块位置。
- Transform 保留原组件位置。现有曲线、渐变、角度转换和模块开关仍使用同一份作者值。
- 原生 PPtr 在其真实字段位置显示目标类型、资产名及可查到的逻辑身份；Material 的贴图从它自己的 `m_TexEnvs` 展开。
- 未支持编辑的原生组件仍显示真实字段和引用，并注明只读；没有类型树的内容明确说明不能编辑，不能静默隐藏。
- Billboard 是 `ParticleSystemRenderer.m_RenderMode` 的值，不是一份必然有独立路径的贴图或 Prefab。渲染模式与材质/网格引用是不同字段。
- 逻辑路径来自索引和来源对象；内嵌 Material 可能使用 Prefab 路径加资产名，不伪造一个独立 `.mat` 路径。
- 折叠内容不递归绘制，不在 draw 中重新从磁盘读取 `.efffx` 或生成整份资源图。

## 解包结果

使用已安装 AnimeStudio 的正式 `--export-eff-effects` 入口、当前游戏 VFS 与索引导出，目录为：

`E:/EIEM_Workspace/assets/beyond/dynamicassets/gameplay/effects/prefabs/`

| 来源目录 | 原节点数 | ParticleSystem 数 |
| --- | ---: | ---: |
| `p_fxui_liino_relax_sp_01.efffx` | 28 | 14 |
| `p_fxbat_liino_relax_react_01.efffx` | 9 | 6 |
| `p_fxbat_liino_relax_react_02.efffx` | 6 | 3 |
| `p_fxbat_liino_relax_react_03.efffx` | 10 | 6 |
| 合计 | 53 | 29 |

角色在游戏资源中拼写为 `liino`。这些是索引中 `relax` 命名的待机相关素材，尚未沿角色动画事件配置核实每份在游戏中具体的播放时机。

素材目录：`E:/EIEM_Workspace/templates/liino-idle-effects/`

- `README.md`：完整节点路径、渲染模式、循环/周期、原发射率和 Burst 条数。
- `catalogue.json`：所有 29 个发射器、原模块、Material→Texture 引用、Mesh 引用、子发射器、祖先游戏脚本及逻辑身份。
- `liino-idle-templates.blend`：独立模板工程，保留四份来源，默认关闭离线预览及“随 Mod 导出”，避免同时预览或意外把四份素材全导出。需要使用的根节点应明确启用，并设置挂载目标。
- `*.effedit.json`：无修改的作者往返样本，不是已安装的游戏 Mod。
- `template-validation.json`：真实 Blender 导入/复制/保存重开验证结果。

主来源包含五个循环 Mesh 粒子系统：`wenli2`、`guangpan`、`yinlv_di1`、`yinlv_di1 (1)`、`line`；其余为一次性 Billboard。部分原始循环系统的 `rateOverTime=1000`，这不是对常态剑特效的推荐值。先单独预览、调整发射率与寿命，再决定是否持续挂载。完整父节点路径比节点名更可靠，因来源中存在重名节点。

## 复用方法与边界

可直接打开模板工程，或从对应 `.efffx/effect.json` 导入来源；在原节点下选择需要的发射器，复制完整发射器或某个真实 Module。持续附刀要明确设置挂载目标、循环、寿命与发射率，不能把一次性来源仅重命名为循环模板。

在当前来源内复制/粘贴可保留已存在的 Mesh、Material、Texture 引用。跨来源粘贴时，目标图必须已包含可精确解析的依赖。任意跨来源/跨 Bundle 的新资源依赖仍会明确拒绝；不能将复制成功的剪贴板等同于已把任意游戏资源加入目标图。可以先以所选来源为作者基础，编辑需要的发射器并挂载到剑，再单独推进跨图资源解析与生命周期。

`EffectSetting`、`EffectAnimation`、Animator 等是原 Prefab 的其他组件，不属于某个 ParticleSystem Module。本轮显示并保留它们，但离线预览不完整执行游戏脚本，预览中的近似/未支持项仍需以支持提示为准。

## 验证与导入修复

真实 lino 主来源存在零缩放节点。旧发射范围预览使用 `matrix_world.inverted()`，导入时抛出 `Matrix.invert(ed): matrix does not have an inverse`。现仅把该预览坐标转换改为与粒子/静态 Renderer 预览相同的 `inverted_safe()`；不修改源缩放、动画或导出参数。

新增 `tools/Blender/test_eiem_effect_liino_templates_blender.py` 在独立 Blender 5.2.3 LTS 后台进程中验证：

- 四份真实来源成功导入，29 个完整发射器和 EmissionModule 剪贴板编码往返。
- 每份来源的同源发射器粘贴生成独立节点，原来源不变。
- 无修改时预览、同步不产生原生对象编辑，不重生成 Mesh 或贴图资源。
- 多帧预览刷新及模板工程保存/重新打开保留原生数据。
- 四份无修改 `effedit` 均通过本机正式 AnimeStudio 原生构建。

结构面板最终覆盖四份 lino 来源与环特效，合计 59 个节点、33 个粒子系统、160 个组件面板。全部组件展开、来源顺序、默认折叠、Material 队列/关键词/纹理引用、未解析组件显示与绘制无磁盘读取/同步/值改写均通过。旧模块启停、对象/材质导航、保存重开及统一曲线 workspace 回归通过；额外 38 项文档/粒子/曲线/离线预览回归通过。

扩大检查时另有三项旧真实 Blender 回归未通过，不能称为全套通过：

| 测试 | 失败位置 | 修改前/后对照 |
| --- | --- | --- |
| `test_eiem_effect_objects_blender.py` | 第 95 行，首次 Mesh 求值递增 revision，实际求值两次 | 两份隔离插件均复现相同缓存失败 |
| `test_eiem_effect_common_material_blender.py` | 第 135 行，PNG 导出的首像素不等于精确预期值 | 两份隔离插件均复现相同像素失败 |
| `test_eiem_effect_gradients_blender.py` | 第 132 行，保存重开后的渐变拖动时间同步不一致 | 两份隔离插件均复现相同同步失败 |

对照副本保留前轮架构修复，只把本轮 `eiem_effect_editor.py`、`eiem_effect_ui.py`、`eiem_effect_preview.py` 恢复到仓库 HEAD；对象测试的修改前副本也使用 HEAD 测试。对照使用同一 Blender 5.2.3 和同一真实来源，确认三处失败在本轮 UI 修改前也存在。本轮未改缓存、PNG 编码或渐变同步实现，也未放宽这三条失败断言；后续应独立处理。

日志及原生构建证据：`bin/liino-idle-research/`；最终结构回归：`bin/effect-native-structure-20261112/native-ui-final.log`；修改前后对照：`bin/effect-native-structure-20261112/comparison/{before,after}/result.json`。

## 本机部署状态

用户保存并退出 Blender PID 4536 后，四仓库再次 fetch，均在 `main` 且与 `origin/main` 相等，保留所有已有未提交改动。重跑正式部署脚本完成作者 DLL 构建、60 文件插件打包、144 项作者/更新测试（1 项跳过），并事务安装到：

`C:/Users/25487/AppData/Roaming/Blender Foundation/Blender/5.2/scripts/addons/EFF_Blender`

全部 60 个安装文件与构建清单的 SHA256 一致，共享工具 126 个文件与部署回执一致；当前共享代为 `e6fd256ff2dc01093f5db4f6758a3e0dc6fb079b58d630cdbbdfe61f8e7f57d6`，其作者 DLL 与本次构建相同。安装器保留旧插件回滚副本。

实际安装包在独立 Blender 5.2.3 后台再次通过五份来源的原生 UI 验证（33 粒子/160 组件），以及四份 lino 来源导入、29 个完整发射器复制、同源粘贴、无修改导出和模板工程保存重开。两项来源验收改为串行运行，避免同时读取同一 `.efffx` 的互斥锁冲突。交付模板指定本机正式 AnimeStudio 编译器，不指向源码 Debug 目录。

成功部署与摘要核验：`bin/local-deployments/20261112-113738-312/Blender.json`、`InstalledVerification.json`；实际安装包日志：同目录 `installed-native-ui.log`、`installed-templates-sequential.log`。此前因进程占用中止的尝试仅为历史记录，见 `bin/liino-idle-research/local-deployment.log`。

本轮没有游戏内画面验收；上述结果仅说明来源、编辑结构与离线往返链路通过。没有提交、推送或发布 Release。
