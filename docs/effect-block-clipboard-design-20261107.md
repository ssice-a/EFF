# 特效块复制与跨文件合并设计

## 目标

在 Blender 的特效编辑器中支持两种操作：

- 在同一个特效内复制模块块、Renderer 块或完整发射器；
- 在不同 `.efffx` 文件之间复制特效块，把多个来源融合到同一个特效工程。

复制结果必须可以继续预览、撤销、保存、重新导出，并保留游戏原生的 TypeTree、引用关系、逻辑资源路径和 Bundle 依赖。

## 不能直接复制二进制块的原因

010 编辑器复制二进制块时，会同时维护 SerializedFile 的对象表、CAB 外部表、PathID、对象拥有关系和引用字段。`m_FileID/m_PathID` 只在当前 SerializedFile 图中有效，直接把一段数据粘贴到另一个特效会导致：

- PPtr 指向目标文件中同编号但不同类型的对象；
- 外部 CAB 的 fileID 指向错误 Bundle；
- GameObject、Transform、ParticleSystem、Renderer 的拥有关系仍指向源节点；
- EffectSetting 的 LOD/发射器表没有登记新对象；
- Blender 预览看似成功，但 AnimeStudio 重建或游戏加载时失败。

因此 Blender 侧要复制的是“特效块的逻辑对象图”，而不是裸二进制区间。粘贴时再为目标来源图分配新的身份并重写所有引用。

## 可移植剪贴板格式

剪贴板使用 JSON 信封，格式名为 `EFF_EFFECT_BLOCK_CLIPBOARD`，版本从 1 开始。小块可以直接放进 `WindowManager.clipboard`；带原生字节或贴图时写入 `%LOCALAPPDATA%/EFF/effect-clipboard/<sha256>.json`，系统剪贴板只保存版本和文件摘要，避免系统文本剪贴板截断大数据。

```json
{
  "format": "EFF_EFFECT_BLOCK_CLIPBOARD",
  "version": 1,
  "kind": "module | renderer | emitter",
  "source": {
    "prefab": "assets/.../source.prefab",
    "manifestSha256": "...",
    "effectSha256": "..."
  },
  "root": {
    "classId": 198,
    "type": "ParticleSystem",
    "module": "EmissionModule",
    "name": "..."
  },
  "objects": [],
  "references": [],
  "assets": []
}
```

`objects` 保存对象类型、TypeTree schema、原生字段数据和源身份；`references` 把每一个 PPtr 字段转换为源对象身份、期望类型、逻辑路径和资源名；`assets` 保存作者自己创建的 Mesh/Material/Texture 的内容摘要及作者数据。最终粘贴结果不能依赖源 CAB、PathID 或源文件的绝对路径。

## 三种复制粒度

### 模块块

例如 `EmissionModule`、`ShapeModule`、`UVModule`。复制时将模块字段映射到目标 ParticleSystem：

1. 校验源、目标都是 class 198，且模块在目标 TypeTree 中存在；
2. 复制模块内的所有可编辑字段，包括曲线、渐变、Burst 和启用状态；
3. 对模块内 PPtr 使用逻辑路径解析，不复制旧的 fileID/pathID；
4. 目标缺少引用时进入依赖处理，而不是写入悬空指针。

这是第一阶段应该实现的跨文件能力，因为它不需要改变 GameObject/Transform 拓扑，也能直接组合不同特效的运动、发射、颜色和 UV 逻辑。

### Renderer / 材质块

Renderer 的 Mesh、材质和贴图引用必须作为一个依赖子图处理。游戏原有资源只保存逻辑路径和资产名，目标工程已有同一资源时复用；作者资源按内容摘要去重，缺失时作为新的作者资源加入目标工程。不能只复制 `m_Materials` 数组，否则会把源文件的 PathID 带入目标。

### 完整发射器块

