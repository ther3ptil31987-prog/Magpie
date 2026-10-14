# HDR 输出时工具栏与浮窗亮度自适应 TODO

日期：2026-10-13（Asia/Shanghai）。状态：功能分支源码实现及隔离验证完成；未构建应用或新 release，未部署，未做真实 HDR 显示验收。

目标：效果链最终输出 HDR 时，上方工具栏、性能分析、实时参数面板及其弹出菜单、提示保持清晰可读；亮度随已经生效的输出参考白调整，并限制过暗或过亮。

用户已选择：**自动跟随参考白，并限制过暗或过亮**。后续已授权按本 TODO 实施并创建功能分支；产品构建、release 与部署交由独立会话。

## 1. 基线与证据边界

- [x] 检查工作树与已有修改。诊断源码基线为 `.worktrees/0610-beta8/source`，`release/0.6.10-beta8`，提交 `cbe2c2cbda103e6cb6f686a5521918ed2501c32e`；该工作树干净。
- [x] `release/v0.6.10-beta8/Magpie-Experimental-x64/build-manifest.json` 指向上述提交，`sourceDirty=false`。日常 `source` 的 HEAD 为 `13adddb6`，有已有未提交修改；本 TODO 归档在那里不代表以旧主线作为实现基线。
- [x] 日常源码与 Beta8 的 `ImGuiImplPS.hlsl`、`ImGuiBackend.cpp` 无版本差异，核心缺口一致。
- [ ] 真实触发会话待确认。进程路径只读查询未取得结果；所查 Beta8 当前及轮转日志没有找到 `IsHdrCompatibilityEnabled: true` 会话，当前保存的效果组未发现 HDR 节点。因此本报告确认源码机制，尚未把具体效果链、显示器和用户所见帧对应起来，也没有进行像素读回或现场复现。

下列源码位置均相对于 `.worktrees/0610-beta8/source`，行号以本次基线为准。

## 2. 已确认的源码缺口

### 2.1 UI 没有 HDR 白点映射

- `OverlayDrawer.cpp:151–160,205`：工具栏、性能分析、实时参数面板统一走 `ImGuiImpl::Draw`。
- `ImGuiImpl.cpp:329–344`、`ImGuiBackend.cpp:32–76`：没有向 UI 像素着色器传入输出色彩域或 UI 参考白。
- `shaders/ImGuiImplPS.hlsl:4–5`：直接返回顶点颜色 RGB，只用字体纹理调制 alpha；没有 sRGB 解码或 nits 缩放。
- `AdaptivePresenter.cpp:35–37,87–88`：HDR 目标为 FP16 / 线性 scRGB。`CompSwapchainPresenter.cpp:150–152` 同样在 HDR 时使用 FP16。
- `Renderer.cpp:1132–1147`：主画面经过效果链后，UI 才绘制进呈现目标。因此增大 HDR 效果器的白点不会自动改变 UI 颜色。

Windows HDR scRGB 中，`(1,1,1)` 代表名义 80 nits。当前不透明白色 UI 因此仍只有约 80 nits 的信号白点，而场景参考白可能已是 203、400 nits 或更高。另有独立的色彩问题：SDR UI 的编码值被直接作为线性值使用；不能只乘一个系数而继续忽略传递函数。

这里的 nits 是输出信号的名义亮度，不是对显示器面板的实测亮度。

### 2.2 半透明底板会让 HDR 高亮透过

- `OverlayDrawer.cpp:899–901`：工具栏背景为 `(15,15,15,180)`，完全展开时底板 alpha 为 `180/255 ≈ 0.706`，并另外乘淡入淡出的整体 alpha。
- `ImGuiBackend.cpp:261–265`：RGB 使用 `SRC_ALPHA / INV_SRC_ALPHA` 混合。
- 在当前线性 HDR 路径，以 1000-nit 白色背景为例，完全展开工具栏底板的信号亮度约为 `80 × 15/255 × 180/255 + 1000 × (1−180/255) = 297.44 nits`，已经超过 80-nit 文字。

这是基于现有代码的合成计算，说明高亮透过可以压过文字；并非用户当前画面的测量结果。仅把文字调到 203 nits 仍不足以解决这个例子。浮窗使用 `ImGui::StyleColorsDark()`，应在实施时核对实际依赖版本的 `WindowBg / PopupBg` alpha，一起验证。

### 2.3 XeSS 的 UI 目标与场景编码不同

- `XeSSFGPresenter.cpp:21–35,432–434`：HDR 场景终端为 HDR10/PQ R10，独立 UI 是 FP16 DirectComposition 表面，采用预乘 alpha。
- `Renderer.cpp:1318–1352`：独立 UI 仍调用同一个 OverlayDrawer。
- UI 应按它自己的 scRGB 目标编码，不能因场景元数据标记为 PQ 就把 UI 也编码为 PQ。
- `FSR3FGPresenter.cpp:573–574` 当前明确不支持 HDR，本任务只回归其 SDR UI，不扩展 FSR3 HDR 能力。

