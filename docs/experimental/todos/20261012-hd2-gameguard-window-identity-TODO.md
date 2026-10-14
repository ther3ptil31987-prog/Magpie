# Helldivers 2 / GameGuard 窗口身份查询兼容修复 TODO

日期：2026-10-12（Asia/Shanghai）。状态：源码修复及隔离回归完成；产品构建、部署和真实游戏验收待独立会话执行。

目标：让 Magpie 在正在运行的 Helldivers 2 窗口上稳定启动和持续运行效果，包括连续启停、切回游戏及窗口模式切换。修正权限提示仅是辅助工作，不能替代恢复功能。

2026-10-12 后续授权：基于本 TODO 实现并创建功能开发分支；不构建或部署新 release。已在 `fix/hd2-window-identity` 实现，工作树为 `C:/Users/81443/Documents/Magpie enhance/worktrees/hd2-window-identity`。交接记录：该分支 `docs/experimental/reviews/20261012-hd2-window-identity-implementation.md`。

## 1. 已确认的阻断机制与证据

- [x] 当前 Magpie 为管理员 / 高完整性级别 `0x3000`，游戏为中完整性级别 `0x2000`，不是游戏实际权限高于 Magpie。
- [x] 2026-10-12 08:32:30—08:35:56 的六次失败均发生在获取源窗口所属进程阶段；尚未进入捕获和效果初始化。失败链为 `GetWindowThreadProcessId` 失败 → `GetProcessHandleFromHwnd` 访问拒绝 → `SrcTracker::Set` 返回 `LowIntegrityLevel` / MP-007。
- [x] 对运行中 Magpie 的只读检查确认：`USER32!GetWindowThreadProcessId` 的跳转链最终进入游戏目录下的 `GameGuard/npggNT64.des`。同次快照中的 `GetForegroundWindow` 入口未显示 NVIDIA 前台兼容 detour。
- [x] 在已加载同一 GameGuard 模块的原生诊断程序中复现：`GetWindowThreadProcessId(hwnd, &pid)` 连续返回 `tid=0, pid=0, error=0`，而 `GetWindowThreadProcessId(hwnd, nullptr)` 成功返回真实线程号。
- [x] 实测后备路径成功：仅取 TID → `OpenThread(THREAD_QUERY_LIMITED_INFORMATION)` → `GetProcessIdOfThread` → 正确 PID → `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION)` → token 完整性查询。普通权限和管理员权限均通过。
- [x] 在 GameGuard 已加载、原查询仍失败的条件下，原生诊断程序通过 Windows Graphics Capture 收到当前游戏窗口的 `2562×1442` 首帧。
- [ ] 产品内完整效果链、长期稳定性、性能及窗口生命周期尚未验收。

“偶尔可用”与拦截生效时机有关是合理假设；历史成功会话没有对应模块状态，不能把具体启动顺序写成已确认根因或稳定解决方法。独立捕获首帧不等于 Magpie / DLSSNR 已修复，也不证明捕获像素内容和游戏内效果质量。

证据目录：`C:/Users/81443/Documents/Magpie enhance/diagnostics/20261012-hd2-startup/`。

| 文件 | 用途 |
| --- | --- |
| `magpie-api-snapshot.json` | 当前 Magpie 模块及 API 跳转链，只读采集 |
| `native-window-probe.txt` | GameGuard 加载后带 PID 查询失败、仅取 TID 成功；UI Automation 返回 PID 0，不适合作为本问题后备方案 |
| `native-thread-fallback.txt` | 普通权限下后备查询及 WGC 首帧 |
| `elevated-thread-fallback.txt` | 管理员权限下后备查询及 WGC 首帧 |
| `verified-fallback-capture.txt` | 实际使用恢复出的 PID 完成进程/token 查询，并取得 WGC 首帧 |
| `Inspect-Magpie.ps1`、`window-probe.cpp`、`build-probe.cmd` | 可复核诊断工具；不是产品修复 |

