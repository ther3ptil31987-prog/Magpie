# FSR3 FG 输出统计与 NIS 叠加修复 TODO

日期：2026-10-12（Asia/Shanghai）。状态：独立分支源码修复与隔离验证完成；产品构建、部署和实机验收按用户要求暂缓。

目标：FSR3 FG 运行时提供有明确口径的输出计数；允许已确认保持矩形坐标映射的 NIS → FSR3 FG 效果链正常启动。上次诊断依据保留于下文。本次实现位于 `fix/beta7-fsr3fg-review-all`，从 Beta7 `a90c7da2` 建立独立工作目录；验证包括原生隔离测试、WARP 与 MSVC `/Zs`，没有构建或部署新版本。完整实现及待验收边界见 [修复交接](../reviews/20261012-fsr3fg-fixes-handoff.md)。

## 1. 结论与证据

### 1.1 无输出帧率：FSR3 presenter 没有向统计链路提供数值

- [x] 截图 1 显示真实输入约 60 FPS，内容输出未知，已报告生成帧率为 `--`。
- [x] 日志证明 FSR3 已初始化并持续成功调度插帧。`magpie.log:1629`，10:11:14.804：`baseSubmits=1800 callbackDispatches=1800 interpolationDispatches=1799 resetPassThroughs=1 invalidMotionSamples=1`，`resetMask=0`。首次历史重置后的计数与每真实输入生成一帧一致；这里的 dispatch 数不是显示器实际显示帧数。
- [x] `FSR3FGPresenter.cpp:824–825` 每次成功提交仍固定设置 `_lastPresentedFrameCount = std::nullopt`。这足以解释截图中的未知输出，不需要假设统计窗口采样异常或插帧没有运行。
- [x] 因果链：FSR3 返回未知 → `Renderer.cpp:1244–1249` 把未知总数与 1 个真实输入传给统计器 → `PresentationFrameRate.h:23–32` 将本统计窗口标为未知 → `OverlayDrawer.cpp:392–400、2224–2228` 显示未知输出和 `--` 生成帧率。
- [x] XeSS 路径不同：`XeSSFGPresenter.cpp:1041–1054` 调用 `xefgSwapChainGetLastPresentStatus`，使用 SDK 的 `framesPresented`，因而能显示截图 3 的 `121/61 FPS` 和约 61 FPS 生成帧率。
- [x] 现有 `tests/validate_fsr3fg_source.py:107` 还断言必须存在 `_lastPresentedFrameCount = std::nullopt`。原目的是避免冒充显示帧率，实际没有为 FSR3 提供另一条可用计数路径。

**根因：统计接口接入缺口。** 不代表本次插帧失效；也不能据现有日志认定屏幕实际获得 120 FPS。

### 1.2 NIS 叠加失败：FSR3 外部运动矢量兼容白名单漏项

- [x] 第一次失败在 `magpie.log:1706–1708`，10:11:44.008：

  ```text
  FSR 3 FG requires optical flow and a supported rectangular effect chain. Remove the incompatible effect: NIS\NIS
  Diagnostic MP-028: system=0
  ```

