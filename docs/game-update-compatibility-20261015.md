# 2026-10-15 游戏更新兼容修复

## 故障与修复

旧 DLL 的 11:11 启动日志记录 `loading input file stamp unavailable`，编译集合准备失败后以 `mods=0` 启动。五个已发布 Mod 的 390 份来源记录所指向的旧 CHK 均已不存在；按当前两层 VFS 的逻辑路径读取并核对真实 SHA256，128 份内容相同、261 份变化、1 份已从当前目录移除。这里按每 Mod 的来源计数，重复依赖也计入；缺少旧 CHK 不能推出 Mesh 内容全部变化。

| Mod | 旧输入 | 当前内容相同 | 当前内容变化 | 当前逻辑来源移除 |
| --- | ---: | ---: | ---: | ---: |
| LZY | 70 | 23 | 47 | 0 |
| Typhoea | 50 | 29 | 21 | 0 |
| ZFY | 45 | 17 | 28 | 0 |
| gly | 162 | 14 | 148 | 0 |
| lino | 63 | 45 | 17 | 1 |

新版 `ModelManager.DiscardPooledObjects` 增加 `System.Boolean unloadImmediately` 参数。旧 DLL 只绑定单参数签名，因而热重载初始化被拒绝。现按完整返回类型、参数类型和 static 属性识别已核验的新旧两个签名，其他签名仍拒绝；新调用明确传 false，清除路径对应对象池，同时沿用游戏的延后资源卸载与既有 source lease。正常重建、空闲池和回滚使用同一调用适配器，不使用旧 RVA 或直接修改游戏对象私有字段。

离线再导出原先会在新旧来源内容冲突时拒绝。现在只有 `--source-index` 经当前两层游戏指纹和实际范围/MD5 验证所产生的完整选择，才可替换变化来源并退休旧来源；不完整选择继续拒绝冲突。当前内容相同的已编译来源可重定位，旧证明仅按相同输入身份复用；来源或依赖变化时重建受其影响的当前编译结果。无游戏来源变化时，原有新增 PFB 继承和并发追赶仍保留。游戏内自动发现、后台补编译与首次交付门控仍未接入，此修复不能宣称下次游戏更新无需操作。

文件读取失败现在报告具体路径与 Win32 错误，避免启动日志只给出无法定位的通用错误。

## 验证与安装

从用户旧索引的隔离副本进行真实增量更新：当前 273608 个 Bundle、3982027 个资产，parsed=122659、reused=150949、removed=12333、FailedBundles=0，构建器约 242.60 秒。其余记录使用逐 Bundle 内容和索引范围证明复用，未重新解析全部游戏包。新索引位于 `bin/game-update-repair-20261015/current-index/index/`，用户 `E:/EIEM_Workspace/index` 未改写。

五个 Mod 在隔离副本上通过真实 `--export-mod --source-index --previous-mod` 重编译。244 个作者文件与原发布逐个 SHA256 一致；只更新各 Mod 的 `compiled.bin` 与 `source-inputs.bin`。当前组合读取为 5 Mod、339 个唯一输入、206 个根、47 个共享输入。Typhoea 的更新编译与冷编译 startup/isolated 回放逐字节一致；真实新增 PFB 继承、重复导出、并发追赶不重写旧 source/proof 的回归通过。

新旧回收接口适配测试先复现新版无法绑定，再修复通过。完整模拟重载覆盖新版布尔实参、正常迁移、空闲池、连续 F10、异步/UI 交付和失败回滚。生产 DLL 的 PE 导入检查及预加载游戏附带运行库后的隔离加载通过。这些离线与模拟结果不证明新版游戏中的实际画面和 F10 已通过。

确认游戏进程已退出后，已备份并安装生产 DLL、Blender 配套编译器以及上述五个已发布 Mod 的十份生成文件。生产 DLL SHA256 为 `40822B9B35E1E9B5137E2F2F8202422F3EF52D3E204ADF97842F363F3B9508A0`；安装后的真实组合读取通过。游戏原始资源、作者 Mesh/材质/贴图、Blender 工程及原工作区索引未改动，未发布未完成的 Endminf/Allure，未发布新 Release。

备份与安装记录：`bin/game-update-repair-20261015/installed-backup/`、同目录 `installation.json`。

## 新会话验证

11:54:36 启动的新游戏会话 PID 35068 已确认加载上述 DLL。日志为 `ENTRY-GENERATION initial=1 mods=5`、`FULL-RELOAD-POOL-API unloadImmediatelyArgument=1 forcedAssetUnload=0`、`FULL-RELOAD-READY` 与 `replacementArmed=1 ruleMemoryArmed=1`；之前的来源读取失败和回收接口签名缺失未再出现。11:55 已记录 lino 角色展示模型完成、作者控制网格注册。此会话 compiledSetMs=5453.915，不包含游戏就绪等待；不把离线读取约 3.3 秒当作实机启动耗时。

已保存启动日志到 `bin/game-update-repair-20261015/game-session-35068-startup.log`。当前可确认新会话成功加载 Mod 并完成接口绑定；尚无 F10 提交日志，角色画面与实际 F10 结果仍待用户确认。此证据仅覆盖当前版本恢复，不代表自动适配下一次游戏更新已实现。

## 2026-10-27 逻辑资源身份与 PFB 重放调查

身份规则已明确：替换目标由 Mesh 的逻辑资源路径和资产名确定；其内容摘要、长度、TypeTree 或顶点结构不是身份。原游戏 Material/Texture 依赖同样以逻辑路径定位。当前 VFS 的物理文件、偏移、长度和摘要只描述这次安装里的载荷位置与完整性，不能单独判定 Mod 失效。

