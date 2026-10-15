# 2026-10-15：解包更新、旧工程导出与 Mod 修复

本轮实现三条独立职责。作者可直接重新解包，再从原 Blender 工程导出，不必使用修复工具。游戏内没有增加资源发现、补编译或 Mod 自动写回；本轮不修改已安装 DLL 或生产 Mod。

## AnimeStudio 与 Blender

- AnimeStudio 的完整 EFF 源包改为暂存生成、验证、源包锁及整目录提交，保留 `.eff-backups`。普通失败保留原完整包，不再开头删除活动源清单。
- 保存生成文件归属和已解包位置。打开当前 VFS/更新索引后更新工作区已有及登记的源包；也可通过 EFF 菜单 `Update previously unpacked EFF resources...` 或 `--refresh-eff-sources` 指定旧输出目录。当前游戏和索引未变时自动检查跳过已有更新结果；显式重新解包可重新生成。
- 以类型、完整逻辑路径和名称保留已有作者文件名、INI 段名；新增资源不会抢占旧资源文件名。更新清单、真实快照、资源和当前索引绑定一起生成。未知文件保留，旧生成文件退休，上一完整目录备份。
- 多个源包的目标和闭包共用两次完整索引扫描，不对每份源包重复全局扫描。找不到已退休 Prefab 时各包独立报告、保留旧输出。
- Blender 在导出期间持有相同源包锁，导出前加载新清单，原生模板缓存随源快照摘要/身份变化而失效。读取 AnimeStudio 记录的当前索引，避免误用旧父目录索引；外部直接改写来源时，发布前清单摘要核对仍会拒绝变化。
- 工程内作者几何、UV、权重、形态键、Action 和参数不因来源更新而重新导入。绑定不明确、结构不兼容等仍报告冲突。

真实隔离验证使用 `G:\zmd\zfy\zfy1.blend`、旧庄方宜源包副本及当前游戏：105 条资源身份、作者文件名和段名保持对应；从旧工程 body 网格用新来源完整导出成功。原 blend SHA256 保持 `1bba15871bf46d4228e643af0859050e52c4acedc246de546f2f382779dc723c`。来源路径仅在后台测试进程中重定向到隔离副本，未改真实工程或旧作者源包。

## 独立修复工具

源码为 `tools/EFF.Repair`，Windows Forms，运行需 .NET 9 Windows Desktop Runtime。包含独立编译器及与 AnimeStudio 相同的只读 VFS/索引代码，不依赖 AnimeStudio UI、Unity 解析器或图形解码库；初版构建约 2.3 MB，不含运行库和游戏索引。

具备游戏识别/手动目录、Mod 文件夹选择或拖入、勾选批量修复、更新检查、对应索引下载/本地导入、任务记录、取消与备份。游戏运行时拒绝提交。比较输出协议，编译前后核对作者文件保持，使用相同 Mod 发布锁并在提交前再次核验作者输入；重复适配同一内容会跳过。备份置于 Mod 父目录的 `.eff-repair-backups`，当前 DLL 只扫描一层且其根无 `mod.ini`，不会作为另一个 Mod 加载。

修复状态保存在工具的 `%LOCALAPPDATA%/EFF/Repair/receipts`，不向严格的 Mod 目录添加记录文件；开发阶段曾发现记录文件会触发未引用文件校验，已移出并补做生产集合读取回归。旧输入描述与编译结果保持成对传给编译器，不能先覆盖旧描述再尝试从旧编译结果反推逻辑来源。

维护端 `--pack-eff-index` 生成含完整目录和 Bundle 内容证明的 gzip 包；`--make-catalog` 生成在线目录。不同盘符/文件时间、相同实际内容可导入；内容不符和损坏文件拒绝。导入重建本地 eidx/辅助证明，AnimeStudio 和 C++ 来源编译器都可接受，不重新解析全部游戏 Bundle。维护端 `.eidx.bundles` 不必随公共包分发。

当前实际索引包 `97429947` 字节，原索引 `830958103` 字节；gzip SHA256 为 `8149CB07DE1793C3AFA2A949BE269D618EF9ECA4480617ACE61B0857DF84890A`。原始目录摘要与包内逐 Bundle 内容证明核验通过。

默认更新源检查 `ssice-a/EFF` 最新 Release 的 `eff-index-catalog.json`。本轮实际在线检查返回尚未发布目录。没有上传索引、发布新 Release 或使用示例 URL 冒充可用下载；在线下载的公开投放仍待维护端发布。

真实旧 ZFY Mod 副本（恢复更新前的 compiled/source-inputs）通过修复工具完成修复，45 输入/21 根的启动与隔离重放和当前版本离线编译一致；作者文件保持，备份存在。状态记录移出 Mod 后，再次重编译与生产集合读取通过，后续调用跳过。游戏视觉验收未包含在本轮。

## 回归与证据

- Blender：19 项离线调用、27 项原生导出核心、15 项发布检查通过（其中原有 1 项跳过）；新增源清单变更拒绝、新来源重新定位及当前索引优先级测试。
- C#：源包失败回退、生成文件退休、未知文件保持、备份和并发读者保护通过。
- 索引：BLC v3/v4/v5、逐 Bundle 增量/冷构建对照、跨路径与文件时间的公共索引导入、内容不符及截断拒绝通过。
- 真实完整索引打包、导入、AnimeStudio 接受、C++ 编译和未变索引零解析复用通过；273608 Bundle 的重复更新 parsed=0/reused=273608。
- WinForms 页面已构建、实际创建并渲染检查。网络尚未发布目录按真实状态展示，不能以此声称公开下载已上线。

证据位于 `bin/update-workflows-20261015`：`source-update-validation.json`、`blender-final.json`、`repair-compare.log`、`repair-repeat.txt`、`online-check.json`、`unchanged-index.log` 和 `repair-ui.png`。单网格旧工程验证不等于全部角色、骨架语义变化或游戏画面均已验收。

## 本地交付状态

- 修复工具位于 `bin/eff-repair/portable/EFF.Repair.exe`，本机工具工作区已导入当前完整索引，可直接执行本地修复。公开在线索引尚未发布。
- Blender 源码及配套 nativepack 来源读取器已更新，原工程文件没有保存或修改。
- 用户确认关闭后，14:09 已备份安装 `dist/net9.0-windows/bin/AnimeStudio.GUI.dll`，安装 SHA256 为 `834B8C0BEDDA392779DCA1C10F8E4F18498AB0B9861F1B80FC03E1350C6EF3ED`。实际安装 DLL 的来源更新命令以隔离空输出运行，返回 `UPDATED 0`、退出码 0；启动器、配置、用户工作区和其他依赖保持。备份及回执见 `bin/update-workflows-20261015/installation.json`。进程检查最初漏掉 `.GUI` 后缀所创建的未使用旁置文件已清理。
- 修复工具计划随 EFF 仓库 Release 作为独立可选下载，完整索引与 `eff-index-catalog.json` 作为单独附件，软件从该发布源检查/下载。已准备本地 `bin/eff-repair/EFF-Repair-v0.1.0-win-x64.zip`，1034352 字节，SHA256 `BC1D1B3BD3E52845FF0709D6D18EFDBF130E3481CD7683B2EF863EDF7268832D`；尚未上传或发布。
- 游戏已安装 DLL SHA256 保持 `40822B9B35E1E9B5137E2F2F8202422F3EF52D3E204ADF97842F363F3B9508A0`，生产 Mod 未改动。
