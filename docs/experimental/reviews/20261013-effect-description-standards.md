# 效果器说明规范化：审查与源码交接

后续反馈（2026-10-13）：用户指出文案仍有防御性描述过多、跨效果信息过密和新旧表达不一致的问题。本文保留上一轮事实修正与验证记录；其中写作完成结论已被后续反馈重新打开，后续按[用户说明重写 TODO](../todos/20261013-effect-description-user-copy-revision-TODO.md)与[修订规范](../EFFECT-DESCRIPTION-GUIDE.md)执行。后续重写现已完成，结果与待实机事项见[本轮文案审查](20261013-effect-description-user-copy-revision.md)。

日期：2026-10-13。分支：`feature/effect-description-standards`。基线：`release/0.6.10-beta8` / `cbe2c2cbda103e6cb6f686a5521918ed2501c32e`。

## 本轮完成范围

按[TODO](../todos/20261013-effect-descriptions-and-guidelines-TODO.md)复核 165 个内置效果器的中英文摘要／详情，8 个分类、17 个子分类、13 个家族、2 个子家族及自定义／通用选择器说明。新增[长期维护规范](../EFFECT-DESCRIPTION-GUIDE.md)，接入文档索引和中英文贡献指南。44 组中英文参数提示／帮助已新增或修订，主要覆盖 xBR、VFX、FSR3 FG、HDR 转换和关联的 DLSSNR／FG 参数。

工作树位于 `worktrees/effect-description-standards`，独立于日常 `source` 和 Beta8 工作树。日常目录已有修改保留。未编译 C++／HLSL、未创建新 release、未部署、未改变运行中的喜鹊或用户配置；未合并／推送。

## F01–F08 处理结论

| 问题 | 完成内容 | 核对依据／边界 |
| --- | --- | --- |
| F01 VSR 生效方式 | 分类修改需重新启动缩放；档位可实时切换 | `NvidiaVfxImageBackend.cpp`、`ScalingRuntime.cpp`；两处界面共用参数帮助 |
| F02 新效果归类 | Upscale → 通用，VFG → 补帧，FSR3 FG → 进阶快捷入口 | 两种语言子分类同槽；精确 ID，未改配置身份 |
| F03 RCAS 不匹配 | 白名单从不存在的 `RCAS\RCAS` 改为内置 `FSR\FSR_RCAS` | 支持契约本来就包含 RCAS；仅修正 ID。补齐 TrueHDR 后允许／拒绝的原生测试，源码交叉检查通过；原生执行和 GPU 组合验收待后续 |
| F04 FSR3 支持列表 | 详情列出当前允许的 SR、VFX、NIS、插值、FSR1 与限帧效果 | 对照 `FSR3FGPolicy.h`；保留 SDR、固定 2×、外部光流、裁剪／几何限制与伪影风险 |
| F05 开发记录混入产品 | 移出队列、时间戳、内部光流和旧配置叙述 | 原有开发记录保留；本报告保留迁移信息定位，不改变实现 |
| F06 新效果用途不清 | 补齐 VSR／Upscale 用途、尺寸、依赖及 TrueHDR 条件 | 当前后端与 NVIDIA VFX 1.3 文档；没有新增性能或画质收益承诺 |
| F07 变体和 HDR | 摘要区分 CuNNy 模型、NNEDI3 神经元／窗口、RAVU RGB／AR／倍率、SMAA NoJitter；压缩通用 HDR 提示 | 133 项通用 SDR 兼容提示保留在各叶子详情。按 A/B/C 路由确认其仍有依据；不将它们误称原生 HDR。保留 CRT、Bicubic、NIS、显式转换和 xBR Hybrid 的特殊说明 |
| F08 校验不足 | 目录校验拒绝空子分类和双语归类不同；记录并验证后端证据哈希；新增入口／资源／旧别名契约检查 | 哈希只检测审查依据漂移，不能替代语义 review 或实机验证 |

额外复核：VFG 的后置段从**首个 TrueHDR**开始，而非从 VFG 标记所在位置开始。详情与运行帮助已写清分界；没有 TrueHDR 时图像效果在插帧前执行。xBR 的亮度权重仅在小细节保护分支生效，已补充提示。TrueHDR 的产品峰值默认值为 1000，未照搬 SDK 文档的 650；范围／默认值由控件继续提供。