对 `gly` 的当前实测显示：逻辑来源 `bundles/windows/main/68c216df3439cc0d11c16c01.ab` 从 672,040 字节变为 671,058 字节，仍是同一个 CAB 成员；TypeTree 数量都是 42，6,341 个对象 ID 保留，另有 875 个新增、881 个删除和 2 个对象大小变化。探针先按 22 个 Mesh 逻辑路径+资产名重新解析当前 Mesh 引用，再反查到 28 个当前 Renderer 消费者。当前 Renderer 恰好仍保留原类、TypeTree 与对象大小；这些相等只是样本观察，不是兼容前提。旧 Mod 对它们的修改集中在 Bounds、BlendShapeWeights、Bones 和 Materials。

当前热重载仍用旧的整成员 span recipe。它因 `recipe reconstructed member digest differs` 拒绝新 PFB。诊断探针进一步跳过整成员摘要后，结果仍无法解析（`invalid native object record`）；因此只删除长度/SHA 校验会生成损坏对象，也可能丢掉游戏新增对象。

在同一份 gly 输入上，诊断原型从当前 PFB 的 Mesh 引用重新发现 28 个 Renderer，再按字段名把可兼容的 Mod 改动叠加到当前对象；输出能重新解析，875/875 个当前新增对象保留，原生对象引用检查通过。合成回归覆盖字段重排、新字段保留、缺失字段及字段类型变化。真实样本中 Bounds 与 Materials 可应用；18 个 BlendShapeWeights/Bones 数组因长度不同而单项跳过。这些字段需要基于 Mesh 逻辑身份及骨骼/形态名称实现专门映射，不能要求整个 Renderer 或 Mesh 结构相等，也不能把单个字段无法映射升级成 Mesh 身份失配。

后续运行时适配必须按逻辑资源路径+资产名解析当前 Mesh，并在当前 PFB 内反查所有消费者；当前物理文件、偏移、长度、摘要、CAB、PathID 和完整 TypeTree 都只是本次安装中的读取或定位信息。Mod 资源替换应以逻辑路径引用，不能把原游戏 Mesh/Material/Texture 的旧载荷复制成 Mod 身份。Renderer 更新保存为字段级语义操作：应用时按当前字段名和所需类型转换，保留未声明字段及当前新增对象；对 Bones、BlendShapeWeights 等索引数组按稳定的骨骼/形态身份单独映射。某个字段不存在或无法转换时记录该操作的兼容结果，不得因此判定 Mesh 逻辑身份或整个 Mod 失效。摘要只用于完整性及旧快照验证，不能充当资源身份或兼容门槛。

生产 DLL 与资源编译器现已接入对象级重放。新导出会在每个 source record 保存逻辑 Mesh 身份、Mesh/Renderer 对象的原始与编辑 TypeTree 值，以及按字段路径叠加所需的数据；运行时对未变化 source 继续使用原有 native view 快路径，source 内容改变时才按逻辑 Mesh 路径和资产名重找 Mesh、反查当前 Renderer，并将可兼容字段写入当前对象。当前新增字段会保留；缺失字段或类型不兼容只跳过对应字段，不会把整个逻辑 Mesh 判为失效。资源块快路径遇到 Mesh 资源变化会退回标准编译，以同步刷新语义记录。

旧 source record 仍可读取，且 source 未变化时仍可正常使用；旧记录没有字段语义数据，source 内容变化时会拒绝重放并提示离线修复。修复应使用 Mod 的 `mod.ini`、作者资源和当前来源索引重新生成运行数据，不要求用户重新导出 Blender 工程。当前语义捕获对未知对象新增/删除或未覆盖的对象改动会将该 source 标记为不完整并拒绝跨版本重放，避免静默生成损坏 PFB。Bones、BlendShapeWeights 等依赖名称身份的数组映射仍需专项实现；长度不同的数组目前不能保证逐项迁移。`tools/diagnostics/game_update_compat_probe.cpp` 包含生产重放函数的合成回归，以及真实数据探针。

## 旧 Mod 迁移策略

游戏更新不应仅因 Mesh 内容、PathID、文件长度或 TypeTree 改变就判定旧 Mod 失效。新格式语义记录可在 DLL 热重载时适配可兼容的对象字段；较旧 compiled.bin 没有这类记录，或 source 中包含未支持的结构改动时，Mod Manager 的修复流程需要用当前索引重新定位目标及消费者，并从作者声明重建当前运行数据。`source-inputs.bin` 是可重新生成的来源映射，不要求作者重新用 Blender 导出。

修复服务将 `EFF_MOD_BLOCKS/2` 视为可供增量追赶的当前缓存。旧块格式、旧单体 `compiled.bin`、损坏或未知格式不作为增量前态：修复只在临时副本删除 `compiled.bin`、`compiled.blocks/` 与 `source-inputs.bin`，再以当前索引冷重建。只有完整构建并校验通过才替换原目录，旧目录保存在相邻备份目录。旧包缺少 `mod.ini` 时不能从运行缓存反推完整的作者资源与命中声明；应恢复原始 Mod 包或 Blender 工程，修复失败不得触碰原目录。

该迁移只适用于保留了完整 `mod.ini` 及其引用作者资源的包。它是当前阶段的离线恢复路径，不代表生产 DLL 已完成跨游戏版本的字段级语义重放；后者仍需在 DLL 与编译器协议里单独实现和验证。
