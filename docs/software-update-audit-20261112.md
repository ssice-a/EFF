# Mod Manager / Blender 软件更新调查

2026-11-12。本轮调查的是软件检查、下载和退出后的安装，不是游戏 F10。不能将“未更新”统一解释为 GitHub 不通；当前复现涉及客户端安装缺陷、查询耦合、公开包滞后，以及短时 API 限流。

## 已证实的问题

| 环节 | 复现与原因 | 本轮处理 |
| --- | --- | --- |
| Manager 下载后安装 | 实际启动的是 Windows PowerShell 5.1；生成脚本使用 `.NET Framework` 没有的 `Path.GetRelativePath`，含支持文件的正常更新包会失败 | 改为有目录边界校验的相对路径截取；补真实 WinPS 5.1 成功安装回归 |
| Manager 安装边界 | 仅 EXE 更新缺回滚目录；无 BOM 脚本不能可靠读取中文路径；新进程没有固定工作目录 | 统一创建回滚目录、UTF-8 BOM/日志编码、启动目录和成功后的暂存清理；实际失败回滚测试通过 |
| Manager 更新查询 | DLL 查询先执行，Manager 查询后执行，共用 20 秒；DLL 失败或超时会使 Manager 完全不查 | 两个查询并发、独立截止时间与结果；一方失败仍处理另一方，并显示组件具体错误 |
| GitHub API | 本轮早先两个 Manager API 返回 403/remaining=0，网页仍 200；11:51 CST 复测两个 API 恢复 200，约 0.5 秒 | 明确区分 API 限流与缺 Release；暂时连接/429/5xx 有限重试，403 不立即反复重试，不引入第三方镜像或绕过校验 |
| Blender continuous | 公开 ZIP 为 `a1a4f49b…` / 0.50.7，本机/公开 main 为 `61587d78…` / 0.50.7；旧代码只要 SHA 不同就提示更新 | 使用 GitHub compare 确认方向；ahead 才更新，behind 提示本机领先，diverged/未知停止自动安装。同版本的新提交仍支持更新 |

Blender 的真实 ZIP 下载返回 200，426,297 字节，大小、SHA256 与包内 revision 均一致。该次问题是公开更新包落后及方向判定错误，不是下载损坏。对同一本机安装，旧函数返回 `available`，修后实际接口回验返回 `latest` 并解释本机领先。

## 保留的校验与失败行为

- 两端继续验证可信下载来源、包大小、路径边界和既有摘要；不因网络不稳定关闭验证。
- Blender 检查/下载最多三次，仅对暂时 DNS、超时、中断、429、5xx 重试；长 Retry-After、403/404、证书错误直接解释原因。下载重试从空文件开始。
- Manager 每个检查独立 20 秒；关闭程序的 cancellation 继续向上传递，不伪装成网络失败。403/长限流不在该窗口盲目重试。
- Manager 保存最近一次检查的分组件状态、版本与错误到 `%LOCALAPPDATA%/EFF-Mod-Manager/update-check.json`；写诊断失败不影响成功结果。
- Manager 更新失败保留暂存 `update.log` 和已核验缓存包，恢复旧文件；成功安装不修改玩家 Mod 与设置。

## Blender 等待退出与任务状态恢复

现有 worker 必须等待所有 `blender.exe` 正常退出；多开 Blender 时只关一个不会安装。不会强关用户程序。任务文件位于 `<addons>/.eff-update-*/job.json`，含 `prepared`、`waitingForBlenderExit`、`installed` 或 `failed`；同目录 `install.log` 保留安装输出。

初次审查发现 UI 只显示准备完成，重启后丢失任务详情。本机三份历史任务均为 `installed`，没有用户那一次失败的现场任务，因此不能把其具体故障认定为等待退出。用户随后授权补齐状态恢复，现已实现：

- 插件注册时恢复当前安装目录的最新有效任务；其他安装目录、越界/链接或损坏的发现记录不会混入当前状态。旧任务缺少新字段仍可读取。
- worker 保存状态更新时间和仍在运行的 Blender PID；只在 PID 变化时写记录，退出等待条件保持不变。
- 有待处理任务时在主线程定时器中每秒读取一次，面板 draw 不读任务文件；终态及插件注销停止定时器。准备、等待、失败、成功与网络检查状态独立显示，不互相覆盖。
- 失败显示具体原因、日志路径及“打开更新日志目录”；安装助手无法启动也会持久化为 failed，避免重启后变成永远等待的 prepared。
- installed 记录须与当前 revision 和受管理文件摘要一致才显示成功；安装已被后续版本替换时显示历史状态，当前文件损坏时显示校验异常，不伪造成功。

这是对状态呈现与失败记录的修复，没有取消等待所有 Blender 退出的要求，也没有改变安装提交/回滚流程。旧 Manager 首次手动升级限制仍如后文所述。

## 验证与证据

