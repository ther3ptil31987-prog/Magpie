# Magpie 0.6.10 Beta7

日期：2026-10-12。本地测试版本，继承 Beta6 `556d4ac2` 的全部修复。

## 中文

- 四类插帧统一使用通用重复帧检测设置，移除效果内独立开关。画面静止约 500ms 后按有效空闲率重绘缓存，不重复运行效果或光流；恢复新内容时重置插帧历史。工具栏 R/s 单独记录缓存重绘提交，不计入生成帧率。

- 帧率与刷新增加全局简单／高级模式，分组显示内容帧率、静态画面、光标与 VRR；保留高级偏好和已有显式重复检测设置。新配置的全局重复检测默认关闭。
- 启用 RTSS 管理后，自动准备 RTSS 与 Magpie 自身配置，再应用当前目标的限帧；失败、取消、重试和结束恢复按会话处理。MSI Afterburner 为可选工具。
- 更新刷新页面：四处帧率控件同行排列、补齐分组图标，简单／高级切换与恢复默认置于标题区。简单模式恢复可见设置，同时保留隐藏的高级偏好。
- RTSS 使用独立状态标题、说明和行内管理区，区分当前目标、工具状态与手动操作结果；重新检测只读检查，应用限帧仍由当前会话重试执行。
- 统一配置页参数列、模式与数值组合及数字输入样式，保留原有范围、绑定和键盘操作；窄窗口下按可用宽度换行。RTSS 配置接管、状态和管理详情使用连续的设置行，并补齐裁剪、倍率及管理展开状态的可访问名称。
- RTSS 配置接管独立成组，关闭联动或跟随源帧率时也可打开工具管理。10 处数字输入的 FPS／px／x 单位移到框外，简单／高级切换使用实际圆角外框。
- 原 DLSS FG 效果接入固定版本的社区兼容运行库，保持 2×～4×与旧配置；增加文件身份校验和失败升级回滚。
- 新增实验性 FSR 3 FG：SDR、固定 2×，必需外部光流，默认 AMDOF 质量模式；旧 None／缺省／非法选择自动迁移，合法光流选择保留。HDR 与不支持的几何组合明确拒绝。
- 修复 FSR3 外部运动副本缺少 SRV 绑定的问题，区分缺失／过期运动与资源错误，保留首个呈现错误详情。DLSS 在 NGX 初始化前主动加载并校验内嵌运行库、注册实际搜索目录，增加各阶段诊断。真实插帧画面与运行稳定性仍需实机验证。
- 统一显示名称为 DLSS FrameGeneration、XeSS FrameGeneration、FSR3 FrameGeneration，保留效果 ID、原有搜索词和配置。
- 同步 XeSS FG／VFG 审阅记录与 Beta6 已有默认值；VFG 保留性能／均衡两档。

各 GPU 架构的 DLSS 社区路由、FSR3 生成画质／成本、真实游戏和 HDR／VRR 显示仍需逐项验收。首次集成结果见工作区 `reports/0610-beta7-integration-20261011`，刷新／RTSS 更新见 `reports/0610-beta7-refresh-update-20261011`，配置页更新见 `reports/0610-beta7-profile-update-20261011`，RTSS 分组更新见 `reports/0610-beta7-rtss-group-update-20261011`，此前 FG 修复的构建与验证见 `reports/0610-beta7-fg-fixes-20261011`。本次 FG 设置与空闲重绘验证见 `reports/0610-beta7-fg-idle-20261012`。实际 WinUI 布局、DPI 和 RTSS 目标帧率仍需原生验收。

## English

- All four FG backends use common duplicate-detection settings. After about 500ms without new content, cached redraws follow the effective idle rate without rerunning effects or motion estimation. New content resets FG history on resume. R/s separately counts cached redraw submissions.

- Adds global Simple/Advanced refresh settings with grouped controls and preserved advanced preferences. New configurations default to no global duplicate detection.
- Prepares RTSS automatically when session management is enabled, with bounded startup, cancellation, retry and restoration. Afterburner remains optional.
- Aligns the four rate controls in rows, adds group icons, and places the mode switch and reset action in the header. Simple-mode reset preserves hidden advanced preferences.
- Separates the active RTSS target status from tool inventory and manual management results. Rechecking is read-only; applying a frame cap still requires the current session's retry action.
- Unifies profile control columns, paired mode/value editors and numeric fields while preserving ranges, bindings and keyboard behavior. Narrow layouts wrap to the available width. RTSS status and management use continuous settings rows, with accessible crop, scale and expansion labels.
- Moves RTSS management into its own group, available even when linkage is disabled or following the source rate. Places FPS/px/x units outside all ten numeric fields and draws a rounded border around the Simple/Advanced selector.
- Integrates the pinned community runtime into the existing DLSS FG effect, retaining 2x–4x and saved settings, with file verification and update rollback.
- Adds experimental SDR-only, fixed-2x FSR 3 FG. External optical flow is required and defaults to AMDOF Quality; old None, missing or invalid selections migrate while valid selections remain. Unsupported HDR or geometry combinations are rejected.
- Fixes the SRV binding on FSR3 external-motion copies, validates resource contracts and retains the first presentation error. Primes and verifies the bundled DLSS runtime before NGX initialization, registers its actual directory and adds stage-specific diagnostics. Real generated output and runtime stability still require hardware validation.
- Unifies display names as DLSS FrameGeneration, XeSS FrameGeneration and FSR3 FrameGeneration while preserving effect IDs, existing search aliases and saved settings.
- Retains all Beta6 fixes and updates stale XeSS FG/VFG review metadata.

GPU architecture coverage, generated-image quality, performance, real games and display behavior remain separate acceptance work. This is a local test release.
