# 当前 LZY Mod 的作者输入验证

日期：2026-10-02。使用正在打开的 `G:\zmd\lzy\1.blend`，按用户确认的现有 `selected_eiem_resources()` 规则取得 **14 个 Mesh 对象**。没有改选择规则、保存或覆盖 `.blend`，没有部署游戏 DLL。

**结论：这份现有 Mod 可以提供新打包方案所需的作者输入，不需要重做模型。** 本轮通过的是作者数据、当前源资源身份、骨骼和依赖的离线预检。2026-10-03 后续已完成 cloth_06 两部件的原生写出、Renderer 扩展和独立离线回读，见 [原生写出验证](cloth06-native-write-validation.md)。游戏资源注册及加载、运行时 VFX 和 F10 仍未通过实机验证；不能把离线成功称为新 Mod 包已可运行。

## Blender 内部格式

Blender 中仍然使用普通 Mesh、材质、贴图、顶点组、骨架和形态键。现有作者二进制 EFFMESH v6 / EFFMAT v1 可用于此次验证，无须因游戏加载方式改变而推翻编辑数据格式。

Blender 导入/准备层后续需要接入新版来源清单，并持久保存来源绑定和稳定部件 ID；打包输出层需要产生游戏兼容的原生序列化资源及装配/控制清单。二者不是同一格式。本轮没有改或安装新的 Blender 导入器、导出器或内嵌库。

## 这份选择的实际结果

| 来源 Mesh 家族 | 非隐藏作者部件 | 导出骨骼槽数 | 实际基础材质 |
|---|---:|---|---|
| body_01 | 1 | 84 | body_01 |
| cloth_01 | 1 | 86 | cloth_01 |
| cloth_02 | 1 | 117 | cloth_02 |
| cloth_03 | 5 | 81 / 84 / 81 / 90 / 90 | cloth_03 |
| cloth_04 | 3 | 42 / 42 / 27 | cloth_04 |
| cloth_05 | 0，按现有隐藏规则处理 | 不导出几何 | 不引入作者材质依赖 |
| cloth_06 | 2 | 43 / 43 | **cloth_02** |

合计：14 个选中对象，7 个来源组，1 个隐藏对象，13 个独立单材质部件。此次快照中 cloth_02 是启用状态；以捕获的选择和隐藏声明为准，不以旧的 `mod.ini` 推定当前状态。

13 份未合并的验证几何共有 **232,070 个输出顶点、1,022,919 个索引**。输出顶点包括 UV/法线接缝展开，因此不应拿 Blender 原始顶点数要求相等。`cloth_04.lian` 的输出为 136,901 个顶点、592,704 个索引，需要 32 位索引，不能截成 16 位。

5 组按键部件显隐、2 个 body 形态键及其控制声明均保留。形态帧权重为 100，作者控制范围为 0–1；原生控制绑定需要明确转换，不能直接把 0–1 当成 0–100。新的形态键通过作者数据检查，不等于游戏已接受其原生载荷。

## 已完成的检查

1. 逐份完整读取 13 份 EFFMESH v6，检查魔数、版本、完整消费文件、哈希、坐标约定、数组长度、有限值、三角形/索引范围、单材质 submesh、UV 维度和形态通道/帧/顶点范围。
2. 所有来源 Mesh 通过**逻辑资源路径 + 子资产身份**解析到新 EFFSOURCE 清单中的确切 CAB/PathID，再核对原生对象表的类型及字节范围。CAB/PathID 是本轮证据，不是跨版本硬编码规则。
3. 骨骼槽允许不同于源 Mesh。body 从原来的 42 槽扩为 84 槽，cloth_06 从 1 槽扩为 43 槽。910 个导出槽的每个来源候选均通过精确 Mesh 身份、原 Renderer `bones[slot]` 和完整层级路径验证，最终每槽都对应到一个无歧义的原生 Transform，全部是既有骨架节点。
4. 原有骨骼槽顺序和 bindpose 与当前重新解包的源数据完全相同；新增槽通过不同源 Mesh 的共同骨骼建立绑定空间转换后，与当前源数据比较，最大矩阵分量差 **3.96e-7**，低于 1e-4 检查阈值。只比较同名骨骼或直接照搬其他 Mesh 的矩阵不足以获得这个结论。
5. 每个输出顶点的四权重布局、索引和归一化符合现有导出契约；最大权重和误差约 1.00e-5。现有写出器对 `.dou` 的 328 个顶点、cloth_03 `.lian` 的 13 个顶点及 cloth_06 `.baohsi` 的 5,138 个顶点采用最强四项并归一化。本轮沿用该契约，没有引入新的静默减权策略。
6. 5 个实际基础材质均有正确源 Material / Shader / 原生纹理引用。cloth_01 没有作者修改，4 个其他材质各有贴图差量，共 7 个 PNG 依赖；导出 PNG 与作者文件逐像素相同，法线贴图保持线性声明。需要保留完整原生材质中未编辑的关键词/标志等字段，不能仅凭作者 `.mat` 重建全部源状态。