## 事实来源及迁移定位

- 说明仍来自现有 3 份 review JSON，按 `Read-EffectCatalogReview.ps1` 合并；没有引入新的运行时配置或生成层。两份完整语言目录与 HDR 子目录均已重新生成。
- 每个有效条目记录 `copyReviewedOn`／`copyDisposition`。22 项记录源码依据哈希（17 项原生／调度／HDR 效果，加 5 项 xBR）。`runtimeEvidence` 是源码路径，**并非实机运行记录**。着色器与后端哈希均按当前分支核对；推荐标签、ID、参数默认值和范围未改。
- 通用 HDR 核对：`Renderer.cpp::GetHdrRoutesForEffect` → `GroupAHdrRoutes.cpp`／`GroupBHdrRoutes.cpp`／`EffectProtocolCatalogC.h` → `HdrEffectBoundary.cpp`／`HdrAdapterDispatcher.cpp`。显式颜色节点和自动效果边界分别核对。
- 移出产品文字的 VFX 旧 ID／模型迁移见 `NvidiaVfxParameters.h`、`NvidiaVfxParameterPolicy.h`；队列与首帧行为见 `NvidiaVfxFrameGenerator.cpp`。FSR3 时间戳、内部光流及旧设置行为见[FSR3 开发说明](../effects/FSR3FG.md)。SDR 白点旧设置保留行为见 `HdrParameterPolicy.h` 和 `HdrComponents.h`。本次不改变这些行为。
- 一手参考：[VFX Upscale 1.3](https://docs.nvidia.com/maxine/vfx/1.3.0/Filters/Upscale.html)、[VFG 1.3](https://docs.nvidia.com/maxine/vfx/1.3.0/Filters/VideoFrameGeneration.html)、[VSR 1.3](https://docs.nvidia.com/maxine/vfx/1.3.0/Filters/VideoSuperResolution.html)、[TrueHDR 1.3](https://docs.nvidia.com/maxine/vfx/1.3.0/Filters/TrueHDR.html)。型号、开放选项、生效方式以本项目代码为准。

## Review 与验证结果

本轮自行复核变更，重点检查错误能力承诺、颜色与处理顺序、参数生效方式、遗失别名及目录重新生成。未发现本轮源码／文案范围内的剩余阻断问题。原生可执行行为和界面效果尚未验收，不能由静态检查推定通过。

| 检查 | 结果 | 能证明什么 |
| --- | --- | --- |
| `scripts/Generate-EffectCatalog.ps1` + `tests/Validate-EffectCatalog.ps1` | 165 项通过 | 生成一致性、ID、双语结构／分类、长度、源码／后端证据 |
| `python -X utf8 tests/validate_effect_descriptions.py` | 6 组通过 | 新分类／入口、安装 ID、旧别名、双语提示键及关键限制的源码／数据合同 |
| `python -X utf8 tests/validate_fsr3fg_source.py` | 60 项通过 | FSR3 既有源码集成检查 |
| `python -X utf8 tests/validate_fsr3fg_review.py` | 45 项通过 | FSR3 既有跨模块／XML 检查 |
| 负例验证 | 3 项按预期拒绝 | 空子分类、仅英文归入另一有效子类、后端哈希失配；所有临时改动逐字节恢复 |
| `git diff --check`、本轮 Markdown 链接／XML 键检查 | 通过 | 格式与文档／资源结构 |
| C++ 原生测试、产品编译／部署、真实 UI／GPU | 未执行 | 按用户分工交给独立会话 |

新增 C++ 用例：`EffectPickerTests.cpp` 明确检查 FSR3 FG 进阶／补帧归类；`NvidiaVfxPolicyTests.cpp` 在 TrueHDR 后接受实际 RCAS／CAS／Bicubic／Bilinear，拒绝旧错误 ID、CAS Scaling、EASU、CRT 和自定义同名效果。必须由构建会话编译并执行，不能将 Python 源码匹配视为这些用例已通过。

## 后续构建会话最小验收

1. 将本功能分支纳入目标构建基线，合并后重新运行上述非编译检查；如果后端证据文件被其他工作改变，重新核对受影响说明再刷新哈希。
2. 编译并运行 `EffectPickerTests.cpp` 和 `NvidiaVfxPolicyTests.cpp` 的原生回归；再按原有流程构建／部署。当前工作树不包含本轮二进制。
3. 在包含本轮提交的程序中分别检查中英文：通用／补帧／进阶入口、旧 RTX 档位与 FSR3FG 搜索、各家族展开、摘要截断、VFG／FSR3 长详情滚动。字体与 DPI 缩放至少检查常用设置。
4. 参数页与运行面板查看新增提示；VSR 分类重新启动缩放、档位实时切换；VFG 模型实时切换、倍率／场切重新启动缩放；FSR3 光流重新启动缩放；xBR 小细节保护条件。
5. 支持的 NVIDIA GPU 上验证 SDR → VFG → TrueHDR → FSR RCAS；验证不支持的后置效果仍拒绝。分别检查 HDR 高光／颜色、伪影与稳定性。此处不承诺特定帧率或延迟收益。

## 全量效果器逐项复核

以下“修改／保留”仅指中英文目录内容（包括必要检索词），不把增加审查日期视为产品修改。两种语言均已做文案对照，尺寸／参数声明通过当前源码哈希核对；相关原生行为按前文重点核对。未进行 165 项逐项 GPU 运行或画质比较。

共 165 项：修改 150，保留 15。

| 效果 ID | 处理 | 双语状态 | 依据入口 |
| --- | --- | --- | --- |
| `ACNet` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/ACNet.hlsl) |
| `Anime4K\Anime4K_3D_AA_Upscale_US` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_3D_AA_Upscale_US.hlsl) |
| `Anime4K\Anime4K_3D_Upscale_US` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_3D_Upscale_US.hlsl) |
| `Anime4K\Anime4K_Denoise_Bilateral_Mean` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Denoise_Bilateral_Mean.hlsl) |
| `Anime4K\Anime4K_Denoise_Bilateral_Median` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Denoise_Bilateral_Median.hlsl) |
| `Anime4K\Anime4K_Denoise_Bilateral_Mode` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Denoise_Bilateral_Mode.hlsl) |
| `Anime4K\Anime4K_Restore_L` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Restore_L.hlsl) |
| `Anime4K\Anime4K_Restore_M` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Restore_M.hlsl) |
| `Anime4K\Anime4K_Restore_S` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Restore_S.hlsl) |
| `Anime4K\Anime4K_Restore_Soft_L` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Restore_Soft_L.hlsl) |
| `Anime4K\Anime4K_Restore_Soft_M` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Restore_Soft_M.hlsl) |
| `Anime4K\Anime4K_Restore_Soft_S` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Restore_Soft_S.hlsl) |
| `Anime4K\Anime4K_Restore_Soft_UL` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Restore_Soft_UL.hlsl) |
| `Anime4K\Anime4K_Restore_Soft_VL` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Restore_Soft_VL.hlsl) |
| `Anime4K\Anime4K_Restore_UL` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Restore_UL.hlsl) |
| `Anime4K\Anime4K_Restore_VL` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Restore_VL.hlsl) |
| `Anime4K\Anime4K_Thin_HQ` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Thin_HQ.hlsl) |
| `Anime4K\Anime4K_Upscale_Denoise_L` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Upscale_Denoise_L.hlsl) |
| `Anime4K\Anime4K_Upscale_Denoise_S` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Upscale_Denoise_S.hlsl) |
| `Anime4K\Anime4K_Upscale_Denoise_UL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Upscale_Denoise_UL.hlsl) |
| `Anime4K\Anime4K_Upscale_Denoise_VL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Upscale_Denoise_VL.hlsl) |
| `Anime4K\Anime4K_Upscale_GAN_x2_M` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Upscale_GAN_x2_M.hlsl) |
| `Anime4K\Anime4K_Upscale_GAN_x2_S` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Upscale_GAN_x2_S.hlsl) |
| `Anime4K\Anime4K_Upscale_GAN_x3_L` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Upscale_GAN_x3_L.hlsl) |
| `Anime4K\Anime4K_Upscale_L` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Upscale_L.hlsl) |
| `Anime4K\Anime4K_Upscale_S` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Upscale_S.hlsl) |
| `Anime4K\Anime4K_Upscale_UL` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Upscale_UL.hlsl) |
| `Anime4K\Anime4K_Upscale_VL` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Anime4K/Anime4K_Upscale_VL.hlsl) |
| `Bicubic` | 保留：文案与事实保持 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Bicubic.hlsl) |
| `Bilinear` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Bilinear.hlsl) |
| `CAS\CAS_Scaling` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CAS/CAS_Scaling.hlsl) |
| `CAS\CAS` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CAS/CAS.hlsl) |
| `CRT\CRT_Easymode` | 保留：文案与事实保持 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CRT/CRT_Easymode.hlsl) |
| `CRT\CRT_Geom` | 保留：文案与事实保持 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CRT/CRT_Geom.hlsl) |
| `CRT\CRT_Hyllian` | 保留：文案与事实保持 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CRT/CRT_Hyllian.hlsl) |
| `CRT\CRT_Lottes` | 保留：文案与事实保持 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CRT/CRT_Lottes.hlsl) |
| `CRT\GTU_v050` | 保留：文案与事实保持 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CRT/GTU_v050.hlsl) |
| `CuNNy\CuNNy-16x16C-NVL-DN` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-16x16C-NVL-DN.hlsl) |
| `CuNNy\CuNNy-16x16C-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-16x16C-NVL.hlsl) |
| `CuNNy\CuNNy-2x4C-NVL-DN` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-2x4C-NVL-DN.hlsl) |
| `CuNNy\CuNNy-2x4C-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-2x4C-NVL.hlsl) |
| `CuNNy\CuNNy-3x4C-NVL-DN` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-3x4C-NVL-DN.hlsl) |
| `CuNNy\CuNNy-3x4C-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-3x4C-NVL.hlsl) |
| `CuNNy\CuNNy-4x16C-NVL-DN` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-4x16C-NVL-DN.hlsl) |
| `CuNNy\CuNNy-4x16C-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-4x16C-NVL.hlsl) |
| `CuNNy\CuNNy-4x4C-NVL-DN` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-4x4C-NVL-DN.hlsl) |
| `CuNNy\CuNNy-4x4C-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-4x4C-NVL.hlsl) |
| `CuNNy\CuNNy-4x8C-NVL-DN` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-4x8C-NVL-DN.hlsl) |
| `CuNNy\CuNNy-4x8C-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-4x8C-NVL.hlsl) |
| `CuNNy\CuNNy-6x8C-NVL-DN` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-6x8C-NVL-DN.hlsl) |
| `CuNNy\CuNNy-6x8C-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-6x8C-NVL.hlsl) |
| `CuNNy\CuNNy-8x16C-NVL-DN` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-8x16C-NVL-DN.hlsl) |
| `CuNNy\CuNNy-8x16C-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-8x16C-NVL.hlsl) |
| `CuNNy\CuNNy-8x4C-NVL-DN` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-8x4C-NVL-DN.hlsl) |
| `CuNNy\CuNNy-8x4C-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-8x4C-NVL.hlsl) |
| `CuNNy\CuNNy-8x8C-NVL-DN` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-8x8C-NVL-DN.hlsl) |
| `CuNNy\CuNNy-8x8C-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy/CuNNy-8x8C-NVL.hlsl) |
| `CuNNy2\CuNNy-3x12-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy2/CuNNy-3x12-NVL.hlsl) |
| `CuNNy2\CuNNy-4x12-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy2/CuNNy-4x12-NVL.hlsl) |
| `CuNNy2\CuNNy-4x16-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy2/CuNNy-4x16-NVL.hlsl) |
| `CuNNy2\CuNNy-4x24-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy2/CuNNy-4x24-NVL.hlsl) |
| `CuNNy2\CuNNy-4x32-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy2/CuNNy-4x32-NVL.hlsl) |
| `CuNNy2\CuNNy-8x32-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy2/CuNNy-8x32-NVL.hlsl) |
| `CuNNy2\CuNNy-fast-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy2/CuNNy-fast-NVL.hlsl) |
| `CuNNy2\CuNNy-faster-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy2/CuNNy-faster-NVL.hlsl) |
| `CuNNy2\CuNNy-veryfast-NVL` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/CuNNy2/CuNNy-veryfast-NVL.hlsl) |
| `Deband` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Deband.hlsl) |
| `Diagnostics\FrameGuidance_Confidence` | 保留：文案与事实保持 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Diagnostics/FrameGuidance_Confidence.hlsl) |
| `Diagnostics\FrameGuidance_Motion` | 保留：文案与事实保持 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Diagnostics/FrameGuidance_Motion.hlsl) |
| `DLSS\DLSS_SR` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希](../../../src/Effects/DLSS/DLSS_SR.hlsl) |
| `DLSSFG\DLSS_FrameGeneration` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希](../../../src/Effects/DLSSFG/DLSS_FrameGeneration.hlsl) |
| `DLSSNR\DLSSNR_AI_Filter` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希](../../../src/Effects/DLSSNR/DLSSNR_AI_Filter.hlsl) |
| `FrameRate_Filter` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希](../../../src/Effects/FrameRate_Filter.hlsl) |
| `FSR\FSR_EASU` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/FSR/FSR_EASU.hlsl) |
| `FSR\FSR_RCAS` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/FSR/FSR_RCAS.hlsl) |
| `FSR2\FSR2_SR` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希](../../../src/Effects/FSR2/FSR2_SR.hlsl) |
| `FSR3\FSR3_SR` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希](../../../src/Effects/FSR3/FSR3_SR.hlsl) |
| `FSR4\FSR4_SR` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希](../../../src/Effects/FSR4/FSR4_SR.hlsl) |
| `FSRCNNX\FSRCNNX_LineArt` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/FSRCNNX/FSRCNNX_LineArt.hlsl) |
| `FSRCNNX\FSRCNNX` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/FSRCNNX/FSRCNNX.hlsl) |
| `FXAA\FXAA_High` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/FXAA/FXAA_High.hlsl) |
| `FXAA\FXAA_Medium` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/FXAA/FXAA_Medium.hlsl) |
| `FXAA\FXAA_Ultra` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/FXAA/FXAA_Ultra.hlsl) |
| `ImageAdjustment` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/ImageAdjustment.hlsl) |
| `Jinc` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Jinc.hlsl) |
| `k7_modernAnime_FHD_x2` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/k7_modernAnime_FHD_x2.hlsl) |
| `Lanczos` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Lanczos.hlsl) |
| `MLAA\MLAA` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/MLAA/MLAA.hlsl) |
| `Nearest` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Nearest.hlsl) |
| `NIS\NIS` | 保留：文案与事实保持 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/NIS/NIS.hlsl) |
| `NIS\NVSharpen` | 保留：文案与事实保持 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/NIS/NVSharpen.hlsl) |
| `NNEDI3\NNEDI3_nns128_win8x4` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/NNEDI3/NNEDI3_nns128_win8x4.hlsl) |
| `NNEDI3\NNEDI3_nns128_win8x6` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/NNEDI3/NNEDI3_nns128_win8x6.hlsl) |
| `NNEDI3\NNEDI3_nns16_win8x4` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/NNEDI3/NNEDI3_nns16_win8x4.hlsl) |
| `NNEDI3\NNEDI3_nns16_win8x6` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/NNEDI3/NNEDI3_nns16_win8x6.hlsl) |
| `NNEDI3\NNEDI3_nns256_win8x4` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/NNEDI3/NNEDI3_nns256_win8x4.hlsl) |
| `NNEDI3\NNEDI3_nns256_win8x6` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/NNEDI3/NNEDI3_nns256_win8x6.hlsl) |
| `NNEDI3\NNEDI3_nns32_win8x4` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/NNEDI3/NNEDI3_nns32_win8x4.hlsl) |
| `NNEDI3\NNEDI3_nns32_win8x6` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/NNEDI3/NNEDI3_nns32_win8x6.hlsl) |
| `NNEDI3\NNEDI3_nns64_win8x4` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/NNEDI3/NNEDI3_nns64_win8x4.hlsl) |
| `NNEDI3\NNEDI3_nns64_win8x6` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/NNEDI3/NNEDI3_nns64_win8x6.hlsl) |
| `Pixel Art\MMPX` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Pixel Art/MMPX.hlsl) |
| `Pixel Art\Pixellate` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Pixel Art/Pixellate.hlsl) |
| `Pixel Art\SharpBilinear` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Pixel Art/SharpBilinear.hlsl) |
| `RAVU\RAVU_3x_R2_RGB` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_3x_R2_RGB.hlsl) |
| `RAVU\RAVU_3x_R2` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_3x_R2.hlsl) |
| `RAVU\RAVU_3x_R3_RGB` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_3x_R3_RGB.hlsl) |
| `RAVU\RAVU_3x_R3` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_3x_R3.hlsl) |
| `RAVU\RAVU_3x_R4_RGB` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_3x_R4_RGB.hlsl) |
| `RAVU\RAVU_3x_R4` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_3x_R4.hlsl) |
| `RAVU\RAVU_Lite_AR_R2` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_Lite_AR_R2.hlsl) |
| `RAVU\RAVU_Lite_AR_R3` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_Lite_AR_R3.hlsl) |
| `RAVU\RAVU_Lite_AR_R4` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_Lite_AR_R4.hlsl) |
| `RAVU\RAVU_Lite_R2` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_Lite_R2.hlsl) |
| `RAVU\RAVU_Lite_R3` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_Lite_R3.hlsl) |
| `RAVU\RAVU_Lite_R4` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_Lite_R4.hlsl) |
| `RAVU\RAVU_R2_RGB` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_R2_RGB.hlsl) |
| `RAVU\RAVU_R2` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_R2.hlsl) |
| `RAVU\RAVU_R3_RGB` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_R3_RGB.hlsl) |
| `RAVU\RAVU_R3` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_R3.hlsl) |
| `RAVU\RAVU_R4_RGB` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_R4_RGB.hlsl) |
| `RAVU\RAVU_R4` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_R4.hlsl) |
| `RAVU\RAVU_Zoom_AR_R2_RGB` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_Zoom_AR_R2_RGB.hlsl) |
| `RAVU\RAVU_Zoom_AR_R2` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_Zoom_AR_R2.hlsl) |
| `RAVU\RAVU_Zoom_AR_R3_RGB` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_Zoom_AR_R3_RGB.hlsl) |
| `RAVU\RAVU_Zoom_AR_R3` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_Zoom_AR_R3.hlsl) |
| `RAVU\RAVU_Zoom_R2_RGB` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_Zoom_R2_RGB.hlsl) |
| `RAVU\RAVU_Zoom_R2` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_Zoom_R2.hlsl) |
| `RAVU\RAVU_Zoom_R3_RGB` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_Zoom_R3_RGB.hlsl) |
| `RAVU\RAVU_Zoom_R3` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/RAVU/RAVU_Zoom_R3.hlsl) |
| `SGSR` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/SGSR.hlsl) |
| `Sharpen\AdaptiveSharpen` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Sharpen/AdaptiveSharpen.hlsl) |
| `Sharpen\FineSharp` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Sharpen/FineSharp.hlsl) |
| `Sharpen\LCAS` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Sharpen/LCAS.hlsl) |
| `Sharpen\LumaSharpen` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/Sharpen/LumaSharpen.hlsl) |
| `SMAA\SMAA_4x_Experimental` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/SMAA/SMAA_4x_Experimental.hlsl) |
| `SMAA\SMAA_4x_NoJitter_Experimental` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/SMAA/SMAA_4x_NoJitter_Experimental.hlsl) |
| `SMAA\SMAA_High` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/SMAA/SMAA_High.hlsl) |
| `SMAA\SMAA_Low` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/SMAA/SMAA_Low.hlsl) |
| `SMAA\SMAA_Medium` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/SMAA/SMAA_Medium.hlsl) |
| `SMAA\SMAA_T2x_Experimental` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/SMAA/SMAA_T2x_Experimental.hlsl) |
| `SMAA\SMAA_T2x_NoJitter_Experimental` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/SMAA/SMAA_T2x_NoJitter_Experimental.hlsl) |
| `SMAA\SMAA_Ultra` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/SMAA/SMAA_Ultra.hlsl) |
| `SSimDownscaler` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/SSimDownscaler.hlsl) |
| `xBR\xBR_2x` | 保留：文案与事实保持 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希；倍率、参数／HDR 回退](../../../src/Effects/xBR/xBR_2x.hlsl) |
| `xBR\xBR_3x` | 保留：文案与事实保持 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希；倍率、参数／HDR 回退](../../../src/Effects/xBR/xBR_3x.hlsl) |
| `xBR\xBR_4x` | 保留：文案与事实保持 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希；倍率、参数／HDR 回退](../../../src/Effects/xBR/xBR_4x.hlsl) |
| `xBR\xBR_NoBlend_3x` | 保留：文案与事实保持 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希；倍率、参数／HDR 回退](../../../src/Effects/xBR/xBR_NoBlend_3x.hlsl) |
| `xBR\xBR_Hybrid_2x` | 修改：摘要、检索 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希；倍率、参数／HDR 回退](../../../src/Effects/xBR/xBR_Hybrid_2x.hlsl) |
| `xBRZ\xBRZ_2x` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/xBRZ/xBRZ_2x.hlsl) |
| `xBRZ\xBRZ_3x` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/xBRZ/xBRZ_3x.hlsl) |
| `xBRZ\xBRZ_4x` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/xBRZ/xBRZ_4x.hlsl) |
| `xBRZ\xBRZ_5x` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/xBRZ/xBRZ_5x.hlsl) |
| `xBRZ\xBRZ_6x` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/xBRZ/xBRZ_6x.hlsl) |
| `xBRZ\xBRZ_Freescale` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希](../../../src/Effects/xBRZ/xBRZ_Freescale.hlsl) |
| `XeSS\XeSS_SR` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希](../../../src/Effects/XeSS/XeSS_SR.hlsl) |
| `XeSSFG\XeSS_FrameGeneration` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希](../../../src/Effects/XeSSFG/XeSS_FrameGeneration.hlsl) |
| `RTXVideo\RTXVideo_Denoise` | 保留：文案与事实保持 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希](../../../src/Effects/RTXVideo/RTXVideo_Denoise.hlsl) |
| `RTXVideo\RTXVideo_VSR` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希](../../../src/Effects/RTXVideo/RTXVideo_VSR.hlsl) |
| `Color\HDR_to_SDR` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希](../../../src/Effects/Color/HDR_to_SDR.hlsl) |
| `Color\SDR_to_HDR` | 修改：详情、检索 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希](../../../src/Effects/Color/SDR_to_HDR.hlsl) |
| `RTXVideo\RTXVideo_HDR` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希](../../../src/Effects/RTXVideo/RTXVideo_HDR.hlsl) |
| `NvidiaVFX\Upscale` | 修改：摘要、详情、子分类、检索 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希](../../../src/Effects/NvidiaVFX/Upscale.hlsl) |
| `NvidiaVFX\VFG` | 修改：摘要、详情、子分类、检索 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希](../../../src/Effects/NvidiaVFX/VFG.hlsl) |
| `FSR3FG\FSR3_FrameGeneration` | 修改：摘要、详情、检索 | 双语复核 | [着色器声明／校验哈希；后端／策略哈希](../../../src/Effects/FSR3FG/FSR3_FrameGeneration.hlsl) |