- [x] 10:11:59.577（1782–1784 行）、10:14:05.496（2068–2070 行）重复同一失败。效果顺序是 `NIS\NIS` → `FSR3FG\FSR3_FrameGeneration`，AMDOF 已选中（`opticalFlowMethod=1`），HDR 关闭，捕获/输出均为 5120×1440。不是用户没选光流。
- [x] `Renderer.cpp:457–463` 调用 `SupportsFSR3FGExternalMotion()`，未通过即返回 `PresentationInitFailed`；该检查在前端 D3D 设备和 FSR3 presenter 初始化之前（469、494 行附近）。失败会话没有进入 NIS shader 编译、FSR3 swapchain 创建或插帧阶段。
- [x] `FSR3FGPolicy.h:88–97` 的显式允许列表包含 FSR EASU/RCAS、Bicubic 等，但没有 `NIS\NIS` 和 `NIS\NVSharpen`。
- [x] NIS 的实际映射是规则的轴向缩放：`NIS.hlsl:91–94` 使用输入/输出比例，`NIS_Scaler.hlsli:710、728` 按像素中心计算源坐标。`NVSharpen.hlsl:21–24` 保持输入尺寸。两者均不引入 CRT 一类的非线性几何变形。
- [x] 现有 `FSR3FGMotion.h:9–22` 已把源坐标运动矢量按 `contentSize / sourceSize` 放大；`Renderer.cpp:1077–1093` 提供最终内容矩形，presenter 通过 `generationRect` 处理输出偏移。因此 NIS 属于当前矩形映射能够表达的效果。实际放行后的图像质量仍需验证。
- [x] 对照会话在 10:12:48–10:13:39 运行 NIS → XeSS FG：`magpie.log:1837–1849` 为配置，1874 行初始化成功，1894 行 NIS 编译成功，截图 3 给出正常帧率。XeSS 初始化路径没有这项 FSR3 专用白名单检查。

**根因：FSR3 的兼容策略遗漏了 NIS。** 这组日志没有发生“NIS 与 FSR3 在 GPU 执行时冲突”。“无法创建画面输出”来自错误分类，不能据此把切换窗口模式作为根因修复。

### 1.3 次要问题：配置拒绝被归入输出创建失败

- [x] 白名单拒绝返回 `PresentationInitFailed`，`ErrorService.cpp:55` 映射到通用输出初始化提示，导致截图 2 的建议与本次原因不符。
- [x] `ErrorService.cpp:275` 已有向详情追加 context 的逻辑，日志也保存了具体效果名。本次不能进一步断言“详情 UI 丢失 context”；需要修正的是错误分类及首屏可理解的原因。

## 2. 修改方案与 TODO

### P1-A：补齐 FSR3 的内容提交统计

推荐先提供与实现证据相符的“内容提交”帧率，保留 SDK 呈现报告与提交计数的区别。既恢复用户可用的数值，也不把 GPU 调度或代理 Present 冒充显示实测。

- [x] 在 `FSR3FGPresenter` 内按单次 SDK frame ID 收集提交结果：本次回调是否执行、返回状态、实际回调 reset、生成提交数量。每次提交前清空本次结果，不能复用上一帧的累计值或状态。
- [x] 在 SDK 回调成功、代理 Present 成功且该次提交被接受后，发布“1 个真实内容帧 + 本次非 CPU reset 的生成提交数量”。使用回调/dispatch 的本次结果，不使用配置倍率，不直接计算 `输入 FPS × 2`。
- [x] 正常生成提交计数为真实 1 / 生成 1；首次、断帧、resize、光流 reset 等历史重置仅计真实 1 / 生成提交 0。SDK 在 reset 路径产生的直通重复图像不得算作新生成内容。
- [x] 若开启 FG 却没有预期回调，或无法确认本次结果，保留未知并记录可诊断状态；API 失败不增加成功提交计数。退出、重建和 resize 不携带上一会话的样本。
- [x] 将 FSR3 的内容提交数作为独立可选结果接入现有 presenter → Renderer → `PresentationFrameRate` 链路。推荐在现有样本中增加“SDK 报告 / 后端提交 / 未知”的口径标记；FSR3 的 SDK 呈现报告仍可保持未知，不把提交数冒填到 `LastPresentedFrameCount()`。SDK 回调和 reset 细节留在 presenter 内，Renderer 只消费结果。
- [x] 统计窗口与 UI 同步携带口径：FSR3 面板显示“内容提交 / 真实输入”“已提交生成帧率”，工具栏提供对应提示；XeSS 保留其 SDK 报告口径。未知和有效 0 必须分开；同一窗口若有无法确认的样本，不补造一个完整输出数。
- [x] 保留现有真实输入按 capture frame ID 去重。空闲重绘只增加 `R/s`，独立 UI 刷新不增加内容/生成帧率；恢复新输入后的第一帧按 reset 处理。
- [x] 更新静态检查：保护“提交不等于显示”的语义与计数分离，不再把一个固定赋值文本当作整个统计需求已满足。补充真实输入 60、生成提交 60 时约 `120/60`，以及 reset、未知、失败、idle 和恢复的行为测试。

