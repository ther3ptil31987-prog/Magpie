# Magpie Experimental 0.6.10 Beta5

本地测试版，继承已部署 Beta4 49de3f25，整合截至 2026-10-09 的功能更新。公开更新入口保持 0.6.9。

## 中文

- **VSR 颜色与 VFG 档位：** VSR 使用统一 RGBA 模型输入输出，在 BGRA 纹理边界显式转换；TrueHDR packed 输出经无损字节纹理导出。VFG 调整为性能／均衡两档，旧 High 配置在加载或导入时迁移为均衡。RTSS 初始化按钮隐藏或恢复后重新测量工具按钮宽度。实际游戏颜色、布局及性能仍需验收。
- **VFX 更新：** 合入 `feature/nvidia-vfx-optimization` 的缓冲轮换、比较线程组归约、纹理视图缓存及编码匹配；四项统一套件改用经签名与哈希检查的十个运行 DLL 和完整许可通知。Async／CUDA 直连保持关闭，详细计时仍按原开关启用；真实 GPU 画质、稳定性及性能另行验收。
- **RTSS 状态操作：** 初始化按钮简化为“初始化”；检测到 Magpie 独立不限帧配置后隐藏，配置改变或检测失败时保留重试。初始化与启动工具采用同页默认控件字号。
- **配置页闪退修复：** RTSS 状态分段列表改用支持 XAML 对象绑定的集合，修复打开配置文件时的 `0x80070057` 异常。增加真实 ItemsSource 回归，覆盖全部状态、管理范围及状态更新。
- **NVIDIA VFX 统一套件：** 本地构建启用匹配的官方 1.3.0.0 Core 与四项功能库。加入 Nvidia VFX Upscale 自动／五档倍率与最终尺寸适配；统一 VSR 的分类与档位、旧别名和参数迁移；接入 SDR VFG 2–8× 及逐输出帧 TrueHDR 后处理。TrueHDR 保留原四项参数、精确输入和步进，新增默认开启的去色带。四槽背压、资源代次与参数 revision 保留；实际 GPU 功能、颜色、稳定性和性能仍需验收。
- **截图内容：** 主页可选择原始输入与最终效果双图、仅最终效果、仅原始输入；默认仅最终效果。双图使用同一真实帧编号，独立保存 input/output 文件；异步等待、会话变化、文件竞争及部分失败有明确处理。效果／通道的独立 PNG／DDS 导出保留。
- **RTSS：** 状态按内容分段换行，加入 Magpie 独立不限帧配置初始化；按每项操作实际可用的接口、字段和运行条件判断能力。版本号用于诊断，恢复条件不阻断开始，停止仅作一次尽力恢复。新配置及尚未迁移的旧配置默认请求管理员启动，之后保留用户主动关闭的选择；实际 UAC、自启动与游戏限帧另行验收。
- **继承功能：** 保留 Beta4 的手动 HDR 链、旧试用组安全迁移、HDR PNG／DDS 导出与清理保护、UWP／参数输入协调、VRR 开销优化及已对齐的开关样式。

## English

This local Beta5 retains deployed Beta4 (49de3f25). The public update entry stays at 0.6.9.

- **VSR color and VFG modes:** use RGBA model I/O with explicit conversion at BGRA texture boundaries and lossless byte-carrier export for packed TrueHDR output. VFG offers Performance/Balanced; loading or importing retired High settings migrates them to Balanced. Re-measure RTSS tool actions when initialization visibility changes. Live game color, layout and performance acceptance remain pending.
- **VFX update:** integrate buffer rotation, group-reduced frame comparisons, cached texture views and encoding matching from `feature/nvidia-vfx-optimization`. Distribute the four-feature suite with ten verified runtime DLLs and license notices. Async/CUDA handoff stay disabled; GPU quality, stability and performance acceptance remain pending.
- **RTSS status actions:** shorten the Initialize label, hide it once Magpie's own unlimited profile is verified, and restore it when initialization cannot be confirmed. Initialize and Launch tools inherit the normal control font size.
- **Profile page crash fix:** use a XAML-bindable RTSS status collection to prevent `0x80070057` when opening a profile. Native ItemsSource regression covers all states, management scopes and status updates.
- **Unified NVIDIA VFX:** enable the matched official 1.3.0.0 Core and Upscale/VSR/VFG/TrueHDR libraries for local testing. Add native upscale selection and final size adaptation, VSR categories and legacy migration, SDR VFG at 2–8x, and per-output-frame TrueHDR processing. Preserve exact HDR parameter input and stepping, with debanding enabled by default. GPU functionality, color, lifetime and performance acceptance remain pending.
- **Screenshot content:** choose paired original input/final output, output only (default), or input only. Pair captures at a real-frame submission boundary; retain asynchronous handling, session validation and collision-safe partial-save behavior. Independent effect/channel PNG and DDS export remains available.
- **RTSS:** segmented status layout, independent unlimited Magpie profile initialization, per-operation capability checks and one best-effort restoration. New and unmigrated settings request administrator startup by default, while later user choices are retained. Physical UI, UAC/startup tasks and actual target FPS require separate acceptance.
- Retain Beta4 manual HDR migration, export/cleanup safeguards, UWP/parameter focus coordination and VRR behavior.

## 来源与验收 / Source and acceptance

功能头：RTSS e830078a、NVIDIA VFX 999e94e2、截图 abfce2ef。原暂停 VRR 分支未纳入。包内 build-manifest.json 记录实际构建选项、提交与逐文件哈希；本轮分支、合并、测试、构建及配置保留证据位于 reports/0610-beta5-20261009。Beta4 完整运行环境保留供回退。

本地功能库验证与软件回归不等同于真实 GPU／WinUI／物理 DPI／游戏／画质及性能验收；本版没有公开发布、推送或改变 RTSS 安装目录 ACL。
