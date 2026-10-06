# 替换规则与资源契约

兼容性依据 `src/eiem_package_protocol.h` 字段及实际数据核验。版本、编译器名称与 ABI 数字仅提供信息。`v35/v36` 分别保存无/有根地址描述，两者支持小型规则。新 reader 保留旧完整输入布局；新规则需 reader 声明其编码能力，相同版本标签不能替代字段契约。

## 作者与输出

`mod.ini` 为唯一作者规则，引用独立 `.mesh/.mat/.tex`。工具自动生成 `source-inputs.bin`，复用现有 AnimeStudio 索引定位全部真实消费者与所需模板，见[命中流程](runtime-resource-discovery.md)。DLL 不读取全局索引。

```text
resource-reload-baseline/
  baseline.tsv          # 原游戏范围、SHA 与根
  source-ranges.tsv
resource-reload-cache/
  owner.tsv
  current.tsv
  sources/<key>.targets
  generations/<key>/
    manifest.bin        # 压缩规则；作者字节按引用读取
    ready.tsv
```

新候选不写完整 `.vfs`、`startup/` 或完整资源阶段缓存。旧全量缓存与旧清单读取分支不是新规则的前置条件。

`current.tsv` 两行为 `EFF_RESOURCE_RELOAD_CURRENT<TAB>1` 与 `<key><TAB><manifest SHA256>`。key 覆盖作者内容、构建语义、字段布局、原来源内容，不包含产品版本或 GameAssembly 摘要。核验成功且作者未变化才原子更新；失败保留旧 current。集合 owner 绑定 mods 目录，成员增删不改变 owner。

## 清单字段

外层 `compressed-rules/xpress-huffman/le64` 保存实际解压大小与压缩载荷，内层 `author-references+prefab-renderer-rules+source-ranges/le64/sha256` 保存：

- 原作者集合/目录、控制、资源文件路径和 SHA、资源块描述、Replace/LOD/Part。
- PFB 根与装配预期、原文件范围和 SHA、每个 Renderer CAB+PathID/原 Mesh 身份。
- 启动/隔离输入摘要、命名映射、骨骼顺序、形态和材质贴图预期。

规则基本字段之后可包含下列按结构名称标识的字段；无扩展的旧规则仍可读取，不以编译器版本号判断：

- `namespace-offset-patches/le64/sha256`：每输入的序列化成员原摘要及等宽修改位置、原值和新值。离线审计 CAB、外部引用及流路径，运行时先核对完整成员摘要再应用位置，拒绝越界、重叠及原值不符。
- `member-rebuilds/source-author-spans/le64/sha256`：每输入的最终成员名、flags、size、SHA 和复制段；复制段引用原游戏成员或作者资源的 offset/size，或包含变化字节。作者 packed frame 必须展开时记录已验证的来源布局与展开摘要，结果按作者资源共享一次。运行时不重建资源索引、不匹配 Renderer、不解析 PFB 层级，按段构造后核对最终成员和完整包摘要。

这些字段保存离线构建结果的重建指令，不保存完整原包或作者大段载荷。小段差异会增大清单，真实 LZY 样本目前约 4.21 MiB，不能继续将旧约 207 KiB 或“小于 1 MiB”作为当前结果。清单大小没有人为限制。

作者 Mesh/贴图载荷不嵌入清单。解压大小、数量、整数、文件范围、字段类型、摘要、重复身份、引用覆盖和数组布局按实际字节核验，没有固定容量上限。`--protocol` 必须成功查询；Blender 拒绝向不能读新编码的旧 DLL 发布，不能猜 EXE 字符串。

## 应用与生命周期

编译器组合完整集合、精确发现目标，在内存验证规则输出并记录复制指令。DLL 读取当前原游戏范围，按离线复制段重建并验证 SHA，通过只读 MemoryStream 交给 Unity 的归档流加载入口；旧清单仍用完整序列化构建器。不使用 Unity Mesh/Material setter 改基础资源，不解码 PNG。

一个原 Renderer 对应完整 0/1/N Part。未声明 Renderer 保留原对象。材质模板克隆为作者对象，原材质/原贴图保留；其他依赖仍走游戏正常加载。完整 Bundle 仍需在内存生成；新复制指令消除运行时 TypeTree 解析、Renderer 匹配与骨骼映射，保留范围读取、归档解码、复制和摘要成本。

首次加载保留原命名，按精确物理路径和 offset 路由。F10 后台准备隔离输入，Unity 帧提交异步加载，闭包完成后核验 PFB，再调用世界/展示 owner 生命周期及动画重绑定。流及其 Byte[] 保留到 Bundle 卸载，保证请求完成后的定位/读取。规则构建或同步提交失败保留游戏原请求；异步原生失败的首次加载回退尚未覆盖，不把模拟通过当作实机验收。

支持 0/1/N Mod，F10 统一提交增删改与空集合。原 Mesh 目标冲突拒绝；相同材质声明可复用，当前不同声明共用同一来源仍按既有冲突规则拒绝。源依赖租约、路径池清理、加载回调、rollback 和旧 Bundle `Unload(false)` 保持同一事务。磁盘监视器不自行交换游戏代际。

## Blender 与验收

Blender 暂存作者资源和来源描述，发布目录后调用配套编译器。导出到游戏 mods 时编译全部安装集合；工作区单作者输出不自动部署。默认全部导出，也支持仅 Mesh、材质与贴图或贴图；PNG 在作者端转成自描述 TEX。不修改或保存用户 .blend。

```text
eff_resource_pack <Mod> <baseline> <cache>
eff_resource_pack --mods <mods> <baseline> <cache>
eff_resource_pack --mods <mods> <baseline> <cache> --watch-parent <game PID>
```

真实规则回读、未声明 Renderer/原材质保留、多 Mod 增删与空集合、内存接口/异步引用、连续 F10/回滚及安装摘要见[热重载验收](hot-reload-testing.md)。这些检查不证明游戏画面、动画、多角色或所有过场已经验收。
