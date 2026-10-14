# FSR3 FG 必须提供外部光流：移除 None，默认 AMDOF 质量

> **已合并，以下为历史诊断与方案归档。** 自 2026-10-12 起，执行范围、任务状态和验收统一以[FG 输入设置统一与空闲重绘：合并执行 TODO](20261012-fg-input-settings-and-idle-redraw-TODO.md)为准。本文旧任务编号不再独立维护，不代表已执行。

日期：2026-10-12（Asia/Shanghai）。状态：**诊断与修改方案已完成；产品修改、编译、部署待执行。**

用户已完成实机对照：启用光流估算后可看到补帧，选择“无”时看不到效果。本方案据此收敛原来的广泛排查：修复 FSR3 FG 的输入选择与默认行为。帧率问号不是这项修复的主问题。

## 1. 结论

**当前问题是把“不提供有效外部运动”的实验路径当成了正常默认模式，不是漏把运动纹理指针设成空。**

- 当前 None 传给 SDK 的是有效的全零运动纹理和恒定深度，并非 `nullptr`。零向量表示该像素没有位移，不表示“请忽略这路数据”。
- 固定 SDK 的 Prepare 实现明确断言 depth 和 motionVectors 非空。将 motionVectors 置空不属于受支持的纯内部光流模式，也不是本次修复方案。
- 内部光流仍运行，但 FSR 插值会同时使用外部运动场和内部光流场。全零外部场仍作为有效主运动场参与运算，内部光流不能保证补救它。
- SDK 内部光流场过滤小位移；当相关邻域均无可用内部光流、外部场又是零时，非 reset 的内部区域可退化为前后颜色混合，无法得到正确的中间运动位置。SDK 调用仍可以返回成功。
- 因此不能说“FSR3 在零向量下对所有画面都绝对没有插值”，但可以确定：**当前 Magpie 的 None 路径不能作为可靠的帧生成模式提供。** 用户对照和本机新日志已足以支持移除它，无需等待修好 FPS 统计再实施。

产品决定：FSR3 FrameGeneration 只提供 **AMDOF / NVOF**；新建、重置和迁移掉 None 时默认 **AMDOF + 质量**。仅针对 FSR3 FG；其他 FG/SR 的 None 不随之删除。

新增的跨 FG 需求单独见[重复帧设置统一与空闲重绘 TODO](20261012-fg-duplicate-filter-and-idle-redraw-TODO.md)：用户已确认静止时只是 FPS 归零、恢复运动可正常更新；需要处理 FG 忽略空闲重绘设置及重复设置的困惑，不将其误诊为卡死。该项不阻塞本文件的运动输入修复。

## 2. 已核验的证据

### 2.1 本机实测与日志

产品基线仍为 `release/0.6.10-beta7` / `34c3d6dd13fe11cd0cae94a2ab23e936a866d95a`，检查时工作树干净。之前的 `fe31c082` 外部运动资源修复已经合入并部署；这次不是继续处理已修复的 SRV 创建错误。

日志：`release/v0.6.10-beta7/Magpie-Experimental-x64/logs/magpie.log`。本轮读取快照为 319341 字节、2789 行；混合编码按替换方式解码，以下 ASCII 参数和计数不受影响。

| 会话 | 配置与资源 | 已有结果 |
| --- | --- | --- |
| 01:00:09.792 开始 | `opticalFlowMethod=0`，`externalMotion=false`，2560×1391 内容、2560×1440 输出 | SDK 仍持续 dispatch；到 source=720 时 interpolationDispatches=717，resetPassThroughs=3（期间捕获会话变化） |
| 01:00:22.404 开始 | 同一捕获窗口/内容尺寸，`opticalFlowMethod=1`，`externalMotion=true`，重复帧过滤仍为 0 | 明确初始化 `Quality-100%`，共享 provider `method=1 quality=1`；到 source=720 时 interpolationDispatches=719，resetPassThroughs=1，invalidMotionSamples=1 |
| 01:00:36.568 开始 | AMDOF 质量，另将重复帧过滤设为 1 | AMDOF 继续初始化并提交；过滤开关是另一个变量，不混入上一组对照 |

