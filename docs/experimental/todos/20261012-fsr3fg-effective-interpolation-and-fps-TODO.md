# FSR3 FG 有效插帧、呈现统计和问号显示 TODO

> **当前执行入口：** 用户已确认将 FSR3 光流默认修复与跨 FG 重复设置/空闲重绘合并，请以[合并执行 TODO](20261012-fg-input-settings-and-idle-redraw-TODO.md)为准。本文保留早期诊断；问号、完整异步输出统计和 Core 名称问题仍作为独立附带待办。

> **2026-10-12 后续结论：** 用户已实测“外部光流有效、None 无可见收益”，本机新日志确认 AMDOF Quality-100% 稳定运行。主要修复方案已收敛到[移除 None、默认 AMDOF 质量](20261012-fsr3fg-require-optical-flow-TODO.md)。该新方案取代本文 P0 的广泛排查优先顺序和条件式默认值决策；本文其余内容保留为较早诊断记录，P1 的问号、统计与名称问题继续独立跟踪。

日期：2026-10-12（Asia/Shanghai）。状态：**只读诊断完成，以下修复和新增诊断均待执行**。

用户反馈：缩放已经可以启动，但工具栏显示“问号 / 原始帧”，切换原始对比及体感没有看到 FSR3 帧生成收益。本轮不修改产品源码/参数/配置，不启动新缩放，不编译部署；仅保存本文。前次修复及验收交接见 [Beta7 运行修复](../reviews/20261011-beta7-fsr3fg-runtime-fix.md)。

## 1. 结论与边界

1. **问号已定位到两层确定原因。** FSR Presenter 每次返回的总呈现帧数都是 `std::nullopt`，工具栏因此输出 `—/基础帧率 FPS`；等宽帧率字体只包含 ASCII，缺少 U+2014，ImGui 回退为 `?`。这解释了问号，不等价于 SDK 没有运行。
2. **最新会话已经持续调用 SDK，未重现之前的 SRV 故障。** 1320 次基础提交、1320 次 callback、1319 次“非宿主 reset 且 SDK 返回成功”的插值 dispatch。不能把 1319 当作有效中间图像数或屏幕显示数。
3. **实际图像/屏幕无收益的唯一根因尚不能从现有日志确定。** 优先检查零运动默认输入导致的插值质量退化、SDK 内部场景变化重置、生成输出是否进入最终呈现，以及原图覆盖层是否正确清除。当前日志缺少区分这些分支的证据。
4. **原始对比有可见意义，但不是 FG 真开关。** 原始模式用独立 DComp 层覆盖原图；底层 FSR 仍运行。关闭对比时清透明层，露出底层输出。它可以观察原图与处理结果，但不能用其计数验证“关闭 FG 后的运行路径”，也不能排除覆盖层/合成节奏干扰。
5. **上次名称统一漏了 Core 叠加层。** App picker/helper 已统一，运行时 `OverlayDrawer::GetEffectDisplayName` 仍保留旧 XeSS 别名并对 FSR/DLSS 回退文件名。列为本次附带 TODO，尚未修复。

不能将用户的体感问题归结为“只是计数显示”。需要同时证明生成图像发生了正确的中间运动，以及这些图像按预期节奏呈现。

## 2. 当前基线与实机日志

- 当前产品分支：`release/0.6.10-beta7`，HEAD / 部署清单均为 `34c3d6dd13fe11cd0cae94a2ab23e936a866d95a`。
- `fe31c082` 已经通过 `213f031c` 合入；随后 `34c3d6dd` 合入独立 DLSS runtime discovery 修复。后续执行必须从最新产品基线准备分支，不直接用旧 `fe31c082` 覆盖产品。
- 检查时 Beta7 工作树干净；FSR Presenter、Motion、FrontendMotionTexture 与 `fe31c082` 没有差异。
- 当前 EXE SHA256：`9281BCB6FBF4B9CD30EAE4312D06CA102481141198D6792BA6F6999CC0A6A392`，实际文件匹配部署清单；两颗 AMD runtime 实际哈希也匹配清单和 SDK 固定值。
- 构建会话记录 `reports/0610-beta7-fg-fixes-20261011/SUMMARY.txt`：已完整构建部署；motion WARP 849 项通过，D3D11 debug layer 已检查。**这些已有结果不包含 AMD SDK 的有效图像/最终显示验收。** 本轮未重跑编译测试。
- 最新运行日志：`release/v0.6.10-beta7/Magpie-Experimental-x64/logs/magpie.log`。本次完整读取 227176 字节 / 2003 行；其中含少量非 UTF-8 字节，统计使用替换解码，以下 FSR ASCII 字段及时间可正常解析。未更改原日志。

