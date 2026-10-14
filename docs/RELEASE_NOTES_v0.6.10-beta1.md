# Magpie Experimental 0.6.10 Beta1

本地测试版本，基于完整的 0.6.9 公开源码整合。公开更新入口仍指向 0.6.9。

## 中文

### UWP 窗口兼容与停止流程

- 正常 Win32 窗口沿用原生 owner；系统拒绝关联且窗口身份、权限和状态均通过核实时，使用显式窗口协作模式。
- 首帧成功后再建立窗口关系；源窗口隐藏、最小化或关闭时，无需等待新捕获帧即可隐藏或退役输出。
- 停止、初始化失败和模式切换先取消呈现并解除窗口关系，再等待后端线程与 SDK 清理；保留现有 NGX 故障隔离。
- 照片和 Forza 的实际兼容性、复杂窗口操作及真实 GPU 停止行为仍需测试。

### 可变刷新率（实验）

- 默认配置与应用配置的“帧率与刷新”新增“可变刷新率（实验）”，默认关闭。启用后重新开始缩放生效。
- 普通效果、DLSS FG 和 XeSS FG 使用共享 DXGI 呈现策略，检查输出路径、显示器和适配器资格，保持交换链创建与调整尺寸的标志一致。
- 普通 DXGI 路径遇到 tearing 合同错误时进行有界回退；XeSS SDK 路径避免重复提交同一帧。
- 旧配置缺失字段、错误字段类型及旧隐藏全局 VRR 字段不会自动启用本选项。
- 该选项表示请求 VRR。NVIDIA 指示器、显示器实时刷新率及实际显示间隔仍需实机验证；NoFocusLoss 兼容实验未纳入本版。

## English

This local test version integrates the UWP and VRR branches on top of the complete public 0.6.9 source. The public update entry remains at 0.6.9.

### UWP window coordination and shutdown

- Keep native ownership for ordinary Win32 windows. Use explicit coordination only after an ownership rejection passes identity, permission and state checks.
- Establish window relationships after the first successful frame. Hide or retire output when the source is hidden, minimized or closed, without waiting for another captured frame.
- Cancel presentation and detach window relationships before backend joins and SDK cleanup during stop, initialization failure and mode changes. Preserve NGX fault isolation.
- Photos, Forza, complex window interactions and real GPU shutdown still require testing.

### Variable refresh rate (experimental)

- Add a default-off option to each profile's frame/refresh settings. Restart scaling after changing it.
- Share a DXGI presentation policy across ordinary effects, DLSS FG and XeSS FG; validate output eligibility and retain swap-chain flags across resize.
- Bound ordinary DXGI tearing-error fallback and avoid submitting a XeSS SDK frame twice.
- Missing or invalid profile fields and the legacy hidden global VRR field do not enable the option.
- The option requests VRR. Driver activation, monitor refresh behavior and actual display intervals still require hardware validation. NoFocusLoss compatibility experiments are outside this Beta1.

## 验证记录 / Validation

合并源：`fix/069-uwp-window-owner`（`b39fdc45`）与 `feature/069-vrr-gsync`（`1d6642ef`）。准确构建提交、功能开关和文件哈希见运行包的 `build-manifest.json`；回归、完整构建及包校验见本地整合审计。离线测试和构建结果不替代实机验收。

Integrated branches: `fix/069-uwp-window-owner` (`b39fdc45`) and `feature/069-vrr-gsync` (`1d6642ef`). The package's `build-manifest.json` records its exact source commit, feature options and file hashes. Local integration audits record regression, full-build and package checks; these do not replace hardware acceptance.
