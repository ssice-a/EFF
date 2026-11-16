# EFF 四仓库边界

EFF、Blender、AnimeStudio、Mod Manager 是四个独立源码仓库，通过文件格式和共享工具接口协作。它们不共享运行时 Unity 对象或进程生命周期。

| 工作树 | 职责 | 当前交付 |
| --- | --- | --- |
| EFF 根仓库 | 游戏运行时、格式契约、原生作者工具及跨端回归 | `eff.dll`、协议、共享工具包 |
| `tools/Blender` | 导入、作者可视编辑、资源导出与往返 | Format 3 Mod、插件 continuous 更新包 |
| `tools/AnimeStudio` | 游戏 VFS/索引、资源及原生关系导出 | 源作者目录、索引和特效来源 |
| `EFF-Mod-Manager` | 安装、启停、修复/迁移与软件更新 | 独立 Windows 程序，附带所需读取/校验依赖 |

## 数据方向

```text
游戏 VFS → AnimeStudio 来源/索引 → Blender 编辑 → mod.ini + 独立资源
                                                  ↓
游戏真实请求 → EFF 按逻辑目标命中 → 准备原生输入 → 游戏反序列化/实例生命周期

Mod Manager → 安装与管理 Mod；按需校验 Format 3、迁移旧 Format 2
```

新输出不要求 `compiled.bin` 或离线消费者缓存。DDS 保存像素，INI 声明位置、逻辑来源、装配与控制；Mesh/物理/动画样本/原生特效保持各自的数据职责。具体以 [ADR-0002](adr/0002-runtime-resource-files.md) 为准。

Manager 在构建时链接固定 AnimeStudio 核心并打包原生依赖，玩家无需安装 AnimeStudio 或 Blender。旧格式迁移仍需要兼容迁移工具集；索引可来自本地、手动包、下载或本地构建，不等于任何缺失作者资源都可修复。

## 版本与部署

三个子目录是 Git submodule，提交分别属于自己的仓库；主仓库 gitlink 只固定提交，不能代表子仓库未提交改动。所有开发留在各自 `main`；先验证受影响源码并部署固定本机目标，再按授权提交/推送。发布只上传本机已核验产物，见 [发布管理](release-management.md)。

旧三仓库/Format 2 说明保存在 [历史快照](archive/snapshots/repository-architecture-before-20261112.md)。
