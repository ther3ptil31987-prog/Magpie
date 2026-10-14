# FSR 3 帧生成效果器集成 TODO

日期：2026-10-11（Asia/Shanghai）。需求来源：[Magpie Issue 19](https://github.com/SAOG0721/Magpie/issues/19)。

状态：2026-10-11 维护者已启动目标模式与源码开发。功能分支 `feature/fsr3-frame-generation` 基于 Beta6 `556d4ac2`；本轮不编译、不部署、不生成 release。源码实现和验证见 [开发交接](../reviews/20261011-fsr3fg-source-development.md)，运行验收由后续编译部署会话执行。

目标：为喜鹊增加可选的实验性 FSR 3 FG 效果器，参考现有 DLSSFG、XeSSFG，实现固定 2× 补帧并与现有效果链、帧刷新、UI 和错误处理保持一致。当前实现 SDR、固定 2× 的实验性源码接入；后续仍需以生成画质、GPU 成本和呈现节奏决定是否继续发布。

## 1 基线与实施范围

- 主要调查基线：`C:\Users\81443\Documents\Magpie enhance\.worktrees\0610-beta6\source`，分支 `release/0.6.10-beta6`，提交 `556d4ac245e0fb83762788e57d2ad0eb5a26247f`。调查时该工作树干净。
- TODO 保存在日常源码 `source/docs/experimental/todos/`。日常源码 `experimental` 调查时为 `13adddb6`，另有无关未提交修改；不能将较早主线误作最新产品实现。
- 本地 SDK：`C:\Users\81443\Documents\Magpie enhance\dependencies\FSR-SDK-v2.3.0`。
- 本地 SDK 已有 `amd_fidelityfx_framegeneration_dx12.dll`；调查时 Beta6 分发目录仅包含 AMD loader 和 upscaler DLL，尚未包含 FG DLL。
- 开始实施前重新核对主线、已有工作树、最新产品修复和构建清单，选择包含应保留修复的准确提交。保护无关改动，不直接覆盖现有工作树或运行包。
- 后续执行依照维护者届时指令推进；构建、部署、上传和 GitHub 回复分别确认其授权范围。本 TODO 不安排自动执行或定时任务。

## 2 已确认事实与拟定方案

| 项目 | 事实或拟定行为 |
| --- | --- |
| Issue 诉求 | 增加 FSR 3 FG 作为另一种补帧选择；用户关于弱于 XeSS FG 的看法没有附比较证据，不作为验收结论 |
| 当前 FSR3 SR | `FSR3Upscaler` 使用 D3D12 和 D3D11 共享资源，仅实现超分 |
| 当前 FSR3FG | `EffectProtocolCatalogC.h` 有颜色协议占位，尚无可运行 FSR3FG 效果器后端 |
| DLSSFG 参考价值 | 主动生成纹理，再交由喜鹊呈现；可参考帧身份、历史重置、资源同步、故障恢复和诊断 |
| XeSSFG 参考价值 | D3D12 代理交换链生成并呈现；可参考 Presenter、共享资源、独立 UI 和容量等待 |
| 首版名称与 ID | 拟显示 `FSR 3 FG`，效果 ID 为 `FSR3FG\FSR3_FrameGeneration`，说明中标注实验性 |
| 首版倍率 | 固定 2×，每对基础帧生成一张中间帧；不复制 2–4× 参数 |
| 超分关系 | 不要求绑定 FSR3 SR，可与已验证的其他超分或图像效果组合 |
| 首版颜色范围 | 先验证 SDR；HDR 作为后续独立验收阶段，未验证组合须明确拒绝或提示 |
| FG 互斥 | 与 DLSSFG、XeSSFG、Nvidia VFX VFG 一条链只启用一个 |
| 光流设置 | 提供关闭外部运动估计、AMDOF、NVOF 的试验组合；默认选择待测 |
| 默认参数 | 固定 2×；重复帧过滤与外部光流默认值依据原型及现有产品规则确定，不预设未经验证的质量优势 |

主方案为新增 `FSR3FGPresenter`：捕获画面经过现有效果链，通过共享资源进入 D3D12，由 AMD FG 上下文和代理交换链完成插帧、排序与呈现。标记效果只用于选择与配置后端；其在列表中的位置不代表已支持任意位置补帧或逐生成帧执行后置效果。

SDK 另有 `FFX_FRAMEGENERATION_FLAG_NO_SWAPCHAIN_CONTEXT_NOTIFY`，可单独生成纹理后复用喜鹊呈现队列。该路线保留为兼容性、延迟和性能对照。AMD 当前文档推荐交换链回调，并不鼓励常规手动 dispatch；如改用直接生成路线，须记录原因并完整承担输出排序、节奏和生命周期，不能同时启用两套呈现调度。

FSR 自带内部光流，但接口仍需要运动向量、深度及相机信息。喜鹊没有游戏真实几何数据，现有 Frame Guidance 提供平面深度与估算运动。固定虚拟相机只能作为实验性替代输入，不应表述为满足游戏原生集成质量。

外部光流关闭只表示不额外提供 AMDOF/NVOF 运动估计，FSR 内部光流仍会运行。外部运动估计可能增加重复计算，画质收益及默认值必须实测。

## 本轮需求边界与源码进度

本轮按最新指令将工作拆为源码交付和后续运行验收。已实现 Presenter、SDK/provider 选择、Frame Guidance 映射、时序/重置、互斥/独立 UI、基础节奏、错误生命周期、构建分发配置及双语说明；详见开发交接中的源码勾选清单。下方含实机语义的条目保持未勾选，避免将源码实现当成运行通过。捕获时间戳优先，缺失时使用带来源的捕获接收间隔估计，不限制 WGC；HDR、超出窗口的裁剪和外部运动的未知几何映射明确拒绝。默认外部运动与重复过滤关闭，待实测再决定最终推荐。

## 3 P0 原型与继续条件

- [x] P0-01 核实实施基线（本轮源码范围完成；运行包未读取、未改动）。记录选用提交、未提交文件、SDK 版本、运行包来源和恢复基线；禁止以旧主线覆盖 Beta6 或更新产品修复。
- [x] P0-02 核对 SDK 头文件、loader、FG DLL 和许可证。保留准确版本、哈希与来源；DLL 文件版本不等于实际算法 provider 版本。
- [ ] P0-03 使用 `ffxQueryDescGetVersions` 查询并选择 FSR FG 3.1.6 及匹配的 3.x 交换链实现。版本 ID 来自查询，不能硬编码；记录实际创建的 provider，避免自动切到 FSR 4。不可用时给出明确错误。
- [ ] P0-04 验证渲染适配器与输出资源的兼容性，检查 D3D12、Shader Model、格式能力及共享资源条件，不仅按显卡厂商品牌判断。
- [ ] P0-05 制作独立 SDR、2× 原型，使用当前版本创建描述及 `PrepareV2`。初版关闭异步生成计算；呈现仍按 SDK 交换链的队列规则处理。
- [ ] P0-06 比较零运动、AMDOF、NVOF，统一使用明确的平面深度和虚拟相机约定。覆盖静止、水平和垂直平移、遮挡显露、细文字、粒子、快速转镜头、场景切换。
- [ ] P0-07 验证真实帧与生成帧的内容和顺序。首帧或重置帧只发布有效真实内容，不能将复制的真实帧计为成功插值。
- [ ] P0-08 测量同一基础输入帧率、分辨率下的生成耗时、总 GPU 成本、显存、延迟及输出间隔，并与 XeSSFG、可用硬件上的 DLSSFG 做对照。
- [ ] P0-09 形成继续结论和默认参数。出现严重 HUD/遮挡伪影、明显负收益或持续节奏问题时，记录复现和限制，先修原型再推进产品接入。

继续条件：原型能稳定生成正确中间帧，实际呈现顺序成立，替代输入的质量与开销达到维护者可接受水平。具体性能阈值和画质结论待测，不预填收益百分比。

## 4 P1 产品集成

- [ ] P1-01 新增 `FSR3FGPresenter.h/.cpp`，覆盖初始化、BeginFrame、EndFrame、容量等待、源帧时序、辅助资源、独立 overlay、Resize 和销毁。
- [ ] P1-02 复用共享资源和 fence 机制，明确前后端线程所有权、D3D11/D3D12 状态转换、队列依赖、缓冲复用条件和适配器 LUID。
- [ ] P1-03 每个基础帧使用相同 SDK 帧身份执行 `Configure → PrepareV2 → Present/生成回调`。创建 FG 与交换链时分别附上匹配的版本描述。
- [ ] P1-04 保留源 `frameId`、`captureSequence`、`resourceGeneration`、捕获时间戳；另管理 SDK 连续帧编号。SDK 连续编号不能掩盖源帧断裂、丢帧或资源代际变化。
- [ ] P1-05 处理场景切换、捕获重启、缩放尺寸变化、设备重建及失效光流的历史重置。帧间隔优先来自有效捕获时序，避免将排队和 Present 阻塞时间误作游戏运动时间。
- [ ] P1-06 定义 FSR 输入契约：运动方向、像素单位、比例、深度取值及正向/反向标志、near/far/FOV、虚拟相机基向量、零 jitter。不能依赖其他效果器或 HDR 开关偶然决定深度清除值。
- [ ] P1-07 明确捕获分辨率、辅助资源分辨率、效果输出、窗口尺寸和 `generationRect` 的映射；覆盖缩放、裁剪、黑边与非标准宽高比。不能直接复制 XeSSFG 现有的尺寸匹配假设。
- [ ] P1-08 处理 CRT 等几何变形对运动/深度坐标的影响。初版对未经验证的组合明确限制；若引入 distortion field，补充资源寿命和映射测试。
- [ ] P1-09 新增标记效果，注册效果工程、目录、搜索关键词、中文与英文说明、其余语言回退及参数可见性。说明补帧在最终呈现端执行。
- [ ] P1-10 扩展 `FrameGenerationEffectKind`、窄/宽字符串分类、链冲突验证及 Renderer 分派；与所有现有 FG 互斥，覆盖导入配置、重复效果和缺失后端。
- [ ] P1-11 将 Frame Guidance 需求收集、前端辅助资源传递和新帧判断从仅识别 XeSS 的分支扩展到 FSR。逐处检查现有 `_isXeSSFrameGenerationActive` 等条件，避免只创建了 Presenter 却漏掉资源路径。
- [ ] P1-12 接入重复帧过滤和静态画面策略。鼠标、工具栏或 UI 单独刷新不推进基础帧历史；源帧没有变化时不反复提交陈旧输入进行补帧。
- [ ] P1-13 调整 `ResolveFrameSyncBackend` 和输入帧率控制：喜鹊限制基础输入节奏，AMD 控制生成/真实帧的输出节奏。避免错误进入 XeLL、叠加 DWM 等待或重复 sleep；Reflex/Anti-Lag 不作为首版必需依赖。
- [ ] P1-14 复用独立 overlay 表面，保持喜鹊工具栏与可独立绘制的鼠标不进入 FG。已烘焙在捕获画面里的游戏 HUD 或光标不能宣称已分离；不得将带 HUD 的画面冒充 `HUDLessColor`。
- [ ] P1-15 明确上下文串行化边界，防止 Prepare、Present 回调、Resize 和销毁并发操作同一 FSR 上下文。即使关闭异步生成计算，也需遵守异步呈现/UI 资源寿命。
- [ ] P1-16 停止时先禁用 FG 和相关回调，排空实际使用资源的队列，再释放 FG 上下文和代理交换链。窗口同一 HWND 的交换链切换须先完成旧交换链释放。
- [ ] P1-17 完成失败处理：SDK 缺失、provider 不可用、资源格式不合法、初始化失败、运行时错误、设备移除、等待超时均提供明确诊断。无法安全恢复时停止缩放；安全退回真实帧时清楚报告 FG 已停用。
- [ ] P1-18 区分捕获帧数、基础提交数、SDK 生成数、呈现提交数和实际显示证据。记录 reset 原因、失效辅助资源、输入/输出尺寸与颜色、provider、适配器和等待耗时。

## 5 P2 HDR 性能与分发

- [ ] P2-01 为 FSR FG 明确 SDR/HDR 输入和输出格式、传递函数、亮度单位、曝光及颜色转换边界。现有 `FSR3FG` HDR 条目只是占位，不能直接作为验收依据。
- [ ] P2-02 验证 HDR 捕获、SDR→HDR、HDR→SDR、FP16/scRGB 与 HDR10 路径中的已支持组合；避免双重色调映射、误解码和错误亮度范围。不将 VFG 的逐生成帧后处理能力默认为 FSR Presenter 已具备。
- [ ] P2-03 核对代理交换链创建/Resize/Present 的 VRR 参数，覆盖窗口、无边框、不同刷新率、跨屏和输出环境变化。能力检查和 Present 标志不代表显示器已激活 VRR。
- [ ] P2-04 SDR 同步路径稳定后再对比异步生成计算。检查增益、显存和资源寿命；没有实测收益不默认开启。
- [ ] P2-05 新增独立构建开关与无 SDK 构建路径，更新编译单元、include 目录、效果复制及运行库复制。
- [ ] P2-06 完成主包、增量/OTA 与安装后文件清单，包含匹配的 AMD loader、FG DLL、必要文档和许可。验证 FSR FG 能在未启用 FSR SR 构建功能的预期组合中正确获取依赖。
- [ ] P2-07 更新效果说明与报错排查指南，注明固定 2×、辅助输入限制、硬件前提和已验证范围。不得把高呈现 FPS 等同于低输入延迟。
- [ ] P2-08 完成下表验收并回归 DLSSFG、XeSSFG、Nvidia VFX VFG 及无 FG 路径。发布与 issue 回复按届时指令执行。

## 6 验收矩阵

| 层级 | 最少覆盖 | 通过证据 |
| --- | --- | --- |
| 参数与状态测试 | 分类、互斥、固定倍率、光流可见性、帧编号、重置、无 SDK、失败回退 | 可重复测试与明确断言 |
| GPU 原型 | 准确 provider、颜色/辅助输入、正确中间帧、顺序、首帧和 reset | 图像样本、GPU 日志、D3D12 调试层无相关错误 |
| 完整捕获链 | 实际 WGC、不同基础帧率、重复帧、静止源、丢帧 | 捕获到呈现的帧身份和时序记录 |
| UI 与生命周期 | 独立鼠标/工具栏、连续启停、Resize、最小化、Alt-Tab、立即重启、设备移除 | 无死锁、陈旧资源、黑帧或错误历史复用 |
| 画质 | 快速转向、遮挡、粒子、细文字、游戏 HUD、字幕、场景切换 | 同源同设置录制及对比结论 |
| 性能与显示 | 60/120/144/165 Hz 等可用环境、窗口/无边框、VRR、RTSS 开关 | PresentMon/显示证据、GPU 时间、显存、延迟和呈现间隔 |
| 硬件 | 支持条件满足的 AMD/NVIDIA/Intel 代表设备 | 分 GPU/驱动记录，不将单卡成功推广到所有设备 |
| 回归与包 | 现有 FG、普通效果链、FSR SR 组合、旧配置、完整包及更新包 | 回归记录与可复现安装/更新检查 |

源码、静态检查、构建成功、SDK 冒烟、完整产品实测和真实显示验收分别记录。缺少的硬件项目保持待验，不填写推测结果。

## 7 代码与官方依据

| 入口 | 用途 |
| --- | --- |
| [Renderer.cpp](</C:/Users/81443/Documents/Magpie enhance/.worktrees/0610-beta6/source/src/Magpie.Core/Renderer.cpp>) | FG 选择、辅助资源、前后端时序和呈现 |
| [DLSSFrameGenerator.h](</C:/Users/81443/Documents/Magpie enhance/.worktrees/0610-beta6/source/src/Magpie.Core/DLSSFrameGenerator.h>) | 主动生成接口与发布回调 |
| [XeSSFGPresenter.h](</C:/Users/81443/Documents/Magpie enhance/.worktrees/0610-beta6/source/src/Magpie.Core/XeSSFGPresenter.h>) | 代理交换链 Presenter 参考 |
| [FSR3Upscaler.cpp](</C:/Users/81443/Documents/Magpie enhance/.worktrees/0610-beta6/source/src/Magpie.Core/FSR3Upscaler.cpp>) | AMD loader、查询和共享资源参考 |
| [FrameGuidanceTypes.h](</C:/Users/81443/Documents/Magpie enhance/.worktrees/0610-beta6/source/src/Magpie.Core/FrameGuidanceTypes.h>) | 帧身份、格式、方向与单位 |
| [FrameGuidanceD3D12Interop.h](</C:/Users/81443/Documents/Magpie enhance/.worktrees/0610-beta6/source/src/Magpie.Core/FrameGuidanceD3D12Interop.h>) | 辅助资源共享与同步 |
| [ScalingOptions.h](</C:/Users/81443/Documents/Magpie enhance/.worktrees/0610-beta6/source/src/Magpie.Core/include/ScalingOptions.h>) | FG 分类与互斥 |
| [FramePacingOptions.h](</C:/Users/81443/Documents/Magpie enhance/.worktrees/0610-beta6/source/src/Magpie.Core/include/FramePacingOptions.h>) | 帧同步后端选择 |
| [EffectProtocolCatalogC.h](</C:/Users/81443/Documents/Magpie enhance/.worktrees/0610-beta6/source/src/Magpie.Core/EffectProtocolCatalogC.h>) | FSR3FG HDR 占位 |
| [SDK FG 接口](</C:/Users/81443/Documents/Magpie enhance/dependencies/FSR-SDK-v2.3.0/Kits/FidelityFX/framegeneration/include/ffx_framegeneration.h>) | 版本、PrepareV2、输出和配置开关 |
| [SDK FSR3 provider](</C:/Users/81443/Documents/Magpie enhance/dependencies/FSR-SDK-v2.3.0/Kits/FidelityFX/framegeneration/fsr3/internal/ffx_provider_fsr3framegeneration.cpp>) | 内置光流与单输出插值实现 |

- [AMD FSR SDK](https://gpuopen.com/amd-fsr-sdk/)：SDK 组件与硬件前提。
- [FSR Frame Generation 3.1.6](https://gpuopen.com/manuals/fsr_sdk/techniques/frame-interpolation/)：算法版本与限制。
- [Frame Generation API](https://gpuopen.com/manuals/fsr_sdk/techniques/frame-interpolation-api/)：输入、回调、线程安全和资源寿命。
- [Frame Generation Swapchain 3.1.7](https://gpuopen.com/manuals/fsr_sdk/techniques/frame-interpolation-swap-chain/)：呈现、UI、节奏和销毁。
- [AMD FSR API 版本选择](https://gpuopen.com/manuals/fsr_sdk/getting-started/ffx-api/#version-selection)：查询与版本覆盖规则。

## 8 后续编译与运行验收入口

源码交付后，由单独会话从本功能分支编译，执行原生策略测试、SDK/捕获链冒烟及 P0-03 至 P0-09 的运行验证。通过后补齐 P1 运行验收，再进入 P2 的 HDR/异步计算与发布工作。按开发交接文件记录准确 provider、图像、性能、GPU/驱动和继续结论。本轮未执行编译、部署或任何实机验收。
