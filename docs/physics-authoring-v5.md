# Physics 作者资源 v5

当前保留的 Blender 作者编解码与 C++ reader 格式是 `EFFPHYS` v5，purpose 为 `authoring`。旧作者 v1、v3、v4 不再读取，须重新导出。`EFFPHYS` v2 的 purpose 是原生源图，属于另一种文档类型；不能把它当作作者 v5 的旧版。

作者文档引用同包内的 EFFSKEL v2，保存组、节点、球体或胶囊碰撞体、基础参数、节点半径曲线以及有限的 `serializeData` 原生参数。拓扑、运行结果字段与无效骨骼引用在导出或解析时拒绝。胶囊的 `span` 表示两个端球心间距；原生长度是 `span + radius + endRadius`。

Blender 的 `eiem_physics_document.py` 是作者侧校验与编码实现；`src/eiem_physics_document.h` 是纯 C++ reader；AnimeStudio 的 `EiemPackageValidator.cs` 对包做结构检查。作者测试覆盖读写及无效输入。旧 Physics runtime 已退休，reader 尚未接入当前生产入口；作者格式可读不等于游戏模拟已实现或验收。

后续游戏接入要求见[三端设计契约](physics-authoring-design.md)，当前范围见[文档索引](README.md)。
