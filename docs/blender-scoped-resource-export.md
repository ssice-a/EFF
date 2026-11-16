# Blender 全量与选中导出

正常作者路径直接生成 Format 3 `mod.ini` 和独立资源，不再以离线 `compiled.bin` 作为新 Mod 输出。选中导出只发布当前选择的作者内容及其声明依赖；具体覆盖、Part/目标和已知 LOD 行为以当前插件界面及 [目标重定向](weapon-retargeting.md) 为准。

导出前核对选择、渲染隐藏状态、Armature 修改器、替换目标与“替换全部已知 LOD”。单独导出通过不代表多 Part/LOD、动画、物理和特效组合已经验收。用户测试用 Mod 的覆盖须有明确授权，不能把本地隔离导出当作游戏已安装。

资源组织和复用见 [INI 装配](format3-ini-assembly.md)；实际原生来源及特效字段见 [当前特效面板](effect-native-structure-20261112.md)。历史离线性能/来源刷新记录保存在 [旧文档快照](archive/snapshots/blender-scoped-resource-export-before-20261112.md)。
