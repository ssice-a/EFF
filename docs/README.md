# EFF 文档索引与当前状态

2026-10-06。生产源码已整理为 `eiem_resource_entry.cpp` 单入口，默认 v36，也可构建 v35 配套 DLL/编译器。旧运行时与其替换执行器已移除；构建只写本地输出。本轮经用户授权，把当前源码的 v35 DLL、编译器和协议文件部署至游戏目录，已完整编译安装的 Mod 集合。

历史 v35c 实机通过同路径身体贴图正向更新：世界实例提交 generation 2，用户确认青绿色贴图、动作、残影和头饰正常。请求到提交 919.54 ms，提交帧主线程约 63.97 ms。当前游戏配置已改为 `reload=F10`、`gui=INSERT`；修正版尚未启动游戏验收。多角色集合热重载、反向恢复、展示和完整装配的验收边界见[热重载测试](hot-reload-testing.md)。

最新待处理反馈为 body 残影消失，当前 PNG/TEX 已恢复原图；优先冷启动检查权重 0 与非零形态下的平 A，并收集已准备的捕获观察。历史青绿色会话通过不代表此反馈已修复，见[残影复查](../diagnostics/body-afterimage-review-20261006/README.md)。

## 当前能力与验证边界

| 能力 | 当前实现 | 验证边界 |
|---|---|---|
| 作者与离线输入 | 格式 2 独立资源；Blender 全部/分资源导出调用配套离线编译器 | 来源、对象图、hash 和原生字段已有离线证据；不等于所有资源变化的实机验收 |
| 首次替换 | 读取最新完整代际，沿游戏原加载入口提交保留原 CAB/Bundle 命名的输入；作者工具生成每个 Mod 的静态来源描述，编译器确认所有 Mesh 消费者 | LZY 已有样本；新的[静态来源发布](runtime-resource-discovery.md)已真实数据构建，实机验收另记 |
| 完整候选加载 | 隔离命名的 Bundle/PFB、闭包与 Mesh/Material/Texture 核验 | v35c 世界 owner 无改动与贴图正向更新通过；其他变化待测 |
| 装配身份 | v36 校验序列化 Renderer 地址、目标 marker 与逐 Mesh 骨骼；v35 使用 marker/骨骼校验 | v36 为源码及本地测试能力，未部署；0/1/N 完整迁移尚未验收 |
| 实例迁移 | 现有实现使用已记录的游戏世界/展示 owner 重建及后续创建路由 | 世界贴图正向更新通过；展示、隐藏、池和所有消费者覆盖未完成 |
| 按键、显隐与形态 | cycle/press/hold、表达式、Renderer 原生 enabled 仲裁、形态绑定与玩家数值持久化 | 历史按键/残影已有用户验收；新代际组合与反向重载仍需覆盖 |
| Ins 控制面板 | 内置 Windows 面板、所选 Mod 按键限制、按键按钮与形态滑块、跨线程版本校验 | 两 Mod 同键隔离及真实控件消息测试通过（前台观察注入）；已部署 v35，游戏 UI/鼠标仍待验收 |
| 缓存与多角色 Mod | 当前源码组合完整 0/1/N 作者集合；F10 提交增删/修改，Ins 只限制按键目标；集合缓存绑定 mods 目录 | 无对应 Mod 的角色保持原版；每个 Mod 自带来源描述，基线按需扩展；当前真实样本为 LZY；已部署 v35，实机待验收 |
| 旧代退休 | 卸载 Bundle 容器时保留存活资源引用，未知所有权隔离 | 不宣称完整 native job fence、persistent pool 或多代资源回收已验证 |
| Lua UI、Skeleton、Physics | 保留适配器及作者编解码 | 尚未接入当前生产宿主的完整运行链 |

完整目标以[渲染装配契约](full-render-assembly-reload.md)为准。单项贴图成功不能替代部件增删改名、不同骨骼槽、多实例回滚和旧代引用退休。

## 阅读顺序与文档职责

| 主题 | 入口 | 职责 |
|---|---|---|
| 项目术语 | [CONTEXT](../CONTEXT.md) | 作者、基线、代际、owner 和消费者边界 |
| 代码与构建 | [代码架构](code-architecture.md)、[源码地图](../src/README.md) | 当前入口、模块职责、v35/v36 配套构建及回归命令 |
| 实机进度 | [热重载测试](hot-reload-testing.md) | 已通过用例、安装版本与下一轮操作 |
| 完整目标 | [渲染装配热重载](full-render-assembly-reload.md) | 0/1/N、登记切换、回滚、未来实例及退休契约 |
| 离线包 | [包契约](offline-resource-package-format.md) | ABI、缓存发布、编译与原生加载 |
| 作者声明 | [INI 格式 2](mod-ini-resource-format-v2.md) | Replace/Part、资源、表达式与二进制格式 |
| 控制与偏好 | [资源输入按键](resource-input-key-controls.md) | 显隐、形态、数值状态及配置事务 |
| Blender 导出 | [分资源导出](blender-scoped-resource-export.md) | 全部/仅 Mesh/材质与贴图/贴图及运行中发布 |
| 源基线 | [导出基线](exporter-source-baseline.md) | pristine 来源与作者数据隔离 |
| 顶点通道 | [顶点数据契约](vertex-data-contract.md) | 原生通道、形变布局与切线生成 |
| 相机 | [反虚化契约](camera-fade.md) | CameraMono 原评估后的清理与验证边界 |
| 后续物理 | [三端设计](physics-authoring-design.md)、[作者 v5](physics-authoring-v5.md)、[源图 v2](physics-authoring-v2.md) | 目标及作者文件，不能代替运行时能力 |
| 仓库与工具 | [三仓库架构](repository-architecture.md)、[资源浏览器](../tools/README.md)、[Blender 插件](https://github.com/ssice-a/EFF-blender/blob/main/README.md) | 所有权、协议、开发与发布关系 |

## 维护规则

1. 明确区分已实现、构建通过、已部署、游戏验收四种状态；历史版本成功不自动归属当前源码。
2. 当前文档描述现有契约和边界；具体日志、哈希和失败尝试留在 `diagnostics/`。
3. 已退休代码和文档不再作为现行入口，源码清理记录不证明游戏能力。
4. 身份、装配及高频探针只在有明确问题和退出条件时启用；独立离线检查继续推进。