关键日志位置：None 参数 2225、初始化 2248；AMDOF 参数 2363、初始化 2386、质量 2403、provider 2413、首帧 2416、稳定段 2438。上述两个主要 FSR 会话未见 error 记录。

用户报告提供“可见效果”的证据；日志确认实际走了不同的运动输入路径，并且 AMDOF 仅初始预热一次运动样本无效。日志中的 dispatch 数本身仍不等于有效中间图像数或显示数。未将不同来源的证据混为 GPU 自动验收。

### 2.2 SDK 输入契约和算法

产品源码行号以当前 Beta7 为准：

- `src/Effects/FSR3FG/FSR3_FrameGeneration.hlsl:18–29`：method 默认 0；AMDOF quality 默认 1。
- `src/Magpie.Core/Renderer.cpp:431–438`：FSR3 的运行时 method 缺省也为 0；AMDOF quality 缺省为 1。
- `src/Magpie.Core/FSR3FGPresenter.cpp:356–365,777–778`：创建全零运动纹理；有效外部运动才改用 motion12。

固定 SDK 根目录：`dependencies/FSR-SDK-v2.3.0/Kits/FidelityFX/`。下列路径位于该根目录：

| SDK 源码 | 结论 |
| --- | --- |
| `framegeneration/fsr3/internal/ffx_frameinterpolation.cpp:929–942` | 断言 depth、motionVectors 都非空，然后注册输入资源；没有“空运动输入就切换纯光流”的分支 |
| `framegeneration/fsr3/internal/ffx_provider_fsr3framegeneration.cpp:570–608` | 内部 optical flow 仍执行，输出按显示尺寸归一化 |
| `framegeneration/fsr3/include/gpu/frameinterpolation/ffx_frameinterpolation_game_motion_vector_field.h:30–75` | 深度主优先级至少为 1，零向量也被打包为有效 primary game motion field |
| `framegeneration/fsr3/include/gpu/frameinterpolation/ffx_frameinterpolation_optical_flow_vector_field.h:40–43,82–84,104–108` | 内部 OF 场使用 `length(vector * InterpolationRectSize()) > 1` 门槛；小位移不参与该场累积 |
| `framegeneration/fsr3/include/gpu/frameinterpolation/ffx_frameinterpolation.h:92–165` | 同时按 game MV / OF 取样，再混合；reset 后前 10 帧偏向 game MV |
| 同文件 `174–177` | 内部 reset 时复制当前帧；这是另一条路径，不把它说成所有 None 失败的唯一原因 |

AMDOF 与 SDK 内部 OF 的作用不同：AMDOF 通过 Magpie 的 Frame Guidance 把估计运动提供给 SDK 的 **motionVectors 输入**，使原本全零的 game MV 分支具备位移；“SDK 自带 OF”不能代替完成这条输入。