SDK 依据与限制：

- 本地 SDK 的 `FrameInterpolationSwapchainDX12.cpp:1920–1950、1989–2020` 显示，回调结果决定是否安排生成帧，实际呈现由工作线程执行；2220 行附近代理 `Present` 返回 `S_OK`。
- 同文件 2330–2332 行的 `GetLastPresentCount()` 转发到底层真实 swapchain。它可用于累计呈现趋势核对，但异步返回、idle 和 reset 重复帧使其不能直接当作“当前真实输入对应的生成数量”，也不能用同一采样窗口的差值简单减真实输入来推导生成帧。
- `ffx_framegeneration.h:76–87、112–115` 的 present callback 带有 frame ID / generated 标记，但它发生在最终呈现前、参与实际图像合成。为计数替换这个 callback 会改变输出路径，本修复不需要这样做。
- 如后续明确要求“已显示帧率”，应另以 PresentMon/ETW 或显示侧测量验收。上述 P1-A 交付的是后端内容提交率，不能宣称已实现屏幕显示 FPS 测量。

### P1-B：放行 NIS 的矩形效果链

- [x] 在 `SupportsFSR3FGExternalMotion()` 中精确加入 `NIS\\NIS`；同时覆盖已检查为等尺寸锐化的 `NIS\\NVSharpen`。
- [x] 保持实际 FG 在最终色彩处理后执行，以及单 FG、外部光流、SDR 与内容矩形边界检查。不新增 marker 排序要求；不删除整项兼容验证，不批量放行未知自定义效果、裁切溢出或非线性形变效果。
- [x] 在 `FSR3FGPolicyTests.cpp` 增加两种 NIS 正向用例，保留 CRT/未知效果拒绝用例。测试对象应为生产策略函数。
- [ ] 运动资源测试已覆盖 1:1、非整数放大、缩放边界和向量幅度；非零内容偏移及完整色彩输出与 `generationRect` 的实机对齐仍待后续验收。
- [ ] 在真实 GPU 会话验证 `NIS → FSR3 FG`：首先复现日志中的 5120×1440、1:1、AMDOF 条件，再测较小输入放大至输出；用同场景的 `NIS → XeSS FG` 作回归对照。检查画面、帧率、持续运行和多次启停，不能仅以通过白名单或启动成功验收。

### P2：纠正兼容性错误提示

- [x] 为不受支持的 FSR3 效果链给出明确分类及本地化摘要，包含不兼容效果名和对应限制。不要把此类配置检查失败继续描述为“无法创建画面输出”。
- [x] 实现可增加精确的效果链不兼容错误类型；若增加 `ScalingError` 项，保持既有 MP 编号稳定，避免插入导致后续枚举值变化。
- [x] 真正的 D3D/swapchain 创建失败继续使用原输出创建错误。更新对应资源、错误服务映射和必要的错误分类回归。

## 3. 验收清单

