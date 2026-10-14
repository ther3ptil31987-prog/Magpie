# UWP 播放器全屏后无法启动效果：诊断与修改 TODO

日期：2026-10-13（北京时间）。状态：定向源码修复和相关隔离回归已完成；完整构建、部署及真实播放器验收由后续独立会话执行。

开发分支：`fix/uwp-player-fullscreen`；工作树：`worktrees/uwp-player-fullscreen`；基线：Beta8 `cbe2c2cbda103e6cb6f686a5521918ed2501c32e`。见[实施与交接记录](../reviews/20261013-uwp-player-fullscreen-source-handoff.md)。

## 1. 本轮范围与用户确认

- 用户报告媒体播放器、电影与电视在普通窗口下可以应用效果，播放器自身全屏后无法启动。
- 用户确认两种操作顺序都受影响：先将播放器全屏再启动 Magpie；Magpie 已生效后再将播放器切到全屏。
- 初始委托仅诊断并整理 TODO；随后用户授权按 TODO 在独立开发分支实施，并明确不编译部署新 Release，构建部署交给单独会话。当前仅编译运行了隔离测试，并做生产源码语法检查；未构建、部署或重启产品，未操作真实播放器窗口。
- 目标是让上述两种顺序均能显示并持续输出效果，且全屏退出、切换前台和关闭播放器时正确退役。

## 2. 证据与实施基线

- 运行日志版本：`0.6.10-beta8`；Windows `10.0.26200`；RTX 5070 Ti；管理员运行。
- 检查的发布源码：`.worktrees/0610-beta8/source`，提交 `cbe2c2cbda103e6cb6f686a5521918ed2501c32e`；检查时工作树干净。
- `release/v0.6.10-beta8/RELEASE-REVIEW.md`、构建审计和上述提交对应；部署 EXE 的实测 SHA-256 为 `8D36EBCD589C016F3D0F9CF06DD8E275FFB2F0F1AA189AE82BF29361A6043A96`，与构建审计一致。
- 本文保存在日常 `source/docs/experimental/todos/`，不表示日常主线 HEAD 就是本次运行包的实现基线。后续实施应选择包含 Beta8 修复的适当分支，保留已有未提交工作。
- [日志快照及校验收据](../../../../../diagnostics/20261013-uwp-fullscreen-startup/receipt.json)于 10:04:41 保存。两份原日志在复制期间的长度、修改时间均未变化；分析引用以下快照行号，避免运行日志轮转后失效。
- [magpie.1.log](../../../../../diagnostics/20261013-uwp-fullscreen-startup/magpie.1.log)与 [magpie.log](../../../../../diagnostics/20261013-uwp-fullscreen-startup/magpie.log)为连续的同版本日志。

### 2.1 同一 UWP HWND 的成功与失败对照

这些会话使用 Graphics Capture、DLSSNR；Magpie 输出均为 `IsWindowedMode=false`，`IsSimulateExclusiveFullscreen=false`。差异是播放器源窗口的状态，不能把它解释为 Magpie 窗口输出与全屏输出的对照。

| 时间 / run | 源 HWND / 尺寸 | owner 设置结果 | 结果与证据 |
| --- | --- | --- | --- |
| 09:54:03.840 / 13 | `0x9138a`，2560×1440 | `system=0`、`actual=0`、`api-failure` | MP-051；`magpie.1.log:3669–3677` |
| 09:54:09.383 / 14 | 同一 HWND，2560×1360（最大化、保留标题栏/工作区） | `system=87`、`actual=0`、`rejected` | 进入 `ManagedUnowned`，有 `first-output-shown`；`magpie.1.log:3768–3769,3830–3832` |
| 09:54:26.635 / 15 | 同一 HWND，2560×1440 | `system=0`、`actual=0`、`api-failure` | MP-051；`magpie.1.log:3987–3995` |
| 09:54:59.938 / 17 | `0xf1300`，1086×731（普通窗口） | `system=87`、`actual=0`、`rejected` | 进入 `ManagedUnowned`，有 `first-output-shown`；`magpie.1.log:4236–4237,4291–4298`及 `magpie.log:2–3` |
| 09:55:05.569 / 18 | 同一 HWND，2560×1440 | `system=0`、`actual=0`、`api-failure` | MP-051；`magpie.log:94–95,149–157` |
| 09:55:09.718 / 19 | 同一 HWND，2560×1440 | `system=0`、`actual=0`、`api-failure` | 重试仍为 MP-051；`magpie.log:249–250,304–312` |