[AMD 官方 Prepare 文档](https://gpuopen.com/manuals/fsr_sdk/techniques/frame-interpolation-api/#dispatch-frame-generation-preparation)也列出 motionVectors、depth 及其输入要求。是否支持空资源的决定以以上固定版本源码为准。

### 2.3 本轮轻量验证

已用 Python 复算源码的小运动门槛：3 组内容矩形 × 水平 0.5 / 1 / 2 / 4 像素位移，共 12 项计算检查通过。

| 输出/内容尺寸 | 1 像素水平位移得到的 SDK 门槛量 | `> 1` |
| --- | --- | --- |
| 2560×1440 / 2560×1440 | 1.000000 | 不通过 |
| 2560×1440 / 2560×1391（新对照） | 1.000000 | 不通过 |
| 2560×1440 / 1924×1177（早先会话） | 0.751563 | 不通过 |

2 / 4 像素在这三组水平计算中通过门槛。这里只验证 shader 公式；内部 OF 本身的估计误差、邻域传播、遮挡、重置和最终图像不在这 12 项检查范围内。未新开缩放、未调用 GPU SDK 做回读、未编译产品。没有必要为了验证已明确的非空契约，向实际 SDK 提交非法空纹理。

## 3. 具体修改方案

### 3.1 选项和旧配置

保留效果 ID、参数 key 和数值语义：`1=AMDOF`、`2=NVOF`，**不重新编号**。删除 FSR3 marker 的 `OPTION 0 None`，method DEFAULT 改为 1，AMDOF quality DEFAULT 维持 1（质量）。

| 输入配置 | 规范化后的行为 |
| --- | --- |
| 新建 / 参数缺省 / 重置默认 | AMDOF + 质量（1,1） |
| 旧 `opticalFlowMethod=0` | 迁移为 AMDOF + 质量（1,1）；记录迁移，下一次正常保存持久化 |
| method 越界、非整数或非有限值 | 回到 AMDOF + 质量；仅规范化当前 FSR3 FG 项 |
| 已有 AMDOF，quality=0 或 1 | 保留性能/质量的明确选择；缺省或非法 quality 才设为 1 |
| 已有 NVOF=2 | 保留 NVOF 及其合法质量档；不因新默认覆盖它 |
| DLSS / XeSS FG、FSR SR 或同名参数的自定义效果 | 保持原有语义 |

缺省 method 和旧 None 一样以完整默认组合（1,1）处理，避免残留的隐藏 AMDOF performance 参数把默认质量改回性能。迁移需幂等，复用现有配置备份/保存流程，不批量重写效果组或重排效果链。

涉及入口必须一致：App 启动读取/导入、添加/复制效果、参数重置、运行中参数会话、Core 运行时读取。不能只隐藏下拉选项：旧 JSON 或绕过 App 的直接启动仍可能带入 0。

### 3.2 运行约束和不可用状态

- FSR3 FG 的正常运行必须请求 AMDOF 或 NVOF，Presenter 必须要求外部运动。共享 `OpticalFlowMethod::None` 枚举仍留给其他功能。
- **保留零运动纹理作为首帧/断流/reset 的合法占位资源**。移除 None 不等于删除它，也不把“真实静止时有效光流全为零”判成故障。
- 延续 frameID/session/generation 与运动资源绑定、SRV 检查及 reset 逻辑。暂缺/过期样本仅做明确的预热或 reset；provider 初始化/执行失败不能静默退回长期 None。
- 外部运动当前只支持已验证的矩形效果链。移除“CRT/畸变请选择 None”的提示，改为明确指出不兼容的效果并要求移除它或停用 FSR3 FG。保留原配置供用户编辑，不擅自删除效果。
- 首版不扩展 distortion field，不在最终输出上再造一套光流，也不修改 AMD DLL/shader 算法。继续 SDR、固定 2×，不重新施加 WGC 专用限制。
- AMDOF 缺失、硬件不支持或 NVOF 初始化失败时给出准确原因；不自动切成 None，不将失败伪装为正常插帧。

### 3.3 构建、说明与 UI 一致性

`EnableAmdOpticalFlow` 目前缺省只跟随 `EnableFSR3ZeroMV`，与 FG 独立。默认改为 AMDOF 后需一起解决构建依赖：FG 开启且 AMDOF 未指定时默认启用 AMDOF；显式禁用 AMDOF 与开启 FSR3 FG 的冲突在构建/打包预检中明确报错。不能产出“提供 FSR3 FG 但其默认 provider 不存在”的正常发行包。

`EffectParameterRules.h:37` 的缺省可见性目前只对 XeSS 设为 AMDOF，需要加入 FSR3，保证未展开参数时质量控件也正确显示。共享 choice 机制按数值查找选项，保留 1/2 可用，但两个 UI 都需回归默认/导入/重置。

同步中英文资源、审阅源与生成目录、分发 README 和初始化错误。说明需要准确写成“FSR3 FG 要求外部运动估算，默认 AMDOF 质量；图像估计运动和合成深度仍属实验性输入”，删掉“默认无”和“未知效果请选择无”的旧指导。

## 4. 待执行 TODO

- [ ] **P0-01 默认值和参数策略统一。** 从执行时最新 Beta7 创建隔离修复分支；FSR3 HLSL 删除 None、默认 AMDOF 质量；Core 对缺省/0/非法值采用同一策略；共享参数可见性同步。其他 FG 默认不改。
- [ ] **P0-02 配置兼容迁移。** 在 `ScalingModesService` 的加载/导入链路接入 FSR3 专用规范化，按第 3.1 节处理；保存迁移计数和幂等性；验证直接 Core 启动、参数重置和运行中保存不会重新引入 None。
- [ ] **P0-03 强制有效运动路径。** 更新 Renderer/Presenter 的外部运动要求；保留合法预热零纹理和静止零向量；缺组件、provider 失败和资源契约错误明确报错。保留之前的 SRV 与帧身份修复。
- [ ] **P0-04 收紧不兼容效果链提示。** 去掉 None 绕过入口；对 CRT、畸变及未验证链列出具体不兼容效果，保留可编辑配置，不静默降级。
- [ ] **P0-05 补齐构建依赖。** 调整 `src/BuildOptions.props` 的 AMDOF 缺省依赖及 MSBuild/`scripts/Build-Release.ps1` 预检，使 manifest 与实际构建一致；覆盖显式禁用冲突和 SkipBuild 打包路径。NVOF 仍可选。
- [ ] **P0-06 同步说明和参数 UI。** 更新 FSR3 中英文资源、`20261011-fsr3fg-effect-description-review.json` 审阅源、生成目录和 `docs/experimental/effects/FSR3FG.md`；检查两处参数 UI 只显示 AMDOF/NVOF、默认显示质量。不把旧历史报告全部改写成新行为。
- [ ] **P1-01 配置与策略回归。** 通过实际生产规范化函数测试缺省、0、1、2、非法数字、质量档保留、幂等、导入/导出/再启动和 Reset；验证其他 FG/SR 及自定义效果不变。更新已有 source/catalog/参数检查，避免只写与实现同构的文本断言。
- [ ] **P1-02 图像与实机回归。** 在后续获准构建的候选版本上，复测用户当前素材，确认不手动选择即可复现本次 AMDOF 质量收益；用 30/60 FPS 带帧编号的平移纹理验证正确中间位置，含 0.5/1/4 像素每输入帧、静止、切场景、黑边、Resize、启停和断流恢复。对照使用不含 FG 的同条件效果组，必要时受控回读前/后/生成帧；只有颜色差异或 dispatch 翻倍不能当作通过。NVOF 在可用硬件上独立验证。
- [ ] **P1-03 构建与回归交接。** 后续单独会话完成构建/部署；复用现有 FSR3 motion resource、history policy、catalog 和参数测试，验证 FG 开/关构建。记录用户实测、源码验证、GPU 图像和显示证据各自状态。普通呈现、DLSS/XeSS FG 不回归。
- [ ] **P2-01 附带问题继续保留。** 问号字形、FSR 异步输出帧率统计及 Core 名称遗漏仍见[原 TODO](20261012-fsr3fg-effective-interpolation-and-fps-TODO.md) 的 P1；作为独立项排期，不阻塞本次明确的输入修复，不用基础 FPS × 2 伪造统计。

## 5. 验收与边界

先实现 P0-01～06，做 P1 验证；不用先建设完整的呈现诊断系统才能修复已确认的默认输入问题。

最低通过条件：新建、旧 None、导入和重置都进入 AMDOF 质量；既有合法 AMDOF/NVOF 选择保留；FSR3 UI 和运行路径均不可再选择 None；缺 provider/不兼容链不会静默退化；首帧正常预热后有稳定的正确中间运动；其他效果不受影响。

当前仅完成用户实测证据复核、源码/SDK 契约检查、12 项公式计算和本 TODO。**尚未执行以上修改，没有修改用户配置，没有编译、部署、提交或推送。** 原广泛排查中的内部 reset / 最终呈现故障只在后续仍有反例时重新调查，不再作为推迟本修复的前置条件。