## 3. 建议的最小修改方案

### 3.1 亮度规则

- [x] 首版建议 `uiWhiteNits = clamp(effectiveOutputWhiteNits, 160, 300)`。160/300 是待实机校准的产品初值，不是 HDR 标准或用户已经确定的精确值。
- [x] 优先使用最终 HDR 输出的已生效参考白；若没有可信值，复用 `Win32Helper::GetMonitorSdrWhiteNits` 读取**输出窗口所在显示器**的 SDR 白点，仍不可用时回退到 203 nits；对结果统一检查有限、正值并限幅。显示器信息缓存到会话/显示变化事件，不逐帧枚举。
- [ ] 可选后续：首版采用固定 300-nit 舒适上限，暂未接入物理显示器峰值。如后续接入已确认有效的输出显示器能力上限，应用于舒适上限；不能把效果器预测峰值误当物理显示器上限。现有 `FrameSourceBase.cpp:294–329` 查询的是源窗口显示器，而 `HdrComponentRuntime.h:31` 会把效果器峰值写入 `displayPeakNits`，两者均不能直接充当独立的输出显示能力证明。
- [x] 按输出参考白而非最高高光亮度调整。`peakNits=1000/2000` 不等于 UI 应输出 1000/2000 nits。场景明暗变化本身不触发 UI 明暗变化，首版不增加直方图、GPU 读回或自动曝光机制。
- [x] 只在最终输出确为 HDR 且 UI 目标为线性 FP16 时启用；链中出现 HDR 节点但末端回到 SDR 时沿用现有 SDR 绘制。

参考白来源的具体规则：

| 输出方式 | 参考白取值与注意点 |
| --- | --- |
| 原生 SDR→HDR | 新字段 `nativeWhiteNits`；兼容旧参数时使用实际比例映射的 `whiteNits × exposure`。生产 shader 在 `HdrSurfaceAdapter.cpp:150–153` 明确乘 exposure；现有 `sdrWhiteNits` 元数据没有单独包含这个旧配置的乘数。不要改写用户原参数。 |
| HDR→SDR→HDR 配对恢复 | 使用最终恢复节点对应的上游配对参考白，遵守 `pairIndex`；不能取链中第一个 HDR 节点，也不能套用原生 SDR 映射的 exposure 规则。 |
| RTX Video HDR / TrueHDR | 采用管线声明的 SDR 白点；目前计划默认是 203 nits。这是 UI 参考约定，不是 SDK 实测的漫反射白；不把 `middleGray` 或 `peakNits` 直接当纸白。当前 VFX TrueHDR 的效果 ID 仍是 `RTXVideo\\RTXVideo_HDR`。 |
| 元数据缺失/无效 | 输出显示器的 SDR 白点，然后 203 nits 回退，最终限幅。 |

`ColorDescription.referenceWhiteNits=80` 是 scRGB 固定单位，不能把它误当效果器的可调纸白；可调值是 `sdrWhiteNits` 及必要的模式语义。

### 3.2 统一在 UI 绘制入口转换

- [x] 在 `Renderer → OverlayDrawer → ImGuiImpl → ImGuiBackend` 传递本次 UI 目标类型和已解析的 UI 白点；只传绘制所需的小量状态，不在每个面板重复查找效果器。
- [x] 为 `ImGuiImplPS.hlsl` 增加受控 HDR 分支与像素常量：`rgb = SRGBToLinear(color.rgb) × uiWhiteNits / 80`；字体覆盖率与 alpha 保留原语义。SDR 分支维持现有输出。
- [x] 按既有 `ImGuiBackend` 生命周期创建、更新和绑定像素常量，包含 ImGui 重置渲染状态回调路径。不得在 8-bit 顶点颜色里提前放大，否则会截断；不得在 HDR shader 输出端把 RGB 再限制到 `[0,1]`。
- [x] 共用现有混合方式：shader 输出未预乘颜色，混合阶段完成预乘。独立透明表面清零后应得到正确的预乘结果，不要在 shader 再乘一次 alpha。
- [x] 统一覆盖文字、图标、折线/柱状图、控件、菜单及 tooltip，避免只修工具栏。

### 3.3 保证底板对比度

- [x] HDR 稳态下将工具栏、性能分析、参数面板以及弹出菜单/tooltip 的底板改为不透明深色，作为首版可靠方案；沿用原有配色，经统一的 sRGB→线性映射绘制。
- [x] 调整底板的 style alpha 或显式背景设置，不全局提高 shader alpha，以免破坏文字抗锯齿、禁用态、阴影与透明边缘。保留工具栏显示/隐藏动画；完全展开时达到可读标准，淡出末端自然不可见不算故障。
- [ ] 可选后续：若实机需要恢复少量透明感，再依据最坏亮场验证具体透明度；不能用低透明度加极亮文字来抵消透光。