- Manager 确定性回归：单组件异常/超时隔离、关闭取消、403 限流、503 后恢复、长 429、404 无 Release。
- 真实 Windows PowerShell 5.1：含嵌套依赖的安装、中文/单引号路径、仅 EXE 包、启动后退出的回滚及原配置保留。
- Blender 更新专项 21 项通过：同版号 ahead、旧包 behind、分叉/身份不符、force/ignore、未知来源、网络分类与中断下载重试；实际 API 对照见本机证据。
- 完整组件回归、发布构建及固定目录安装按 [本机部署规程](release-management.md) 执行，安装结果以本轮回执与文件摘要为准。

本机调查证据：

- `diagnostics/updater-audit-20261112/manager-github-probe.json`：含 11:51 成功复测和早先限流观察；早先观察未保存精确时间，已在文件中注明。
- `bin/update-audit-20261112/blender-public-network.json`、`blender-public-package-verification.json`、`blender-source-check-after-fix.json`。
- `bin/update-audit-20261112/manager-build-deploy.log`、`blender-build-deploy.log`。

这些网络结果只代表本机本次请求，不能推断所有用户/地区都可连接。发布目录滞后还需按本地先部署、再上传现成包的规程解决；本轮用户未要求推送或发布，不擅自修改远端 continuous/Release。

旧 Manager 的安装脚本由正在运行的旧程序生成。以后发布修复包并不会自动修复这一次旧脚本的执行：已经受该缺陷影响的用户，需要先手动解压完整新版覆盖一次，之后才能使用修复后的自更新器。只替换 EXE 或宣称“发布后所有旧客户端会自愈”都不成立。

## 本机部署结果

四仓库均为 `main`，部署前 fetch 后本地 HEAD 与 origin/main 一致；现有工作区改动保留。未创建分支、提交、推送或发布。

| 组件 | 固定目标 | 回执与校验 |
| --- | --- | --- |
| Manager | `E:/EFF-Mod-Manager` | `bin/local-deployments/20261112-115736-574/Manager.json`、`InstalledVerification.json`；9 个安装文件核对构建输出/部署回执 |
| Blender | 用户 Blender 5.2 的 `scripts/addons/EFF_Blender` | `bin/local-deployments/20261112-115848-224/Blender.json`、`InstalledVerification.json`；60 个插件文件与本次包一致 |
| 共享作者工具 | `E:/EIEM_Workspace/tools/d89ec95fdec50426b24c6e28ab29b684375b0b59569182def9037391a5fd0871` | 本轮 `shared-author/SharedAuthor.json`；126 个文件摘要一致 |

Blender 完整作者/更新套件运行 153 项，通过，1 项跳过；Manager 完整工具与 6 组集成测试、Windows 发布构建通过。实际安装的 Blender 更新函数联网返回“本机领先，无需安装旧包”，约 1.9 秒。Manager 生产查询与实际 ZIP 下载返回成功，识别 EFF v2.0.12、Manager v0.2.3，并通过下载大小/SHA256 检查；下载只在隔离测试目录，不用公开旧包覆盖本机修复版。

汇总证据为 `bin/update-audit-20261112/installed-verification.json` 和 `manager-live-update-check.log`。文档相对链接检查覆盖 47 份文件，无断链；三个有改动仓库 `git diff --check` 通过。没有更换游戏 DLL、用户 Mod 或 Blender 工程。

## 文档与代码归档

9 份已替代方案移入 `docs/archive/compiled-pipeline/`、`effects/`；7 份旧入口/日志保留完整快照，并重写当前索引、领域模型、架构、实现状态和 F10 测试入口。相对 Markdown 链接随移动修正。

五组无当前调用者的旧调查工具（9 个源码/构建文件）移到 `tools/archive/compiled-pipeline/`，修正 builder 的仓库路径与相对 include。仍服务构建、发布、迁移或现行诊断的旧命名代码保留原处；没有删除生产模块、用户捕获、Mod 或历史 worktree。具体名单见 [归档索引](archive/README.md) 和 [工具归档](../tools/archive/compiled-pipeline/README.md)。

## 后续修复验证：更新任务状态

新增回归在修复前失败，修复后软件更新专项共 27 项通过。真实 Blender 5.2.3 LTS 在独立 `--background --factory-startup` 进程中完成四条路径：注册恢复失败与日志入口；等待 PID/定时器及终态/注销清理；成功前校验安装；真实安装操作器的 helper 启动失败持久化及重注册恢复。全部任务、包和安装目标使用临时目录，不读取或保存用户工程。

源码验收见 `bin/update-status-20261112/source-blender.json`、`source-blender.log`；组件构建与安装见同目录 `build-deploy.log`。对应自动测试为 Blender 仓库 `test_eiem_update.py`、`test_eiem_release.py` 和 `test_eiem_update_status_blender.py`。

该后续修复已部署到固定 Blender 5.2 插件目录，完整套件 159 项通过（1 项跳过）。回执 `bin/local-deployments/20261112-121449-776/Blender.json`、`InstalledVerification.json`；60 个插件文件与 126 个共享工具文件摘要核对通过。实际安装插件在 Blender 5.2.3 LTS 后台重跑上述四条路径通过，证据 `bin/update-status-20261112/installed-blender.json`。当前共享代为 `4f6c55e604afb8f4e9e3379344ba20452d030e98484dc3d3e18bdaec69cc25e6`。本轮只更新 Blender 及其配套共享作者工具，未提交、推送或发布。
