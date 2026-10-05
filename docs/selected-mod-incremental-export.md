# LZY 全选择 INI 增量导出与部署

## 当前发布格式

用户确认的 14 对象已通过新的 `mod.ini + meshes/materials/textures` 写出和重建检查。旧实验的 `native-pack.json + native-payload.bin` 不再是当前导出格式；没有兼容读取或格式 fallback。

源构建目录：`E:\EIEM_Workspace\exports\LZY-selected-native-texture-ini`。

用户指定目录：`D:\Hypergryph Launcher\games\Endfield Game\plugin\mods\LZY`。部署前将上一版 37 文件的 INI 目录备份到 `E:/EIEM_Workspace/exports/LZY-before-texture-ini`，随后只发布经过验证的新资源；实际目录验收报告见下方“验证记录”。

新目录共 26 个文件、136,112,232 字节（约 129.81 MiB）：

| 内容 | 数量 |
| --- | ---: |
| mod.ini | 1（15,314 字节，447 行） |
| meshes/*.mesh | 13 |
| materials/*.mat | 5 |
| INI 中的 Texture 声明 | 11（不生成侧文件） |
| textures/*.tex | 7（共用像素载荷） |

旧 JSON 为 47,762 行、1,797,473 字节。诊断及来源候选不再发布，骨骼绑定放进 Mesh 二进制数据；INI 保存必要声明，包括完整可读 Texture 描述。离线 PNG 解码与 mip 生成保留，游戏侧只处理原生字段与资源引用。主体体积仍来自 RGBA32 完整 mip 像素，尚未进行原生压缩或内嵌核心性能优化。

**导出和离线重建已通过；当前游戏 DLL 不支持 format 2，游戏资源加载入口尚未接通。此次没有构建/部署 DLL，没有保存 Blender 工程或加载 RenderDoc。** 新格式 INI 不等于旧执行器已能加载它。

## 选中内容

沿用此前确认的作者快照。14 个选中对象包含 13 个几何部件与一条隐藏源组规则，共 232,070 个顶点和 1,022,919 个索引。

| 源组 | 几何部件 |
| --- | ---: |
| body_01 | 1 |
| cloth_01 | 1 |
| cloth_02 | 1 |
| cloth_03 | 5 |
| cloth_04 | 3 |
| cloth_05 | 隐藏规则，不带原几何 |
| cloth_06 | 2 |

body 的两个形态通道、100% 原生帧、形态范围 bounds 均保持此前验证结果；136,901 顶点的 cloth_04.lian 使用 32 位索引。13 个部件的 910 个骨骼槽保留其精确路径及顺序，不发布大体积来源候选。

INI 声明 5 个部件状态与 2 个形态状态、9 个按键输入、2 个形态控制。多个按键可以写同一个状态，Part 的单条显隐条件读取状态。两个按键正反切换同一变量的行为已用离线测试验证；游戏输入和实例绑定尚未接通。

## 增量与装配边界

INI 记录 7 组源 Mesh 到部件的映射及明确的 LOD 身份，不列 PFB 消费者名单。13 份几何输入用于 47 个 Mesh/LOD 重建。Material/Texture 声明基于原生模板来源，绑定到 Mod 部件，不隐式全局覆盖所有共享材质消费者。

包不含完整 Mesh SerializedFile、原模型、PFB、AssetBundle、原骨架或未选资源。Material 当前只有贴图编辑，因此 `.mat` 是类型化的空字段增量；具体贴图关系写在 INI，未改基础材质仍从当前源模板保留。

源逻辑路径与资产名用于身份解析。CAB/PathID 与 VFS 指纹只在工作区保留作为当前验证证据。加载器需要从当前原生资源输入重建合法对象/流/依赖，对命中的装配使用位置展开 Renderer；多个 Mod 修改不同 Mesh 时在同一当前输入上组合。同目标冲突仍须明确处理，多 Mod 合并尚未实现。

不同 LOD 的 UV descriptor 原生高位标志由代码继承当前目标。几何的 14 个关联字段全部明确覆盖，不因当前 LOD0 值相同而省略，也不发布策略开关。

完整 88 文件原源闭包和物化 PFB 只在离线验证目录。当前静态副本包含 60 个 Renderer 装配实例、32 个新增子 Renderer、5 个隐藏源 Renderer；它不能证明动态残影、冲刺、UI/NPC 新实例和自定义登记已经在游戏内生效。

## 验证记录

- 原生独立回读参考：`diagnostics/prefab-resource-validation-20261002/LZY-selected-native-verification-02.json`。
- 作者/回读贴图逐像素参考：`diagnostics/prefab-resource-validation-20261002/LZY-selected-native-pixels-final.json`。
- 新目录完整重建：`diagnostics/prefab-resource-validation-20261002/LZY-selected-texture-ini-verification.json`。
- 实际部署目录重建：`diagnostics/prefab-resource-validation-20261002/LZY-deployed-texture-ini-verification.json`。
- INI 采样参数到原生字段的对应验证：`diagnostics/prefab-resource-validation-20261002/LZY-texture-ini-settings-verification.json`。
- 部署文件哈希和备份：`diagnostics/prefab-resource-validation-20261002/LZY-texture-ini-deployment.json`。

重建校验包括所有 47 个 Mesh 的原生对象载荷哈希、作者顶点/索引计数、骨骼路径、形态目录、初始显隐/权重、各部件材质引用、11 个纹理目标的像素和原生字段、材质贴图属性引用。INI Texture 声明校验图像尺寸、mip 数、编码、色彩空间与采样范围；重建所有纹理描述后的原生载荷哈希与原独立验证结果一致。图像字节与前版完全一致。目录读取器同时拒绝缺失/重复引用、未定义状态、不一致的共享 cycle 状态集合、损坏资源、越界路径和未引用原始文件。

没有 DLL 构建或游戏内验收。新资源注册、实际加载时机、消费者类型适配、VFX 和 F10 原生重建仍待实现。

## 复现

输出路径必须不存在，原有证据不覆盖。`--delta-output` 保留为增量目录的 CLI 参数名，输出只有格式 2。

```powershell
python tools/nativepack/selected_experiment.py --capture E:/EIEM_Workspace/exports/LZY-author-validation-20261002/capture.json --preflight diagnostics/prefab-resource-validation-20261002/LZY-selected-author-preflight-final.json --schema diagnostics/prefab-resource-validation-20261002/native-schema.json --output E:/EIEM_Workspace/exports/LZY-selected-native-validation-texture-ini --delta-output E:/EIEM_Workspace/exports/LZY-selected-native-texture-ini --mod-id LZY

python tools/nativepack/verify_delta.py --package E:/EIEM_Workspace/exports/LZY-selected-native-texture-ini --native-report diagnostics/prefab-resource-validation-20261002/LZY-selected-native-verification-02.json --schema diagnostics/prefab-resource-validation-20261002/native-schema.json --output diagnostics/prefab-resource-validation-20261002/LZY-selected-texture-ini-verification.json
```

二进制布局、INI 语义、控制和注册边界见 [格式文档](mod-ini-resource-format-v2.md)。正式 Blender 内嵌入口继续按一次内存快照交给同一打包核心的设计推进，现有 CLI 输入仍是已验证的作者捕获。


## 纹理声明移入 INI

本轮只调整纹理描述的存放和读取，保持离线解码、mip 生成及 `.tex` 像素数据不变。之前 37 文件的 INI 导出另存备份到 `E:/EIEM_Workspace/exports/LZY-before-texture-ini`。新包没有 `.texture`；11 个 Texture 区块直接声明规格与采样。11 项格式/状态测试通过，其中新增可编辑采样参数、载荷与声明不一致、非法设置及旧侧文件拒绝的检查。47 个 Mesh/LOD、11 个纹理目标已从新目录重建，与原独立回读参考一致。