## 分类／家族／通用说明逐项复核

| 对象 | 处理 | 结论 |
| --- | --- | --- |
| 分类 `upscale` / 放大与超分 | 保留 | 双语用途与共同限制复核 |
| 子分类 `upscale` / 通用 | 保留 | 双语对应关系、选择方向复核 |
| 子分类 `upscale` / 动画线条 | 保留 | 双语对应关系、选择方向复核 |
| 子分类 `upscale` / 像素画 | 保留 | 双语对应关系、选择方向复核 |
| 子分类 `upscale` / 时序重建 | 保留 | 双语对应关系、选择方向复核 |
| 子分类 `upscale` / 缩小与尺寸整理 | 保留 | 双语对应关系、选择方向复核 |
| 分类 `cleanup` / 清理修复 | 保留 | 双语用途与共同限制复核 |
| 子分类 `cleanup` / 降噪 | 保留 | 双语对应关系、选择方向复核 |
| 子分类 `cleanup` / 去色带 | 保留 | 双语对应关系、选择方向复核 |
| 子分类 `cleanup` / 线条修复 | 保留 | 双语对应关系、选择方向复核 |
| 分类 `antialiasing` / 抗锯齿 | 保留 | 双语用途与共同限制复核 |
| 子分类 `antialiasing` / 空间 | 保留 | 双语对应关系、选择方向复核 |
| 子分类 `antialiasing` / 时序 | 保留 | 双语对应关系、选择方向复核 |
| 分类 `sharpen` / 锐化细节 | 保留 | 双语用途与共同限制复核 |
| 子分类 `sharpen` / 通用锐化 | 保留 | 双语对应关系、选择方向复核 |
| 分类 `frame` / 补帧与帧率 | 修改 | 双语用途与共同限制复核 |
| 子分类 `frame` / 补帧 | 保留 | 双语对应关系、选择方向复核 |
| 子分类 `frame` / 帧率调度 | 保留 | 双语对应关系、选择方向复核 |
| 分类 `style` / 风格显示 | 保留 | 双语用途与共同限制复核 |
| 子分类 `style` / CRT | 保留 | 双语对应关系、选择方向复核 |
| 子分类 `style` / 图像调整 | 保留 | 双语对应关系、选择方向复核 |
| 子分类 `style` / AI 画面重塑 | 保留 | 双语对应关系、选择方向复核 |
| 分类 `diagnostic` / 诊断调试 | 保留 | 双语用途与共同限制复核 |
| 子分类 `diagnostic` / 诊断 | 保留 | 双语对应关系、选择方向复核 |
| 分类 `hdr-components` / HDR 组件 | 修改 | 双语用途与共同限制复核 |
| 家族 `anime4k` / Anime4K | 保留 | 双语变体含义、适用场景与非画质排名复核 |
| 家族 `cas` / CAS | 保留 | 双语变体含义、适用场景与非画质排名复核 |
| 家族 `cunny` / CuNNy | 保留 | 双语变体含义、适用场景与非画质排名复核 |
| 家族 `fsr1` / FSR | 保留 | 双语变体含义、适用场景与非画质排名复核 |
| 家族 `fsrcnnx` / FSRCNNX | 保留 | 双语变体含义、适用场景与非画质排名复核 |
| 家族 `fxaa` / FXAA | 保留 | 双语变体含义、适用场景与非画质排名复核 |
| 家族 `nis` / NIS | 保留 | 双语变体含义、适用场景与非画质排名复核 |
| 家族 `nnedi3` / NNEDI3 | 保留 | 双语变体含义、适用场景与非画质排名复核 |
| 家族 `ravu` / RAVU | 保留 | 双语变体含义、适用场景与非画质排名复核 |
| 家族 `smaa` / SMAA | 修改 | 双语变体含义、适用场景与非画质排名复核 |
| 家族 `xbr` / xBR | 保留 | 双语变体含义、适用场景与非画质排名复核 |
| 家族 `xbrz` / xBRZ | 保留 | 双语变体含义、适用场景与非画质排名复核 |
| 家族 `xessfg` / XeSS Frame Generation | 保留 | 双语变体含义、适用场景与非画质排名复核 |
| 子家族 `original` / CuNNy 原版 | 保留 | 双语变体含义、适用场景与非画质排名复核 |
| 子家族 `v2` / CuNNy2 | 保留 | 双语变体含义、适用场景与非画质排名复核 |
| `EffectPicker_CustomSummary` | 保留 | 双语通用用途／未知第三方边界符合规范 |
| `EffectPicker_CustomDetails` | 保留 | 双语通用用途／未知第三方边界符合规范 |
| `EffectPicker_CustomCategoryDescription` | 保留 | 双语通用用途／未知第三方边界符合规范 |
| `EffectPicker_AllDescription` | 保留 | 双语通用用途／未知第三方边界符合规范 |
| `EffectPicker_GettingStartedDescription` | 保留 | 双语通用用途／未知第三方边界符合规范 |
| `EffectPicker_AdvancedDescription` | 保留 | 双语通用用途／未知第三方边界符合规范 |
| `EffectPicker_NoMatchesDescription` | 保留 | 双语通用用途／未知第三方边界符合规范 |
