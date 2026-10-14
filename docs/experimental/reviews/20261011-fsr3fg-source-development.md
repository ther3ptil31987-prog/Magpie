# Issue 19 — FSR3 FG 源码开发与编译交接

日期：2026-10-11。范围来自本会话最新指令：开始开发、建立功能分支和目标；**本会话不编译、不部署、不生成新 release**。GPU 画质、性能及硬件验收交给后续会话，原 TODO 的实测继续条件仍有效，不因源码完成而视为通过。

## 基线与交付位置

- 功能分支：`feature/fsr3-frame-generation`。
- 工作树：`C:\Users\81443\Documents\Magpie enhance\worktrees\fsr3-frame-generation`。
- 基线：`556d4ac245e0fb83762788e57d2ad0eb5a26247f`，来自干净的 `release/0.6.10-beta6`，包括 VFX/TrueHDR 与后端生命周期修复。
- 保留 `source` 中已有未提交文件，不合并较旧 `experimental`，不改动运行包。另有独立的 `feature/0610-frame-refresh-modes` 工作树，本分支未读取或纳入其后续开发；后续集成需检查 Renderer/FramePacingOptions 的并行变更。
- 应用管理工作树工具因当前会话位于源码父目录返回 `Not a git repository`，随后通过 `git worktree add` 在实际源码仓库创建独立分支；没有改变原会话默认目录。

## 本轮源码需求

- [x] 固定 2×、支持捕获时间戳优先及带来源的捕获接收间隔估计、独立于超分、默认外部运动关闭和重复过滤关闭的 SDR 标记效果。
- [x] `FSR3FGPresenter`：D3D11/12 共享纹理、同 LUID、能力检查、PrepareV2、AMD FG 与代理交换链。
- [x] 查询 provider 并精确选择 FG 3.1.6 / 交换链 3.1.7，附 SDK ABI 版本；没有硬编码 provider ID、ML 自动替换或二进制补丁。
- [x] 源帧身份和连续 SDK ID 分离；重复输入抑制，捕获时间优先、CPU 捕获接收间隔备用、来源切换及丢帧/资源/会话/辅助资源重置策略。不限制 WGC。
- [x] 平面深度和虚拟相机独立契约；外部运动从最终效果尺寸映射到内容尺寸，黑边用 generationRect 定位。
- [x] 可解释的 SDR/HDR、裁剪与外部运动几何限制；未知几何效果可关闭外部运动试验内部光流。
- [x] Frame Guidance 收集/共享/消费者路由、FG 互斥、重复帧过滤和独立 UI 接入。
- [x] 基础输入由 StepTimer 限速，AMD 输出调度；不进入 XeLL 或重复前端限速。
- [x] 上下文串行化、停用/排空/销毁顺序、失败停止和无法安全清理时的进程重启要求。
- [x] 日志区分基础提交、回调调度、reset 复制帧和插值调度；不伪造显示帧数。
- [x] 独立构建开关、无 SDK stub、工程文件、运行库/许可/说明复制、包内容检查。
- [x] 双语目录、现有本地化参数复用及其余语言英文目录回退。
- [x] 纯 CPU 策略测试源码与后续编译 runner、源码/SDK 一致性审计脚本、HDR 端点回归断言。

## 源码验证记录

在源码层核对现有实现后撤回临时 WGC 限制：DLSSFG 的 NGX 调用不传帧时间差，其输出节奏由 Renderer 的 CaptureFrameCadence 管理；XeSSFG 原生路径用提交间隔估计，补丁兼容路径在捕获时间不可用时回退到扣除已测额外等待后的提交间隔。FSR 借鉴“捕获时间优先、估计回退”的原则，但用更早的后端捕获接收时刻作为备用，避免新增对较晚 Present 时刻的依赖。现有 DLSSFG/XeSSFG 时钟逻辑不变。

已运行：

```powershell
python -X utf8 tests/validate_fsr3fg_source.py --sdk-directory 'C:/Users/81443/Documents/Magpie enhance/dependencies/FSR-SDK-v2.3.0'
python -X utf8 tests/test_fsr3fg_package.py --sdk-directory 'C:/Users/81443/Documents/Magpie enhance/dependencies/FSR-SDK-v2.3.0'
pwsh -NoProfile -File scripts/Generate-EffectCatalog.ps1 -Check
git diff --check
```

