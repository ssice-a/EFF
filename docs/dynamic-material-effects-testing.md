# 动态材质与特效验证

2026-10-19 本地开发版。普通 eff.dll 统一执行材质与特效，Blender 编辑/预览/构建/导出。

## 材质

选作者 Mesh/材质，在原 EFF 材质面板配置来源 Shader 功能/关键字/贴图。材质属性 → EFF 动态材质参数 → 添加材质参数动画，从已有数值/颜色/向量选择参数，对动画值插帧；配置范围、循环、初始播放和倍率。颜色/向量四分量需各自关键帧。透明模式、关键字、贴图引用不作为连续动画。

全部资源或仅材质与贴图导出更新选中材质动画，其他部分导出保留。游戏 Ins 支持播放/暂停/停止和倍率。每 Renderer 第 0 槽 MaterialPropertyBlock 保留游戏读写基线，不改共享 Material，停止/删除/模型释放恢复最新基线。默认 60Hz 离线采样，共用运行时内核，不运行 Blender 驱动器/NLA。暂停/常量采样不重复写入。

_EmissionBrightness 的 1 → 8 → 1 可验证，视觉依赖实际 Shader 分支；_SpecialDissolveScheduleOffset 是原有遮罩阈值，不保证任意材质产生循环亮带。

## 特效

完整操作见[空物体编辑说明](../tools/Blender/docs/effects-editor.md)。导入 effect.json 后根 Empty 的对象属性直接展开模块/数组/曲线。可独立 type=effect Mod 或随人物导出，挂指定骨骼，跟随 Renderer.enabled/activeInHierarchy。事件触发暂不支持。

游戏 F9 从磁盘读取特效，不重建人物；快捷键为 plugin/eff.ini 的 [Hotkeys] effects_reload，改后 F10 重读。声明/删除是完整快照，后续模型事件不会复活已删特效；成功准备后替换旧副本。无需 PID、实例列表或调试 DLL。

预览使用实际 Mesh/UV/PNG；基础运动、颜色/透明度、范围/图集/跟随可见。Noise/Shader 近似，复杂模块提示未还原。2026-10-20 已增加用户 Mesh 与二维图片，通过来源 schema 克隆独立 Mesh/Texture2D/Material；完整新增发射器和任意 Shader/依赖类型仍未开放。操作见空物体编辑说明的自定义 Mesh 章节。

## 验证边界

Python 编解码/部分导出/摘要拒绝、独立发布/state.ini 保留通过。后台 Blender 检验类型、模块/数组、实时刷新、世界出生变换、保存/重开与三轮注册，保留材质 Action/slot 采样回归。三份真实光点/花瓣来源分别做未修改和修改重建。Endmin body 的 44 骨骼槽/完整头骨挂点通过；独立包通过 Python → C++ → 编译作者结构往返。

C++ 覆盖材质隔离、原生失败保留/换代、Mesh 可见性、F9 增删/改挂点、无人物 Mod 的原 Mesh 挂载。日志为 bin/effects-rework-*；普通构建不启动 opt-in 管道。示例/测试包在 E:/EIEM_Workspace/effects/petals-lightpoints-20261019/rework-review，测试 Mod 不安装游戏。

自动测试/部署不代表实机画面验收；脚本、音频、复杂池/长期生命周期和端到端性能仍需游戏确认，不承诺毫秒级。实际安装/备份摘要见 bin/effects-rework-deploy-*/deployment.json。
