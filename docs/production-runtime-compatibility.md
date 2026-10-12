# 生产 DLL 与宿主 C++ 运行库

2026-10-12，v1.4.0 GitHub 正式附件在游戏启动时崩溃，eff_log.txt 为零字节。Windows 两次记录 MSVCP140.dll 14.29.30139.0、异常 C0000005、偏移 0x13020；Player.log 的调用链来自 eff.dll 的启动日志。

## 可复现的对照

`production_dll_load_smoke` 以 /MT 编译，子进程先加载指定宿主目录的运行库，再加载真实 eff.dll，调用导出控制函数并读取启动日志。它不加载 GameAssembly、不启动游戏、不操作 Mod。

| DLL | 预加载运行库 | 结果 |
| --- | --- | --- |
| GitHub v1.4.0（链接器 14.51） | 游戏附带 14.29.30139 | C0000005，MSVCP140 + 0x13020，与实机相同 |
| 相同 GitHub v1.4.0 | 系统 14.51 | 通过 |
| 本地旧工具链生产构建（链接器 14.38） | 游戏附带 14.29 | 通过 |
| 修正后的 /MT 生产构建 | 游戏附带 14.29、系统 14.51 | 启动日志及控制锁通过 |

GitHub 产物没有导入 _Mtx_init_in_situ，本地产物使用该初始化函数。微软 STL 17.10 的 mutex constexpr 默认初始化改变，旧运行库可能在首次锁定时空指针访问，见 [STL 发布记录](https://github.com/microsoft/STL/releases/tag/vs-2022-17.10)。系统已安装新版 Redistributable 也不能改变游戏优先加载 app-local 旧文件的事实。

## 构建约束

- 生产 entry、ImGui、两个代理均使用 /MT，插件内部 C++ 对象的构造、锁定、销毁使用自己的实现。插件导出仅交换 C 接口字段/调用方缓冲区，不跨模块转移 STL 或 CRT 分配所有权。
- ImGui 静态和动态 CRT 产物存到不同构建目录，避免缓存混用；诊断工具仍可保留自身 /MD 构建。
- 静态 CRT 保留线程 attach/detach 通知，参见 [DisableThreadLibraryCalls 要求](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-disablethreadlibrarycalls)。
- verify_production_dll.py 直接读取 PE 导入表，拒绝正式 DLL 及同目录代理对 MSVCP、VCRUNTIME、UCRT 的外部依赖；继续排除临时诊断标记。对 v1.4.0 的 guard 明确失败。
- 流水线加载真正的生产 DLL，而不是只测试包含源文件的模拟程序。旧游戏运行库的对照在本地运行；CI 不分发游戏文件。

本地静态运行库修正版部署后，用户确认可正常进入场景；新会话 PID 38312 的日志已继续到资源装配、UI 完成与按键控制。最终发布构建还保留了静态 CRT 所需的线程通知，并由发布流水线重新验证。

数据协议不变，当前 v0.41.0 作者输出无需因这一修复重导出。运行构建的宿主兼容验证与 Mod 游戏内功能验收仍是不同的步骤。