两组窗口类均为 `ApplicationFrameWindow`，同一对照内 PID/TID/HWND 不变。失败时 `ownerStyle=0x940b0000`，成功时为 `0x95cf0000` 或 `0x94cf0000`。这些值用于复核状态变化，不能硬编码为兼容条件。

日志没有保存足够的应用包身份，不能把上述两个 HWND 精确分别指认为“媒体播放器”和“电影与电视”。09:59 的 `WinUIDesktopWin32WindowClass` 会话也成功，但只覆盖普通/最大化尺寸，不能替代该应用真正全屏的验收。

### 2.2 首个致命失败点

以 run 18 为例：

1. `magpie.log:138`：`DLSSNR STATUS: Feature=18 created=true`。
2. `magpie.log:149–150`：WGC 取得有效 2560×1440 帧，后端接受捕获序列。
3. `magpie.log:153`：`DLSSNR pass 1: frame=1 result=0x1 SEH=0x0`。
4. `magpie.log:154–155`：首次 Present，进入首帧显示路径。
5. `magpie.log:156–157`：设置 owner 后读回为空，被归为 `api-failure system=0`，报告 MP-051。
6. `magpie.log:158` 起：输出窗口开始退役，效果没有进入可见、持续运行状态。

四次 MP-051 均具有“有效捕获 → DLSSNR 首帧处理 → owner 门禁失败”的顺序。因此本次稳定启动失败已定位在输出窗口协作准入，不能归因为没有捕获到全屏、DLSSNR 不支持或缺少管理员权限。首帧提交日志中的 `showing scaling window` 写在 `_Show()` 调用之前，不等于窗口已经显示成功。

## 3. 因果链与确定性

### 已确认

- `src/Magpie.Core/include/ScalingWindowOwner.h:61–69` 先清空 last error，再调用 `SetWindowLongPtrW(GWLP_HWNDPARENT)`，立即保存返回值和错误码，随后读回 `GW_OWNER`。
- 同文件 `90–97`：实际 owner 等于请求值才为 `Applied`；实际 owner 为空时，只有错误码 87 被归为 `Rejected`，错误码 0 被归为 `ApiFailure`。
- `SourceWindowCoordinator.h:107–114`：只有已建立原生 owner，或 `CanContinueUnowned()` 为真的结果能通过。当前后者仅包含 `Rejected`。
- `ScalingWindow.cpp:454–502`：首帧前验证源身份、响应状态和完整性级别后尝试 owner 设置。上述 `ApiFailure` 不能通过 `Accept()`，因而报告 MP-051 并停止。
- 当前测试 `scripts/tests/scaling_window_owner_policy.cpp:244–251` 明确断言只有 87 能继续，0 必须失败。因此这不是“漏写了零返回判断”，而是已有保守策略没有覆盖真实 UWP 全屏下的无错误码、关系未建立状态。

### 解释与边界

**根因定位：UWP 全屏下的 owner 设置结果变化，触发了过窄的兼容准入条件，Magpie 主动终止本来已经完成首帧处理的会话。**

[Microsoft 的 SetWindowLongPtrW 文档](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowlongptrw)说明返回值是旧值，零返回值本身不能表示失败，必须结合清零后的 last error 判断。但这也不能代替实际 owner 关系的验证：本次日志显示 `rawReturn=0, system=0, actual=0`，不能直接当作 `NativeOwned` 成功。Windows 内部为何没有建立关系，现有证据不足以确定；本文不把它写成已确认的系统 bug、独占全屏限制或 DRM 限制。