### 3.4 与实际已显示画面同步

- [x] 复用现有已成功应用参数与帧发布链路。`Renderer.cpp:2524–2530` 成功后更新运行时 HDR 计划；`:2236–2265` 计算输出元数据；`:4321–4347` 发布到前台。
- [x] 将已解析的有效参考白随对应帧快照发布。现有元数据足够时直接使用；旧 exposure 等模式无法无歧义表达时，附带一个专用 UI 白点标量即可，不为本功能改动全局 HDR 色彩协议语义。
- [x] 普通重绘选对应 base frame 的快照；稳定帧/独立叠加层跟随实际成功呈现的场景快照。前台不得直接读取后台可变 `_runtimeEffectOptions` 或 `_pipelineOutputMetadata`，也不能使用尚未应用的参数面板草稿。
- [x] 调节成功、复位成功、参数失败回退、静止画面、无新捕获帧时重绘均保持画面/UI 配对；必要时复用既有 overlay dirty / idle redraw 信号。
- [x] HDR/SDR 切换、重启、尺寸变化及输出显示器变化时重新解析目标状态；没有有效场景快照的首帧使用已验证初始化值或上述回退值。

## 4. 实施 TODO 与验收

- [x] 基于诊断时 Beta8 对应代码或已经整合它的更新主线实施；先复查当时工作树，保留其他未提交工作。不要把旧日常主线直接覆盖当前运行版本。
- [x] 完成亮度解析、帧快照传递、ImGui shader 和 HDR 底板四项修改；无需新增设置页、手动亮度开关、后台服务或重排用户效果链。
- [x] 复用现有 HDR 组件测试，针对性覆盖参考白来源、配对、旧 exposure、无效数值、160/203/300 边界以及最终输出 SDR。扩展已有能力足够的测试入口，不机械跑完整无关迁移/目录套件。
- [x] 用 WARP/离屏验证真实生产 UI shader：白点 203→scRGB 2.5375、300→3.75；灰色/彩色的 sRGB 解码；透明度与字体覆盖率；透明表面只预乘一次；SDR 分支保持原输出。加入 1000/4000-nit 明亮背景，确认底板阻挡、文字与图表可见。
- [x] 复用 frontend/idle redraw 检查，覆盖成功呈现与失败/丢帧，以及参数成功/拒绝时的 UI 白点同步。仅在相关路径改动后运行对应 Toolbar/ParameterInput 回归。
- [x] 四个生产 C++ 翻译单元通过 MSVC `/Zs /W4 /WX`；生产 HLSL 通过 FXC `/WX`，像素常量布局和绑定经 WARP 验证。未构建应用 EXE/DLL。
- [ ] 后续会话执行完整应用构建、release 打包、部署及启动。
- [ ] 真实 HDR 屏幕验收：普通自适应呈现、Composition 呈现、XeSS 独立 UI；按可用硬件覆盖 DLSS/VFX HDR 呈现组合。FSR3 仅做 SDR 回归。
- [ ] 场景至少覆盖暗场、天空/雪地/白色网页等亮场；UI 覆盖工具栏、性能分析、参数面板、下拉菜单、tooltip、禁用态和图表。调整输出参考白、复位、静止源、切换前后效果、窗口/全屏及多屏移动。
- [ ] 与用户共同校准 160～300 nits 初值及不透明底板观感；确认清楚可读且不会因高光上限变化而刺眼。保留普通 SDR 的颜色、透明度、输入与布局行为。

完成标准：用户原始 HDR 场景下的工具栏与浮窗可读，随已生效的参考白在约定上下限内变化，亮场不再透过底板压过文字，SDR 无回退。源码审查、数值计算、WARP 与真实显示分别记录，不能互相替代。

## 5. 参考

- [Microsoft：DirectX Advanced Color / SDR white level adjustment](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range)。支持 scRGB 的 80-nit 单位、线性合成与白点缩放；本方案的 160～300 nits 限幅及不透明底板属于本项目待验收的产品选择。


## 6. 本轮源码交付

分支：`feature/hdr-overlay-brightness`，基于 Beta8 / `cbe2c2cb`。实现和验证记录见 [源码交接](../reviews/20261013-hdr-overlay-brightness-implementation.md)。已通过 62 个 WARP 用例、提交快照测试、四个生产翻译单元的语法检查、工具栏与参数输入回归，以及 77 项 FG idle 源码检查。完整应用构建、新 release、部署和真实 HDR 显示验收仍待后续独立会话。
