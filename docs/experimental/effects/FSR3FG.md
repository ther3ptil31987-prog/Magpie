# FSR3 FrameGeneration

状态：实验性功能，固定 2×、仅 SDR。已整合到 Beta8，并作为 0.6.10 待审核分发内容。源码、原生隔离与 WARP 检查不能替代真实游戏画质、性能及显示验收；准确构建结果以分发清单为准。默认源码构建开关关闭。

## 使用范围

- 效果 ID：`FSR3FG\FSR3_FrameGeneration`。固定 2×，不支持 3×/4×/多帧倍率；导入配置里的 `multiplier` 不改变倍率。
- 补帧在最终图像处理后执行，不要求同时使用 FSR 超分；一条链只允许一个 FG。
- 不限制为 WGC。优先使用捕获时间戳；缺失时使用接受新捕获帧时记录的 CPU 单调时钟作为间隔估计，并明确标记来源。不同捕获方法仍分别待实测。
- 首版仅支持 SDR / RGBA8 / BT.709。HDR 兼容模式、HDR 转换组件会被明确拒绝，HDR 接入留待单独开发与验收。
- 必须使用外部光流，默认 AMDOF 质量，保留 NVOF。旧“无”/缺省/非法 method 迁移为 AMDOF 质量；已有合法 AMDOF 性能/质量或 NVOF 选择保留。FSR 自带内部光流仍运行。
- 重复帧检测统一使用通用设置；移除效果级开关和覆盖。当前允许的外部运动链由 `FSR3FGPolicy.h` 的 `SupportsFSR3FGExternalMotion` 列出：FSR/DLSS/XeSS SR、DLSSNR、VFX Upscale/VSR、基础矩形缩放、NIS/NVSharpen、FSR EASU/RCAS、帧率过滤及 FSR FG 标记。CRT、几何变形和未知自定义效果会明确拒绝，请从该效果组移除。尚未实现 distortion field。
- 支持画面在输出内部的缩放、黑边与不同宽高比映射；内容矩形超出输出的裁剪被拒绝。游戏自己画入图像内部的电影黑边不等于喜鹊的输出黑边。
- 喜鹊工具栏与可独立绘制的鼠标走单独的 DirectComposition UI 层。游戏 HUD 和捕获中已经烘焙的鼠标仍留在图像中，无法自动分离。
- 喜鹊限制基础帧输入，AMD 交换链控制插值和真实帧呈现。三种帧同步选择在 FSR FG 下均使用基础帧 Async 限速；不依赖 XeLL、Reflex 或 Anti-Lag。补帧不增加游戏逻辑帧率，也不能据此宣称降低输入延迟。

<a id="compatible-effect-list"></a>

## 完整效果组合清单 / Compatible effects

使用固定 2× SDR 补帧时，效果组可包含以下条目。清单与 `FSR3FGPolicy.h::SupportsFSR3FGExternalMotion` 对应；`Utility\FrameRate_Filter` 为旧 ID 兼容入口。组合须保持矩形坐标关系，画面完整位于输出区域内。HDR 组件、CRT、几何变形和清单外效果不在当前支持范围。

For fixed 2x SDR generation, the group can contain the entries below. This list follows `SupportsFSR3FGExternalMotion`; `Utility\FrameRate_Filter` is a legacy ID. Chains must preserve rectangular coordinates with the image entirely inside the output area. HDR components, CRT, geometry distortion, and unlisted effects are outside the current scope.

