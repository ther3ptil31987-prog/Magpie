# FSR3 FG 完整源码 Review

> 实施更新：本报告保留修复前的证据和行号。修复分支、变更映射、隔离验证及尚未完成的实机验收见 [修复交接](20261012-fsr3fg-fixes-handoff.md)。

日期：2026-10-12（Asia/Shanghai）。基线：`release/0.6.10-beta7`，`a90c7da2829f48587ee2ec8f37e12095a4e91ac8`。

范围：FSR3 presenter、SDK 初始化与版本绑定、D3D11/D3D12 资源交接、运动映射、时序/reset、内容/idle/UI 呈现、参数变更、resize/退出、统计和构建打包契约。对照本地 SDK 2.3.0 的 FG 3.1.6 / swapchain 3.1.7 源码、用户日志与截图。未修改产品代码或运行包；执行了隔离 CPU/WARP 检查和模拟探针，没有启动产品、注入真实 GPU 故障或进行显示侧验收。

源码路径基准：`C:/Users/81443/Documents/Magpie enhance/.worktrees/0610-beta7/source/`。下列 `src/...:行号` 均相对此基准。SDK 路径基准：`C:/Users/81443/Documents/Magpie enhance/dependencies/FSR-SDK-v2.3.0/Kits/FidelityFX/`。

先前的“输出统计缺失”“NIS 白名单漏项”及其错误分类问题仍成立，本次不重复计为新发现。新增 **6 项可行动问题：2 项 P1、4 项 P2**。

## R1 · P1：fence 事件唤醒被直接当作目标值完成

位置：`src/Magpie.Core/FSR3FGPresenter.cpp:100–109`，重点 107–109 行。

`WaitForFence` 只在进入等待前读一次完成值。`WaitForSingleObject` 返回 `WAIT_OBJECT_0` 后直接返回 true，没有再次判断完成值是否达到本次目标，也没有重新检查 `UINT64_MAX`（device removed）。

具体触发链：一次 GPU 等待超时后，其 `SetEventOnCompletion` 注册不会随超时自动取消；随后 `Shutdown → DisableAndDrain → WaitForQueue` 复用同一个事件等待更大的 fence 值。旧目标稍后完成产生的事件，可以提前满足新一次 CPU 等待。即使没有超时，设备移除发生在首次检查之后，也可能唤醒事件而被误判成功。

隔离探针直接提取生产 `WaitForFence` 函数，模拟事件/fence 状态，观察到：

```text
Control: target 42 completes before wake -> accepted.
Stale event: target=42 completed=41 returned=true fatal=false
Removal during wait: completed=UINT64_MAX returned=true fatal=false
```

影响：调用方不能以返回 true 证明命令分配器可以复用或队列已经排空；异常清理的“确认安全后释放，否则保留资源”保证存在漏洞。模拟证实的是错误完成判定，未在实机复现资源损坏或崩溃。

修复：将完成值作为唯一成功条件，事件仅用作唤醒通知。等待循环在唤醒后复查 `GetCompletedValue` 与设备移除状态，使用一个有截止时间的总等待预算；超时与清理阶段复用事件也必须遵守同一条件。不要只补一个无条件 `ResetEvent`，那不能证明 GPU 已完成。

验收：正常完成、旧事件先到、新目标后到、等待期间设备移除、超时后进入 drain；未达到目标时不得返回成功。

## R2 · P1：必需的外部光流运行失败后，会永久静默直通

位置：`src/Magpie.Core/Renderer.cpp:1089–1093、4401–4423`；`FrameGuidanceService.cpp:658–680`；`FSR3FGPresenter.cpp:674–680`。

具体触发链：

