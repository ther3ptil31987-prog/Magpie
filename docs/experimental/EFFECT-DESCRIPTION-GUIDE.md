# 效果器说明维护规范 / Effect description guide

适用于内置效果器的简体中文／英文选择器摘要、详情、分类、子分类、家族、子家族，以及相关参数提示。

2026-10-13 写作方向修订：说明首先帮助用户理解和选择效果器。用户读完应能回答：**它做什么、画面会怎样变化、适合什么内容、如何开始使用。** 以这个阅读结果验收，再检查事实与技术一致性。新增效果和旧效果使用相同的信息顺序、语气与篇幅尺度。

本规范已用于本轮 165 项双语说明重写，见[执行 TODO](todos/20261013-effect-description-user-copy-revision-TODO.md)与[样稿、逐项审查及交接](reviews/20261013-effect-description-user-copy-revision.md)。文案审查与静态检查已完成，编译部署及真实界面／GPU 验收由后续会话执行。

## 写作主线

每个效果器围绕自身展开：作用与可感知变化 → 适用素材 → 最关键的使用方法。用熟悉的词说明能做什么，参数名或模型名后给出用户能理解的含义。

摘要通常一句话；详情主体通常为 2–4 句、一个短段。中文详情主体以约 60–120 字作为写作目标，英文表达同等信息；这是编辑尺度，不是程序硬限制。简单效果可以更短，复杂效果先检查信息归属，再补充确有必要的条件。新接入的原生效果也遵守这一尺度。

直接影响启用的显卡、颜色域、尺寸或倍率条件，放在详情末尾的简短条件句中。沿用现有详情字段即可，无需为此增加新界面。用户从搜索直接进入条目，也应能独立理解主体和关键条件。

## 内容分层

| 层级 | 需要回答 | 写法 |
| --- | --- | --- |
| 显示名 | 这是什么效果？ | 使用稳定、可识别的算法／产品名称；与 EffectHelper 保持一致，不带实现阶段记录 |
| 摘要 | 做什么，画面有何变化？ | 一句话，中文通常 20–35 字，用途和效果在前；倍率等能帮助区分的特征按需加入。两种语言均不得换行或超过现有 90 字符检查 |
| 详情 | 适合什么素材，如何开始使用？ | 介绍自身作用、场景及一两个关键用法；必要启用条件收在末尾短句。主体独立成立，阅读不依赖认识其他效果器 |
| 分类／子分类 | 如何按用途选择？ | 说明共同用途和选择方向；跨算法的入门选择建议放在这里 |
| 家族／子家族 | 不同版本怎么选？ | 集中解释 DN、Soft、RGB、AR、NoJitter、倍率和模型规格与用途的关系；叶子只突出自身有用的区别 |
| 参数提示 | 调整后会怎样，何时有效？ | 用界面名称说明实际作用、直接取舍、适用条件和生效方式；范围／默认值／步进由现有控件补充 |
| 自定义通用说明 | 哪里获取未收录效果的可靠说明？ | 引导查看作者的用途、尺寸、链顺序、HDR、运行库和硬件要求，不替第三方承诺能力 |

## 信息取舍与归属

| 信息 | 放置位置 | 编辑要求 |
| --- | --- | --- |
| 本效果的用途、画面变化、适用素材、关键操作 | 当前效果摘要／详情主体 | 优先表达，讲清一条主线 |
| 显卡、SDR／HDR、固定倍率等直接启用条件 | 当前效果详情末尾短句 | 具体、简短；相同条件只有在当前条目确实需要时重复 |
| 调节强度的作用、直接副作用、参数生效方式 | 对应参数提示 | 例如锐化会增强边缘，过高会出现光晕；无需给每个条目附一段通用提醒 |
| 同类算法如何选、同家族各变体如何选 | 分类／家族说明 | 让比较发生在选类或选变体的位置 |
| 可选组合、完整白名单、多步 HDR／补帧配置 | 现有用户使用文档／组合说明 | 单项详情只保留影响使用的简短结论；必要的直接依赖可点名，避免罗列其他效果器 |
| SDK 限制来源、队列、格式、迁移、验证记录 | 开发文档／审查记录 | 作为维护依据，不逐条搬入产品说明 |

