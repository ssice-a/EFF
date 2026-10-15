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