播放器切全屏引起源尺寸变化时，`ScalingWindow.cpp:1980–2001` 请求退役重建，`ScalingRuntime.cpp:505–516` 稳定后调用 `RestartAfterSrcRepositioned()`，该函数再次进入 `Start()` 和同一首帧协作门禁。因此这一门禁能解释用户报告的两种操作顺序。现有日志包含尺寸变化与同 epoch 重建，但没有完整记录一次“已成功显示 → 切全屏 → 自动重建 → MP-051”的连续链；该顺序仍需修复后的专门实机验收。

### 独立待验证现象

09:54:30.843（run 16）和 09:55:15.975（run 21）在 WGC 启动时出现 `0x8007139F` / MP-011，同时邻近记录出现无效句柄（圆角 API `0x80070006`、`GetClassName` 错误 1400；见 `magpie.1.log:4136–4147`、`magpie.log:595–606`）。这与源窗口在初始化期间消失/更换相符，但不能仅凭日志区分用户关闭、窗口替换或生命周期竞争。它发生在另一阶段，不是上述四次 MP-051 的原因；不要靠改报错文字或修圆角清理替代主修复。

## 4. 修改方案与 TODO

### T01 / P0：定向支持 UWP 的 owner 未建立状态

- [x] 增加 `NotApplied` / `owner-not-applied` 分类，保留原始返回值、错误码和读回结果，不将其冒充 `Applied`。
- [x] 在现有协调准入边界为该结果增加受限分支，复用 `ManagedUnowned`。新分支只覆盖顶层 `ApplicationFrameWindow`；`Windows.UI.Core.CoreWindow`、WinUI 和普通 Win32 的零错误异常保留拒绝，已加入反例。
- [x] 新分支仅在以下条件同时成立时准入：已确认的源类型；源 capture/root 的 HWND、PID、TID、类身份仍匹配且 capture 等于 root；输出仍是本线程创建的有效顶层窗口；源/输出均非 child、无自引用或 owner 环；源权限检查成功；原始结果为零错误、零返回值且没有 owner；准入时再次复核身份和空 owner。
- [x] 非目标窗口的同样零错误结果仍按失败处理；访问拒绝、未知错误、失效/复用 HWND、错误 owner、身份/权限复核失败不进入兼容路径。保留现有 87 兼容行为和正常 `NativeOwned` 行为。
- [x] 不用窗口标题、某个历史 PID/HWND、固定样式常量或 2560×1440 尺寸作为匹配条件；全局 `CanContinueUnowned()` 仍仅接受 87 拒绝状态。

主要修改位置：`ScalingWindowOwner.h`、`SourceWindowCoordinator.h` 和 `ScalingWindow::_EstablishWindowCoordination()` 的现有职责边界。无需新增服务、用户设置或另一套捕获后端。

### T02 / P0：复用显示和重建生命周期

- [x] 两条入口复用现有首帧门禁；受控测试覆盖首次零错误准入、保留 epoch 重建后 87→0 再准入，以及之后出现真实权限错误时终止，不缓存上次许可。
- [x] 进入 `ManagedUnowned` 后，继续先调用 `UpdateManagedOrder()` 验证层级，再走 `_Show()` 的 `SWP_NOACTIVATE / SWP_NOZORDER` 显示路径。注入层级准备失败时不会发成功通知或成功显示日志。
- [x] 保留“首帧准备前无 owner、输出窗口先退役、之后才执行阻塞 SDK 清理”的既有约束；26 个双进程退役场景通过。
- [x] 复用既有参数输入层级、外部窗口层级、弹窗、隐藏、最小化、关闭和过期事件的隔离回归。实际播放器的全屏往返及焦点恢复仍在 T05 验收，不以这些测试代替。

### T03 / P1：补充能区分本故障的日志