| 场景 | 必须满足 |
| --- | --- |
| FSR3 FG 单独运行，持续有效的约 60 FPS 输入 | 提交口径约 120/60，约 60 生成提交；来源标记明确；首个 reset 不虚增 |
| 首帧、缺失/断开的输入、光流 reset、resize | 重置帧不计生成提交，无跨会话旧计数 |
| SDK 回调失败、Present 失败、未知回调结果 | 不误报固定 2×；有效 0 与未知区分 |
| 静止超过 idle 阈值、只有光标/UI、随后恢复 | 内容/生成率衰减；idle 仅在 R/s；恢复首帧 reset |
| NIS → FSR3 FG，1:1 和实际放大 | 不再被策略拒绝；运动/内容区域正确；无新增 SDK 错误 |
| NIS → FSR3 FG，AMDOF 与 NVOF | 两种已支持光流路径分别验证；不要以一种成功代替另一种 |
| NIS → XeSS FG | 原有功能与 SDK 帧率统计保持正常 |
| CRT/未知效果、HDR、内容越界 | 保留拒绝，提示指出实际限制 |
| 对照底层 swapchain 与显示侧记录 | 判断是否有排队/丢弃；不将内容提交率写成实际显示率 |

隔离验证复用 `FSR3FGPolicyTests`、`FSR3FGMotionResourceTests`、`FGSettingsIdleTests` 和现有 FSR3 源码检查；按改动补充少量统计语义用例。产品构建、部署、真实 GPU/游戏和显示侧验收暂缓，不能用当前隔离测试替代；后续需另获构建/部署授权。

## 4. 基线、证据与边界

- 源码检查基线：`C:/Users/81443/Documents/Magpie enhance/.worktrees/0610-beta7/source`，`release/0.6.10-beta7` / `a90c7da2829f48587ee2ec8f37e12095a4e91ac8`。
- 当前本地包 manifest 记录产品构建 `a0f7145f41e16d3788a996fbfb7dbb179b1c0f4e`。本轮已核对上述关键 FSR3/policy/Renderer/统计/UI 文件从产品构建到检查基线没有差异。
- 用户日志仅标注 `0.6.10-beta7`，报错字符串、调用位置与该基线吻合；附件没有用户机器的二进制 hash，不能声称已经逐文件证明用户运行包完全一致。
- 原始附件：`C:/Users/81443/Downloads/logs.rar`，SHA-256 `9F882EBEF18F1C495F7B0FB0042944D29B0895189CEA9F35F68BFA16BEA57FBC`。归档只含 `magpie.1.log` 与 `magpie.log`，未附 dump、PresentMon 或 SDK 屏幕呈现追踪。
- 证据副本：`C:/Users/81443/Documents/Magpie enhance/diagnostics/20261012-fsr3fg-output-nis/`；保留原始 RAR、两个日志、三张截图及 `evidence-manifest.json`。本文件日志行号对应此副本。
- SDK 检查基线：`dependencies/FSR-SDK-v2.3.0/Kits/FidelityFX`，FG 3.1.6 / swapchain 3.1.7。
- 本轮确认的是统计缺失和启动拒绝的因果链；未实施修复，未在用户机器验证 NIS 放行后的运行质量，也未实测显示帧率。截图中的 FSR3 效果行耗时不足以代表独立 DX12 插帧全过程 GPU 用时，不作为这两项修复的验收指标。

## 5. 后续完整 Review 新增 TODO（2026-10-12）

完整证据、触发链及验证边界见 [FSR3 FG 完整源码 Review](../reviews/20261012-fsr3fg-full-review.md)。以下保留上轮发现的 6 项问题；本分支已完成源码修复，复选框仅代表实现及隔离验证状态，实机待验收另列。