<!-- fsr3fg-supported-effects:start -->
| 效果 / Effect | 效果 ID / Effect ID |
| --- | --- |
| FSR3 FrameGeneration | `FSR3FG\FSR3_FrameGeneration` |
| DLSS SR | `DLSS\DLSS_SR` |
| FSR2 SR | `FSR2\FSR2_SR` |
| FSR3 SR | `FSR3\FSR3_SR` |
| FSR4 SR | `FSR4\FSR4_SR` |
| XeSS SR | `XeSS\XeSS_SR` |
| DLSSNR | `DLSSNR\DLSSNR_AI_Filter` |
| Nvidia VFX VSR | `RTXVideo\RTXVideo_VSR` |
| Nvidia VFX Upscale | `NvidiaVFX\Upscale` |
| NIS | `NIS\NIS` |
| NVSharpen | `NIS\NVSharpen` |
| Bilinear | `Bilinear` |
| Bicubic | `Bicubic` |
| Lanczos | `Lanczos` |
| Nearest | `Nearest` |
| 帧率限制 / Frame-rate limiter | `FrameRate_Filter` |
| 旧版限帧 ID / Legacy limiter ID | `Utility\FrameRate_Filter` |
| FSR EASU | `FSR\FSR_EASU` |
| FSR RCAS | `FSR\FSR_RCAS` |
<!-- fsr3fg-supported-effects:end -->

