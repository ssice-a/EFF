# 2026-11-12 架构审查修复

本轮在 main 上修复已确认的入口契约与资源生命周期问题，不改变 INI 唯一装配权威、DDS-only 像素来源和游戏真实请求驱动的运行时替换。

## 控制状态

按键、Ins 数值、动画播放/速度和 Action 通过统一的状态提交路径：先校验增量，再准备动画所有权与候选状态，最后提交并立即失效该安装实例的有效值缓存。失败不写非法值、不停止当前动画。每个安装实例拥有独立 revision；静止 Mod 不因其它 Mod 的动画而重算。

Parameter 缺省值直接继承已有 double，不经过六位小数文本往返。F10 参数身份使用稳定变量名，结合类型、值域与 reload 策略迁移；节标题变化不重置同一变量。

## 资源与装配

Mesh 文件内旧 Part 名是作者来源信息，当前装配 Part 名、材质绑定及 material_slot 由 INI 决定。多个 Part 可复用相同 Mesh 文件，原生字段与文件边界等完整性校验继续保留。

Catalog 增加按引用租约清理缓存的入口：先释放无使用者的 Material，再清理无使用者的 Texture/Mesh/Physics/Skeleton。活跃视图及读取者持有自己的不可变数据租约，另一个目标仍存活不再要求整 Mod 保留全部资源。清理后再次打开文件仍核对本代文件属性指纹，拒绝未经过 F10 的资源变更，避免混用新旧数据。

新 INI 动画只在登记阶段编译通道映射。采样载荷在实际命中的游戏 owner 需要播放时加载，同文件的多个 clip 共享只读解码数据；停止或最后 owner 退休后释放，暂停状态也能退休。未命中的 autoplay 不读取采样文件。文件变更须经 F10；同一代际的损坏样本失败会缓存，避免每帧重复读取大文件。控制更新保留现有 owner 活动租约，资源变更代际等待新 owner。

Blender 导入的 Texture 显式逻辑来源和 linear 优先于原材质槽默认；未显式声明时才使用槽默认。干净 DDS 字节原样导出；不同来源或采样语义的同像素 Image 不误合并。

## Manager

作者 ID 标识可复用内容，安装目录标识一次安装。相同包可安装到多个独立目录，保持作者 ID。

Format 3 修复使用附带的共享 C++ 声明校验器，不再调用旧 --export-mod 或生成 compiled.bin/source-inputs.bin。校验器检查装配、控制表达式、声明文件存在与路径边界；原生载荷内容及当前游戏逻辑命中仍由运行时按需验证。缺失作者资源无法凭空恢复，错误会指出文件并保留原 Mod。

索引获取支持当前本地索引、用户导入的包、手动下载缓存、网络下载与失败后的本地构建。Manager 自带游戏格式读取器与原生解压依赖，不要求用户另装 AnimeStudio、Blender。

## 验证与边界

针对性回归覆盖真实控制入口、Format 3 Catalog、原生随机读取/装配、Manager 生产修复/索引入口和 Blender 后台往返；实机游戏动画及 F10 视觉验收仍需单独进行。

没有按文件长度拆入口实现，也没有在缺乏复现的情况下声称或修改 F10 原子提交故障。更大范围的运行时所有权收拢可后续逐步推进。

### 自动验证

- 控制编译、真实按键/面板适配入口和形态桥接回归通过，覆盖同批值更新、非法值不改变播放状态、每 Mod 缓存和坏样本隔离。
- 最终源码的 `runtime_author_catalog_smoke`、`native_random_access_smoke`、`full_resource_reload_smoke` 通过。F10 使用模拟 native API，覆盖多实例、控制专用更新、资源代际和最后 owner 退休，不等于实机视觉验收。
- 动画播放器、骨骼动画、作者元数据、运行时声明和代际差异回归通过。`runtime_controls_package_smoke` 仅完成编译，未以旧格式 fixture 运行。
- Blender 部署套件执行 144 项测试，结果通过（1 项跳过）；实际安装目录在 Blender 5.2.0 LTS 的独立后台进程中通过 13 项材质导入/导出测试，没有加载用户工程。
- Manager 的 17 组工具测试、6 组集成测试、C ABI 校验和 Windows single-file 发布构建通过。集成测试使用真实 VFS/UnityFS 小型夹具，验证断网本地构建、手动包与本地缓存导入，不代表扫描了整套游戏数据。
- root、Blender 和 Manager 的 `git diff --check` 通过。

已知边界：`test_eiem_author_state.py:53` 的 Shape 复制 ID 断言在未修改的 HEAD 上也失败，本轮未更改该无关行为；旧 `build_entry_reload_smoke.cmd` 缺少当前 Format 3 编译开关，被已有守卫拒绝，本轮以现行完整 F10 回归验证。Format 2 迁移仍需可用的兼容迁移工具集；本轮解决的是 Manager 索引链路对额外安装软件和联网的依赖。

### 本机部署

本轮保持全部仓库在 main；修改仍在工作区，未提交、推送或发布 Release。部署回执里的 revision 是当前 HEAD，实际安装内容以文件摘要为准。

| 组件 | 安装位置 | 回执 |
| --- | --- | --- |
| DLL | `D:\Hypergryph Launcher\games\Endfield Game\plugin` | `bin/local-deployments/20261112-071843-869/Dll.json` |
| Blender | `C:\Users\25487\AppData\Roaming\Blender Foundation\Blender\5.2\scripts\addons\EFF_Blender` | `bin/local-deployments/20261112-071711-946/Blender.json` |
| Manager | `E:\EFF-Mod-Manager` | `bin/local-deployments/20261112-071843-865/Manager.json` |
| 共享作者工具 | `E:\EIEM_Workspace\tools\d07252551122813dddd6c9d2dcd0a8f13ceb56a8190a76b90a0b6d6a46d5d50e` | `bin/local-deployments/architecture-final-shared-author/SharedAuthor.json` |

安装后逐一校验 DLL/Manager 回执文件以及 Blender 清单内的 60 个文件，摘要均一致。最终共享作者 DLL 与本次构建产物、Manager 附带 DLL 字节一致。用户 INI、Mods 和诊断目录保持原状。AnimeStudio 本轮未改动，未重新部署。
