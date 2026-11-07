# 特效导回闭环架构审查

状态：2026-11-07 静态审查，尚未完成 Blender 实机验收。

## 结论

当前特效有两条不同的导出协议：

1. `.effedit.json` 是基于 `.efffx` 源的增量编辑文档，包含原生字段变更、发射器操作和作者资源 additions。
2. Format 3 `effect-author` 是可发布的作者资源，包含 `.efffx` 源压缩包、Blender workspace 和挂载声明。

Blender 现在有 `.effedit.json` 的原生字段导入入口，但还不能从该文档恢复作者资源 additions 或发射器拓扑。独立特效 Mod 发布器 `eiem_effect_export.publish()` 仍是占位实现，因此“导出一个独立特效包，再导回 Blender，再发布”这条链路还没有闭合。

另外，Format 3 回导以前只恢复源 workspace。对于只复用游戏 Mesh 的特效，这足够恢复逻辑字段；对于没有随包携带挂载 Mesh 的 effect-only 包，导入后没有 Blender Mesh 指针，重新生成挂载声明会失败。现在导入器会保留并复用已经校验过的逻辑挂载声明，不复制游戏资源，也不写空 PPtr。

## 当前模块关系

```text
.efffx source
    -> eiem_effect_document.load_source
    -> eiem_effect_editor.import_workspace
    -> Blender preview / author projection
    -> eiem_effect_assets.export_document (.effedit.json)
    -> AnimeStudio --build-eff-effects
    -> Format 3 effect runtime + effect-author
```

问题在于 `import_workspace`、`export_document` 和 Format 3 author snapshot 之间没有一个明确的 Effect Source Artifact 接口。各处直接拼接 workspace、编辑文档和挂载声明，导致源身份、作者增量和运行时构建物的职责混在一起。

## 已修复的闭环断点

- Format 3 导入保留 `effect-author` 内的 effect ID；同一场景中发生冲突时才生成新 ID。
- Format 3 导入使用 `preserve_source=True`，不会在导回时隐式关闭源自动发射控制器。
- 导入器保存完整的 `eff_effect_attachment` 声明，包括逻辑目标、目标 Prefab、Renderer 路径、骨骼路径、按键和挂载偏移。
- 重新导出时，如果游戏 Mesh 没有被作者包携带，则从这份已校验声明生成挂载记录，继续按逻辑路径复用游戏资源。

这解决的是“作者资源已经存在，但 Blender 中没有本地游戏 Mesh 指针”的回导问题；它不等于已经实现独立 effect-only Mod 发布。

## 下一步推进顺序

### 1. 建立 Effect Source Artifact 深模块

把以下操作收敛到一个接口：

- `load(source_dir)`：读取并校验 `.efffx` 源。
- `apply_edit(source, edit)`：校验 source manifest/effect digest 后应用 `.effedit.json`。
- `snapshot_from_blend(root)`：生成原生 workspace、挂载声明和作者资源增量。
- `materialize_to_blend(snapshot)`：恢复 Blender 可编辑投影。
- `write_author_resource(snapshot)`：生成 Format 3 `effect-author`。

`eiem_effect_editor` 只负责 Blender 投影，`eiem_effect_assets` 只负责作者 Mesh/Material/Texture 增量，`eiem_format` 只负责封装和校验。这样测试可以在同一个 seam 上验证导出、导回和再导出。

### 2. 扩展 `.effedit.json` 导入

当前已经支持无作者资源、无拓扑操作的原生字段导入。导入时必须校验：

- `sourceManifestSha256` 和 `sourceEffectSha256`；
- 每个对象的 `sourceDataSha256`；
- 发射器新增/删除操作的模板和父子关系；
- Mesh/Material/Texture additions 是否能在当前源中重新绑定。

不能把 `.effedit.json` 当作完整特效源，也不能直接把 CAB/PathID 写回另一个源文件。

### 3. 实现独立 effect-only 发布

`eiem_effect_export.publish()` 目前仍然抛出“独立版特效写回尚未接入 Format 3”。应让它接收完整的 Effect Source Artifact，而不是只接收已经编译的 `runtime.bin` 路径。发布物应包含：

- `effect-author` 作者快照；
- effect runtime resource；
- `controls.effres` 中的 EffectAttachment；
- 逻辑游戏资源引用；
- 作者 Mesh/Material/Texture additions（仅在确实被修改时）。

### 4. 真实 Blender 回归测试

新增测试必须执行：

```text
导入 .efffx
  -> 修改发射器参数 / 曲线 / 子发射器 / 逻辑资源路径
  -> 导出 Format 3 Mod
  -> 清空或重启 Blender
  -> 导入 Format 3 Mod
  -> 再次导出
  -> 比较 workspace、挂载声明、作者资源和运行时资源
```

纯 envelope 测试不能替代这条 seam。当前环境没有可连接的 Blender MCP，也没有本机 Blender 命令行，因此本轮只完成静态修复和纯测试，实机闭环仍需在 Blender 可用后执行。

