# VSR 通道修复、RTSS 宽度恢复与 VFG 两档源码交接

日期：2026-10-09（Asia/Shanghai）。继续 `feature/nvidia-vfx-optimization`，起点 `9e257cf2`，功能源码基线 `970dd5f9`。纳入 Beta5 `c640f9c2` 的 Windows PowerShell 内置模块导入修复。没有编译 Magpie／HLSL、创建新 release、打包、部署或启动产品。

## 本次结果

- VSR 固定 SDK 内部输入和输出为 RGBA/U8，在绑定图像之前显式设 RGB8。D3D11 边界按实际纹理格式复制；需要 BGRA 时使用缓存线性 GPU 缓冲显式转换，避免混合模型 I/O。默认执行不增加 CPU 像素读回；RGBA 同编码不增加转换缓冲。公共传输层核对尺寸、格式、类型、layout、pitch、内存位置及 Map 前后 wrapper 合同，纹理重绑定和缓冲重建前仍排空流。
- RTSS 工具按钮内容列改为 Auto/Auto。初始化可见性变化时，用可撤销的弱引用监听清除工具按钮临时 Width/MaxWidth，并让本地操作区域重新测量和排列。其他 225 DIP 输入列、WrapPanel 全局逻辑、按钮菜单和 RTSS 状态／启动行为保持原合同。真实 WinUI 宽度尚未验收。
- VFG 仅性能（Low=0）／均衡（Medium=1，默认）。配置启动加载和显式导入共同调用精确 ID 的迁移函数，只有旧合法 High=2 改为 1；缺省、0/1、未知／非整数／非有限值不由迁移函数改写。有效 HDR 恢复链也迁移实际参数，但保留原始备份字符串及无效恢复节点。启动计划、初始化、直接 ApplyModel 和运行中 float→uint 转换均受两档白名单保护；候选加载失败的旧实例回滚合同不变。
- 同步中英／繁体选项、双语效果说明及生成目录；既有预览的 VFG 下拉也改为两档。VSR 的 High/Ultra 不受影响。组件 DLL、十文件离线分发清单、TrueHDR 的对比度／饱和度／中灰／峰值范围和全部步进保持既定合同。

## VSR 根因证据与限度

在实际 RTX 5070 Ti、驱动 616.92（CIM `32.0.16.1692`）、匹配 Core 1.3.0.0 上，使用独立 Python ctypes 探针创建合成 D3D11 色块，并选择同一适配器的 CUDA primary context。没有读取桌面或窗口内容。

| 对照 | 结果 |
| --- | --- |
| 旧路径：四种 RGBA/BGRA 输入输出组合，各自无模型往返和标准 Medium VSR | 6/8；两条混合 I/O 在 CUDA 模型输出首次发生 R/B 互换，无模型往返及同编码模型路径正常 |
| 旧路径加显式 ImageEncodingMode=RGB8 | 同样 6/8；显式设置本身没有修复混合编码 |
| RGBA 模型 I/O＋同编码 array 复制＋线性转换，五分类全部 18 个合法档位及往返 | 76/76；所有采样 alpha 为 255 |
| Map 前 wrapper 格式／类型／layout／CUDA array 描述符核对 | 8/8 |
| 962×542 输入、1924×1084 超分输出，非对齐行距四格式组合与往返 | 8/8 |

例如原始红块为 RGBA `[224,24,16,255]`：旧 BGRA→RGBA 模型输出按声明格式解读为 `[15.37,23.35,224.35,255]`；新路线最终 D3D11 输出为 `[223.00,23.60,15.16,255]`。这是本机合成测试确认的混合 I/O 问题；尚未得到用户异常的完整捕获／档位／链配方，不能声称实际用户会话已经修复。探针直接调用 SDK，尚未运行新编译的 Magpie C++ 后端、HDR 适配、最终呈现、生命周期或性能验收。

