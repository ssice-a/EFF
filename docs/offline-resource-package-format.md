# 完整资源离线包契约

当前 ABI：`EFF_NATIVE_PACK_V35_1`。作者输入仍为 `mod.ini format=2` 与独立 `.mesh/.mat/.tex`。Blender、命令行、游戏关联监视器共用 `eff_resource_pack.exe`。DLL 读取编译后的原生输入和二进制清单，F10 不解析 TypeTree、不构建 Mesh/Renderer/Material/Texture、不转换 PNG。新 TEX 自描述尺寸/格式/mip，Texture INI 不声明 source；旧 ABI 缓存需要重新编译。

## 输出与首次加载

```text
resource-reload-cache/
  current.tsv
  geometry/<geometry-key>/
    stage.tsv
    input-001.vfs ... input-068.vfs
  generations/<content-key>/
    manifest.bin
    ready.tsv
    loading-inputs.tsv
    input-001.vfs ... input-068.vfs
    startup/input-001.vfs ... input-068.vfs
```

`current.tsv` 两行分别是 `EFF_RESOURCE_RELOAD_CURRENT<TAB>1` 和 `<key><TAB><manifest SHA256>`。完整文件、清单和作者一致性检查完成后才原子发布。失败保留旧 current。key 包含作者摘要、输入契约、编译器 ABI、GameAssembly、基线和来源输入 SHA；旧 ABI 被拒绝。

启动时外部打包器先确认当前作者的完整包，DLL 从最新 current 选择作者、控制图及该代 `loading-inputs.tsv`。`startup/` 保留游戏原 CAB/Bundle 命名；首次替换沿游戏自己的资源加载发生，不抢先请求额外 Unity 资源。旧 `startup.tsv` 和静态输入表保留为回退文件，v34 使用代际表。

`resource-reload-baseline/source-ranges.tsv` 提供与 pristine 输入对应的物理文件、offset、size 和 SHA。打包器生成 60 条有实际变更的加载替换记录，另外 8 个未改变输入仍在完整图中。加载表的输出路径必须指向最终游戏缓存，不能直接复制其他机器生成的绝对路径表。

## 离线构建

二进制清单保留完整作者数据、块描述、表达式 AST、按键、数值、持久化、规则/LOD、输入 hash、外部引用/闭包/根、命名映射、每 Mesh 的顶点数/有序骨骼/形态和材质贴图预期。小端长度/数量有界；错 ABI、SHA、游戏版本、越界和部分输出拒绝。目前 LZY 清单仍约 137 MB，未作载荷紧缩。

几何缓存包含原命名下已构建的 Mesh、Renderer、骨骼/绑定姿态及控制标识。像素、材质字段、按键/数值修改复用此阶段；Mesh、部件拓扑、材质源身份或基线修改重新构建。贴图/材质阶段、最终命名隔离与完整包输出仍在外部执行。

实测：真实 LZY 冷构建约 22.4 秒，命中几何阶段约 12.4 秒；68 个输入在发布阶段受控并行完成命名、编码、解码校验和写盘。编辑后立即 F10、离线包尚未完成时，会等外部构建；这不是已准备包的原生提交耗时。136 个冷/缓存构建文件逐字节相同，Mesh 索引、材质字段、贴图像素和控制编辑均验证了实际发布结果。

## Blender、PNG 和命令行

Blender 正式导出先发布一致作者目录，再调用同一 EXE；完整候选发布后才报告离线准备成功。只导出显式选中的 Mesh，不由编译器修改场景、选择、活动对象或保存 .blend。目标依次取 `EFF_RELOAD_GAME`、插件 `nativepack/offline-target.json`、直接导出的游戏 `plugin/mods` 路径。

游戏 mods 导出写游戏缓存；工作区导出写并列 `<Mod>.reload`，不会自动部署作者目录。缺同版本编译器、基线或 GameAssembly 时完整离线输出失败。游戏画面验收与离线验证分别记录。

PNG 由 Blender 导出端直接转换为自描述 `.tex`，不需要手动 EXE 操作。默认全部导出，也可选择仅 Mesh、仅材质与贴图或仅贴图，见 [分资源导出](blender-scoped-resource-export.md)。DLL 不解码 PNG；共享编译器由 Blender 自动调用。

```text
eff_resource_pack <Mod> <baseline> <cache> <GameAssembly.dll>
eff_resource_pack <Mod> <baseline> <cache> <GameAssembly.dll> --watch
eff_resource_pack <Mod> <baseline> <cache> <GameAssembly.dll> --watch-parent <game PID>
```

监视器排除 state.ini、PNG/PSD 源文件及 eff_texture_convert.exe；作者稳定 500 ms 后构建。辅助文件可放在 Mod 内，不要求 INI 声明。互斥锁只在构建期间持有，同 key 校验复用，关联游戏退出后监视器退出。

## F10 与原生生命周期

按 F10 直接请求当前 Mod，无世界识别/稳定窗口门槛。后台选择当前作者对应的完整候选、准备控制和小型全局配置；Unity 线程才调用 ResourceManager、隔离 Bundle/PFB 加载。最多 4 个 Bundle 请求在途，闭包完成后请求根 PFB，核验按软帧预算推进。未完成或失败时旧实例继续显示，过期请求不能迟到提交。

世界 owner 走游戏 `RemoveMainPart → DiscardPooledObjects → LoadMainModelSync → ChangeEntityFollowEffect`。已跟踪的 UI 展示模型走 `ReleaseModel → pool discard → SetupData(原配置) → StartSyncLoad`，保留 ReleaseModel 会清除的值类型配置。先释放所有当前 owner，再清理全部匹配路径的普通对象池，包括当前没有可见实例的路径，随后重新装配。每 Mesh 的骨骼路径/顺序、bindposes、形态和材质的原版/作者贴图选择保持。提交时合并玩家数值，交换控制/作者图并清理 Renderer 与形变源的绑定缓存。

旧代以 Unity `AssetBundle.Unload(false)` 退休 Bundle 容器和强引用，不销毁仍被 Renderer/残影引用的 Mesh、材质或贴图。源码依赖 lease 每路径保留一次；残影仍走实际 native Release。失败且已曝光或所有权未核实的代际保留隔离，未声明穷尽原生 job fence。取消旧的 3 代提交限制。

当前覆盖单 Mod、该游戏基线的 LZY 世界/UI 两个根及被准确记录的模型部件生命周期。persistent pool 与没有已知部件生命周期的消费者仍拒绝迁移；未实现所有角色/资源的通用基线发现。资源类型覆盖不等于所有 owner 都已适配。

v34b 已部署，修正了辅助 PNG/EXE 被严格目录校验拒绝导致启动未替换，以及 TEX 改名删除旧路径后 F10 构建中断的问题。转换器输出与 PNG 同名并更新全部共享 INI 路径；Blender reader 同步。生产目录保留用户辅助文件后编译成功，隔离改名链路通过生产 F10 磁盘入口和原生字段回读。

本地生产加载表/清单、136 文件比较、真实原生字段回读、事务/回滚/旧容器退休及连续 F10 mock 通过。生产后台 20 次已准备 F10 请求平均 0.296 ms、最慢 0.466 ms，首次候选验证约 1189 ms；均非端到端实机耗时。完整实机提交、画面与耗时由用户测试，尚未验收。

操作与证据见 [验证记录](full-resource-reload-validation.md)。临时青绿色身体贴图已恢复原作者内容；F10 不自动变色，需要用户自己编辑/导出/转换资源来观察变化。
