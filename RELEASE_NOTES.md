# EFF v1.8.1

配套：[EFF Blender v0.45.0](https://github.com/ssice-a/EFF-blender/releases/tag/v0.45.0)、[AnimeStudio v1.7.0](https://github.com/ssice-a/AnimeStudio/releases/tag/v1.7.0)。

- 减少重复资源加载：编译结果保留实际修改、Prefab 根和必要消费者，未改的末端依赖继续由游戏加载；多个 Mod 仍按同一事务组合，不减少 PFB 命中覆盖。
- 修复 F10 的 `reload model part load ownership was not recorded`：观察游戏内部主模型加载流程，正确保留组件与部件归属。覆盖加载中释放及新建部件，原函数只调用一次。实机同一会话七次提交成功、无拒绝；未覆盖场景仍需反馈。
- 来源按逻辑 Bundle 路径和实际内容重新定位，不再把旧 CHK 文件名、偏移当作内容身份。相同内容移动位置可继续使用；真实内容/结构变化仍需安全离线适配。
- Blender 全部资源重复导出减少未改 Mesh、材质及贴图重复处理。游戏小更新后自动增量刷新工作区索引并继续导出，无需打开 AnimeStudio；不在游戏内后台编译或回写 Mod。
- EFF Repair v0.2.0 按逻辑内容复用索引及修复记录，位置或索引时间变化不重复修复。附当前完整公共索引，支持跨机器下载导入。

## 下载与安装

- `EFF_v1.8.1_dll.zip`：退出游戏后解压到游戏根目录，保留个人 `plugin/eff.ini`、Mods 和 `state.ini`。只含生产 DLL、代理和默认配置，不含作者工具或诊断探针；静态链接 C/C++ 运行实现，当前代理支持 `C:\Windows`。
- `EFF-Repair-v0.2.0-win-x64.zip`：独立游戏外修复工具，需要 .NET 9 Windows Desktop Runtime。退出游戏后修复，保留原包备份。
- `EFF-Endfield-Index-20261017.effindex.gz`：修复工具自动下载的完整索引；不含 Mesh/贴图或作者 Mod，解压约 831 MB。旧内容对应索引仍可从先前版本取得。

无需仅为升级 DLL 重新导出全部 Mod。若具体来源内容发生变化，作者可用新版 Blender 导出，玩家可使用修复工具。游戏小更新不等于所有旧资源失效，也不代表可以跳过实际内容校验。

本机固定十网格 ALL 导出：单顶点修改 11.32 → 10.23 秒，不变重导 5.76 → 5.08 秒，输出逐文件一致。这是本机样本，不承诺固定提速比例。只读依赖裁剪没有证明持续掉帧或峰值内存问题全部解决；几何变更仍走完整离线编译。

## English

Reduces duplicated native inputs while retaining edited resources, consumers and Prefab coverage. Fixes F10 ownership tracking across the game's internal main-model load/release sequence. Logical Bundle identity and content proofs tolerate CHK relocation without accepting changed bytes. Blender can refresh stale indexes incrementally and resume the same export. EFF Repair v0.2.0 and a current portable full index are included. No background in-game compilation is introduced. Preserve personal configuration and Mods; close the game before installing or repairing.