“不保证更好”“不代表画质排名”“不保证降低延迟”等通常属于维护者避免过度承诺的写作原则，不作为通用产品句子反复出现。作者应直接采用有依据、具体的表述。保留会直接影响操作的事实，例如“适用于 SDR”“固定 2×”“需要重新启动缩放”；不能靠删除否定词扩大实际能力。

“请比较”“注意”“可能”等词不做机械禁用。每次使用都应对应本效果的具体选择或可操作的取舍。仅为免责而追加、无助于当前选择的句子删除；真实但属于组合或维护背景的信息移到相应文档。共享的 HDR／补帧提醒逐项判断归属，不用另一句通用警告替换后继续批量附加。

提及其他效果器前，判断它是否是当前操作必需的直接依赖。可选搭配和替代推荐移出主体；实现中使用了某个缩放算法，并不自动意味着用户需要在说明里认识它。组合详表需先有真实、明确的文档落点；当前界面没有链接入口时，不写“点击查看”或假定详情支持 Markdown 链接。此轮不扩展界面功能。

## 事实与术语

本节供维护者核实准确性。核实后的信息按前节归属选择，不要求将每条技术结论写入用户说明。

- 描述本项目当前实现，而非厂商 SDK 的能力全集。原生 `.hlsl` 标记不能证明功能；核对后端、能力检查、参数策略、颜色边界与效果链策略。
- 硬件条件以所选**渲染 GPU**为准。只写可核对的架构／运行库限制，不根据品牌名推断支持，也不无依据增加驱动最低版本。
- 分清实时生效、重新启动缩放、完整重启喜鹊。参数面板与运行面板必须使用相同语义；初始化错误要求重启进程，不代表普通参数变更也如此。
- 分清输入尺寸、原生处理尺寸、最终输出尺寸；固定倍率、每轴倍率和补帧倍率不能混用。自动适配需要说明何时有额外缩放。
- HDR 原生／浮点路径、SDR 兼容转换、显式 HDR → SDR／SDR → HDR、特殊回退是不同能力。核对 `Renderer.cpp`、`GroupAHdrRoutes.cpp`、`GroupBHdrRoutes.cpp`、`EffectProtocolCatalogC.h` 以及具体后端。自动边界仍存在，不因显式组件更新而批量删除兼容说明。
- 完整支持列表在组合说明中维护，并按实际策略核对；叶子详情保留影响启用的简短结论。链位置必须与执行分界一致。维护者应核对 VFG 的首个 TrueHDR 分界等事实，但可选链的实现细节不必写进每个条目。
- 维护者区分真实输入、内容提交、生成输出或显示实测帧率。产品正文说明实际作用；性能或延迟收益只写有对应测量依据的结论，避免通过反复附加免责声明来弥补含糊表述。
- 移出队列结构、时间戳回退、内部像素格式、旧配置迁移和发布历史。确实影响操作的迁移限制保留简短提示，详细记录链接到开发／迁移文档。
- 中英文逐项核对倍率、单位、条件、默认值和生效语义。英文按自然语序组织，不把中文短语逐字拼接。

| 中文 | 英文 | 注意 |
| --- | --- | --- |
| 超分／放大 | super resolution / upscaling | 不把普通插值全部称为 AI 超分 |
| 补帧／帧数倍率 | frame generation / frame multiplier | 不与空间放大倍率混淆 |
| 光流 | optical flow | 区分外部估计与游戏真实运动信息 |
| 采样抖动 | sampling jitter | 不译为画面抖动；NoJitter 仍可能使用帧历史 |
| 重新启动缩放 | restart scaling | 与 restart Magpie 区分 |
| SDR 兼容转换 | SDR-compatible conversion | 不等于原生 HDR，也不保证无损 |
| 尼特 | nits | 中文与英文的数值、单位必须一致 |

