# Helldivers 2 窗口身份兼容修复：源码交接

日期：2026-10-12。用户本轮要求源码实现和开发分支；发布包构建、部署由单独会话负责。

## 分支与基线

- 开发分支：`fix/hd2-window-identity`，交付提交为该分支 HEAD。
- 工作树：`C:/Users/81443/Documents/Magpie enhance/worktrees/hd2-window-identity`。
- 基线：`release/0.6.10-beta7` 的 `a90c7da2829f48587ee2ec8f37e12095a4e91ac8`。
- 本轮没有合并其他功能分支、推送远端、生成新 release、部署或重启 Magpie/游戏。日常 `source` 中只同步了本任务原有 TODO 的状态，其他未提交内容未改。
- 对应 [TODO](../todos/20261012-hd2-gameguard-window-identity-TODO.md) 保留诊断证据和未执行的游戏验收项目。

## 行为变化

诊断证据显示，GameGuard 在 Magpie 进程内拦截了带 PID 输出的 `GetWindowThreadProcessId`；同次原生实验中的仅取 TID 调用、线程归属查询、token 查询及 WGC 首帧可以成功。先前 Magpie 因此无法取得游戏进程并提前停止启动，即使 Magpie 的完整性级别确实高于游戏。

新增 `WindowIdentity.h::QueryWindowThreadProcessId`，保留正常查询的快速路径；带 PID 查询失败或输出不完整时，使用仅取 TID、`OpenThread(THREAD_QUERY_LIMITED_INFORMATION)`、`GetProcessIdOfThread` 恢复身份。线程句柄用 RAII 管理，持有到窗口/TID/窗口类复核结束。失败输出零 PID/TID，关闭句柄后保留失败错误码。

接入位置：

| 位置 | 作用 |
| --- | --- |
| `Win32Helper::GetWindowProcessHandle` | 恢复进程路径、配置匹配和 token/IL 查询；原 Oleacc 后备保持在新路径之后 |
| `SourceWindowCoordinator` | capture/root 身份、Ready 校验、正确 PID 的 WinEvent 注册，明确拒绝零身份 |
| `SourceWindowRelations` | 会话窗口注册与匹配 |
| `NvidiaForegroundCompatibility` | 初始化、工作线程前台判断和日志中的进程身份 |
| `ScalingService::IsPopupWindow` | 同进程弹窗判断；查询失败的两个零 PID 不再被当成同进程 |
| `RtssService` | 目标解析、会话校验及 UWP 子窗口解析，保留进程创建时间等原检查 |

没有改动实际 IL 比较或权限提示文案，没有对查询失败放行，没有改写 GameGuard、系统 API 或游戏内存。其余直接带 PID 调用经审查仅剩 AppX 读取和 Magpie 自身 TouchHelper 查询，未扩大修改；原本的仅取 TID 调用保持不变。

## 热路径和诊断

身份查询无全局缓存、锁、堆分配或跨调用句柄。普通窗口仍只做原来的一次 PID/TID 查询。capture=root 时，SourceWindowCoordinator 复用本次身份查询；NVIDIA 原有 MatchesPair 也复用相同身份。受影响的前台查询仍会打开/关闭线程句柄，真实游戏开销须后续测量，不能将测试替身的调用次数当成性能结论。

进程查询首次成功使用后备时记录 HWND/PID/TID；失败记录 IsWindow、TID/class、OpenThread、GetProcessIdOfThread 或窗口复核阶段及错误码。协调和 SDK 工作线程中的逐帧查询不输出日志。

## 已执行验证

测试产物目录：`C:/Users/81443/Documents/Magpie enhance/diagnostics/20261012-hd2-implementation/`。这些是独立测试程序和语法日志，不是 Magpie 发布包。

| 检查 | 结果和边界 |
| --- | --- |
| `WindowIdentityTests` | 正常、带 PID 失败且 error=0、部分身份、最小权限、拒绝访问、零 PID、清理错误码、查询期间销毁/TID/类变化、无跨调用缓存，全部通过 |
| `WindowIdentityIntegrationTests` | 提取实际生产进程/token/启动准入、弹窗、RTSS 身份代码；在本进程隐藏测试窗口上模拟原查询失败，验证身份恢复、旧后备、零 PID 不误匹配、RTSS 创建时间/关闭检查，通过。RTSS 测试不写真实配置 |
| `scaling_window_owner_policy` | 原查询失败后 Read/Ready 和 PID 范围的事件注册可用；拦截状态切换和销毁无误放行；真实高 IL 分支、权限查询失败继续拒绝，通过 |
| `scaling_window_owner` | 原有双进程 owner/Z-order 探针通过，含故意阻塞的阳性对照 |
| `scaling_window_lifecycle` | 26 个完整退出/重建探针通过。基线夹具缺少现有 mailbox 成员、RunId 和清理返回值，已仅补齐测试替身，不修改对应生产退出逻辑 |
| `Run-NvidiaForegroundTests.ps1` | 原回归和新增后备身份、失败拒绝、过期 PID/类、4 个并发读线程通过。普通路径原 API 次数保持 13/16；相同 source/root 的后备只打开一次线程句柄。源代码绑定检查通过 |
| `Check-WindowCoordinationSyntax.ps1` | Win32Helper、NvidiaForegroundCompatibility、ScalingWindow、ScalingRuntime、Renderer、GraphicsCaptureFrameSource、SrcTracker、ParameterInputHost、CursorManager 九个 Core 编译单元 `/Zs /W4 /WX` 通过 |

上述测试替身和本机测试窗口不构成真实 GameGuard/Helldivers 2 效果验收。App 修改函数经过生产源码提取执行，未完整构建 App；未宣称全仓所有测试通过。

复核入口（在本开发工作树执行）：

```powershell
pwsh -NoProfile -File tests/Run-ScalingWindowOwnerTests.ps1 -TestNames WindowIdentityTests,WindowIdentityIntegrationTests,scaling_window_owner_policy,scaling_window_owner,scaling_window_lifecycle
pwsh -NoProfile -File tests/Run-NvidiaForegroundTests.ps1
pwsh -NoProfile -File tests/Check-WindowCoordinationSyntax.ps1
```

## 后续编译部署会话

1. 从本功能分支取修复，复查接收分支基线和其他并行功能的合并情况；不要直接用较旧日常主线覆盖 beta7 内容。
2. 按该会话的授权构建产品；部署前备份实际运行包和用户配置，记录最终提交/manifest/哈希及恢复位置。
3. 在 GameGuard 已加载、原查询故障成立时，确认后备 PID/TID → 实际 IL 检查 → source coordination → WGC 首帧 → DLSSNR 初始化及可见效果输出。
4. 按 TODO 验证普通/管理员、启动顺序、连续启停、Alt+Tab、最小化恢复、窗口/无边框、游戏退出重启及普通桌面窗口回归；测量后备查询开销和长时间稳定性。
5. 如出现新的 owner、捕获或效果失败，按新的首个失败继续定位；当前交付仅证明本次已知阻断机制的源码修复及隔离回归。

当前状态：源码已实现；针对性测试与语法检查通过；产品未构建、未部署；真实游戏效果尚未验收。