原始日志：`release/v0.6.10-beta7/Magpie-Experimental-x64/logs/magpie.log`。该日志会轮转，后续执行前保留相关片段。复现时的 PID `24268`、TID `39356`、HWND `0x90CEE` 仅为历史证据，严禁在实现或后续测试中硬编码。

## 2. 实施基线

- [x] 已复查工作树和未提交文件，基于干净的 `release/0.6.10-beta7` / `a90c7da2` 创建隔离开发分支；日常主线和其它开发分支的改动未混入。本轮未替换运行包，下列包信息仍是诊断时的历史证据。
- 诊断使用的源码工作树：`.worktrees/0610-beta7/source`，本轮结束时 HEAD 为 `a90c7da2829f48587ee2ec8f37e12095a4e91ac8`。
- 被诊断的运行包是 `release/v0.6.10-beta7/Magpie-Experimental-x64`，manifest 提交为 `a0f7145f41e16d3788a996fbfb7dbb179b1c0f4e`，与上述 HEAD 不同。此次相关 `Win32Helper.cpp` / `SrcTracker.cpp` 对比无差异；不能据此认为所有文件或运行行为相同。
- 已核对运行包 `Magpie.exe` SHA-256 与 manifest 一致：`2678F00829E46B6F5FCB0356BEE38DC0BCA33496DDD6A00C8CB00647972156E8`。
- TODO 归档在日常源码 `source/docs/experimental/todos/`，不代表应以较旧日常主线覆盖运行包。

## 3. P0：修复窗口身份解析

- [x] 在现有 Win32 窗口帮助代码中实现一个统一的身份查询入口，返回经验证的 PID / TID；不要创建独立服务或全局无期限身份缓存。
- [x] 保留普通查询快速路径。仅当带 PID 的查询失败或返回无效身份时，执行以下后备路径：

  ```text
  GetWindowThreadProcessId(hwnd, nullptr)
      → OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid)
      → GetProcessIdOfThread(thread)
      → 校验窗口仍存在、所属线程未变化
      → 使用所得 PID 继续进程路径 / 完整性查询
  ```

- [x] 使用 RAII 关闭线程句柄；PID / TID 为零、线程访问失败、窗口已关闭或查询期间身份变化时，返回失败，不输出部分有效身份。
- [x] 将后备路径放在 `GetWindowProcessHandle` 的原有高权限备用路径之前，使本问题不再依赖 `GetProcessHandleFromHwnd`。保留其他窗口原有兼容行为，避免顺带删除无关历史支持。
- [x] 保留真实完整性级别比较和窗口有效性检查；不以“查询失败就放行”代替身份恢复。
- [x] 后备路径仅使用公开的只读查询接口，不修改 GameGuard、系统 API 或游戏内存，不加载驱动、不恢复/移除第三方钩子。

## 4. P1：覆盖启动后的窗口生命周期

以下带 PID 查询必须审查并按需要接入统一入口；只修改 `SrcTracker` 的权限检查不足以完成修复。

- [x] `Win32Helper.cpp`：进程句柄、路径和权限查询，恢复配置匹配与启动检查。
- [x] `SourceWindowCoordinator.h`：capture/root 身份读取、`Matches` / `Ready` 校验及事件注册使用正确 PID，避免首帧前失败或后续误认为源窗口已变化。
- [x] `SourceWindowRelations.h`：会话窗口身份注册与匹配，保持 PID/TID/窗口类及销毁后的身份保护。
- [x] `NvidiaForegroundCompatibility.cpp`：源窗口身份及前台匹配，避免效果启动后兼容判断仍使用失效的带 PID 查询。
- [x] `ScalingService.cpp` 的弹窗归属判断、`RtssService.cpp` 的目标进程解析：检查游戏相关路径，防止失败的零 PID 导致误匹配或选错进程。
- [x] 审查其他调用点，仅处理属于本次游戏窗口路径的必要项；已经正常工作的仅取 TID 调用不机械替换。
- [x] 对每帧/前台查询热路径检查额外线程句柄查询成本。优先复用既有会话身份和有效性机制；如需缓存，必须绑定线程/窗口生命周期并处理销毁、重建和句柄复用，不能只凭 HWND 永久信任缓存。

