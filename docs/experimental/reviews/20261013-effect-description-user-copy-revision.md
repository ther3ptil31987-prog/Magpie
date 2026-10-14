# 效果器用户说明重写审查

日期：2026-10-13。执行分支：`feature/effect-description-standards`；起点：`e77f24ba`。

## 双语样稿与阅读评审

先检查这 8 项，再按同一尺度改写全量。用途在前，接适用素材与起步方法，必要启用条件收尾。硬件与 SDR 等事实保留；可选组合、完整名单和开发背景分别归使用文档与历史审查记录。

### Nvidia VFX VSR

`RTXVideo\RTXVideo_VSR`

AI 视频增强，可放大、降噪或去模糊。

增强视频画面的清晰度，适合视频播放与串流内容。先按素材选择放大、降噪或去模糊分类，再调节质量档位；降噪和去模糊保持输入尺寸。需要受支持的 NVIDIA 渲染 GPU 与配套 VFX 运行库，流媒体分类需 Ampere 及更新架构。

AI video enhancement with upscaling, denoising, or deblurring.

Enhances clarity in video playback and streams. Choose an upscaling, denoising, or deblurring category for your content, then set the quality level; denoising and deblurring keep the input size. Requires a supported NVIDIA rendering GPU and matching VFX runtime. Streaming requires Ampere or newer.

### Nvidia VFX Upscale

`NvidiaVFX\Upscale`

放大视频画面，并调节边缘清晰度。

适合视频播放和串流画面的放大。可选择固定倍率，或自动适配目标尺寸；通过锐化强度调整画面的清晰程度。需要受支持的 NVIDIA 渲染 GPU 与配套 VFX 运行库。

Upscales video with adjustable edge sharpness.

Enlarges video playback and streams. Choose a fixed scale or automatically fit the target size, then adjust sharpness to refine edge definition. Requires a supported NVIDIA rendering GPU and matching VFX runtime.

### Nvidia VFX VFG

`NvidiaVFX\VFG`

在视频帧之间补帧，提供 2×～8× 帧数倍率。

为连续视频画面生成中间帧，让运动呈现更连贯，保持画面尺寸。可从 2×、均衡模型开始，再按处理余量调整倍率。处理 SDR 输入；需要 Windows x64、Ada／Blackwell 架构的 NVIDIA 渲染 GPU 与配套 VFX 运行库。

Generates intermediate video frames at 2x–8x frame multipliers.

Adds intermediate frames to continuous video for smoother motion while keeping the image size. Start with 2x and the Balanced model, then adjust the multiplier to suit processing headroom. Takes SDR input; requires Windows x64, an Ada/Blackwell NVIDIA rendering GPU, and the matching VFX runtime.

### FSR3 FrameGeneration

`FSR3FG\FSR3_FrameGeneration`

生成中间帧，让 SDR 画面以固定 2× 倍率补帧。

在连续画面之间生成中间帧，适合改善游戏和视频的运动连贯性，保持画面尺寸。从默认 AMDOF 质量档开始，按处理余量调整光流质量。实验性功能，需支持该功能的 x64 程序、渲染 GPU 与配套运行库；仅支持 SDR 及已适配的效果组合，画面须完整位于输出区域内。

Generates intermediate SDR frames at a fixed 2x multiplier.

Adds intermediate frames for smoother motion in games and video while keeping the image size. Start with the default AMDOF Quality setting and adjust optical-flow quality to suit processing headroom. Experimental; requires an x64 build with this feature, a supported rendering GPU, and matching runtimes. SDR and compatible effect chains only; the image must fit entirely within the output area.

### Nvidia VFX TrueHDR

`RTXVideo\RTXVideo_HDR`

用 AI 将 SDR 画面扩展为 HDR 亮度与颜色。

重新塑造 SDR 画面的亮度和颜色，适合在 HDR 屏幕上观看视频与游戏，保持画面尺寸。先按显示器能力设置峰值亮度，再调节对比度与饱和度。需要 SDR 输入、受支持的 NVIDIA 渲染 GPU 与配套 VFX 运行库，输出屏幕须开启 Windows HDR。

Uses AI to expand SDR brightness and color into HDR.

