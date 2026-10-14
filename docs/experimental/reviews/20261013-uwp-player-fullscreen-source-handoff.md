# UWP 播放器全屏启动：源码实施交接

日期：2026-10-13。状态：源码和隔离验证完成，完整产品构建、部署和真实播放器验收未执行。

## 分支与边界

- 开发分支：`fix/uwp-player-fullscreen`。
- 开发工作树：`C:/Users/81443/Documents/Magpie enhance/worktrees/uwp-player-fullscreen`。
- 基线：Beta8 发布提交 `cbe2c2cbda103e6cb6f686a5521918ed2501c32e`，不是日常 `experimental` 的较旧 HEAD。
- 用户明确将后续完整构建、部署交给独立会话。本轮只编译执行隔离测试，并做生产源码 `/Zs` 语法检查；未生成或替换 Release 包，未重启 Magpie，也未操作真实播放器窗口。
- 未合并、未推送。日常源码仅更新本任务的 TODO，原有其余未提交内容保持。
- 完整诊断、日志时间线和剩余验收项见[TODO](../todos/20261013-uwp-player-fullscreen-startup-TODO.md)。

## 最终实现

1. `ScalingWindowOwner.h` 增加 `NotApplied` 状态：Windows 不报错但 owner 关系未建立时，不再混称 API 失败，也不当作原生 owner 已建立。`CanContinueUnowned()` 保留原来的 87 规则。
2. `SourceWindowCoordinator::Accept()` 为该结果增加受限路径。只接受 capture=root 的 `ApplicationFrameWindow`、零错误和零旧 owner 返回值；保留源 PID/TID/类身份、输出所属线程、顶层窗口、完整性级别和 owner 关系检查，并在准入前再次复核。正常 Win32、WinUI 和 CoreWindow 的该异常仍不放行。
3. 复用现有 `ManagedUnowned` 窗口层级处理；首次启动和源尺寸变化后的重建都使用当前结果重新准入。没有修改捕获后端、效果链、前台策略或 SDK 退役顺序。
4. 日志明确区分 `owner-not-applied`、`reason=uwp-owner-not-applied`、`preparing first display` 与 `_Show()` 成功后的 `Scaling output shown`。

新增回归使用本测试进程创建的隐藏 Win32 窗口，模拟指定类名和 API 返回结果，并调用抽取的生产首帧/准入代码。它们验证策略与生命周期，不是 Windows UWP 应用的实机兼容证明。

## 验证结果

证据目录：`C:/Users/81443/Documents/Magpie enhance/reports/uwp-player-fullscreen-source-20261013/`。

| 检查 | 结果 | 证据 |
| --- | --- | --- |
| 旧实现 + 新目标回归 | 在预期的 `ManagedUnowned` 准入断言失败，确认能复现该策略缺口 | `regression-before.log` |
| 最终窗口 owner 策略 | 通过；普通/FIFO 首帧、双向重建、权限/窗口/关系反例和层级失败 | `owner-policy.log` |
| 窗口身份单测及生产路径集成 | 通过，包括线程查询后备、权限检查与身份生命周期 | `related-regressions.log` |
| 两进程完整退役 | 26 场景通过；正对照出现约 2.3 秒等待，正常退役探针源层级操作为 0 ms | `related-regressions.log` |
| 捕获关闭与重建 | 12 场景 + 1000 次重建，8 项集成顺序检查通过 | `related-regressions.log` |
| 生产源码语法 | 9 个翻译单元 `/Zs /W4 /WX` 通过，没有产品链接或打包 | `syntax/syntax.log` |
| 补丁检查 | `git diff --check` 通过 | 最终交接收据 |

测试实现期间修正了一处 fixture：层级已经正确时生产代码不会调用 `SetWindowPos`，因此需要先让测试输出的置顶状态不同，再注入该 API 失败。没有为满足测试修改产品层级算法。

## 复核命令

在本开发工作树中运行；输出目录应在工作树外，不触及 Release：

```powershell
pwsh -NoProfile -File tests/Run-ScalingWindowOwnerTests.ps1 `
  -OutputDirectory ../../reports/uwp-player-fullscreen-source-20261013/owner-policy `
  -TestNames scaling_window_owner_policy

pwsh -NoProfile -File tests/Run-ScalingWindowOwnerTests.ps1 `
  -OutputDirectory ../../reports/uwp-player-fullscreen-source-20261013/related-regressions `
  -TestNames WindowIdentityTests,WindowIdentityIntegrationTests,scaling_window_lifecycle,capture_shutdown

pwsh -NoProfile -File tests/Check-WindowCoordinationSyntax.ps1 `
  -OutputDirectory ../../reports/uwp-player-fullscreen-source-20261013/syntax
```

上述检查均已完成；后续若只构建本提交且源码没有变动，无需为形式重复同一组源码验证。

## 后续构建部署会话

1. 从本分支提交整合到所选发布基线，保留该会话已有工作；不要把较旧日常主线整体覆盖到 Beta8。
2. 按项目发布脚本完成完整构建、依赖/资源校验和部署；版本号、部署目录及运行包替换由该会话确定。本轮没有预先创建新 Release 或修改版本。
3. 分别对“媒体播放器”和“电影与电视”记录实际应用身份/版本，验证先全屏再启动、先启动再全屏两种顺序，以及普通窗口/最大化/退出全屏、Alt+Tab、暂停继续、最小化恢复和关闭。每款应用至少完成 5 次全屏往返。
4. 对本次目标状态，日志应先出现 `status=owner-not-applied`，随后 `mode=ManagedUnowned reason=uwp-owner-not-applied`，最终出现 `Scaling output shown`；同时必须目视确认效果处理后的运动画面持续输出。
5. 如果真实应用走的是其他窗口类，先保留其身份与失败日志，不直接扩大白名单。若主路径恢复后仍出现 MP-011 / `0x8007139F`，按 TODO T06 区分真实关闭、窗口替换与 WGC 状态竞争。

验收前不能声称播放器全屏问题已实机解决，也不能把本轮通过的源码检查、隐藏窗口测试或首帧日志当作显示/画质/性能验收。