| 时间（本地）/ 日志行 | 证据 |
| --- | --- |
| 00:00:53.785 / 1897–1904 | 仅 `FSR3FG\FSR3_FrameGeneration`，参数为空，采用默认 None；PotPlayer 窗口 |
| 00:00:53.905 / 1924 | RTX 5070 Ti，2560×1440 SDR，固定 2×，`externalMotion=false` |
| 00:00:53.906–53.962 / 1926–1935 | WGC，捕获 1924×1177，基础限速 60，VRR 关闭 |
| 00:00:53.983 / 1951 | source/sdk=1，resetMask=81，bootstrap；1 次 reset pass-through |
| 00:00:55.999 / 1956 | source/sdk=120，resetMask=0，119 次非 reset dispatch |
| 00:01:16.301 / 1994 | source/sdk=1320，resetMask=0，inputMs=16.666，callback=1320，interpolationDispatches=1319，resetPassThroughs=1，invalidMotionSamples=0 |
| 00:01:16.546–16.548 / 1995–1998 | 用户热键结束缩放；此 FSR 会话没有 error 日志 |

这段会话中原图对比打开 8 次、关闭 8 次。日志只能证明约 60 次/秒的接收/基础提交；**PotPlayer 素材真实帧率、不同图像帧率及最终显示帧率仍未知**。重复帧过滤当时关闭，不能把 WGC 到达频率直接视为素材运动采样频率。

## 3. 源码定位

以下产品行号以 Beta7 `34c3d6dd` 为准；SDK 路径相对本地 `dependencies/FSR-SDK-v2.3.0/Kits/FidelityFX/`。

### A. 总帧率未知和缺字形（已确认）

- `src/Magpie.Core/FSR3FGPresenter.cpp:804`：成功提交后无条件 `_lastPresentedFrameCount = std::nullopt`。
- `Renderer.cpp:1233–1238`、`PresentationFrameRate.h:22–27`：记录基础帧并将总数标记为未知。
- `OverlayDrawer.cpp:392–397`：未知时格式为 `—/{} FPS`。
- `OverlayDrawer.cpp:633–650,1100`、`OverlayHelper.h:45–46`：帧率用 `_fontMonoNumbers`，范围仅 ASCII 数字和其余 Basic Latin，没有 U+2014。已检查本地 ImGui fallback 路径，缺字形会优先找 replacement character，再找 ASCII `?`。
- `XeSSFGPresenter.cpp:999–1004`：通过 `xefgSwapChainGetLastPresentStatus(...).framesPresented` 获取 SDK 报告数量。FSR 没有接入对应统计，不能简单照抄 XeSS 的每次同步回传语义。

### B. 插值成功计数缺少图像证据（已确认）

- `FSR3FGPresenter.cpp:529–547`：callback 的 reset 布尔值为假且 `ffxDispatch` 返回成功就增加 `interpolations`，未读取生成图像，也未观察最终呈现。
- SDK `framegeneration/fsr3/include/gpu/frameinterpolation/ffx_frameinterpolation_setup.h:32`：GPU 还会因 `HasSceneChanged()` 将内部历史计数归零。
- SDK `ffx_frameinterpolation_callbacks_hlsl.h:412–424`：场景切换状态覆盖最近 4 个历史位。
- SDK `ffx_frameinterpolation.h:174–178`：内部计数为 0 时复制当前帧。该内部结果不会反写宿主的 `desc->reset`，所以当前 `resetPassThroughs=1` **不能排除 SDK 内部仍在复制当前帧**。本次是否频繁发生尚未测量。
- 已启用 `FFX_FRAMEGENERATION_ENABLE_DEBUG_CHECKING`，但没有配置 `ffxConfigureDescGlobalDebug1` 消息回调；默认 debugger 输出未进入 Magpie 日志。

### C. 默认 None 不是纯光流模式（输入事实确认，影响程度待测）

- `FSR3FGPresenter.cpp:353–363,775–780`：无外部运动时传零 motion，深度恒为 1.0；相机也为合成参数。
- SDK provider `framegeneration/fsr3/internal/ffx_provider_fsr3framegeneration.cpp:570–587` 确实运行内部 optical flow，不能说 None 关闭了全部光流。
- 但 SDK `ffx_frameinterpolation_game_motion_vector_field.h:46–75` 仍将输入的零向量建立为 game motion field；平面深度并不自动把这一路标成无效。
- SDK `ffx_frameinterpolation.h:87–165` 同时用 game motion 与 optical-flow field 重投影，并以相似度/遮挡结果混合；reset 后前 10 个内部帧偏向 game motion。
- 因此“None + 零运动 + 平面深度”的真实表现可能是弱运动补偿、混合/重影或复制，**不能由内部 OF 确实执行推断已获得有效插帧**。尚没有当前素材回读，不能宣布这是本次唯一根因，也不能仅改默认值就交付。