Reshapes SDR brightness and color for viewing video and games on an HDR display, keeping the image size. Set peak brightness for your display, then adjust contrast and saturation. Requires SDR input, a supported NVIDIA rendering GPU, the matching VFX runtime, and Windows HDR enabled on the output display.

### Bicubic

`Bicubic`

缩放图像，可调节画面的平滑度与锐度。

通过三次插值缩放照片、视频和界面，兼顾边缘清晰度与平滑过渡。在效果组中设置目标尺寸，先使用默认参数，再用 B、C 调整柔和程度与边缘锐度。支持 HDR 浮点处理。

Resizes images with adjustable smoothness and sharpness.

Uses cubic interpolation to resize photos, video, and interfaces with clear edges and smooth transitions. Set the target size in the effect group, start with the defaults, then tune B and C for softness and edge sharpness. Supports floating-point HDR processing.

### CuNNy 4x4C NVL DN

`CuNNy\CuNNy-4x4C-NVL-DN`

动画 2× 放大并降噪，使用 4x4C 模型。

放大动画与视觉小说画面，同时减轻颗粒和压缩噪声。适合轮廓清楚、带有噪声的素材，可直接作为主要放大步骤使用。采用 4 层、4 通道模型，固定放大 2×。

Upscales anime 2x with denoising using the 4x4C model.

Enlarges anime and visual novels while reducing grain and compression noise. Suits noisy content with defined outlines and can serve as the main upscaling step. Uses a 4-layer, 4-channel model at a fixed 2x scale.

### xBR Hybrid 2x

`xBR\xBR_Hybrid_2x`

为已抗锯齿的 SDR 像素画重建轮廓，放大 2×。

重建已有平滑边缘的像素画轮廓，再放大为 2×，适合经过抗锯齿处理的 SDR 游戏与图像。可直接作为主要放大步骤使用。HDR 兼容模式改用最近邻 2× 放大。

Reconstructs anti-aliased SDR pixel-art outlines at 2x scale.

Reconstructs softened pixel-art outlines and enlarges them 2x, for SDR games and images that already have anti-aliasing. Use it as the main upscaling step. HDR compatibility mode uses nearest-neighbor 2x scaling instead.

样稿评审：每项均能独立回答用途、适用画面和起步方法。VFG 与 FSR3 FG 使用相同叙述顺序；前者保留 SDR 输入与架构要求，后者保留固定倍率、组合范围和画面边界。Upscale 不再介绍 VSR 或内部适配算法，CuNNy 不再附画质排名免责声明，Hybrid 保留实际 HDR 回退。复杂原生效果允许条件句略长，不为凑篇幅删除启用条件。

## 执行结果与信息归属

已完成文案 review 和非编译验证。本轮从 `e77f24ba` 继续，在原功能分支修改有效说明来源、资源、生成目录、相关使用文档和静态校验；未改变效果运行逻辑、着色器、ID、名称、分类、推荐等级、参数键／默认值／范围。前一轮 `96dc2251` 的 RCAS 修复与 `8e215462` 的分类／入口修正继续保留。