- [x] 保留源类、API 原始结果与实际 owner；准入日志新增 `reason=uwp-owner-not-applied` 及身份/权限复核标记，沿用 run/epoch。原 `continueUnowned` 改为 `unownedByErrorCode`，明确它只描述错误码提供的兼容依据。
- [x] 首帧提交日志改为 `preparing first display`，仅在 `_Show()` 成功后写入 `Scaling output shown`；失败分类使用 `owner-not-applied`，不再模糊为 `api-failure system=0`。
- [x] 已评估应用身份日志需求：本故障由现有 HWND/PID/TID/源类和新增准入原因即可定位，暂不扩展查询或收集媒体路径；两款应用的包名/版本留作 T05 实机记录。

### T04 / P0：复用现有回归，改变错误的适用范围

- [x] 扩充 `scaling_window_owner_policy.cpp`；新用例在旧代码上先复现准入失败，修复后通过。普通 Win32 零错误异常仍拒绝，原生 owner 成功用例保留。
- [x] 保留并覆盖 87、5、1400、未知错误、意外 owner、销毁窗口、child/self/cycle、权限失败及身份变化反例；新分支额外覆盖非零旧 owner、API 后关系改变和捕获 child。
- [x] 普通首帧和 DLSSFG FIFO 首帧均经过生产准入代码；测试覆盖保留身份的双向重建、权限错误终止、层级失败时不发成功通知。过期事件/旧 epoch、源销毁锁存仍执行原有回归。
- [x] 窗口策略、窗口身份单测与集成、26 个双进程退役场景、捕获关闭 12 场景/1000 次重建全部通过；9 个生产源文件通过 `/Zs /W4 /WX`，`git diff --check` 通过。未做完整产品构建。

### T05 / P0：真实播放器验收

- [ ] 分别记录“媒体播放器”和“电影与电视”的实际应用身份/版本，用同一个本地视频、同一捕获方式和效果配置验证。
- [ ] 基线：播放器普通窗口、最大化，Magpie 均能显示效果。
- [ ] 顺序 A：播放器先全屏，再启动 Magpie；需出现真实显示成功记录，并持续看到处理后的运动画面。
- [ ] 顺序 B：先在普通窗口启用效果，再把播放器切到全屏；自动重建后继续显示，退出全屏也能恢复。
- [ ] 每款应用连续切换至少 5 次，并覆盖一次 Alt+Tab、最小化/恢复、暂停/继续和关闭；无残留覆盖窗、焦点抢占或异常后台输出。
- [ ] 先验证当前 Graphics Capture + DLSSNR 配置；用简单效果作单变量对照。检查至少一个普通 Win32 播放器（可使用日志中曾成功的 PotPlayer），确认正常 owner 行为未受影响。
- [ ] 只有受控源码测试、完整产品构建以及以上真实显示/连续播放验收分别完成后，才能标记对应阶段通过。捕获首帧、NGX 成功、Present 日志和离屏测试都不能单独代替实机成功。

### T06 / P1：按证据决定是否需要独立处理 MP-011

- [ ] 主路径修复后复测初始化期间切换全屏；若源未关闭仍稳定出现 MP-011，采集源销毁/更换事件与捕获 target lifetime，再判断是否存在 WGC 初始化竞争。
- [ ] 若确认只是用户关闭源窗口，验证退役即可；若确认同一有效源的状态过渡，提出有边界的等待/重建方案。现阶段不加入忽略所有 `0x8007139F`、无限重试或随意绑定新 HWND 的逻辑。

## 5. 完成条件

- [x] 保存原始日志证据，核对发布包与源码版本。
- [x] 找到并交叉验证四次 MP-051 的首个致命失败点和成功对照。
- [x] 明确用户的两种触发顺序，列出主修复、反例回归和真实验收项目。
- [x] 实施定向修复并通过相关回归。
- [ ] 完整构建和部署候选版本（后续实施阶段另行安排）。
- [ ] 两款播放器按 T05 完成真实全屏输出验收，确认是否存在需要单独处理的 MP-011。