## 装配预案及边界

保持各作者部件独立，不再为了旧执行器把 cloth_03/04/06 合成一个多槽 Mesh。每次匹配到源 Mesh 使用上下文，预案为每个作者部件建立一个单材质 Mesh/Renderer，并保留相应骨骼、基础材质、显隐和形态控制。

沿用这次导出的 LOD0–3 契约，通过源层级使用上下文的 Mesh PPtr 获得各 LOD 的资源身份，随后收集清单内全部使用同一身份的 Renderer。此来源 PFB 中有 **26 个目标 Mesh 资源、33 次 Renderer 使用**，计划展开为 **60 个 Renderer 实例**并去除隐藏 cloth_05 的 **5 次使用**。这些数字汇总全部 LOD 和重复使用者，不是同一帧可见 Renderer 数，也不是 RenderDoc Draw/Pass 数。

该预案仅覆盖本次源 PFB 的依赖闭包，不能声称已验证 UI、NPC、过场等全部消费者。生产方案继续使用加载时的统一资源身份规则，不把此次清单变为永久 PFB 白名单。新增 Renderer 如何进入游戏的 LOD、控制、消融实例与残影采集流程，仍需要原生装配写出及实机验证。原 Renderer 的 bounds 也不能直接套到修改后的几何上，需依据作者几何正确生成。

报告里的 `fixture:*` 是此次快照的诊断 ID，**不是已实现的稳定部件 ID**。正式导入/导出还需持久 ID，使对象改名后按键与形态目标不丢失。

## 输出与复现

- 作者快照：`E:\EIEM_Workspace\exports\LZY-author-validation-20261002\capture.json`
- 分部几何：同目录 `parts/`，13 份完整检查输入。
- 当前源基线：`E:\EIEM_Workspace\exports\source-baseline-20261002\assets\beyond\dynamicassets\gameplay\actors\postmodels\characters\chr_0032_lizhiyan_postmodel.eff`
- 最终离线预检与展开预案：`diagnostics/prefab-resource-validation-20261002/LZY-selected-author-preflight-final.json`
- 捕获脚本：`tools/diagnostics/capture_selected_mod_author.py`
- 离线验证脚本：`tools/diagnostics/validate_selected_mod_author.py`

`existing-author-export/` 仅用于核验现有材质/贴图/控制依赖的语义，是诊断输入，不是新增游戏替换路径或 fallback；`parts/` 也不是游戏原生 Mesh 文件。

离线复现命令（输出路径必须不存在，保留之前证据）：

```powershell
python tools\diagnostics\validate_selected_mod_author.py `
  E:\EIEM_Workspace\exports\LZY-author-validation-20261002\capture.json `
  --output diagnostics\prefab-resource-validation-20261002\LZY-selected-author-preflight-new.json
```

运行环境需要 Pillow 和 NumPy，前者核对纹理像素，后者校验绑定矩阵。本轮使用现有 Python 环境，没有安装依赖。校验不通过会写出明确失败报告并返回非零状态，不改变规则以掩盖失败。

作者分部捕获约 7.50 秒，现有完整作者依赖导出约 11.22 秒；这两次操作服务不同验证目标，**不是内嵌原生核心速度测试**。最终离线预检约 3.1 秒。

2026-10-03 已用这份真实 Mod 的 cloth_06 两部件完成原生写出/独立回读和装配展开，结果见 [详细验证](cloth06-native-write-validation.md)。其他部件、原生形态键、游戏加载及新 Renderer 的 VFX 登记仍需继续验收。


## 全选择导出后续

14 个已确认对象现已完成全选择原生增量导出（13 个几何部件和一条隐藏源组规则），并复制到用户指定的 LZY 目录。body 形态键及大部件 32 位索引均通过离线回读；包不含完整原模型/PFB/Bundle。现 DLL 尚未读取新包格式，落盘不能作为游戏替换成功证据，详见 [增量部署状态](selected-mod-incremental-export.md)。


当前后续导出已统一为 INI 目录格式 2，原生数据计算继续复用；发布层使用可读部件名，不发布上述 fixture 诊断 ID。当前 447 行 INI（包含完整纹理声明）、13 个 Mesh、5 个 Material 与共享纹理载荷已通过重建。正式 Blender 持久部件 ID 和游戏加载仍待接通，详见 [当前导出](selected-mod-incremental-export.md)。
