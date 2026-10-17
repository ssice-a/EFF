# 离线导出提速

2026-10-17 新增[Blender 全部资源重复导出优化与实测](blender-export-performance-20261017.md)：快照失效粒度、材质字段与磁盘图片解码复用，以及离线字节搜索。真实单顶点导出逐文件相同；几何变更的逐输入增量编译仍未实现。

2026-10-13。独立目标：在相同作者输入与消费者覆盖下减少完整冷导出耗时。保持编译输入视图后端与当前 compiled.bin 格式；不以缓存命中代替冷编译优化。不把游戏新增消费者发现计入本项完成状态。

## 已实现

- `ObjectFields` 只投影读取需要的字段，其他字段仍按 schema 与字节范围跳过。完整修改对象仍走解码/编码核验；投影结果禁止当作完整对象写回。
- 消费者匹配只读精确 Mesh/owner 引用；静态组件配对仍按 GameObject 的实际组件列表核验。
- PFB 根归属在单个 SerializedFile 内共享 Transform owner/parent/children 索引。每个根保持独立的遍历与 Renderer 集合；循环、重复子节点、父子不一致仍拒绝。
- 完整装配描述在同一成员内共享不可变 GraphData，根地址和组件序号分别构建。
- Renderer 骨架查询使用只读投影；实际修改或克隆节点才完整解码。
- 元数据阶段的原始/修改后 GraphData 直接生成 pristine 描述，后续证明与命名阶段复用同一成员 layout；巨大的 GraphData 在每个输入完成元数据后释放。
- 每来源证明独立编码/压缩，最多 4 个 worker 并行，按来源顺序拼接。完整产物格式与字节顺序不变。
- 装配字典以自身紧凑编码去重，省掉重复写出完整骨骼路径再算摘要的中间工作。去重按完整编码值判断，保留字符串、骨骼 palette 和装配的首次使用顺序。

没有本轮移除完整包 Encode/Decode 验证，也没有宣称已优化峰值内存。现有 8 个输入 worker 上限保留；后续根据实际内存数据调整工作调度，不减少覆盖。

## 验证

当前 Blender 中 Endminf Allure 的选择独立导出为 `bin/export-speed-20261013/Allure`，不写游戏 Mod、不保存 blend、不改选择。24 Mesh、7 Material、11 Texture、3 目标组，与此前历史 8 Material 的快照不同。

固定这份新作者快照，新旧编译器各从没有 compiled.bin 的目录运行，作者文件逐一核验不变。初轮为 17.78 → 16.22 秒；随后完整骨架投影轮为 27.88 → 25.16 秒。游戏启动及机器负载不同，跨轮绝对时间不可直接比较；Windows 文件缓存未清空。两轮每次新旧产物逐字节相同：60,947,816 字节，SHA256 `36CEE72570B2A1C29F700940A99E8AC393D54AC3A42E5276426954BD2905FBAD`。

第二轮峰值 private bytes 为 3,362,902,016 → 3,396,411,392，未降低；不能宣称完成内存优化。Typhoea 独立冷编译为 5.16 → 4.74 秒，产物逐字节相同。精确数字以 diagnostics/export-speed-*.json 为准。

模拟回归覆盖同包多根、静态/蒙皮、0/1/N 部件、RectTransform、LOD/helper、非法归属、重复子节点及截断投影；集合组合回归覆盖共享来源。Blender 离线提交接口的 12 项测试通过。构建、安装、真实 Blender 导出分别记录；这些不证明热更新适配已接入。

反向顺序对照（优化版先跑）为 25.44 秒与原版 27.49 秒，产物仍逐字节相同。当前 Blender 完整隔离导出由 38.86 → 34.53 秒，作者/贴图/INI/来源清单/compiled.bin 所有输出逐文件相同，选择与活动对象保持，未保存工程。机器负载和 Windows 文件缓存未控制；这是本机真实样本，不代表所有 Mod 的固定百分比收益。

上一轮配套编译器安装 SHA256 为 `899D46694026D2E95F45B851EF5C4529AF1B88DF68CE84F1FF04A849A5EC7AE5`，旧 EXE 保存于 bin/export-speed-20261013。没有重新发布游戏里的 Mod。

隔离的 Typhoea 材质参数修改仍走现有增量分支（约 0.80 秒）；50 个 native view 与完整冷编译相同，空原生贴图槽绑定回归通过。该测试修改的是临时副本。

## 后续本轮对照

仍固定同一 Allure 作者快照，150 个输入，没有 compiled.bin，每轮作者文件保持不变，最终输出继续逐字节等于上述 SHA256。GraphData/layout 复用的同轮对照为 28.981 → 28.089 秒；证明并行的独立同轮对照为 14.955 → 14.089 秒。两轮机器负载不同，不能把 29 → 14 秒当作优化收益。

紧凑装配字典的同轮对照为 26.663 → 23.700 秒。`compiled-encode-assemblies` 为 4,623 → 2,528 ms，该旧计时段还包含 portable 清单复制、清单编码与压缩，不能全部算作装配字典本身耗时。反向顺序测量完整本轮改动（新版先运行）为新版 23.928 秒、原版 28.535 秒；这是本机样本约 16% 的冷编译改善。峰值 private bytes 为原版 3,365,687,296、新版 3,390,943,232，没有降低峰值内存。

Typhoea 的独立冷编译为 2.683 → 2.523 秒；5,840,912 字节，SHA256 `237980F05DB93F7E04E1FA6181DE76304B7A6BB864F6B88CAAF6DD639B441ED4`，逐字节一致。峰值 private bytes 为 1,166,499,840 → 1,248,956,416，后续还需单独优化内存。Windows 文件缓存未清空，不宣称所有 Mod 都有固定比例收益。

报告位于 diagnostics/export-speed-analysis-pair-20261013.json、export-speed-proof-parallel-20261013.json、export-speed-assembly-compact-20261013.json、export-speed-analysis-final-reverse-20261013.json 和 export-speed-analysis-typhoea-20261013.json。固定字典回归覆盖重复/共享骨骼、空 palette、顺序变化、Renderer 标志、helper 序号及浮点负零，校验旧编码的固定字节摘要并解码核对全部装配。真实单文件存储与材质增量分支也通过回归。

本轮编译器已备份安装至 tools/Blender/nativepack/eff_resource_pack.exe；当前 Blender 插件目录是指向 tools/Blender 的 Junction，两端实际使用同一文件。SHA256 `4E65AE922EE9259C6C70684E24E27A532DBC61E657B9E0B50322D7940301BD03`。备份位于 bin/export-speed-20261013/compiler-analysis-reuse/blender-installed-before-current.exe。实时 Blender 来源/协议查询确认下一次导出选择此编译器，无需重启。共享头文件的生产 DLL 构建、PE 校验与隔离加载通过；没有安装游戏 DLL、重新发布 Allure 或保存用户工程。

## 后续独立改进

1. 继续优化 Renderer 层级/部件构建和清单复制，先把相应墙钟计时独立出来，避免重复分析与巨大的临时副本。
2. 逐项替换中间整包生成和回读，要求最终输入视图及隔离输入语义相同。
3. 以测得的阶段内存控制并发和释放时机，记录完整墙钟与峰值，不相加 worker 时间当墙钟。

每项改动通过对应回归后再扩大测试；完整覆盖与产物等价是发布前条件。