推荐标签沿用当前审查策略：`first_try` 为易入门起点，`situational` 为素材或条件相关，`advanced` 为实验接入或需要主动设置的流程，`diagnostic` 为诊断。计算开销大不自动意味着实验性；“进阶”快捷入口是一份独立清单，不能由推荐标签推断。

## 正反例

| 避免 | 使用 | 原因 |
| --- | --- | --- |
| “常规、高码率、流媒体、降噪、去模糊，两级选择。” | “AI 视频增强，可放大、降噪或去模糊。” | 摘要先说明用途，控件枚举留给参数区域 |
| “分类改变重启应用。” | “修改分类需重新启动缩放；质量档位可实时切换。” | 准确区分参数生效边界 |
| 在 FSR3 FG 详情罗列十几个其他效果 | 主体介绍补帧作用与使用方式；必要的组合限制收在条件句，详表放组合说明 | 降低阅读一个条目所需的前置知识 |
| “首帧仅输出一次，有界队列逐张消费。” | “保持输入尺寸，在连续帧之间生成中间帧。” | 产品说明不讲与用户决策无关的队列实现 |
| “型号不代表画质排名，请比较不同模型。” | 在家族说明中解释层数／通道数；叶子讲当前变体的用途 | 用有用差异替代重复防御性表述 |
| “NoJitter 是空间抗锯齿。” | “NoJitter 取消额外采样抖动，仍使用帧历史。” | 名称不能代替源码事实 |

Upscale 的主体写法示例（必要硬件条件另置末尾短句）：

> 摘要：放大视频画面，并调节边缘清晰度。
>
> 详情：适合视频播放和串流画面的放大。可选择固定倍率，或自动适配目标尺寸；通过锐化强度调整画面的清晰程度。

该示例摘录已实施的主体文案，完整条目另含硬件与运行库条件。本轮先审阅 VSR、Upscale、VFG、FSR3 FG、TrueHDR 和典型旧效果的双语样稿，再按同一尺度覆盖全量。以后修改同样先校准有代表性的条目，再检查全量一致性。

## 修改入口与生成

当前生成入口是 [Read-EffectCatalogReview.ps1](../../scripts/Read-EffectCatalogReview.ps1)，按以下顺序合并：

1. [基础审查](reviews/20261002-effect-description-review.json)：既有效果及全部分类／家族。
2. [VFX 补充](reviews/20261009-nvidia-vfx-suite-effect-description-review.json)：覆盖 VSR／TrueHDR，并新增 Upscale／VFG。
3. [FSR3 FG 补充](reviews/20261011-fsr3fg-effect-description-review.json)：新增 FSR3 FG。

同一 ID 以合并后的有效条目为准；修改其当前来源，不把被覆盖的旧条目当成实际显示内容。补充审查是现有组织方式，无需为一次文案调整再加一层覆盖。

运行 [Generate-EffectCatalog.ps1](../../scripts/Generate-EffectCatalog.ps1) 生成 `src/Magpie/EffectCatalog/{zh-Hans,en-US,hdr-components}.json`。**不能只改生成产物**。效果 ID 和参数键属于配置兼容协议，改说明不能随意改 ID。

参数提示位于 `src/Magpie/Resources.language-{zh-Hans,en-US}.resw` 的 `EffectParam_<效果键>_<参数键>_Description`。效果键里的非字母数字字符变为 `_`。现有 `EffectParameterLocalization::Tooltip` 供参数页与运行面板共用，不要为相同文字增加第二套入口。查不到说明时按现有标签／数字提示回退。