1. 光流 provider 运行失败后，`FrameGuidanceService` 返回 zero fallback；连续失败 3 次将 `ready=false`，本次会话以后继续使用 Zero Motion。provider resize 失败也会进入该状态（553–559 行）。
2. Renderer 拒绝将 `isZero` fallback 作为有效外部运动，故 `_frontendMotionValid=false`。
3. 调用 `SetFrameGuidance` 时无条件把 `!_frontendMotionValid` 合入 `requiresHistoryReset`。
4. presenter 的缺失运动分支只有在 `!requiresHistoryReset` 时才报致命错误。因此这条实际调用链永远绕过该报错，后续每个输入都带 Guidance reset，持续按直通帧运行。

影响：FSR3 的“外部光流必需、运行失败明确停止”契约没有落实。用户仍看见 FSR3 启用，但不再产生有效插帧；当前统计缺失还会掩盖退化。用户所附成功会话没有发生这一失败，不能把它追认成本次截图的原因。

修复：跨共享槽保留光流失败/fallback 的原因与持续状态，不把“没有有效运动”自动等同于允许的首帧 reset。FSR3 对 provider 已停用、同步失败及非正常连续缺失给出明确失败；首次、捕获重启、合法 resize 的短暂 bootstrap 单独允许。正常静止画面的有效零向量必须继续接受，不能按向量数值全零判断失败。其它允许 Zero fallback 的消费者保持原策略。

验收：故障注入 provider 连续失败、resize 失败及同步失败；首帧/正常静止不误报；FSR3 不得无限 reset 并继续宣称正常。

## R3 · P2：应用的 VRR/tearing 关闭策略没有约束 SDK 实际呈现

位置：`src/Magpie.Core/FSR3FGPresenter.cpp:811`、`PresenterBase.cpp:180–188`；SDK `framegeneration/fsr3/dx12/FrameInterpolationSwapchainDX12.cpp:675–680、1239–1249、1955–1983`。

Magpie 通过 `_vrr.PresentFlags()` 控制 tearing；关闭时调用代理 `Present(0, 0)`。但固定 SDK：

- 底层创建在机器支持时自行增加 `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING`，并通过描述查询隐藏其内部修改。
- 插帧路径不使用调用方传入的 Present flags；工作线程只根据 tearing support、exclusive fullscreen 与 SyncInterval 自行生成 flags。
- Magpie 总是传 SyncInterval=0，所以即使应用关闭 VRR、或环境检查要求撤回 tearing 请求，SDK 在非独占路径仍会带 `DXGI_PRESENT_ALLOW_TEARING`。

探针组合生产 `VRRPresentationPolicy` 与提取的 SDK flags 表达式，复现：

```text
VRR off: application creation=0x0 present=0x0; SDK creation=0x840 worker present=0x200
```

影响：设置和 VRR 日志只能描述代理入口，不能代表底层请求。允许 tearing 不等于显示器已启用 VRR/G-SYNC；本次没有测量实际撕裂或驱动 VRR 状态。

修复：为固定 SDK 定义明确的呈现策略适配。在现有 SDK 能力内核查 SyncInterval 的映射；若使用非零 SyncInterval 关闭 tearing，要同步验证输出节奏、输入限速及延迟，不能直接改成 1 后宣布完成。若必须保留 `sync=0 / no tearing`，当前代理实现无法仅靠 flags 达成，需要受控的 SDK 适配或明确能力限制。不要让 UI 声称一个后端不支持的关闭保证，也不要仅修改 Magpie 日志掩盖实际请求。

验收：观察底层真实 swapchain 的 flags，而非仅查看 `PresenterBase` 日志；覆盖 VRR 开/关、显示器迁移/拓扑变化、idle 与恢复。

## R4 · P2：静止输入时，已接受的 Live 参数更新不会发布到画面

位置：`src/Magpie.Core/Renderer.cpp:2506–2519、4108–4111`。

触发：已有 FSR3 会话，画面静止且重复检测不再产生新捕获，修改支持 Live 的上游参数，例如已在白名单内的 FSR RCAS 锐化强度。