FSR 源码审计通过 118 项检查，覆盖项目 XML、注册、参数/目录、本地化、DLL 哈希、时间回退接线和公共 SDK 描述字段。包验证器的独立临时文件夹测试通过：接受完整匹配依赖，拒绝缺失或错误 provider；未加载 DLL。5 个相关 PowerShell 脚本通过语法解析。上述检查不会执行编译器、HLSL 或 GPU，不能证明 C++ 类型正确、SDK 初始化成功或实际补帧效果。

`tests/Validate-EffectCatalog.ps1` 在功能分支和未修改的 Beta6 基线均失败于 `DLSSFG\DLSS_FrameGeneration` 的历史审阅哈希过期。新 FSR 条目的哈希、参数默认值及中英目录独立检查通过。本轮未借机更新其他效果的历史审阅。

未运行：任何 C++/HLSL 编译、native policy 可执行文件、SDK smoke、Magpie 启动、GPU/实机/游戏测试、PresentMon、部署或发布。文档中的源码完成勾选与原 TODO 的运行验收分开。

## 后续会话执行清单

1. 核对本分支提交及工作树状态，与届时最新产品分支/帧刷新分支按实际 diff 集成；不要从旧 `source/experimental` 直接构建并覆盖新修复。
2. 先运行源码审计和 `tests/Run-FSR3FGPolicyTests.ps1`。后者需要 VS C++ 工具链，会编译独立测试；本会话没有运行。
3. 用本地 `BuildOptions.props.user` 或 MSBuild 属性设 `EnableFSR3FrameGeneration=true`、`FSR3SdkDir=<SDK 2.3.0 根目录>`，显式恢复该工作树所需的 NuGet/Conan 和其余 SDK 依赖。不要依赖其他工作树的忽略文件自动出现。
4. 分别编译 x64 FG 开启、FG 关闭/无 SDK、FG 开启但 FSR SR 关闭的组合。优先编译 Presenter、Renderer 和资源工程，解决编译/链接诊断后才运行。
5. 完整包检查：`scripts/Test-FSR3FGPackage.ps1 -PackageDirectory <输出目录>`；检查安装/OTA 实际文件清单及更新后两颗 DLL 的哈希。Updater 的 `PackageFiles.h` 是旧版本清理表，本功能不为未发布版本修改它；新文件沿用递归包清单和移动逻辑。
6. 开启 D3D12 调试层，先进行 SDR、原生大小、无外部运动的 SDK/完整捕获冒烟。查准确 provider、首帧/reset 内容、遮挡场景、基础帧与生成帧顺序。SDK 的 reset pass-through 可能增加一次相同内容呈现，不应计为有效插值。
7. 覆盖 WGC / Desktop Duplication / GDI / DwmSharedSurface，确认没有原生时间戳时日志使用 capture-admission-estimate 且不每帧 reset。再验证缩放/黑边/宽高比、AMDOF/NVOF、失效光流、重复过滤、固定/自动/源内容模式、静止源及只动鼠标/工具栏。检查静止 UI 刷新不增加基础提交/历史。
8. 测连续启停、Resize、最小化恢复、Alt-Tab、捕获中断、设备移除、等待超时及缺失/错误运行库。重点确认 AMD 内部等待和异常销毁行为；宿主资源只在确认退役后释放。
9. 按原 TODO 的 P0-06～09 比较同源同设置的零运动、AMD/NVIDIA 外部运动、XeSSFG 与可用硬件上的 DLSSFG；记录图像、GPU 时间、显存、延迟和显示间隔后决定质量默认值及是否继续发布。
10. 回归 DLSSFG、XeSSFG、Nvidia VFX VFG、普通呈现及 FSR SR，执行现有 FrameSync/HDR/参数/前后台生命周期测试。修复并确认历史目录哈希问题与本改动无关。HDR 支持/异步计算仍为独立后续开发；VRR 已接公共策略，但真实显示激活未验。

详细输入、默认值、许可文件、SDK SHA256 与双语限制说明见 [FSR3FG.md](../effects/FSR3FG.md)。全部 GPU/显示项目保持待验；未经新授权，不推送、发布或回复 Issue 19。
