# 2026-10-15：原生形态键导出帧身份

用户当前未保存的 jsspsi 工程在导出游戏原有形态键时报 `shape frame channel identity`。实际对象为 `MeshS_actor_jsspsi_brow_01_lod0_1`，四个游戏原有通道及其哈希、权重均有效。

真实来源的现代 Unity `m_Shapes.shapes` 帧结构只有 firstVertex、vertexCount、hasNormals、hasTangents，没有帧名称字段。AnimeStudio 交换文件因此保留空的旧版 frame.name；Blender 导入将其记录为元数据 source_name。原导出函数误把这一空字符串用作 EFF 内部帧身份，原生 writer 随后与所属 channel.name 比较并拒绝。这不是用户形态键损坏，也不需要删键或重导入。

修复在 Blender 导出边界处理：已导入帧的内部名称明确采用拥有它的通道名，不再读取旧版 source_name。保留通道名、哈希、帧范围、权重、位置/法线/切线增量；多帧通道内的各帧使用相同通道身份。新建形态键继续以自身名字建立通道，删除帧继续跳过。原生 writer 的严格身份校验保留。

新增三个回归先复现相同异常，再验证空帧名、多帧不同旧标签和缺标签、作者新增键及导入键删除后的范围和身份。Blender 导出相关 73 项检查通过（1 项原有跳过）。

当前 Blender 只热更新 `export_blend_shapes` 函数并清除导出快照缓存，没有重新注册插件或修改场景形态键、元数据、通道、权重，没有保存工程。真实眉毛的 3,044 个导出顶点、4 个通道通过原生 Mesh writer，两个材质分块的形态字段编码一致；修复前后形态增量、权重、通道及非名称帧字段逐项相同。

证据：`bin/shape-frame-identity-20261015/live-native-validation.json` 和 `live-part-validation.json`。本轮没有修改游戏 Mod、DLL 或重新发布 Release；实际完整 Mod 导出及游戏表现不由单 Mesh 编码验收代替。