### D. 对比和最终呈现（已确认路径，最终行为待测）

- `Renderer.cpp:1800–1816`：`SetPassThroughActive` 只切原图状态，没有通知 FG 停用。
- `Renderer.cpp:1115–1123,1296–1347`：独立 overlay 下底层继续输入处理图像，原图对比另行覆盖到 DComp 层；退出对比清透明。
- `FSR3FGPresenter.cpp:732`：每个基础帧仍配置 `frameGenerationEnabled=true`。
- SDK `FrameInterpolationSwapchainDX12.cpp:1921–1950,1991–2012`：callback 成功后提交生成命令，生成帧和真实帧进入异步呈现队列；`presentToSwapChain` 在约 702 行调用内部 DXGI Present。Magpie 当前既没有观察该路径的图像，也没有记录每张生成/真实帧的呈现合成回调。
- SDK `FrameInterpolationSwapchainDX12.cpp:2324–2332`：`GetFrameStatistics` / `GetLastPresentCount` 转发到底层真实 swapchain，可作为后续统计探针入口。后者是 Present 调用数，不能直接冒充实际显示帧数。

### E. 运行中叠加层名称遗漏（已确认）

`OverlayDrawer.cpp:667–677` 仍单独处理 XeSS 旧名，其结果用于性能窗口、工具栏效果菜单和运行时参数标题（776、1042、1753 等处）。应复用统一的窄/宽字符显示名映射，避免 App 与 Core 再次分叉。

## 4. 待执行 TODO

### P0：证明中间图像和实际呈现

- [ ] **P0-01 固定复现输入和基线。** 从执行时最新 Beta7 准备隔离修复分支，记录 SDK/EXE 哈希、显卡、显示刷新率、WGC、基础限速、None/AMDOF/NVOF、对比状态。确认素材真实 FPS；另用确定的 30/60 FPS 匀速平移纹理/带帧编号的测试源，区分捕获重复帧与独立图像。当前素材 FPS 未知时保留 TBD。
- [ ] **P0-02 完成 SDK 诊断链。** 接入 SDK debug message callback；提供受控诊断开关，启用 tear lines、reset indicators、debug view/pacing lines，核对本 SDK 中 configure/prepare 的 flags 转发。记录宿主 reset 与 SDK debug 显示的场景重置，不把二者混成一个计数。增加会话结束总计和来源标签，将现有 `interpolations` 明确命名为 dispatch 成功计数。
- [ ] **P0-03 取得生成图像证据。** 在 SDK generation/present 回调的合法 D3D12 命令阶段做有界异步 readback，以 frameID、捕获会话、资源代次匹配前帧/当前帧/生成帧。测量生成帧分别与两端的差异，核对匀速目标中点位置，排除复制帧和简单混合造成的假阳性。先测试无黑边 1:1，再复现 1924×1177 内容矩形及缩放。
- [ ] **P0-04 检查图像是否真正进入呈现。** 用 `ffxCallbackDescFrameGenerationPresent::isGeneratedFrame`、frameID、时间记录生成/真实帧合成事件；通过 SDK 转发的 DXGI 统计和 PresentMon/ETW 对照最终节奏，区分“调度成功”“合成提交”“显示”。若替换 present callback，必须继续完成 `currentBackBuffer → outputSwapChainBuffer` 的正确复制/状态转换，不能只计数然后返回成功；SDK worker 中不调用前端 D3D11/UI，不持主线程 apiMutex，不同步阻塞读回。Resize/Stop 先停回调、排空后释放诊断资源。
- [ ] **P0-05 增加真正 FG 开/关对照。** 诊断模式在相同捕获、基础 FPS、图像链和 swapchain 条件下通过 SDK 配置切换 generation enabled，恢复时正确 reset。单独验证关闭原图比较后透明层确实清除。保留原有原图比较用途，不能把它偷偷改成所有 FG 的算法开关；对比原图覆盖期间明确说明底层处理仍活跃。
- [ ] **P0-06 根据证据修正算法输入。** 对同一运动片段比较 None / AMDOF / 支持硬件上的 NVOF，以及可信已知运动场。若可信运动/AMDOF 能产生正确中间位置而 None 不能，优先让 FSR 首次使用选择已验证的外部运动来源，并为 None 的能力/限制给出明确说明；处理已有配置的显式选择，不静默重写用户参数。若所有来源都失败，先处理内部 reset、场景检测、颜色/历史输入、内容矩形和资源同步；若生成正确而显示失败，修呈现/覆盖/节奏路径。不得通过写死 2× 数字、关闭必要 reset 或改 SDK 保留 flags 掩盖故障。