- [x] **R1 / P1：修复 fence 完成判定。** 事件唤醒后循环复查完成值/设备移除，遵守总超时；覆盖旧事件提前唤醒及 timeout 后 drain，不以一次 WAIT_OBJECT_0 证明资源可以释放。提取生产函数的隔离探针已复现错误成功返回。
- [x] **R2 / P1：让 FSR3 的必需光流失败明确结束/报错。** 跨槽保留 provider fallback/失败原因，区分合法 bootstrap 和 provider 被永久停用；不能把所有无效运动都自动视为允许的 reset。保留其它消费者的既有 Zero fallback 策略。
- [x] **R3 / P2：适配 FSR SDK 实际呈现策略。** 修复应用 VRR 关闭而 SDK 工作线程仍请求 tearing 的契约不一致；评估 SyncInterval 映射或明确能力限制，以固定 SDK 工作线程表达式验证底层 flags 判定，不能只改日志。
- [ ] **R3 实机验收：** 真实底层 flags、帧节奏、背压及延迟测量（本轮不构建/部署）。
- [x] **R4 / P2：静止源支持 Live 参数重绘。** 将参数产生的新输出按独立 revision 发布为不插帧的重绘，不增加捕获帧 ID 或生成提交计数；避免被“不是新捕获”门禁整体丢弃。
- [x] **R5 / P2：传递最终效果输出的 history revision。** 上游参数或处理历史变化时，FSR3 在第一个新版本输入 reset 一次；不要每个正常输入都 reset。与 R4 共用已有版本概念，分别验证动态和静态源。
- [x] **R6 / P2：修正性能计时展示范围。** proxy FG 未测量时明确显示未知/不含本体，或者增加异步 D3D12 GPU 时间查询；不将 marker copy 的约 0.019 ms 当作整个 FG 成本。

对 P1-A 的口径补充：成功且 CPU reset=false 的 dispatch 也可能在 SDK GPU scene-change 检测后走直通。上述“非 CPU reset 的生成提交数量”只能落地为明确标注来源的“生成提交/调度数量”，不能据此承诺独立中间画面数量，更不能称为显示实测 FPS。详见完整 Review 的统计限定。

修复前完整 Review 的历史验证：129 项 FSR3 静态检查、77 项设置/idle 静态检查、48 项原生 FSR3 策略、438 项原生设置/idle、通用重复捕获提取测试、849 项 WARP 运动资源检查及当前本地包 payload 检查通过；fence/VRR 提取探针复现 R1/R3。未构建产品、部署、重启或运行实际游戏验收。


## 6. 实施结果（2026-10-12）

- [x] P1-A：单次成功 callback + Present 提交计数；独立 provenance；CPU reset/idle/失败/未知不虚增；无 multiplier 推算。
- [x] P1-B：仅新增 `NIS\NIS` / `NIS\NVSharpen`；CRT/未知效果仍拒绝。
- [x] P2：追加效果链/HDR 错误，既有 MP 编号不变；首屏列出不兼容效果名；中英繁体资源齐全。
- [x] R1：fence 事件唤醒后循环检查完成值、设备移除和总截止时间。
- [x] R2：provider 运行失败、停用、同步失败跨槽传递并停止 FSR3；单次 bootstrap 与有效零向量独立判断。
- [x] R3：固定 SDK 使用有效 SyncInterval 适配：允许 tearing 为 0，否则为 1；日志记录实际值，原 SDK DLL 不变。
- [x] R4：Live 参数修改产生的新处理版本可发布非 FG 重绘；不增加真实输入，重绘仅计 R/s。
- [x] R5：最终效果 history revision 随颜色同槽传递，新版本首次真实输入重置，普通输入不反复重置。
- [x] R6：FSR3/XeSS marker 耗时标为输入复制，FG 本体未测量，总计明确排除 SDK 补帧。
- [x] 隔离与源码验证：策略 66、设置/idle 444、故障/发布/SDK flags 69、WARP 1131；源检查 130 + 77 + 45；产品翻译单元 `/Zs` 通过（不生成 OBJ/EXE/DLL）。
- [ ] 新产品构建及链接验证（本轮明确不执行）。
- [ ] 真实 GPU、游戏、UI、5120×1440 NIS/AMDOF 与 NVOF、Live 参数、启停/resize、VRR 多显示器/延迟、PresentMon 验收（本轮明确不部署）。

R3 的取舍：固定 SDK 无法仅靠 flags 提供 `sync=0 / no tearing`；本次在不修改受签名和 hash 固定的 SDK DLL 前提下使用其已有 vsync 契约。关闭 VRR/tearing 时将等待垂直同步，节奏和延迟必须实机评估；没有宣称两种模式性能等价。
