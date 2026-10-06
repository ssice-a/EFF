# EFF 三仓库边界

EFF 的运行时、Blender 作者工具和资源解包器是三个独立产品，分别发布、回滚和迭代。
它们通过明确字段结构的文件格式通信，兼容性由实际结构决定，不共享进程、Unity 对象或生命周期状态。

| 仓库 | 唯一职责 | 当前输出 |
|---|---|---|
| [ssice-a/EFF](https://github.com/ssice-a/EFF) | 游戏内资源入口、离线编译器、格式规范、跨端契约测试 | 配套 DLL/编译器；格式 2 原生输入与 v35/v36 清单 |
| [ssice-a/EFF-blender](https://github.com/ssice-a/EFF-blender) | Blender 导入、编辑、LOD/材质/切换作者数据和 Mod 导出 | 源交换 reader；`mod.ini format=2` 与独立 `.mesh/.mat/.tex` |
| [ssice-a/AnimeStudio](https://github.com/ssice-a/AnimeStudio) | Endfield VFS 索引、原始资源与 PFB 关系提取、源作者数据导出 | EFFMESH v6、EFFSKEL v2、Material、Texture、Physics 源图 |

## 依赖方向

```text
游戏 VFS
  -> AnimeStudio 源包（Mesh v6、Skeleton v2）
      -> Blender 编辑与格式 2 作者包
          -> EFF 配套离线编译器 + pristine 游戏基线
              -> 原生加载输入与完整清单
                  -> EFF DLL 加载、核验并迁移已适配实例
```

AnimeStudio 不生成运行时指针或控制 Renderer 生命周期。Blender 不解包游戏 VFS，也不猜测
PFB 实例。基础资源在外部编译，DLL 消费完整候选及控制图，不在帧内重建作者几何/贴图。Lua UI 与 Skeleton/Physics 作者 reader 保留，但尚未接入当前生产宿主完整运行链。

## 协议所有权

规范和兼容矩阵放在 EFF 的 `docs/`，C++ reader 与跨端集成测试也由 EFF 维护。
生产端实现分别留在各自仓库。格式变更按以下顺序完成：

1. 在 EFF 文档和 reader 测试中定义字段结构、编码和不兼容结构的拒绝条件，版本标签不作为门槛。
2. 在 AnimeStudio 或 EFF-blender 实现对应 writer，并用真实 reader 回归。
3. 先在工具仓库分别提交和验证，再更新 EFF 中的 submodule 固定提交；需要发布时三仓库各自发布。

当前无需建立第四个“格式仓库”。协议仍同时依赖游戏内 C++ reader 和两个生产端，过早再拆一层
只会增加版本协调成本。如果未来出现第三方独立消费者，再把纯格式 fixture/schema 抽为独立包。

## Submodule 的含义

两个目录都是独立 Git 仓库的 submodule，不会把源码历史并入 EFF。三个开发工作树都位于同一个文件树：

- `tools/Blender` 直接作为 Blender 的日常开发工作树，修改后先在该目录提交到 EFF-blender，再更新 EFF 的 gitlink。
- `tools/AnimeStudio` 直接作为 AnimeStudio 的日常开发工作树，修改后先在该目录提交到 AnimeStudio，再更新 EFF 的 gitlink。原先单独放在 `E:\vscode\AnimeStudio` 的源码已同步到此处。
- `E:\vscode\blender` 是另一套 Blender 插件，不是 EFF-blender；EFF 的 Blender 代码只维护 `tools/Blender`。

克隆后执行：

```powershell
git submodule update --init --recursive
```

提交顺序：AnimeStudio 和 EFF-blender 各自提交；EFF 更新两个 gitlink 并提交。任何一端仍有未提交修改时，父仓库的 gitlink 不能表达完整源码版本。三仓库的 Release 独立创建；EFF 的 workflow 打包配套 DLL、编译器、协议、代理与配置，不混入 Blender/AnimeStudio 产品包。本轮清理仍在本地验证，没有新增发布；当前入口及检查见[代码架构](code-architecture.md)。