初次设置、共用帧率选项和颜色组合见[内置效果使用说明](../../Built-in%20effects.md#current-branch-setup)。

For initial setup, shared frame-rate controls, and color chains, see the [built-in effect guide](../../Built-in%20effects.md#current-branch-setup).

## 实验性输入契约

运动向量来自现有 Frame Guidance，方向为当前帧到前帧，单位为像素。后端先适配到最终效果输出，Presenter 再按内容矩形缩放向量，在辅助纹理左上角写入；`generationRect` 表示内容在整个交换链中的位置。首帧或明确 reset 时允许合法零纹理占位；正常插帧须有对应的光流输入。provider 运行/resize/同步失败会明确停止，连续缺失不能以 reset 无限直通。有效的全零光流代表静止，不是错误。缓存重绘不提交新的光流样本。

前端在共享槽事务内复制运动纹理，FSR 的稳定副本带有 `D3D11_BIND_SHADER_RESOURCE` 供重映射采样。帧身份匹配但资源格式、布局、绑定或设备不符时明确报错停止；这类内部契约错误不当作普通的暂缺运动数据处理。

深度为独立生成的正向有限平面深度 1.0，不读取 HDR/SR 的深度清除选项。虚拟相机 near=0.1、far=1000、垂直 FOV=60°、右/上/前为单位轴、位置与 jitter 为零。这不等价于真实游戏深度、相机或运动输入。快速运动、遮挡、文字和 HUD 质量必须实测。

源帧 ID、捕获会话、资源代际及捕获时间戳独立于连续 SDK ID。WGC 的原生捕获时间优先；其他捕获方式使用后端在接受新帧时已有的 `_lastCapturedFrameTime`，通过独立元数据字段传给 Presenter。它是接收间隔估计，可能受捕获调度与背压影响，不是游戏渲染时长，也不取自较晚的发布/Present 时刻。日志分别标记 `capture-timestamp`、`capture-admission-estimate` 和首帧 `bootstrap`；时钟来源切换重置一次，两种时钟不混算。重复 UI 刷新不提交基础帧；帧跳跃、会话/资源变化、无效时间、超过 500ms 的间隔、光流/几何变化和上游处理版本变化重置历史。SDK 在首帧或 reset 时可能呈现真实内容的复制帧，它们单独计为 `resetPassThroughs`，不计入 `interpolationDispatches`。

`baseSubmits` 是代理交换链提交，`callbackDispatches` / `interpolationDispatches` 是 SDK 命令调度数量，均不是屏幕实际显示帧数。UI 不显示由 `baseFPS × 2` 伪造的实测帧率；显示事件仍需 PresentMon/显示端证据。

## 通用重复检测与空闲重绘

DLSS / XeSS / FSR3 / VFG 不再提供 `duplicateFrameFiltering`。旧效果字段会迁移移除，不覆盖通用保存值；简单模式继续使用既有策略，高级模式使用通用设置，3D 模式规则与普通效果一致。

启用已有“空闲持续重绘”后，约 0.5 秒没有新内容才进入缓存重绘，避免把 24/30 FPS 的正常输入间隔误判为空闲并反复重置插帧。随后按有效重绘率呈现最后完成的输出，受内容上限和显示刷新率限制，倍率不乘到重绘率。关闭则保持最后画面等待。恢复内容、队列中的真实/生成帧优先；捕获中断或错误不当作正常空闲。

FSR3 在重绘期间通过 SDK 关闭生成，只复制和 Present；不运行 Prepare/内部光流/插帧，不修改捕获时间戳或源帧身份。SDK 呈现序号仍递增，恢复新内容时重置插帧历史。空闲重绘不重跑前后置效果，包括依赖 TIME 的效果；FG 下这些效果仍随新内容推进，普通非 FG 的动态效果语义不变。

工具栏 `R/s` 与性能面板单独显示成功提交的缓存重绘次数；不合入内容/真实 FPS，也不当作屏幕扫描率。无新内容时性能面板显示“等待新画面”。FSR3 显示“内容提交 / 真实输入”和“已提交生成帧率”，来自本次成功回调及代理 Present 的提交结果；CPU reset 的复制帧不计生成，未知回调保留未知。GPU 内部场景切换仍可能产生复制帧，提交数不等于 SDK 呈现报告、具有新像素的有效中间帧数或屏幕实测帧率。XeSS 保留其 SDK 报告口径。

静止源上的 Live 参数修改会立即发布新处理版本并以非 FG 重绘显示；它不伪造捕获/运动输入、不计真实或生成帧率，后续新输入重置历史。性能面板里的 FSR3/XeSS 时间只测到 D3D11 输入复制，SDK 的 D3D12 补帧耗时显示为未测量，总计明确不含该部分。

## VRR / 垂直同步

固定交换链 SDK 3.1.7 的插帧路径忽略调用者的 tearing flags。为执行应用的 tearing 策略，允许 tearing 时使用 `Present(0, ALLOW_TEARING)`，关闭或环境不允许时使用 `Present(1, 0)`；idle 也遵守该策略。SDK 可能仍在内部创建允许 tearing 的交换链，但同步呈现不发出 tearing 请求。允许 tearing 不等于驱动或显示器已经启用 VRR。

这一适配未修改 SDK DLL；非零 SyncInterval 可能改变基础帧背压和延迟，多显示器、实际刷新节奏和输入延迟仍需实机验收。

## 构建与依赖（交给构建会话执行）

仅 x64 开启 `EnableFSR3FrameGeneration=true`，`FSR3SdkDir` 指向 FSR SDK 2.3.0 根目录。该功能独立于 `EnableFSR3ZeroMV`，不隐式启用 SR，但会在未指定时启用 `EnableAmdOpticalFlow=true`；显式关闭 AMDOF 与 FSR3 FG 冲突，构建和打包预检均会拒绝。NVOF 仍按原能力检查启用。`-SkipBuild` 也检查 marker 和二进制中的 AMDOF 能力标记，旧产物不能与新设置混包。

运行库必须来自同一 SDK：

| 文件 | SHA256 |
| --- | --- |
| `amd_fidelityfx_loader_dx12.dll` | `E2D85AA05A9BD9ED8B38935FDF5199372CCA6F74C12015143BB6F945EE1608AA` |
| `amd_fidelityfx_framegeneration_dx12.dll` | `02297BEEDD285E822D3A64F314CF00FAF378DCEC0EDC47FF0C4DD71B3A8C2F18` |

DLL 文件版本不代表算法版本。通过 `ffxQueryDescGetVersions` 获取 provider ID，精确选择 FG **3.1.6** 和交换链 **3.1.7**；禁止缺失时自动替换成 ML FG 4。创建描述附带当前 SDK ABI 版本描述，准备阶段使用 `PrepareV2`。不使用二进制补丁或厂商身份覆盖。

硬件检查包括所选适配器 LUID、D3D12 FL12.0、SM6.2、typed UAV load/store、必要格式与实际共享资源创建。通过这些检查只表示满足初始化前提，不能推断画质、实时性能或全部显卡兼容性。

工程会复制 loader、FG DLL、标记效果、`AMD-FSR-SDK-LICENSE.md`、`AMD-FSR-SDK-THIRD-PARTY.md` 和本说明 `FSR3FG-README.md`。`Build-Release.ps1` 的递归文件清单包含它们，启用该功能时会调用只读 `Test-FSR3FGPackage.ps1` 检查。升级/OTA 必须沿用完整包清单并验证实际安装，不能只复制 Magpie.exe。

## 错误与生命周期

缺少 DLL/provider、HDR/几何条件不满足、设备/资源创建失败会保留步骤与错误码。运行期 SDK 错误、设备移除或宿主等待超时会停止本次缩放，不悄悄展示为 FG 正常。查看日志中的 `FSR3FG` 行。

运行期首个 Presenter 失败的阶段和 HRESULT 会进入错误详情；运动资源错误附带帧、会话、代次和纹理描述。AMD SDK 返回值保留在上下文中，系统错误字段使用 `E_FAIL`，不把 AMD 枚举伪装成 HRESULT。后续清理失败不会覆盖首因。

Prepare、Configure、代理 Present、Resize 和销毁由前端串行执行并保护上下文；固定 SDK 的 FG callback 在 Present 内同步执行，不操作 UI。异步生成计算关闭，但 SDK 仍有异步呈现线程。停止/Resize 先禁用 FG 和回调，等待 SDK 呈现与两侧 GPU 工作，再销毁 FG、交换链及 DLL。若 SDK/GPU 清理无法确认，会保留该会话资源到进程退出并拒绝再次启用 FSR FG；需要完整重启喜鹊。宿主 fence/容量等待有超时诊断，SDK 自身内部等待仍需故障注入实测。

## English summary

Experimental FSR3 FrameGeneration, fixed 2x SDR. Beta7 includes the initial integration; this required-optical-flow and idle-redraw revision has not yet been compiled or GPU tested. No WGC-only restriction: native capture timestamps take priority; missing timestamps use a separately labeled CPU capture-admission estimate, not game render time. Independent of FSR upscaling. Uses pinned FSR 3.1.6 and swapchain 3.1.7 providers from SDK 2.3.0, with PrepareV2 and synchronous generation compute. Requires an x64 build with `EnableFSR3FrameGeneration=true`; supply `FSR3SdkDir` explicitly.

External motion is required: AMDOF Quality by default, with NVOF also available. Retired None/missing settings migrate to AMDOF Quality. Internal FSR optical flow remains active. Duplicate detection uses common settings. Enabled idle redraw replays cached output after about 0.5 seconds without new content; it does not advance capture/motion/FG history. R/s counts successful redraw submissions separately from content FPS. Synthetic depth and a virtual camera do not replace game geometry. Validate occlusion, fast motion, HUD/text, cost and latency. HDR, out-of-window cropping and unsupported distortion chains are rejected. Only one frame generator is allowed per group. This revision is not compiled or GPU-validated yet.

Magpie's overlay is separate; baked game HUD is not. Native timestamps or a labeled capture-admission estimate drive temporal input; source discontinuities and clock-source changes reset history. SDK reset pass-throughs and dispatch counts are not evidence of successful interpolation or display events. Runtime failure stops scaling. If shutdown cannot prove resources retired, restart Magpie before using FSR FG again.

References: [AMD FG API](https://gpuopen.com/manuals/fsr_sdk/techniques/frame-interpolation-api/), [swapchain](https://gpuopen.com/manuals/fsr_sdk/techniques/frame-interpolation-swap-chain/), [FSR 3.1.6](https://gpuopen.com/manuals/fsr_sdk/techniques/frame-interpolation/).