实现参照 [官方 VSR 8-bit 合同](https://docs.nvidia.com/maxine/vfx/1.3.0/Filters/VideoSuperResolution.html)与 [RGBA I/O 官方示例](https://github.com/NVIDIA-Maxine/VFX-SDK-Samples/blob/52011f89c1741d06b40ea312af1f20be8be9ec62/apps/VideoSuperResEffectApp/VideoSuperResEffectApp.cpp#L630)。实际每帧额外 GPU 复制开销未测量；BGRA 边界每侧多一个缓存线性缓冲及一次线性通道转换，纳入后续产品预算测量。

## 同时修正 TrueHDR 的 packed 输出导出

颜色链检查发现旧 `NvCVImage_InitFromD3D11Texture(R10G10B10A2_UNORM)` 返回 -9（像素格式不支持）。[CUDA D3D11 互操作文档](https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__D3D11.html)的支持格式表也不包含该格式；[TrueHDR 输出合同](https://docs.nvidia.com/maxine/vfx/1.3.0/Filters/TrueHDR.html)要求 RGB10A2/PQ/BT.2020。因此旧直接输出路线不能据直接 SDK 非零读回测试认定可用于 Magpie。

SDK 输出仍是 RGB10A2/P32。新增非拥有的 RGBA/U8 字节视图，同编码复制至可互操作的 RGBA8 纹理，不修改 SDK 的拥有描述符，不把 10-bit 颜色量化成 8-bit。新增 HDR 适配模式 9 从四个原始字节重建 uint32、解出 10/10/10/2 位，再共用模式 6 的 PQ→80-nit scRGB 与 Rec.2020→Rec.709 转换；已有颜色矩阵不变，其他 HDR 模式保留原执行路线。

64×64、4096 像素的 CUDA→D3D11 往返位值完全一致，覆盖变化的 RGB 和全部两位 alpha；CPU float32 模拟 UNORM 字节重建也一致。此项**没有执行新 HLSL、TrueHDR 模型＋新导出完整链或实际 HDR 呈现**。已在既有 WARP 用例添加模式 9 字节纹理与模式 6 原生 R10 纹理的 FP32／FP16、PQ、色域和 alpha 对照，留给编译会话执行。

## 已执行的检查

- `Validate-NvidiaVfxSource.py`：105 项 SDK 声明、项目、资源、参数兼容及两档选项检查通过。
- `Validate-NvidiaVfxOptimization.py`：61 项结构检查通过；Async、CudaHandoff 默认仍关闭。
- `Generate-EffectCatalog.ps1 -Check`、`Validate-EffectCatalog.ps1`：164 个源码效果与双语目录一致，旧别名保留。
- Windows PowerShell 5.1 的运行库完整性测试：7 项通过，使用独立报告夹中的 fixture；没有修改 SDK 或发布目录中的组件文件。
- ProfilePage XAML、资源和项目 XML 解析、HTML/JS 预览语法／两档选项核对、探针 Python 语法与 `git diff --check`。
- HDR 着色器源与常量布局已提取为后续 WARP fixture。此项仅提取，未编译、未运行 WARP。

新扩展的 `tests/NvidiaVfxPolicyTests.cpp` 包含两种字符类型、迁移幂等、无关效果、缺省、合法／非法值、迁移后计划、倍率／其他参数保留和 runtime 白名单行为测试；本会话未编译或运行该 C++ 测试。

独立证据目录：[reports/vsr-color-fix-20261009](<C:/Users/81443/Documents/Magpie enhance/reports/vsr-color-fix-20261009>)。主要原始报告：`current-path.json`、`mixed-explicit-encoding.json`、`rgba-bridge-matrix.json`、`wrapper-contract.json`、`stride-packed-contract.json`（保留直接 R10 失败证据）、`stride-packed-byte-carrier.json`、`suite-source.json`、`optimization-source.json` 和 `runtime-fixtures/runtime-manifest-tests.json`。便于随分支移交的摘要见[颜色证据摘要](20261009-vsr-color-evidence.json)。

## 编译会话继续事项

1. 使用本功能分支及其完整提交，先编译 C++／XAML／运行时 HLSL；执行扩展的 NvidiaVfxPolicyTests 和既有 HDR WARP 模式 9 对照。独立 GPU 探针的通过不能替代产品编译。
2. 实际加载／导入旧 High 配置，保存／导出后确认 model=1、原始备份未改，并检验两面板、模型切换成功／失败回滚与默认均衡。SDK 2 不应进入正常产品执行。
3. 真实页面验证初始化隐藏→工具恢复内容宽度及反向变化，窄窗／长翻译、浅深主题、100/150/200% DPI、菜单和焦点。窄窗的文本换行也须实际测量。
4. 产品色块与代表素材：RGBA/BGRA 捕获、五类 VSR、前置 HLSL、VSR→TrueHDR、VSR→VFG→TrueHDR、合法 HDR→SDR；真实／生成帧、参数修改、重建、停启和失败恢复分别验证。TrueHDR 保留原参数暴露与步进，对照实际 PQ／scRGB 输出。
5. 测量 BGRA 线性转换的 GPU 成本、显存和整个 VFG→TrueHDR 帧预算。不要据本次颜色探针声称性能改善；F17 截止时间丢帧仍未实现。

可重跑探针：在本工作树执行 `tests/Probe-NvidiaVfxColor.py --runtime <匹配的现有包目录> --report <外部JSON路径> --bridge --explicit-encoding --matrix`；非对齐与 packed 检查使用 `--width 962 --height 542 --packed-check --packed-bridge`，可省略 `--matrix`。只生成独立合成测试报告，不构建产品。

关联：[本次 TODO](../todos/20261009-vsr-color-rtss-width-vfg-model-TODO.md)、[总 TODO](../todos/20261009-nvidia-vfx-suite-upgrade-TODO.md)、[前一优化源码交接](20261009-nvidia-vfx-optimization-source-handoff.md)。
