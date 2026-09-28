# EFF 领域模型

现行文档、工作区状态和阅读顺序：[文档索引](docs/README.md)。
历史实验及部署记录统一放在 `docs/archive/`，不作为当前实现说明。

## 核心对象

- **Resource（资源）**：离线 Mesh、Material、Texture、Skeleton 及 Physics 作者数据；与实例状态分开。
- **Render action（渲染操作）**：命中原 Mesh 消费者，组织 mesh、material、skip 和 submesh 显隐等动作。
- **Model instance（模型实例）**：游戏中的模型根，拥有独立 Transform、骨架、Renderer 和生命周期。
- **Observed instance（已观察实例）**：生命周期入口发现的模型弱引用，独立于 Mod 开关，F10 用于重新匹配。
- **Prefab（PFB）**：原游戏序列化对象图，提供资源关系和实例来源；不限制 Render 的 Mesh 消费者范围，也不替代 Mesh 资源身份。
- **Mod program**：解析后的资源声明、Render 动作和关系索引，不包含 Unity 对象。
- **Update request**：Reconcile 重试、Reapply 状态重应用、Reload 磁盘重载；只在 Unity 线程修改对象。

## 现行约束

- 一个资源的所有匹配消费者均受规则影响；mesh、material、skip 和 submesh 显隐各自有明确语义。
- `EntityRenderHelper` 遍历中的 `RendererInfo._Init` 只完成游戏的干净源材质采样，完整 helper
  返回后才统一提交资源；不经过 helper 的直接 NPC/UI Renderer 在独立 `_Init` 完成后进入同一个
  Mesh 身份执行器。入口可以不同，替换实现不能分叉。
- 同名 Mesh 不证明来源相同；不能以缺省骨骼索引、猜测 PFB 名称或吞错掩盖缺失依赖。
- Blender 只导出显式选择的网格。对象相机关表示游戏隐藏；眼睛/显示器只影响编辑预览。
- 源骨骼槽、bind pose 和有效顶点通道保留。EFFMESH v6 的每个槽按“源 Mesh 身份 + 原始槽号”候选
  从当前模型实例的原生 `bones[]` 解析；不能按名称或层级索引猜测，也不能跨实例借 Transform。
- state.ini 只保存仍声明的玩家数值，不保存动作、Unity 对象或模拟状态。
- Mod UI 的触发、窗口和布局属于 INI/Lua；DLL 提供宿主与接口，没有默认 Mod UI 快捷键。
- 物理使用游戏原生系统。Physics 作者文件不等于原生配置完整往返，也不等于已实现游戏模拟。
- 骨链构建序、模拟槽、Animator 写回槽和 Mesh palette 分别映射，不能共用数字索引。
- 函数返回、跟踪归零和无丢日志不能证明 Task/Job/动画写回已退出，组合使用也不能替代退出契约。
- 生命周期缺少实测证据或明确 completion fence 时标记“尚未验证”；仅阻塞依赖该结论的操作，不阻塞独立的源码实现、编译、静态分析和测试。
- 隐藏 Mesh 不等于删除物理骨骼；新增节点必须保留到消费者和原生工作不再引用它们。
- 自动测试、构建、部署和游戏验收分别记录。源码中的版本字符串不等于已构建或已安装版本。

物理接入的历史证据和未完成条件见[原生调查归档](docs/physics-runtime-redesign.md)，计划不作为验证结果；
完整作者链路要求见[三端设计契约](docs/physics-authoring-design.md)。