### P1：帧率显示和名称补齐

- [ ] **P1-01 修复未知值问号。** 优先用现有字体支持的 ASCII `--` 表示未知，并通过本地化提示写明“输出帧率未测量 / 基础输入帧率”；或补 U+2014 字形并更新 `ImGuiFontsCacheManager.cpp` 的字体缓存版本。覆盖工具栏、性能窗口、不同语言/DPI 和旧字体缓存。未知不能显示为零或宣称无补帧。
- [ ] **P1-02 接入有来源的异步呈现统计。** 验证本机 SDK 3.1.7 转发的 DXGI GetFrameStatistics 能力；有效时使用累计计数/时间窗口，处理统计不可用、disjoint、回绕、启动/停止/Resize。若只能取得回调/Present 调用数，标为“SDK 提交”，不标为“屏幕显示”；无可靠数据继续未知。FSR 的异步统计不能硬塞成当前基础帧的同步返回值，避免滞后一帧导致丢基础帧/双计数。绝不使用基础 FPS × 2 伪造总数。
- [ ] **P1-03 让状态可解释。** 将固定倍率、基础输入率、SDK 是否调度以及统计是否可用分别表达；reset/预热不算已证实的有效中间帧。诊断完成前不因调用返回成功就展示“插帧正常”。
- [ ] **P1-04 补齐 Core 叠加层名称。** 将 DLSS / XeSS / FSR3 的 `FrameGeneration` 显示名映射提到 App/Core 可共用的代码，覆盖 picker、列表、组合名、冲突提示、工具栏效果列表、运行中参数页和性能窗口；保留原 ID、搜索词与 Nvidia VFX 系列名。

### P2：回归与验收

- [ ] **P2-01 SDK 图像回归。** 真实 FSR SDK 跑已知运动序列，验证中间位置、非复制帧比例、连续历史和内部 reset 指示；覆盖冷启动前 10 帧/稳定段、静止、场景切换、光流暂缺恢复、黑边、尺寸变化。已有 WARP 849 项只覆盖运动纹理/重映射，不能替代本项。
- [ ] **P2-02 呈现验收。** 在刷新率足够的显示器上，基础 30/60 FPS 各跑至少 60 秒，核对真实/生成图像交替、显示间隔、丢帧/重复显示及可见运动收益；显示 FPS 翻倍但图像只是复制/混合不算通过。对比开关、隐藏工具栏、光标独立刷新、VRR 关/开分组，先固定一组再拓展。
- [ ] **P2-03 生命周期和 UI 回归。** 启停、Alt-Tab、最小化/恢复、Resize、SDK 回调退出、统计重置；验证所有名字和未知状态字形。回归 XeSS 统计与外部光流、DLSS、普通呈现；不修改其他 FG 已有统计含义。
- [ ] **P2-04 分层交接。** 记录源码检查、编译、运动资源测试、SDK 图像测试、SDK 呈现统计和屏幕实测的各自结果。后续编译/部署按届时用户授权执行，保留当前 Beta7 和回滚点。

## 5. 执行顺序与通过条件

先 P0-01～05 建立图像/呈现证据，依据分支结果做 P0-06；P1-01、P1-04 可独立修复；P1-02 依赖 P0-04 的统计能力核验。最后做 P2，而不是先把工具栏数字改成 120 再判断成功。

最低通过条件：已知匀速运动的生成帧位于正确中间位置，并在实际输出路径显示；长时间稳定段不能一直内部 reset/复制；FG 真开关有可重复差异；UI 数字有明确数据来源，未知没有缺字形；原有配置与其他 FG 无回归。不能保证所有素材都获得同等收益，运动遮挡/HUD 质量仍需独立记录。

## 6. 参考与本轮边界

- [AMD Frame Generation API](https://gpuopen.com/manuals/fsr_sdk/techniques/frame-interpolation-api/)：debug callback、reset/tear/debug view 和呈现 callback；实现时以本地 SDK 2.3.0 实际字段/转发为准。
- [AMD Swapchain 3.1.7](https://gpuopen.com/manuals/fsr_sdk/techniques/frame-interpolation-swap-chain/)：插值命令、UI 合成和异步呈现队列职责。
- [Microsoft GetLastPresentCount](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-getlastpresentcount)：Present 调用次数。
- [Microsoft DXGI_FRAME_STATISTICS](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ns-dxgi-dxgi_frame_statistics)：显示统计与 Present 调用次数并不等同，需核验可用性。

本轮仅读取当前源码/固定 SDK、构建验收记录和现有日志，核验 3 个部署文件哈希，解析上述 FSR 会话，检查字体范围和现有名称路由，并新建本文。没有运行编译器、加载 SDK 做新测试、切换设置、修改源码、部署、提交或推送。