| 信息 | 当前落点与处理 |
| --- | --- |
| 自身用途、画面变化、适用素材与起步 | 165 个效果的双语摘要／详情；中文详情 49–130 字，中位数 70 字，原为 95 字 |
| 摘要保留／修改 | 中文改写 163 项、保留 2 项；英文改写 164 项、保留 1 项；165 项双语详情均重写 |
| 比较与变体选择 | 8 分类、17 子分类、13 家族、2 子家族，共 40 项双语说明；算法选择与型号含义集中在这一层 |
| 133 项相同 HDR 末尾提醒 | 从叶子移到共用使用说明，记录颜色与高光受转换影响的事实；未替换为另一句重复警告 |
| HDR 特殊行为 | Bicubic 的浮点 HDR、xBR Hybrid 的最近邻回退、NoBlend 的 SDR 调色板、HDR 组件的输入／输出条件保留在自身详情 |
| VFG 可选 HDR 链与后置名单 | [内置效果使用说明](../../Built-in%20effects.md#current-branch-setup)，完整 6 个策略 ID（含补帧标记），说明首个 TrueHDR 分界 |
| FSR3 FG 完整组合 | [FSR3 FG 说明](../effects/FSR3FG.md#compatible-effect-list)，19 个策略 ID（含自身与旧限帧别名）；叶子保留 SDR、固定 2×、兼容范围与画面边界条件 |
| 参数作用、条件与生效方式 | 70 对双语资源：65 对参数提示，3 对自定义说明与 2 对适配提示；其中 21 对新增为原生超分／补帧光流提示 |
| 默认模型、初始化回退、帧率共用规则 | 相关使用文档；分类保留初次选择需要的共用信息 |
| 技术依据与开发背景 | 既有 source/review/runtimeEvidence 与历史审查，不进入产品正文 |

使用文档从中英文 README 的“效果组与参数调节”入口可达，并继续链接 FSR3 FG 完整表。现有选择器不支持这些 Markdown 链接，本轮没有声称新增应用内入口。

## 阅读评审结论

按照用途是否独立、素材是否明确、起步是否可操作、必要条件是否准确、中英文是否等义检查全量。对共享文案的家族先检查各写法，再逐项核对倍率、规格与变体；0.6.9 起的 5 个 xBR，以及 VSR、Upscale、VFG、TrueHDR、FSR3 FG 和相关原生效果单独复核。

- 叶子主体聚焦当前效果；其他效果名称仅在 HDR 配对这一直接使用关系中出现。原有“请比较”“不代表画质排名”“不保证降低延迟”等通用句不再占据每项说明。
- 统一采用用途／场景／关键用法／必要条件的顺序；较复杂的原生效果允许条件句稍长。中文摘要最长 28 字，英文最长 71 字，均在现有 90 字符检查内。
- NoJitter 明确仍使用历史帧；xBR Hybrid 的 HDR 回退、NoBlend 的颜色范围和 FSR3 FG 的 SDR 范围继续可见。必要的“仅”“可能”按具体行为保留。
- 实际阅读修正了 NNEDI3 英文冠词、RAVU 重复措辞、Anime4K 详情缺少放大主语及 SMAA 英文不自然表达。模型加载提示使用条件语气，避免把短暂停顿写成必然。
- 旧 XeSS FG 文案曾将 NVOF 限于 2×，与当前使用文档及 `Renderer.cpp` 的通用光流选择路径不符；新文案取消该旧限制，实际可用性继续由硬件能力检查决定。DLSS 的固定 Preset J 信息归使用文档，不占据叶子主体。
- 本轮没有读者实测或程序界面验收。这里的结论是源码与文本层面的自审，实际换行、截断、提示位置和 GPU 行为留给后续构建。

## 验证与后续交接

| 检查 | 结果 |
| --- | --- |
| Generate-EffectCatalog.ps1 + Validate-EffectCatalog.ps1 | 165 项、双语、分类与子分类、旧名检索、着色器及后端证据、生成产物一致性通过 |
| validate_effect_descriptions.py | 7 组通过；兼容名单从详情断言迁到使用文档，并与实际策略 ID 集合比对；参数提示双语且两处界面继续共用 Tooltip |
| validate_fsr3fg_source.py | 60 项通过，源码检查 |
| validate_fsr3fg_review.py | 45 项通过，源码／XML 检查 |
| 变更边界核对 | 165 项结构、source 声明、ID、显示名、归类及等级不变；资源只改说明／提示，无标签、选项值和占位符变化 |
| 文档入口、资源和格式 | 362 个本地链接、2 个显式锚点、资源重复键／占位符、双语数字一致性、165 行逐项记录及 git diff --check 均通过 |
| 原生测试、产品编译、部署与 release | 本轮未执行 |
| 界面、GPU、颜色、性能与延迟 | 待后续独立会话验收 |

编译会话使用本分支包含本轮文案提交的 HEAD；不要仅取旧提交 `8e215462`。按[执行 TODO](../todos/20261013-effect-description-user-copy-revision-TODO.md)的 U01／U02 检查中英摘要、详情、家族展开、搜索与参数页／运行面板提示，继续前一轮 RCAS 原生用例及所需 GPU 组合验收。

本轮不更新版本号、发布标签、release 文件或用户配置，不合并或推送分支。

## 逐项修改／保留记录

以下 165 行均表示双语详情已重写并复核。摘要按实际变化记录；信息处理只列与这一轮有关的重点，能力事实继续由原审查证据维护。

| 效果 ID | 中文摘要 | 摘要（中／英） | 信息处理 |
| --- | --- | --- | --- |
| `ACNet` | 动画 2× 放大，同时减轻噪声。 | 保留／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_3D_AA_Upscale_US` | 放大 3D 画面至 2×，同时平滑锯齿。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_3D_Upscale_US` | 放大 3D 画面至 2×，重建边缘细节。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Denoise_Bilateral_Mean` | 减轻动画颗粒噪声，使用双边均值滤波。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Denoise_Bilateral_Median` | 减轻动画颗粒噪声，使用双边中值滤波。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Denoise_Bilateral_Mode` | 减轻动画颗粒噪声，使用双边众数滤波。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Restore_L` | 修复动画线条，采用 L 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Restore_M` | 修复动画线条，采用 M 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Restore_S` | 修复动画线条，采用 S 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Restore_Soft_L` | 修复动画线条，采用柔和的 Soft L 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Restore_Soft_M` | 修复动画线条，采用柔和的 Soft M 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Restore_Soft_S` | 修复动画线条，采用柔和的 Soft S 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Restore_Soft_UL` | 修复动画线条，采用柔和的 Soft UL 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Restore_Soft_VL` | 修复动画线条，采用柔和的 Soft VL 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Restore_UL` | 修复动画线条，采用 UL 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Restore_VL` | 修复动画线条，采用 VL 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Thin_HQ` | 细化动画中偏粗的轮廓线条。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Upscale_Denoise_L` | 动画 2× 放大并降噪，使用 L 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Upscale_Denoise_S` | 动画 2× 放大并降噪，使用 S 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Upscale_Denoise_UL` | 动画 2× 放大并降噪，使用 UL 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Upscale_Denoise_VL` | 动画 2× 放大并降噪，使用 VL 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Upscale_GAN_x2_M` | 动画 2× 放大并重塑纹理，使用 M 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Upscale_GAN_x2_S` | 动画 2× 放大并重塑纹理，使用 S 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Upscale_GAN_x3_L` | 动画 3× 放大并重塑纹理，使用 L 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Upscale_L` | 动画 2× 放大与线条重建，使用 L 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Upscale_S` | 动画 2× 放大与线条重建，使用 S 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Upscale_UL` | 动画 2× 放大与线条重建，使用 UL 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Anime4K\Anime4K_Upscale_VL` | 动画 2× 放大与线条重建，使用 VL 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Bicubic` | 缩放图像，可调节画面的平滑度与锐度。 | 改／改 | 自身用途与操作保留；比较及共用取舍归上层／指南 |
| `Bilinear` | 快速缩放图像，形成柔和的像素过渡。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CAS\CAS_Scaling` | 缩放图像，同时增强边缘与局部对比。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CAS\CAS` | 根据局部对比增强边缘清晰度。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CRT\CRT_Easymode` | 用扫描线、遮罩与辉光呈现平面 CRT 风格。 | 改／改 | SDR 风格与 HDR 共用条件归分类／指南；自身外观与起步保留 |
| `CRT\CRT_Geom` | 模拟曲面显像管的弯曲屏幕与扫描线。 | 改／改 | SDR 风格与 HDR 共用条件归分类／指南；自身外观与起步保留 |
| `CRT\CRT_Hyllian` | 用清晰的扫描线与磷光遮罩呈现 CRT 风格。 | 改／改 | SDR 风格与 HDR 共用条件归分类／指南；自身外观与起步保留 |
| `CRT\CRT_Lottes` | 模拟显像管的扫描线、阴影遮罩与亮度分布。 | 改／改 | SDR 风格与 HDR 共用条件归分类／指南；自身外观与起步保留 |
| `CRT\GTU_v050` | 模拟旧式视频的柔化、混色与扫描线。 | 改／改 | SDR 风格与 HDR 共用条件归分类／指南；自身外观与起步保留 |
| `CuNNy\CuNNy-16x16C-NVL-DN` | 动画 2× 放大并降噪，使用 16x16C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-16x16C-NVL` | 动画 2× 放大，使用 16x16C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-2x4C-NVL-DN` | 动画 2× 放大并降噪，使用 2x4C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-2x4C-NVL` | 动画 2× 放大，使用 2x4C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-3x4C-NVL-DN` | 动画 2× 放大并降噪，使用 3x4C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-3x4C-NVL` | 动画 2× 放大，使用 3x4C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-4x16C-NVL-DN` | 动画 2× 放大并降噪，使用 4x16C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-4x16C-NVL` | 动画 2× 放大，使用 4x16C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-4x4C-NVL-DN` | 动画 2× 放大并降噪，使用 4x4C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-4x4C-NVL` | 动画 2× 放大，使用 4x4C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-4x8C-NVL-DN` | 动画 2× 放大并降噪，使用 4x8C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-4x8C-NVL` | 动画 2× 放大，使用 4x8C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-6x8C-NVL-DN` | 动画 2× 放大并降噪，使用 6x8C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-6x8C-NVL` | 动画 2× 放大，使用 6x8C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-8x16C-NVL-DN` | 动画 2× 放大并降噪，使用 8x16C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-8x16C-NVL` | 动画 2× 放大，使用 8x16C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-8x4C-NVL-DN` | 动画 2× 放大并降噪，使用 8x4C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-8x4C-NVL` | 动画 2× 放大，使用 8x4C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-8x8C-NVL-DN` | 动画 2× 放大并降噪，使用 8x8C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy\CuNNy-8x8C-NVL` | 动画 2× 放大，使用 8x8C 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy2\CuNNy-3x12-NVL` | 动画 2× 放大，使用 3x12 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy2\CuNNy-4x12-NVL` | 动画 2× 放大，使用 4x12 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy2\CuNNy-4x16-NVL` | 动画 2× 放大，使用 4x16 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy2\CuNNy-4x24-NVL` | 动画 2× 放大，使用 4x24 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy2\CuNNy-4x32-NVL` | 动画 2× 放大，使用 4x32 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy2\CuNNy-8x32-NVL` | 动画 2× 放大，使用 8x32 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy2\CuNNy-fast-NVL` | 动画 2× 放大，使用 fast 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy2\CuNNy-faster-NVL` | 动画 2× 放大，使用 faster 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `CuNNy2\CuNNy-veryfast-NVL` | 动画 2× 放大，使用 veryfast 模型。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Deband` | 平滑天空与暗部渐变，减轻色带。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Diagnostics\FrameGuidance_Confidence` | 用色图查看帧间运动估计的可靠程度。 | 改／改 | 诊断用途、颜色含义与起步保留；观看免责声明删除 |
| `Diagnostics\FrameGuidance_Motion` | 用色图查看连续捕获帧的运动方向与幅度。 | 改／改 | 诊断用途、颜色含义与起步保留；观看免责声明删除 |
| `DLSS\DLSS_SR` | DLSS 时序超分，结合连续帧重建放大细节。 | 改／改 | 用途与直接条件保留；参数／组合／回退归提示或指南 |
| `DLSSFG\DLSS_FrameGeneration` | 在连续画面之间补帧，可选 2×～4× 倍率。 | 改／改 | 用途与直接条件保留；参数／组合／回退归提示或指南 |
| `DLSSNR\DLSSNR_AI_Filter` | 用 AI 重塑纹理与细节，保持画面尺寸。 | 改／改 | 用途与直接条件保留；参数／组合／回退归提示或指南 |
| `FrameRate_Filter` | 限制捕获与处理的基础帧率。 | 改／改 | 基础限帧用途与设置入口保留；多个上限规则归指南 |
| `FSR\FSR_EASU` | 根据边缘方向重建细节，放大当前画面。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `FSR\FSR_RCAS` | 增强缩放后画面的边缘与局部细节。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `FSR2\FSR2_SR` | FSR2 时序超分，结合连续帧重建放大细节。 | 改／改 | 用途与直接条件保留；参数／组合／回退归提示或指南 |
| `FSR3\FSR3_SR` | FSR3 时序超分，结合连续帧重建放大细节。 | 改／改 | 用途与直接条件保留；参数／组合／回退归提示或指南 |
| `FSR4\FSR4_SR` | FSR4 时序超分，结合连续帧重建放大细节。 | 改／改 | 用途与直接条件保留；参数／组合／回退归提示或指南 |
| `FSRCNNX\FSRCNNX_LineArt` | 使用神经网络将动画与线稿放大至 2×。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `FSRCNNX\FSRCNNX` | 使用神经网络将图像放大至 2×。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `FXAA\FXAA_High` | 平滑当前画面的锯齿，FXAA High 预设。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `FXAA\FXAA_Medium` | 平滑当前画面的锯齿，FXAA Medium 预设。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `FXAA\FXAA_Ultra` | 平滑当前画面的锯齿，FXAA Ultra 预设。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `ImageAdjustment` | 调整图像的亮度、对比度、Gamma 和颜色。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Jinc` | 重采样缩放图像，保留细节与平滑过渡。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `k7_modernAnime_FHD_x2` | 重建全高清动画的线条与细节，放大 2×。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Lanczos` | 以偏锐利的边缘表现缩放图像。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `MLAA\MLAA` | 识别轮廓形状，平滑当前帧的边缘锯齿。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Nearest` | 直接复制像素颜色，保留清晰的像素块。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `NIS\NIS` | 缩放当前画面，同时增强边缘清晰度。 | 改／改 | 自身用途与操作保留；比较及共用取舍归上层／指南 |
| `NIS\NVSharpen` | 增强边缘与局部细节，保持画面尺寸。 | 改／改 | 自身用途与操作保留；比较及共用取舍归上层／指南 |
| `NNEDI3\NNEDI3_nns128_win8x4` | 沿图像边缘插值放大 2×，128 神经元／8×4 窗口。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `NNEDI3\NNEDI3_nns128_win8x6` | 沿图像边缘插值放大 2×，128 神经元／8×6 窗口。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `NNEDI3\NNEDI3_nns16_win8x4` | 沿图像边缘插值放大 2×，16 神经元／8×4 窗口。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `NNEDI3\NNEDI3_nns16_win8x6` | 沿图像边缘插值放大 2×，16 神经元／8×6 窗口。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `NNEDI3\NNEDI3_nns256_win8x4` | 沿图像边缘插值放大 2×，256 神经元／8×4 窗口。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `NNEDI3\NNEDI3_nns256_win8x6` | 沿图像边缘插值放大 2×，256 神经元／8×6 窗口。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `NNEDI3\NNEDI3_nns32_win8x4` | 沿图像边缘插值放大 2×，32 神经元／8×4 窗口。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `NNEDI3\NNEDI3_nns32_win8x6` | 沿图像边缘插值放大 2×，32 神经元／8×6 窗口。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `NNEDI3\NNEDI3_nns64_win8x4` | 沿图像边缘插值放大 2×，64 神经元／8×4 窗口。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `NNEDI3\NNEDI3_nns64_win8x6` | 沿图像边缘插值放大 2×，64 神经元／8×6 窗口。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Pixel Art\MMPX` | 保留像素块面并修整斜线，放大 2×。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Pixel Art\Pixellate` | 按像素覆盖面积缩放像素画。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Pixel Art\SharpBilinear` | 按目标尺寸放大像素画，保留较清晰的像素块。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_3x_R2_RGB` | 放大 3× 并重建彩色边缘，R2 RGB。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_3x_R2` | 放大 3× 并重建亮度细节，R2。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_3x_R3_RGB` | 放大 3× 并重建彩色边缘，R3 RGB。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_3x_R3` | 放大 3× 并重建亮度细节，R3。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_3x_R4_RGB` | 放大 3× 并重建彩色边缘，R4 RGB。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_3x_R4` | 放大 3× 并重建亮度细节，R4。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_Lite_AR_R2` | 放大 2× 并重建亮度细节，Lite AR R2。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_Lite_AR_R3` | 放大 2× 并重建亮度细节，Lite AR R3。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_Lite_AR_R4` | 放大 2× 并重建亮度细节，Lite AR R4。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_Lite_R2` | 放大 2× 并重建亮度细节，Lite R2。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_Lite_R3` | 放大 2× 并重建亮度细节，Lite R3。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_Lite_R4` | 放大 2× 并重建亮度细节，Lite R4。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_R2_RGB` | 放大 2× 并重建彩色边缘，R2 RGB。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_R2` | 放大 2× 并重建亮度细节，R2。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_R3_RGB` | 放大 2× 并重建彩色边缘，R3 RGB。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_R3` | 放大 2× 并重建亮度细节，R3。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_R4_RGB` | 放大 2× 并重建彩色边缘，R4 RGB。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_R4` | 放大 2× 并重建亮度细节，R4。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_Zoom_AR_R2_RGB` | 按目标尺寸放大并重建彩色边缘，AR R2 RGB。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_Zoom_AR_R2` | 按目标尺寸放大并重建亮度细节，AR R2。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_Zoom_AR_R3_RGB` | 按目标尺寸放大并重建彩色边缘，AR R3 RGB。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_Zoom_AR_R3` | 按目标尺寸放大并重建亮度细节，AR R3。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_Zoom_R2_RGB` | 按目标尺寸放大并重建彩色边缘，R2 RGB。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_Zoom_R2` | 按目标尺寸放大并重建亮度细节，R2。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_Zoom_R3_RGB` | 按目标尺寸放大并重建彩色边缘，R3 RGB。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `RAVU\RAVU_Zoom_R3` | 按目标尺寸放大并重建亮度细节，R3。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `SGSR` | 重建当前画面的边缘与细节，实现空间放大。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Sharpen\AdaptiveSharpen` | 根据局部边缘特征自适应增强清晰度。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Sharpen\FineSharp` | 增强细小纹理，并控制边缘光圈。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Sharpen\LCAS` | 线性缩放图像，同时增强局部对比。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `Sharpen\LumaSharpen` | 通过亮度变化增强画面边缘与纹理。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `SMAA\SMAA_4x_Experimental` | 结合连续帧平滑锯齿，SMAA 4x。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `SMAA\SMAA_4x_NoJitter_Experimental` | 结合连续帧平滑锯齿，SMAA 4x 无额外采样抖动。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `SMAA\SMAA_High` | 平滑当前画面的锯齿，SMAA High 预设。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `SMAA\SMAA_Low` | 平滑当前画面的锯齿，SMAA Low 预设。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `SMAA\SMAA_Medium` | 平滑当前画面的锯齿，SMAA Medium 预设。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `SMAA\SMAA_T2x_Experimental` | 结合连续帧平滑锯齿，SMAA T2x。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `SMAA\SMAA_T2x_NoJitter_Experimental` | 结合连续帧平滑锯齿，SMAA T2x 无额外采样抖动。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `SMAA\SMAA_Ultra` | 平滑当前画面的锯齿，SMAA Ultra 预设。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `SSimDownscaler` | 缩小图像，保留细节与局部对比。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `xBR\xBR_2x` | 像素画 2× 放大，平滑斜线与轮廓。 | 改／改 | 用途、倍率与细节保护保留；通用 HDR 归指南 |
| `xBR\xBR_3x` | 像素画 3× 放大，平滑斜线与轮廓。 | 改／改 | 用途、倍率与细节保护保留；通用 HDR 归指南 |
| `xBR\xBR_4x` | 像素画 4× 放大，平滑斜线与轮廓。 | 改／改 | 用途、倍率与细节保护保留；通用 HDR 归指南 |
| `xBR\xBR_NoBlend_3x` | 像素画 3× 放大，保留输入调色板。 | 改／改 | SDR 输入调色板范围保留；算法参数归提示 |
| `xBR\xBR_Hybrid_2x` | 为已抗锯齿的 SDR 像素画重建轮廓，放大 2×。 | 改／改 | SDR 重建与 HDR 最近邻回退保留 |
| `xBRZ\xBRZ_2x` | 平滑像素画轮廓，固定放大 2×。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `xBRZ\xBRZ_3x` | 平滑像素画轮廓，固定放大 3×。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `xBRZ\xBRZ_4x` | 平滑像素画轮廓，固定放大 4×。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `xBRZ\xBRZ_5x` | 平滑像素画轮廓，固定放大 5×。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `xBRZ\xBRZ_6x` | 平滑像素画轮廓，固定放大 6×。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `xBRZ\xBRZ_Freescale` | 平滑像素画轮廓，按目标尺寸放大。 | 改／改 | 通用 HDR 提醒移至共用说明；自身用途与规格保留 |
| `XeSS\XeSS_SR` | XeSS 时序超分，结合连续帧重建放大细节。 | 改／改 | 用途与直接条件保留；参数／组合／回退归提示或指南 |
| `XeSSFG\XeSS_FrameGeneration` | 为游戏与视频补帧，可选 2×～4× 倍率。 | 改／改 | 用途与直接条件保留；参数／组合／回退归提示或指南 |
| `RTXVideo\RTXVideo_Denoise` | 用 AI 减轻视频颗粒与压缩噪声。 | 改／改 | 用途与直接条件保留；参数／组合／回退归提示或指南 |
| `RTXVideo\RTXVideo_VSR` | AI 视频增强，可放大、降噪或去模糊。 | 保留／保留 | 用途与直接条件保留；参数／组合／回退归提示或指南 |
| `Color\HDR_to_SDR` | 将 HDR 亮度映射为 SDR，供处理或观看。 | 改／改 | 用途与直接条件保留；参数／组合／回退归提示或指南 |
| `Color\SDR_to_HDR` | 将 SDR 映射到 HDR，或恢复上游兼容映射。 | 改／改 | 用途与直接条件保留；参数／组合／回退归提示或指南 |
| `RTXVideo\RTXVideo_HDR` | 用 AI 将 SDR 画面扩展为 HDR 亮度与颜色。 | 改／改 | 用途与直接条件保留；参数／组合／回退归提示或指南 |
| `NvidiaVFX\Upscale` | 放大视频画面，并调节边缘清晰度。 | 改／改 | 用途与直接条件保留；参数／组合／回退归提示或指南 |
| `NvidiaVFX\VFG` | 在视频帧之间补帧，提供 2×～8× 帧数倍率。 | 改／改 | 自身补帧与启用条件保留；名单与帧率规则归指南 |
| `FSR3FG\FSR3_FrameGeneration` | 生成中间帧，让 SDR 画面以固定 2× 倍率补帧。 | 改／改 | 自身补帧与启用条件保留；名单与帧率规则归指南 |

## 分类／家族复核覆盖

| 层级 | ID／名称 | 数量 |
| --- | --- | --- |
| 分类及子分类 | upscale：放大与超分、通用、动画线条、像素画、时序重建、缩小与尺寸整理 | 6 |
| 分类及子分类 | cleanup：清理修复、降噪、去色带、线条修复 | 4 |
| 分类及子分类 | antialiasing：抗锯齿、空间、时序 | 3 |
| 分类及子分类 | sharpen：锐化细节、通用锐化 | 2 |
| 分类及子分类 | frame：补帧与帧率、补帧、帧率调度 | 3 |
| 分类及子分类 | style：风格显示、CRT、图像调整、AI 画面重塑 | 4 |
| 分类及子分类 | diagnostic：诊断调试、诊断 | 2 |
| 分类及子分类 | hdr-components：HDR 组件 | 1 |
| 家族 | anime4k：Anime4K | 1 |
| 家族 | cas：CAS | 1 |
| 家族 | cunny：CuNNy | 1 |
| 家族 | fsr1：FSR | 1 |
| 家族 | fsrcnnx：FSRCNNX | 1 |
| 家族 | fxaa：FXAA | 1 |
| 家族 | nis：NIS | 1 |
| 家族 | nnedi3：NNEDI3 | 1 |
| 家族 | ravu：RAVU | 1 |
| 家族 | smaa：SMAA | 1 |
| 家族 | xbr：xBR | 1 |
| 家族 | xbrz：xBRZ | 1 |
| 家族 | xessfg：XeSS Frame Generation | 1 |
| 子家族 | original：CuNNy 原版 | 1 |
| 子家族 | v2：CuNNy2 | 1 |