参数已应用并置 `_forceNextRender=true`，后台也按新的参数版本重跑效果；但尾部 `!isNewCaptureFrame && _HasProxyFrameGenerator()` 无条件 return，跳过 `_CompleteBackendFrame` 和发布。前端及 idle replay 继续显示旧的已完成纹理，直到下一次真实新输入到来。该门禁也影响其它 FG 路径，FSR3 会直接遇到它。

修复：区分“同一内容的无变化重复执行”与“用户参数修改产生的新输出”。新输出可以按独立的输出 revision 发布，并作为不插帧的重绘呈现；不得增加捕获帧 ID、伪造光流样本或把它计作真实输入。后续第一次有效输入需要重新建立对应的 FG 历史。复用现有 effect output revision，不新增一套通用任务框架。

验收：静止源/重复检测开启，修改锐化等 Live 参数后立即看到变化；真实输入和生成计数不被虚增，后续继续运动时恢复插帧。

## R5 · P2：上游处理版本变化没有传入 FSR3 的历史重置

位置：`src/Magpie.Core/Renderer.cpp:2506–2508、3935–3968、4338–4352`；`FSR3FGPolicy.h:9–11、34–63`。

触发：持续有新捕获的 FSR3 会话，修改上游着色器或支持 Live 的 native 效果参数，输出尺寸、capture sequence 和 resource generation 不变。

后台已有 `EffectFrameState` 的 output/history revision，参数变化也能沿效果链传播。该版本最终没有带入 FSR3 的源样本：FSR3 只看帧 ID、时间、捕获会话、资源代次和光流/几何 reset；publication 的参数版本仅在 VFG 分支填写。若新输入连续、光流正常、几何未变，FSR3 会将修改前后两种处理结果作为同一历史连续插值。

影响：跨处理版本的混合与拖影风险；源码确认缺少重置传播，具体视觉表现尚未实机复现。SDK 内部场景切换检测不保证把每次参数修改识别为 scene cut。

修复：把最终效果输出的 history revision 随颜色/运动/身份同槽发布，由 presenter 在版本改变时 reset 一次；普通新捕获不改变该 revision。不要从前端直接无同步读取后台正在修改的参数状态。本项与 R4 可共用输出版本传递，但动态输入的 reset 和静止输入的重绘是两个需要分别验收的行为。

验收：新捕获连续时改变上游参数，首个新版本恰好 reset 一次；未改变参数的连续输入不反复 reset；失败的参数更新和不影响当前输出的隐藏参数不触发无谓重置。

## R6 · P2：性能面板把占位 shader 耗时当作 FSR3 效果耗时

位置：`src/Effects/FSR3FG/FSR3_FrameGeneration.hlsl:49–50`；`Renderer.cpp:3914、4104`；`OverlayDrawer.cpp:2246–2250、2320–2322`；`FSR3FGPresenter.cpp:740–840`。

FSR3 marker 的唯一 pass 是纹理采样直通。效果 profiler 只覆盖后台 D3D11 pass，而 Prepare、FSR 内部光流及插帧位于前端独立 D3D12 队列，未纳入该计时。UI 仍用 `FSR3_FrameGeneration` 名称展示 marker 数字并计入“总计”，使截图中的约 0.019 ms 容易被理解成完整补帧成本。

修复：在没有有效 SDK GPU 计时前，明确将 proxy FG 本体时间显示为未测量，或将现有数字标为输入复制并标明总计不包含 FG；不要以 CPU Present 墙钟时间替代 GPU 时间。若增加计时，使用 D3D12 timestamp/query 在实际执行队列测量 Prepare/内部 OF/FG，异步回收结果，避免每帧同步等 GPU。共享 profiler 的 XeSS 情况也要回归，但不顺带重构无关效果。

验收：profiling 关闭不额外同步；启用后数字的范围与标签一致，不能再用 marker 小耗时代表整个 FSR3。

## 对上一轮统计方案的补充限定