## 5. P2：相称的验证

- [x] 复用相关现有窗口协调、owner 和 NVIDIA 前台测试，必要时扩展已有 Win32 调用替身，覆盖真实故障语义：普通窗口成功；带 PID 查询失败但仅取 TID 成功；后备线程查询失败；零 PID；窗口查询期间销毁/变化；实际更高权限仍被拒绝。
- [x] 现有检查入口可从 `tests/Run-ScalingWindowOwnerTests.ps1`、`tests/Check-WindowCoordinationSyntax.ps1`、`tests/Run-NvidiaForegroundTests.ps1` 核对适用范围；不要把语法/替身测试当作游戏验收。
- [ ] 在授权构建和部署后，保留原包与配置恢复点，用修复版对同一游戏确认：正确 PID/TID → 权限检查通过 → source coordination 就绪 → 捕获首帧 → DLSSNR 初始化和实际输出首帧。
- [ ] 实机覆盖普通/管理员 Magpie、游戏先启动/后启动、连续启停、Alt+Tab 返回、最小化恢复、窗口/无边框切换。记录真实窗口是否重建，以及每次启动成功或失败步骤；不宣称尚未执行的组合通过。
- [ ] 游戏退出和重启后，不误捕获复用的 HWND、不保留失效线程/进程身份；停止效果后窗口顺序和输入行为正常。
- [ ] 回归至少一个普通桌面窗口，并确认前台判断、参数面板与启用中的 VRR/RTSS 路径没有因身份解析变化而回退。
- [ ] 实测后备查询在热路径的开销和持续运行稳定性；根据新增失败或未解决风险扩大验证，不重复无新信息的检查。

完成标准：修复版 Magpie 在 GameGuard 已加载、原查询可复现失败的条件下，稳定运行游戏效果并通过相关生命周期验收。诊断程序首帧成功、提示变化或单次成功均不足以标记完成。

## 6. 辅助诊断与交付

- [x] 增加适量后备路径诊断，记录首次使用、PID/TID、具体失败阶段，避免每帧刷日志。错误分类可随主修复纠正，但不是主交付物。
- [x] 已保存源码、测试和后续部署交接记录；实现提交以 `fix/hd2-window-identity` 分支 HEAD 为准。尚无新运行包或部署恢复点，后续部署会话须自行建立。
- [ ] 如仍有后续捕获/owner/效果初始化失败，依据新的首个失败继续定位，不将其混同为本次已确认的窗口身份查询问题。

参考：[GetProcessIdOfThread](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocessidofthread)、[线程查询权限](https://learn.microsoft.com/en-us/windows/win32/procthread/thread-security-and-access-rights)。

## 7. 本轮源码交付补充

- 公共入口位于 `src/Magpie.Core/include/WindowIdentity.h`，正常路径不打开线程句柄；失败后使用最小查询权限和 RAII，并复核 HWND/TID/窗口类。
- 不缓存跨调用身份；capture/root 相同时复用本次校验。NVIDIA 测试确认同一 source/root 的后备查询每次只打开一个线程句柄，普通路径 API 次数不增加；这不是游戏性能验收。
- 六个选定测试程序通过（身份单元、进程/token/弹窗/RTSS 集成、owner policy、owner、26 个 lifecycle 探针、NVIDIA 前台）；九个 Core 编译单元通过 `/Zs /W4 /WX` 语法检查。App 的修改函数经过生产源码提取测试，未进行完整 App 构建。
- 首次线程后备成功记录 HWND/PID/TID，失败记录内部阶段和 Win32 错误；热路径不刷日志。权限提示文案未改，真实 IL 比较仍保留。
- 后续未勾选的实机项继续作为验收标准，不因本轮源码/隔离测试通过而标记游戏已修复。
