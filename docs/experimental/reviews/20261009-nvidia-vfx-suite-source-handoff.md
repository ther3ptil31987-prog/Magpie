# Nvidia VFX 功能分支：源码交接

2026-10-09，Asia/Shanghai。分支 `feature/nvidia-vfx-suite`，工作目录 `C:\Users\81443\Documents\Magpie enhance\worktrees\nvidia-vfx-suite`。基线为已部署 Beta4 的 `49de3f25478008c962e24015cf81d7e9772b9a2b`。

本阶段完成源码候选、开发依赖引用、目录与参数迁移、静态检查和交互预览。**没有编译 C++／HLSL，没有创建或部署 release，没有加载功能 DLL 进行 GPU 实测。** 下列源码状态不能作为产品、颜色、性能或资源生命周期验收。

## 已写入源码

- 公共 VFX 运行层锁定程序目录的组件，检查 Core 1.3.0.0；在 `NvCVImage` 构造前完成代理装载。CUDA primary context 对应实际后端渲染 D3D11 适配器，每次调用 push/pop；每个实例拥有独立 stream，销毁前同步并注销纹理。图像通过 D3D11↔CUDA 传输，不回传整张图像到 CPU。
- `Nvidia VFX Upscale` 的缩放入口为自动／4/3×／1.5×／2×／3×／4×。原生倍率使用整数分数，自动跳过非整数尺寸，从低档选择；4× 不足时由同一个现有 Bicubic 算法继续放大。自动先解析等比内容尺寸，最终留边／裁切由布局处理，避免追加 Fill 拉伸。适配器位于该效果内部，因此中间链也能消费最终目标尺寸；相同尺寸跳过适配。运行面板显示实际原生及 Bicubic 尺寸。
- `Nvidia VFX VSR` 保留旧 ID 和 `strength` 档位键。常规 1–4，高码率 16–19，流媒体 21／23，降噪 8–11，去模糊 12–15。配置页和运行面板都过滤流媒体无效选项；每类在隐藏配置键中保存档位／缩放。降噪、去模糊保持输入尺寸。旧 Denoise／八个后缀别名迁移为统一 VSR；Denoise 不再成为新的独立选择器条目。
- VSR 连续 SDK `Strength` 与用户档位分离，不增加第三个首层控件。源码在 SDK 支持查询时读取并保留该运行库默认值、校验 0–1，并记录日志；文档未列出默认值，**具体返回值及旧新版画质对比仍需实际加载确认**。
- `Nvidia VFX VFG` 是效果列表里的后端选择标记。真正执行边界位于第一项 TrueHDR 之前，未选择 TrueHDR 时位于 SDR 图像链之后。共享 FG 分类／互斥、保存和基础帧率使用 2–8×；Windows x64 检查实际渲染 GPU 的 Ada／Blackwell compute 架构。默认 2×／Medium／自动场切和重复帧过滤开启。
- 首帧 A 只发布一次；每对 A/B 逐张生成 1…M−1，再发布 B。双帧 SDR、SDK 临时输出、后置输入、最终发布资源分别保留。SDK 缓冲在发布完成复制后才复用；前置历史 revision、捕获 sequence 或资源代次变化会断开历史。VFG 失败回退真实帧；连续失败三次后停止该实例的插帧尝试，模型切换或历史重置可重新尝试。
- VFG 设置 SDK 自动场切；首个输出与 B 完全相同时，旁路其余插帧，避免场切／静态旁路重复发布。比较在 GPU 上完成，CPU 只读取 **4 字节标志**。这属于输出旁路检测，不能代替实际素材中的场切质量验收。
- 每张输出单独执行 TrueHDR 和支持的后置链，绕开捕获帧级复用。首版后置候选白名单为 TrueHDR／Bicubic／Bilinear／CAS／RCAS；时序或未确认后置效果在建资源前拒绝，列表不重排。直接 HDR 输入拒绝；显式 HDR→SDR 后可补帧再生成 HDR。光标／工具栏保留在前端。
- 呈现与原图对照使用原有 **四槽有界环**。队列容量与 2–8× 的 SDK 倍率分开；发布逐张等待槽位，不一次积累七张额外 HDR 输出。基础限帧不再把 VFG 倍率截到 4。发布元数据携带 FrameIndex／倍率、前帧 ID、参数 revision、捕获序列、资源代次及中间时间戳。
- 参数更新等待旧 revision 的队列消费完。VFG 模型采用完整候选实例事务并清历史；倍率、重复帧过滤、自动场切重启应用。后置 TrueHDR 快照在同一组帧内不变，前置修改清历史。TrueHDR／Upscale／VSR 的候选设置失败保留当前实例，并走旧值恢复路径。
- `Nvidia VFX TrueHDR` 在 `EnableNvidiaVfx=true` 的开发构建中替换旧后端，保留 `RTXVideo\RTXVideo_HDR` 和原四键。RGB10A2／PQ／BT.2020 输出显式解码到 FP16/scRGB，复用现有 80-nit 归一化路径。去色带默认开启，内部映射 `DebandingOff=0`。
- 原四项范围／默认／输入精度／普通与 Shift 刻度保持 Beta4；101 和 1003 nits 不按滑块刻度重新量化。未启用统一套件的构建仍保留原桥接恢复路径；统一套件构建不再构建该专属桥接。旧独立完整运行环境没有删除。
- 中英文效果说明、搜索别名与参数资源已同步。目录生成器把 20261009 的四项 VFX 补充审查合并到原审查中；20261002 历史审查保留原内容，不复制一整套无改动目录。