`GenerateFrame` 中的 `interpolationDispatches` 只区分 CPU 可见的 reset。SDK GPU shader 的 `ffx_frameinterpolation_setup.h:32` 还可因 `HasSceneChanged()` 清零历史；`ffx_frameinterpolation.h:172–181` 随后直接复制当前帧。该 GPU 决策不会自动反馈成 Magpie 的 CPU `desc->reset`。

因此，上次 TODO 中“成功非 reset 调度”的统计只能称为生成提交/调度计数，不能承诺是具有新中间画面内容的“有效插帧率”。这里不是要求增加逐帧像素读回，而是要求保留准确的计数口径；底层呈现和显示实测继续独立。该项修正统计方案，不计入上述 6 个新缺陷。

## 已检查但没有新增缺陷结论的路径

- FG/交换链 provider 的精确版本选择、DLL 固定路径、x64 与 AMDOF 构建前置条件、发布文件 pin。
- D3D11/D3D12 适配器 LUID 对齐、正常路径双向 fence、共享颜色/运动格式、运动 SRV 修复、内容区域缩放与像素方向。
- reset、idle SDK ID 与源帧 ID 分离；正常重复输入拒绝；捕获 session/resource/timestamp 跳变重置；idle 不执行 Prepare/FG。
- SDK 返回的等待句柄是 DuplicateHandle，应用关闭自己的副本是合理的；未将此误报为 SDK handle 被过早关闭。
- resize 排空后销毁/重建 FG 资源；正常退出先排空、释放 SDK context/COM，再卸载 DLL。R1 是异常排空完成判定的缺陷，不等于正常退出顺序全错。
- 原图对比走独立 overlay，FSR 持续接收处理后的图像；不需要因单纯显示/隐藏对比层而重置 FSR 色彩历史。
- FSR3 自身光流选项使用 restart 模式；R4/R5 指上游支持 Live 的效果参数，不指直接热切 FSR3 光流 provider。

## 验证结果与待验证范围

| 检查 | 本轮结果 | 证明范围 |
| --- | --- | --- |
| FSR3 源码检查 | 129 项通过 | 注册、符号、契约文本等静态关系 |
| FG 设置/idle 静态检查 | 77 项通过 | 现有跨文件策略 |
| FSR3FGPolicyTests | 48 项通过 | 编译执行生产 CPU 策略 |
| FGSettingsIdleTests | 438 项通过 | 参数、idle/history 与帧率 CPU 行为 |
| 通用重复捕获策略提取测试 | 通过 | 全局模式、游戏模式、像素/metadata 与捕获重启 |
| FSR3FGMotionResourceTests | 849 项通过，D3D11 debug layer 检查通过 | WARP 上共享复制、SRV、向量映射与边界 |
| 当前本地包 FSR3 payload 检查 | 通过 | DLL hash、marker、AMDOF 编译能力标记 |
| fence/VRR 隔离探针 | 复现 R1/R3 的错误判定/flags 不一致 | 提取生产函数与 SDK 表达式；模拟状态，不是实机故障复现 |

这些通过项不覆盖 R2 的跨模块错误传播、R4/R5 的产品热调链路，也不包含 FSR3 SDK 端到端、真实 GPU 画质/延迟、HDR、多显示器/VRR 或屏幕实际显示帧率。不能据此宣称完整运行验收通过。SDK 内部部分等待没有宿主可取消超时，设备丢失/呈现线程异常还需要隔离进程故障注入；目前不另作一个未经复现的“必然死锁”结论。

证据目录：`C:/Users/81443/Documents/Magpie enhance/diagnostics/20261012-fsr3fg-output-nis/full-review/`。其中 `build_review_probes.py` 从固定源码提取探针，`review_probes.cpp` 为输出，`probe-manifest.json` 记录输入 SHA-256，`probe-results.txt` 为实测 stdout。原生测试 EXE/OBJ 仅位于临时目录；运行结果另记 `validation-summary.json`。

实施顺序建议：先 R1/R2，随后原有 NIS/统计问题与 R3，再做 R4/R5/R6；所有修改继续以原始失败场景及本报告的针对性验收为准。