组合与共用设置的现有落点为[内置效果使用说明](../Built-in%20effects.md#current-branch-setup)，中英文仓库首页均提供入口；FSR3 FG 的[完整清单](effects/FSR3FG.md#compatible-effect-list)由该文档继续链接。自动检查将文档中的支持 ID 与实际策略比对。选择器详情本身不提供这些文档的点击入口，维护者不能把仓库入口描述成应用内链接。

`search` 收纳当前名称、用途词、参数词、必要别名与当前说明；更新文字时同步刷新，不不断追加整段旧说明。保留有效旧文件名、RTX 档位后缀、FSR3FG 等检索入口。分类有子分类时，每个效果须落入其中一个；简体中文与英文应对应同一子分类位置。

## 审查证据

每个效果保留 `source`（着色器路径、SHA-256、尺寸／参数声明）和 `review`。新增或行为变更时核对当前文件，不把旧审查状态直接等同于当前验收。

- `status: source-reviewed` 仅表示源码审查。
- `copyReviewedOn`、`copyDisposition`、`copyScope` 记录说明复核日期、修改／保留结论及边界。
- `runtimeEvidence` 是历史字段名，实际内容为后端／策略等源码路径，**不是实机运行证据**。
- 原生效果除标记着色器外，还应列出与说明有关的后端、参数生效和链限制文件，并在 `runtimeEvidenceSha256` 记录对应 SHA-256。哈希按 UTF-8 文件字节归一化 CRLF 为 LF 后计算。现有检查要求记录的路径与哈希一一对应，并强制 VFX／RTX／FSR3FG 有后端依据；其他原生效果也应按此维护。
- 后端哈希变化时，先核对受影响的事实，再更新证据；不要仅重算哈希绕过审查。哈希没有变化也不自动证明说明正确。
- `externalReferences` 记录具体版本的一手文档。SDK 默认值与产品默认值不同，以项目参数声明和实际策略为准。
- `notVerified` 明确未做的构建、真实界面、SDK、GPU、颜色、性能或延迟验证。审查报告分别记录这些状态。

## 验证与交付

首先人工审阅说明：不了解算法的用户能否说出“它做什么、适合什么画面、怎么开始用”？读懂单项是否需要先认识一串其他效果？新旧效果的语气和信息密度是否一致？没有帮助选择的免责声明、重复提醒或开发记录是否仍占据主体？必要启用条件是否简明且准确？这些问题是文案验收的首要标准。

从源码仓库根目录执行：

```powershell
pwsh -NoProfile -File scripts/Generate-EffectCatalog.ps1
pwsh -NoProfile -File tests/Validate-EffectCatalog.ps1
python -X utf8 tests/validate_effect_descriptions.py
git diff --check
```

这些检查覆盖生成一致性、安装 ID、双语结构／子分类、长度、旧名检索、新效果入口、相关提示资源和审查依据。检查通过只表示这些合同成立；文案是否易懂、是否聚焦自身，仍按前面的人工标准评审。若旧测试要求在详情中出现完整兼容名单，迁移文案时同步调整证据位置与断言，避免测试迫使产品继续堆砌名单。

如改变选择器模型或链策略，更新相应 `EffectPickerTests.cpp`／`NvidiaVfxPolicyTests.cpp` 的行为用例。在允许编译的会话执行原生测试；静态匹配不能代替执行。变更后仅运行相关检查，不因规范要求而无条件全项目构建。

使用包含本轮提交的程序，分别在中英文下检查摘要截断、长详情滚动、家族展开、分类／进阶入口、旧名搜索、参数页及运行面板提示。原生效果还需按改变的能力做实际启动和 GPU 验证。未构建时保留为待验收，交接基线、分支、测试命令与限制。

## English essentials

Help users understand what an effect does, how it changes the picture, what content it suits, and how to start using it. Keep each entry focused on itself. Use one sentence for the summary and usually one short paragraph for the main description, with the same tone and information density across old and new effects.

Keep essential activation requirements in a brief closing condition. Put parameter behaviour in parameter help, comparisons in category/family guidance, optional chains and full compatibility lists in usage documentation, and implementation evidence in review records. Use specific, factual language rather than repeated defensive disclaimers. A necessary limitation still belongs where it affects the user's decision. Assess readability before treating structural checks as completion.

Update the effective review JSON and both languages, regenerate the catalogs, then run the checks above. Keep IDs and useful legacy search aliases stable. Native marker shaders need backend and policy evidence; `runtimeEvidence` names source files, not runtime tests. Refresh recorded hashes only after reviewing the change. Report source checks, native test execution, product build, deployment, UI and GPU acceptance separately.
