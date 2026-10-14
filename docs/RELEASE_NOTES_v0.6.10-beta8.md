# Magpie 0.6.10 Beta8

日期：2026-10-13。本地测试版本，继承 Beta7 `a90c7da2` 及 Beta6 的全部已合入内容。

## 中文

- 再次精简 165 个效果的双语说明：用途、适用素材和起步方法在前，必要启用条件收尾；参数提示同步更新，完整兼容列表移至使用指南。效果 ID、参数默认值、算法和运行行为保持不变。

- 新增 DLSSNR 持续状态与分类恢复引导：区分未运行、启动中、运行中、单层跳过及多层停止，保留原始失败和环境信息；正常参数限制与后端故障分开显示。
- 更新 165 个内置效果器的双语说明、分类和参数提示，修正 VFG 后置白名单中的 FSR RCAS 实际 ID。
- HDR 工具栏和浮窗根据已提交画面的参考白适配亮度，范围 160～300 nits，并提高底板不透明度；普通 SDR 路径保留。
- 对经过窗口身份、权限与关系复核的 UWP ApplicationFrameWindow 全屏源，允许受限的无原生 owner 启动路径。真实播放器兼容性仍待验收。

- 修复受影响游戏窗口的进程身份查询：常规 PID/TID 查询失败时，通过窗口线程的最小查询权限恢复进程身份，并复核窗口有效性。接入启动、窗口协调、NVIDIA 前台兼容及 RTSS 目标解析。实际权限校验保持生效。
- FSR3 支持 NIS 和 NVSharpen 的矩形效果组合，继续限制 SDR 和单个 FG；不支持的效果链与 HDR 配置显示独立错误。
- FSR3 显示成功提交的内容/生成帧计数，并标记 Submitted 口径。该数值不等于显示 FPS 或有效新中间画面数；未知回调仍保留未知。FSR3/XeSS 输入复制耗时与尚未测量的 SDK 补帧成本明确区分。
- 修复 FSR3 fence 旧事件误唤醒；光流 provider/同步故障不再无限直通。静止源修改 Live 参数可更新画面，并在下一真实输入重置插帧历史。
- FSR3 不允许 tearing 时使用 SDK 支持的垂直同步（SyncInterval=1）；允许时为 0。关闭 VRR 后的背压、节奏和延迟可能变化，需实机验证。
- 保留 Beta7 的通用重复帧设置、缓存空闲重绘、AMDOF Quality 默认值，以及 DLSS 社区运行库、RTSS 和配置页修复。

## English

- Refines bilingual copy for all 165 effects around purpose, suitable content and getting started. Updates parameter help and moves full compatibility lists into usage guides while preserving effect IDs, defaults, algorithms and runtime behavior.

- Adds persistent DLSSNR state and cause-specific recovery guidance, preserving original failures and environment details.
- Updates bilingual descriptions and parameter help for 165 built-in effects; corrects the installed FSR RCAS ID in the VFG post-effect policy.
- Adapts HDR overlay white to the presented scene within 160–300 nits, with opaque backgrounds; retains the SDR path.
- Admits narrowly verified fullscreen UWP ApplicationFrameWindow sources without a native owner relationship. Real-player acceptance remains pending.

- Recovers source process identity through a limited thread query when the normal PID/TID query fails, then revalidates the window. Startup, coordination, NVIDIA foreground compatibility and RTSS share the fix; existing privilege checks remain enforced.
- Allows rectangular NIS/NVSharpen chains with SDR FSR3 FG and reports unsupported chains/HDR separately.
- Reports successful FSR3 content submissions with a Submitted label. Counts do not establish display FPS or unique interpolated images. Unknown callback results remain unknown, and input-copy cost is separate from unmeasured SDK FG cost.
- Rechecks fence completion after wakeups, rejects mandatory optical-flow failures, updates static output after Live parameter edits and resets history on the next real input.
- Uses the pinned FSR3 SDK's supported vsync mode when tearing is disallowed. Actual pacing and latency require hardware validation.

## 验证边界

首次 Beta8 构建记录见 `reports/0610-beta8-integration-20261013`；本次功能更新见 `reports/0610-beta8-feature-update-20261013`。真实 Helldivers 2/GameGuard 启动与捕获、NIS+FSR3 画质、静止/恢复、VRR/HDR/多屏和性能验收仍需实机执行。Beta7 保留为回退版本。

本次说明更新的构建及部署记录：`reports/0610-beta8-copy-update-20261013`。
