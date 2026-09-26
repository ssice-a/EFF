# Physics 作者资源 v5

当前 Blender 导出、DLL 解析的作者格式是 `EIEPHYS` v5，purpose 为 `authoring`。旧作者 v1、v3、v4 不再读取，须重新导出。`EIEPHYS` v2 的 purpose 是原生源图，属于另一种文档类型；不能把它当作作者 v5 的旧版。

作者文档引用同包内的 EIESKEL v2，保存组、节点、球体或胶囊碰撞体、基础参数、节点半径曲线以及有限的 `serializeData` 原生参数。拓扑、运行结果字段与无效骨骼引用在导出或解析时拒绝。胶囊的 `span` 表示两个端球心间距；原生长度是 `span + radius + endRadius`。

Blender 的 `eiem_physics_document.py` 是作者侧校验与编码实现；DLL 的 `eiem_physics_document.h` 是运行时解析实现；AnimeStudio 的 `EiemPackageValidator.cs` 对包做结构检查。现有宿主测试覆盖读写、无效输入与提交失败恢复。原生 Physics runtime 默认关闭，作者格式可读不等于游戏模拟已验收。

历史实验与旧格式说明见[归档](archive/physics-authoring-history-20260927.md)。