## 组件与后续构建输入

官方开发根目录：

`C:\Users\81443\Documents\Magpie enhance\dependencies\NVIDIA-VFX-SDK-1.3.0.0-windows-x64\core\VideoFX`

Core 与四个功能包版本均为 1.3.0.0，功能包 NGC 版本为 `1.3.0.0_lib_windows`。当前已取得的四项均由官方安装器确认 library-only；本地 GPU 120 映射 SM100，但本次没有独立 SM100 模型文件。不要因此推断其他 GPU 已验收，或把其他 SDK 效果的模型需求套用到这四项。

后续编译会话显式设置 `EnableNvidiaVfx=true` 和上述 `VFXSdkDir`，清除旧的 `VFXIncludeDir`／`VFXProxyDir`／`VFXRuntimeDir` 覆盖，或分别指向当前 `nvvfx/include`／`nvvfx/src`／`bin`。构建默认仍关闭该开发开关；没有改本地用户覆盖文件。

新增目标会检查 Windows x64 和开发文件。私有验证输出清单包含匹配 Core、四项功能 bin、Core／功能许可证；不再遗漏 VFG／Upscale，或把其他版本的 TrueHDR 从旧桥接覆盖进来。公共发布前仍需审查再分发范围及最小运行依赖；本阶段没有宣称取得公开再分发许可。**NGC key 只属于组件获取，运行时无需该开发者 key，源码和发布清单不保存它。**

## 本阶段验证

- `tests/Validate-NvidiaVfxSource.py`：90 项通过；检查 XML／工程注册、官方 SDK 符号、六个功能 DLL SHA-256、模块构造前装载、共享槽位容量、TrueHDR 范围／默认和 Beta4 精确输入／步进策略、中英文资源唯一性。
- `tests/Validate-EffectCatalog.ps1`：164 个源码效果与两个目录一致；新显示名及旧别名检查通过。目录生成 `-Check` 通过。
- `git diff --check`：通过。
- 浏览器实际预览：流媒体仅 Medium／Ultra；640×360→2560×1440→3840×2160；精确输入 101／1003 保留，普通右键 101→105、Shift 峰值右键 1003→1050；8× 的七张中间帧逐张 TrueHDR，320px 下换行。它不代表 WinUI 或 NVIDIA 实际输出验收。
- 新增 `NvidiaVfxPolicyTests.cpp`；R1 测试更新统一 VSR 迁移期望。**两者均未编译或运行**，下一阶段必须执行。

本地证据在 `C:\Users\81443\Documents\Magpie enhance\reports\nvidia-vfx-suite-source-20261009`。`pre-resume.patch`／`pre-resume-untracked` 保存暂停时草稿，`beta4-fallback-manifest.json` 记录原包 Core 1.2.0.0、VSR companion 1.8.2.0、HDR companion 1.1.0.0 及桥接的哈希。恢复产品基线继续使用原 `release/v0.6.10-beta4/Magpie-Experimental-x64`，本次没有替换它。主要 `source/experimental` 工作树的现存修改保留。

## 后续会话必须完成

1. 在候选输出目录编译统一套件及无套件回退构建，执行新策略测试、R1／HDR 精确输入、保存迁移、FG 分类／时序、颜色和现有组件回归。不得复用本次静态 PASS 作为 C++ 编译证明。
2. 实际 RTX GPU 加载四项，确认 NVCV 的 RGBA／BGRA／RGB10A2 D3D11 互操作、默认 VSR Strength、多功能／NGX 共存、不同适配器、重建与释放。重点覆盖 TrueHDR 在列表首位、VFG 标记在前／后、RGBA 与 BGRA 输入，以及 HDR→SDR→VFG→TrueHDR。
3. 对照旧 Beta4 TrueHDR：80-nit scRGB、PQ／BT.2020／通道和 alpha、101／1003 参数、色带、暗部与高光。达到验收后再把新后端作为发布替换。
4. 检查 VFG 首帧、场切、重复帧、2–8×、失败、模型切换、参数 revision、切屏／停止／重启及四槽背压。记录 SDK+逐帧后处理+发布的预算日志和实际呈现间隔；4 字节旁路检测仍有同步开销。现有单捕获帧 profiler 不能代表多张后置处理总开销，要结合新增组预算日志／FrameTrace 和 GPU 测量。
5. 实际 WinUI 两级下拉、保存／回滚、整数输入、窄窗口／DPI；验证 Upscale 固定／自动、中间链、Fit／Fill、设备上限、4× 后继续放大，确保最终适配一次。
6. 按版本／平台／实际渲染 GPU 制作组件分发与最小依赖清单，核对许可、缺包／不支持提示及恢复材料；通过后再按维护者后续授权构建／部署 release。

依据：[Upscale](https://docs.nvidia.com/maxine/vfx/1.3.0/Filters/Upscale.html)、[VSR](https://docs.nvidia.com/maxine/vfx/1.3.0/Filters/VideoSuperResolution.html)、[VFG](https://docs.nvidia.com/maxine/vfx/1.3.0/Filters/VideoFrameGeneration.html)、[TrueHDR](https://docs.nvidia.com/maxine/vfx/1.3.0/Filters/TrueHDR.html)、[SDK 安装](https://docs.nvidia.com/maxine/vfx/1.3.0/WindowsVFXSDK/InstalltheVFXSDK.html)。
