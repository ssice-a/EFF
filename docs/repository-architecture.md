# EIEM 三仓库边界

EIEM 的运行时、Blender 作者工具和资源解包器是三个独立产品，分别发布、回滚和迭代。
它们通过版本化文件格式通信，不共享进程、Unity 对象或生命周期状态。

| 仓库 | 唯一职责 | 当前输出 |
|---|---|---|
| [ssice-a/EIEM](https://github.com/ssice-a/EIEM) | 游戏内 DLL、INI/Lua 运行时、EIEM 格式规范、跨端契约测试 | DLL 发布包；读取 EIEMESH v6 |
| [ssice-a/EIEM-blender](https://github.com/ssice-a/EIEM-blender) | Blender 导入、编辑、LOD/材质/切换作者数据和 Mod 导出 | EIEMESH v6 与 `mod.ini` |
| [ssice-a/AnimeStudio](https://github.com/ssice-a/AnimeStudio) | Endfield VFS 索引、原始资源与 PFB 关系提取、源作者数据导出 | EIEMESH v6、EIESKEL v2、Material、Texture、Physics 源图 |

## 依赖方向

```text
游戏 VFS
  -> AnimeStudio 源包（Mesh v6、Skeleton v2）
      -> Blender 编辑与 Mod 包（Mesh v6）
          -> EIEM DLL 解析并应用到当前游戏实例
```

AnimeStudio 不生成运行时指针或控制 Renderer 生命周期。Blender 不解包游戏 VFS，也不猜测
PFB 实例。DLL 不重建作者数据；它只消费完整、已验证的资源和动作声明。

## 协议所有权

规范和兼容矩阵放在 EIEM 的 `docs/`，C++ reader 与跨端集成测试也由 EIEM 维护。
生产端实现分别留在各自仓库。格式变更按以下顺序完成：

1. 在 EIEM 文档和 reader 测试中定义当前版本与旧版拒绝条件。
2. 在 AnimeStudio 或 EIEM-blender 实现对应 writer，并用真实 reader 回归。
3. 先在工具仓库分别提交和验证，再更新 EIEM 中的 submodule 固定提交；需要发布时三仓库各自发布。

当前无需建立第四个“格式仓库”。协议仍同时依赖游戏内 C++ reader 和两个生产端，过早再拆一层
只会增加版本协调成本。如果未来出现第三方独立消费者，再把纯格式 fixture/schema 抽为独立包。

## Submodule 的含义

两个目录都是独立 Git 仓库的 submodule，不会把源码历史并入 EIEM。三个开发工作树都位于同一个文件树：

- `tools/Blender` 直接作为 Blender 的日常开发工作树，修改后先在该目录提交到 EIEM-blender，再更新 EIEM 的 gitlink。
- `tools/AnimeStudio` 直接作为 AnimeStudio 的日常开发工作树，修改后先在该目录提交到 AnimeStudio，再更新 EIEM 的 gitlink。原先单独放在 `E:\vscode\AnimeStudio` 的源码已同步到此处。
- `E:\vscode\blender` 是另一套 Blender 插件，不是 EIEM-blender；EIEM 的 Blender 代码只维护 `tools/Blender`。

克隆后执行：

```powershell
git submodule update --init --recursive
```

提交顺序：AnimeStudio 和 EIEM-blender 各自提交；EIEM 更新两个 gitlink 并提交。任何一端仍有未提交修改时，父仓库的 gitlink 不能表达完整源码版本。三仓库的 Release 独立创建；EIEM 的 Release workflow 只发布 DLL，不把工具产物混入同一版本号。本轮清理仍在本地验证，没有新增发布。详见[本地项目管理状态](project-management-20260926.md)。
