# EFF v2.0.2

本次为 Format 3 架构重构，配套 EFF Blender v0.50.0、AnimeStudio v1.9.0、EFF Mod Manager v0.2.0。

- DLL 只接受 Format 3 自描述资源目录；旧格式转换统一由 Mod Manager 离线完成，运行时不增加旧格式兼容分支。
- 改为按游戏请求准备原生资源输入，复用当前 CAB/path ID、依赖与反序列化流程；无需在 Mod 内携带 compiled.bin 或 source-inputs.bin。
- Mesh、材质、贴图、物理、特效、控制资源独立保存；公共来源索引与作者工具使用共享位置和 SHA256 校验。
- 修复变身、大招换模、回收和跨场景后切换组丢失状态：游戏原生显隐意图与作者过滤后的实际显隐分别管理，UI 与大世界共用控制值。
- 修复特效挂载 Part 标识、原生材质外部 CAB 的直接依赖解析、材质属性移除与热重载生命周期。
- 修复按需加载下特效挂载骨骼表未发布的问题；lx 特效与大招换模后的按键切换均已由用户实机确认。
- Blender 导出只保留实际使用的骨骼槽位；新增可撤销的空顶点组清理，保留所有正权重。

## 安装与迁移

退出游戏后，将 EFF_v2.0.2_dll.zip 解压到游戏根目录。保留个人 plugin/eff.ini、plugin/mods 与 state.ini。旧 Mod 先用 EFF Mod Manager v0.2.0 迁移为 Format 3，或用配套 Blender 重新导出。共享作者工具供离线作者与迁移工具使用，无需安装到游戏目录。

## 验证

按键切换问题已由用户在游戏中确认修复。发布工作流继续验证依赖图、材质移除、模型显隐生命周期、热重载事务、DLL 加载、索引更新和共享工具收据。资源权重修复属于本地 Mod 资产修改，不包含用户 Mod 或工程到公开发布包。

- Fixed the standalone migration package missing NumPy; v2.0.1 did not reach publication.

## English

Format 3 is the only runtime format. Legacy conversion runs offline in Mod Manager. Demand-driven native inputs retain game-owned deserialization and bounded dependency discovery. Fixes native visibility snapshots after transformation, effect Part bindings, direct material dependencies and reload lifecycle handling. The Blender exporter compacts unused skin palette entries and provides undoable empty-group cleanup. Preserve personal settings and migrate older Mods before upgrading.
