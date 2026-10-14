# FSR3 FG 全部 review 问题修复交接

日期：2026-10-12。分支：`fix/beta7-fsr3fg-review-all`。基线：Beta7 `a90c7da2829f48587ee2ec8f37e12095a4e91ac8`。

工作目录：`C:/Users/81443/Documents/Magpie enhance/worktrees/beta7-fsr3fg-review-all`。主工作区已有修改与 Beta7 发布工作区均保留。修复已实现并完成下列源码/隔离验证；按用户要求没有构建、打包、部署或启动新版本。

## 变更与证据对应

| 问题 | 实现 | 主要验证 |
| --- | --- | --- |
| FSR3 输出帧率未知 | presenter 逐次清空回调结果，仅在 callback/Present/同步都成功后发布内容提交数；reset 仅真实 1，idle 不产生内容样本；未知回调不猜倍率 | 回调、失败、reset、idle 原生提取测试；120/60 统计窗口测试 |
| 统计口径混淆 | 独立 `LastContentFrameCount` 与 `Reported/Submitted`；FSR3 的 `LastPresentedFrameCount` 仍未知；统计窗口和工具栏提示/面板标签同步携带来源 | 统计、未知/混合窗口、来源及本地化检查 |
| NIS 被预检拒绝 | 白名单精确增加 `NIS\NIS` 与 `NIS\NVSharpen`；保留单 FG、SDR、矩形及未知效果拒绝 | 原生生产策略；WARP 1:1/非整数缩放和边界；未冒充完整 NIS+FSR3 实机验证 |
| 配置错误误报输出创建失败 | 追加效果链与 SDR 限制错误，旧 MP 编号不变；效果链首屏摘要包含具体效果名 | 枚举前缀一致性、中英繁体 XML、ErrorService `/Zs` |
| R1：fence 误唤醒 | 事件只用于唤醒，循环检查目标完成值和设备移除，所有唤醒共享 3 秒截止时间 | 提取生产 WaitForFence，注入旧事件、设备移除、超时后 drain、注册/等待失败 |
| R2：光流失败无限直通 | 将 provider failure/stopped、同步失败与合法 reset 分开，同共享槽传给前端；FSR3 必需光流缺失停止，连续 bootstrap 也停止；其它消费者的 fallback 不变 | 提取生产来源判定；原生 FSR3 admission 测试；跨槽集成检查 |
| R3：VRR 关闭未落实 | FSR3 使用 `SyncInterval=allowTearing?0:1`；默认 presenter 仍为 0；同一入口覆盖真实和 idle 提交，日志使用实际同步值 | 生产策略配合固定 SDK 工作线程 flags 表达式，覆盖开关、能力、跨适配器/拓扑/跨屏限制 |
| R4：静止源 Live 参数更新不可见 | 允许已改变效果 history revision 的同捕获输出发布，并以 BeginIdleFrame 呈现；不提交源时间/光流，不增加真实/生成计数，正常确认共享槽 | 生产 EffectFrameState + 发布门禁提取测试；前端非 FG 路径/计数/槽确认集成检查 |
| R5：参数修改未重置插帧历史 | 最终效果 history revision 随颜色同槽发布，FSR3 在新版本首个真实输入 reset 一次；相同源帧仍不能作为插帧输入 | 参数版本变化、普通连续输入、同捕获重绘的原生历史策略测试 |
| R6：复制 shader 时间冒充 FG 成本 | FSR3/XeSS 行注明“仅输入复制”；补帧 GPU 本体未测量，总计明确不含 SDK 补帧；未新增 GPU 同步或 CPU 假计时 | 资源/集成检查、OverlayDrawer `/Zs`；UI 实测待后续 |

生成提交计数排除 CPU 可见 reset，但 SDK 的 GPU 场景切换仍可能产生复制帧。它不等于有效新中间画面的数量，也不等于实际显示 FPS。

R3 是明确的能力适配：固定 SDK 无法仅靠 flags 提供 `sync=0 / no tearing`。本次保留签名/固定 hash 的 DLL，关闭 tearing 时改用 SDK 支持的垂直同步。该模式可能影响背压、节奏和延迟，不能宣称与旧模式性能等价。源/隔离检查证明传参和 SDK flags 判定相符，不能证明显示器 VRR 已激活或实际撕裂结果。

## 已执行验证

| 项目 | 结果 |
| --- | --- |
| `validate_fsr3fg_source.py --sdk-directory ...` | 130 项通过 |
| `validate_fg_settings_idle.py` | 77 项通过 |
| `validate_fsr3fg_review.py` | 45 项通过 |
| `Run-FSR3FGPolicyTests.ps1` | 66 项通过 |
| `Run-FGSettingsIdleTests.ps1` | 444 项通过；通用 FG/non-FG 重复检测提取回归通过 |
| `Run-FSR3FGReviewTests.ps1 -SdkDirectory ...` | 69 项通过（生产函数提取、模拟 API/队列、固定 SDK 表达式） |
| `Run-FSR3FGMotionResourceTests.ps1` | 1131 项通过；WARP D3D11 debug layer 无错误 |
| MSVC `/Zs /Y-` | FSR3FGPresenter、PresenterBase、FrameGuidanceService、Renderer、OverlayDrawer 均通过，使用现有 Beta7 SDK 定义 |
| SDK 关闭配置与应用错误服务 `/Zs` | FSR3FGPresenter 无 SDK 分支及 ErrorService 通过 |
| `git diff --check` | 通过 |

临时测试产物/提取记录位于 `C:/Users/81443/AppData/Local/Temp/Magpie-FSR3-review-fixes/`；没有向产品输出目录写入新 EXE/DLL。`/Zs` 只检查语法和类型，不生成 OBJ、不链接。原生测试编译的独立测试 EXE 与产品构建明确分开。

核心语法检查复用 `tests/check_duplicate_frame_native_syntax.py`，参数为既有 `release/v0.6.10-beta7` 和上述五个翻译单元；应用与无 SDK 检查使用同一读取 tlog 的方式，仅切换项目/移除 `MP_ENABLE_FSR3_FRAME_GENERATION`。没有运行 MSBuild 产品目标。

## 后续验收（本轮未执行）

- 产品全量构建及链接；当前验证不能证明整包可发布。
- 原始 5120×1440、NIS 1:1 + AMDOF；较小输入放大、非零黑边偏移；分别测试 NVOF，与 NIS + XeSS 对照。
- 真实游戏中的统计、首帧/reset、静止/恢复、Live 锐化修改、持续运行、多次启停、resize 与实际 provider 故障。
- VRR 开/关、多显示器迁移、拓扑变化、idle/resume 的真实底层呈现请求、帧节奏及输入延迟。
- PresentMon/ETW 或显示侧帧率；实际 UI/本地化布局与补帧画质。

原始诊断：[输出统计/NIS TODO](../todos/20261012-fsr3fg-output-statistics-nis-TODO.md)、[完整 review](20261012-fsr3fg-full-review.md)。原始证据仍在工作区 `diagnostics/20261012-fsr3fg-output-nis/`，未改写用户日志或截图。