完整发射器包含 GameObject、Transform、ParticleSystem、Renderer 以及 EffectSetting 的登记信息。它可以复用现有 `topology.duplicate` 的同文件逻辑，但跨文件时需要：

1. 为所有对象生成新的目标 CAB 内 PathID；
2. 重写内部 PPtr、父子 Transform、组件列表和 Renderer 资源引用；
3. 把外部游戏资源按逻辑路径解析到目标来源图；
4. 把作者资源加入 `assets`，并让 AnimeStudio 重建对应 SerializedFile；
5. 更新 EffectSetting 的 LOD 发射器记录；
6. 重新生成 Blender 预览对象和 `eff_effect_components` 映射。

完整发射器跨文件复制需要 AnimeStudio 的 `emitters` 输入支持“可移植对象记录”，不能只接受目标 effect.json 里的 template 身份。

## 依赖解析策略

粘贴时每个引用都进入以下状态之一：

- `reuse-game`: 目标来源图存在唯一匹配的逻辑路径、资产名和兼容类型，直接复用游戏资源；
- `reuse-author`: 目标工程已有相同内容摘要的作者资源，复用现有资源；
- `import-author`: 源剪贴板包含作者资源，复制到目标工程并重新分配身份；
- `unresolved`: 没有唯一匹配。粘贴操作停止并显示缺少的逻辑路径、类型和资产名。

默认不自动猜测同名资源，也不把整个源 `.efffx` 的 Bundle 全部复制进目标。用户可以在依赖窗口逐项选择复用、导入或取消。

## Blender 交互

- 每个折叠特效块标题栏提供“复制块”和“粘贴块”；
- ParticleSystem 的模块块只允许粘贴到兼容的 ParticleSystem；
- Renderer 块提供“替换当前 Renderer”或“作为新发射器材质”两个明确动作；
- 发射器根节点提供“粘贴为新发射器”；
- 粘贴前显示依赖摘要和冲突列表，粘贴后一次性写入 Blender Undo；
- 预览刷新、`eff_effect_components` 更新和源状态保存必须作为同一个事务；
- 剪贴板只存可移植数据，不能保存 Blender 对象指针、临时目录或绝对路径。

## 实现顺序

1. 纯 Python `eiem_effect_clipboard.py`：生成/校验剪贴板信封，收集 PPtr 逻辑引用，执行模块块复制与粘贴；
2. Blender UI：在模块块和 Renderer 块标题栏接入复制/粘贴按钮，增加依赖解析对话框；
3. 纯模型和 Blender 回归：同文件、跨文件、缺失依赖、类型不兼容、撤销/重做、剪贴板损坏；
4. AnimeStudio：增加跨文件完整发射器的可移植对象输入和安全 PathID 分配；
5. 真实来源构建回归：检查 SerializedFile 对象表、EffectSetting LOD、Bundle 成员和 runtime.bin 的确定性；
6. 最后再做跨 Mod 发布与游戏实机验证。

## 当前结论

现有同文件发射器复制可以继续复用；跨文件复制不能通过把源二进制块直接贴进目标来实现。最小可用版本应先完成“模块块 + 逻辑资源引用重绑定”，再扩展到完整发射器。这样既能实现不同特效融合，也不会破坏按需资源、游戏原有资源复用和当前 Format 3 的自描述约束。

## 当前实现状态

2026-11-07 已完成第一阶段：`eiem_effect_clipboard.py` 提供模块块信封、校验、编码/解码和目标逻辑资源重绑定；Blender 的 ParticleSystem 模块标题栏已接入复制/粘贴按钮。粘贴使用独立试算 Editor，引用无法唯一解析时不会写回目标工程。纯模型测试、插件注册测试和本地部署回归均通过。

当前按钮覆盖 `InitialModule`、`EmissionModule`、`ShapeModule` 及其他已显示的 ParticleSystem 模块；Renderer、材质和完整发射器跨文件复制尚未开放，避免在 AnimeStudio 尚未支持可移植对象图之前生成悬空 SerializedFile 引用。
