# 源 Mesh 命中与资源加载流程

## 离线确定目标

1. AnimeStudio 导出源包：manifest 保留准确路径、CAB/PathID、字段模板和引用身份；作者工作区保留已有资产索引。
2. `mod.ini` 的 Replace 指定源 Mesh 身份，Part 指定作者 Mesh/材质。材质和贴图来源用于模板和构建依赖，不扩大替换目标。
3. Blender 自带的来源工具复用现有索引，读取相关原始包范围，查找所有精确 Mesh 消费者。蒙皮 Renderer 直接引用 Mesh；静态 MeshFilter 通过同 GameObject 的 MeshRenderer 定位。所有匹配 PFB/Renderer 写入该 Mod 的 `source-inputs.bin`。不解析/保存全游戏原包，也不新增全局资源索引。
4. Blender obtains the shared EFF author compiler and compiles this Mod into `compiled.bin` object edits and copy/relocation rules. The verified tools are cached below the user-selected resource root. One source Renderer may expand to 0/1/N author Parts; undeclared Mesh consumers are unchanged and material templates are cloned into independent author objects.
5. 暂存资源与编译结果全部完成后发布 Mod。来源声明未变化时复用来源描述；不读取当前游戏画面或扫描场景 Renderer。

## 运行时使用结果

DLL 在启动和 F10 读取完整 Mod 目录，组合各独立编译结果。游戏请求已知文件范围时，DLL 读取 pristine 字节、应用规则，在 Unity 创建资源和 Prefab 实例前提供候选输入。共用 PFB 的已编译对象记录与列表修改合并，冲突拒绝；没有再次资源编译或动态猜测目标。

F10 在后台准备隔离候选，Unity 线程加载依赖闭包并验证 Prefab，再对已登记的模型 owner 调用游戏自己的释放/加载与动画/武器/音频重绑定。世界/展示归属从加载回调及实际组件/路径确认。UIModelLoader 单独登记准确实例；其完成回调仅恢复一次协程，不能重放为页面刷新。候选通过原 LoadModelAsync 创建并跨帧等待，保留旧人物；原生完成后，通过实际 PhaseCharItem 的 BindGameObject 接管新根，恢复当前层、位置、显隐、动画和附属资源，再卸载旧根。缺少实际页面绑定、失败或取消时保留原显示；UI 实机验收仍未通过。新角色即使尚未出现也有离线目标，之后正常加载时使用当前规则。

增删改及空集合走同一完整集合流程。没有 Mod 的目标使用原来源。删除时保留上一集合的来源证明以生成 pristine 候选，后续集合不会无限积累已退休来源。监视器只准备候选，F10 才提交代际。更新 DLL 必须重启游戏。

## 文件与限制

玩家分发作者资源、INI 与 compiled.bin；source-inputs.bin 是来源准备的派生描述。索引、原游戏快照、Blender 工具和 AnimeStudio DLL 不进入游戏运行包。实测 LZY 原作者目录 131.17 MiB，编译规则 11.25 MiB，合计 142.42 MiB；没有保存完整原包。兼容性以数据结构、引用及来源摘要核验。

静态 Mesh 资源支持不自动扩大 F10 的生命周期覆盖。独立场景物件与人群 NPC 仍需对应游戏 owner 的重建/状态保持入口；未知活动 owner 拒绝刷新并保留旧代际。跨文件根、persistent pool 与完整多代资源退休仍有验收边界。详见[输出契约](offline-resource-package-format.md)和[当前测试](hot-reload-testing.md)。
