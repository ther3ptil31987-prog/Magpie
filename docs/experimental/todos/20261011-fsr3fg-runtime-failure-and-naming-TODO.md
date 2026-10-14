# FSR3 FG 运行失败定位与命名统一 TODO

日期：2026-10-11（Asia/Shanghai）。状态：**本次源码修复和命名统一完成；未编译部署，原生测试与实机验收待后续会话执行**。

前次诊断只读检查实现、已部署包与现有日志，整理后续方案；没有修改产品源码、名称、配置或运行包，没有编译、重新启动缩放、部署或推送。本次执行记录见第 7 节。本文件补充原 [Issue 19 TODO](20261011-issue19-fsr3-frame-generation-TODO.md)，不把原先的源码交付状态当作运行验收。

## 1. 当前结论

**本机这次 FSR3 FG 停止的直接原因已定位：前端稳定光流纹理没有 `D3D11_BIND_SHADER_RESOURCE`，FSR3FGPresenter 却要为它创建 SRV。**

失败发生于外部光流路径，SDK 初始化和首帧已经成功。此次只看到一张 reset 复制帧，尚没有成功插值调度记录。不能据此宣称 FSR3 FG 整体不支持该 GPU，也不能把关闭外部光流视为已验证修复。

后续优先修复纹理使用契约并增加实际 D3D 资源回归，再统一显示名称。保持 SDR、固定 2×、现有效果 ID 和参数默认值；其他限制不在本次修复中扩展。

## 2. 调查基线与证据

- 产品源码：`C:\Users\81443\Documents\Magpie enhance\.worktrees\0610-beta7\source`，`release/0.6.10-beta7`，提交 `744d78994023e99d82901ef1e3c9b85a3d3b5f02`；检查时干净。
- 原功能分支：`feature/fsr3-frame-generation`，提交 `46ae3dd8aeadc85c7ecd67a17d60bf98d64f9b1e`；是当前 Beta7 的祖先。两边 `FSR3FGPresenter.cpp`、`FSR3FGPolicy.h` 相同，因此不是 Beta7 修改这两个文件造成的回归。
- 已部署包：`C:\Users\81443\Documents\Magpie enhance\release\v0.6.10-beta7\Magpie-Experimental-x64`。`build-manifest.json` 指向上述 Beta7 提交，`EnableFSR3FrameGeneration=true`。
- 实际核验 EXE、FG provider、loader、FSR3 marker 的 SHA-256 均匹配当前包清单；两颗 AMD DLL 也匹配原 SDK 固定值。
- 日志：上述包内 `logs\magpie.log`，本次读取长度 99,430 字节，最后写入时间 2026-10-11 20:05:11。日志由运行进程占用，未强制关闭进程或修改日志；以下按本轮读取的行号和时间定位。

| 本地时间 | 日志行 | 事实 |
| --- | --- | --- |
| 20:04:11.954 | 734–738 | 仅启用 `FSR3FG\FSR3_FrameGeneration`，`opticalFlowMethod=1`（AMDOF） |
| 20:04:12.003 | 752–755 | 渲染 GPU 为 NVIDIA GeForce RTX 5070 Ti，PCI `10DE:2C05` |
| 20:04:12.087 | 758–759 | 找到 FG 3.1.6 / swapchain 3.1.7 provider |
| 20:04:12.895 | 762 | FSR3FG 初始化成功：2560×1440、SDR RGBA8、2×、外部运动开启 |
| 20:04:12.916–12.999 | 769–780 | WGC 初始化及 marker 编译成功，捕获 1924×1177，AMD 光流初始化成功 |
| 20:04:13.078 | 795 | `baseSubmits=1 callbackDispatches=1 interpolationDispatches=0 resetPassThroughs=1 invalidMotionSamples=1` |
| 20:04:13.111 | 800–801 | **首个 FSR3 致命错误：`FSR3FG create source motion SRV`，`HRESULT: 0x80070057`** |
| 20:04:13.115 | 802 | 上层报告 `MP-028`，`system=0`，随后结束缩放 |

后面的 20:04:49 DLSSFG `0xbad00004` 是另一次独立尝试，不合并为 FSR3 的原因。更早的热键注册错误及退出时 `LastErrorCode: 1400` 也不是本次首个 FSR3 错误。

## 3. 根因及与 XeSSFG 的差异

以下源码行号均针对 Beta7 `744d7899`：

1. `Renderer.cpp:3193–3204`：后端共享光流是 `R16G16_FLOAT`，正确带有 `D3D11_BIND_SHADER_RESOURCE`。
2. `Renderer::_UpdateFrontendBase`，`Renderer.cpp:935–958`：将共享光流复制到 `_frontendMotionTexture`。创建这张前端稳定副本时，**第 949 行清成 `motionDesc.BindFlags = 0`**。
3. `Renderer.cpp:1094–1103`：将这张稳定副本交给 Presenter 的 `SetFrameGuidance`。
4. `FSR3FGPresenter.cpp:687–707`：检查格式、尺寸与采样数，随后第 695 行 `CreateShaderResourceView(motion, nullptr, ...)`，再用 compute shader 重映射光流。
5. Microsoft 明确规定：用于 `CreateShaderResourceView` 的资源必须创建时带 `D3D11_BIND_SHADER_RESOURCE`。因此第 2 步与第 4 步不兼容，和日志中的 `E_INVALIDARG` 一致。[官方 API 契约](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createshaderresourceview)

XeSSFG 的对应处理在 `XeSSFGPresenter.cpp:696–725`：清空自己的 motion UAV，再 `CopySubresourceRegion` 复制稳定纹理的内容；不对输入稳定纹理创建 SRV。因此沿用 XeSSFG 的 `BindFlags=0` 创建方式时，必须考虑 FSR 新增的着色器读取需求。

首帧可呈现也符合实现：首帧外部 motion 尚未有效，进入零运动/reset 路径；首次有效外部运动到达时才创建 SRV，随后失败。AMDOF 和 NVOF 最终经过同一张稳定纹理，因此 **NVOF 也存在同一结构性缺陷**；本轮没有 NVOF 实机复现记录。

其他模式的边界：`None` 在 `FSR3FGPresenter.cpp:685` 提前返回，不进入故障 SRV 路径，FSR 内部光流仍工作；它是后续对照用例，不能以代码分支推断其运行/画质已合格。WGC 当时有有效原生时间戳，本次错误不支持重新施加 WGC 限制。

## 4. 同时确认的缺口

- **诊断丢失原始错误。** `Impl::Fail` 只写日志和设置 fatal；`Renderer::_ReportPresenterFailure` 第 1184 行给用户的上下文固定，系统码为 0。实际 `0x80070057` 没有传到上层报告。
- **名称来源不一致。** 添加效果器面板使用目录名称；已添加条目/组合名称等通过 `EffectHelper::GetDisplayName`。FSR 目录显示 `FSR 3 FG（实验性）` / `FSR 3 FG (Experimental)`，Helper 没有 FSR 映射，返回 `FSR3_FrameGeneration`。只改 JSON 不能统一所有入口。
- **测试没有覆盖资源契约。** 本轮重新运行 `validate_fsr3fg_source.py --sdk-directory ...` 仍通过 118 项；该脚本检查接线、元数据和 API 符号，不创建 D3D 纹理/SRV。此前 Beta7 的 native policy 测试记录为 48 项通过，同样不覆盖 GPU 资源。编译成功和这两组测试都无法发现本次错误。

## 5. 待执行 TODO

### P0 — 修复外部运动纹理契约

- [x] **P0-01 确认修复基线。** 执行时重新核对最新部署清单、分支和脏文件，从包含已部署 Beta7 修复的准确提交准备修复分支。原功能分支比当前产品旧，不直接拿它构建覆盖 Beta7。保留其他会话的工作。
- [x] **P0-02 让 FSR 稳定光流副本可被采样。** 在 `Renderer::_UpdateFrontendBase` 创建 `_frontendMotionTexture` 时，根据 FSR Presenter 的实际需求设置 `D3D11_BIND_SHADER_RESOURCE`；保留 `D3D11_USAGE_DEFAULT`、无 CPU access、无共享标记。XeSS copy-only 路径可保持原契约。重建判断除尺寸、格式外纳入所需 bind 能力，避免复用不满足要求的纹理。
- [x] **P0-03 补充消费端校验。** SRV 创建前检查格式、维度、Mip/Array、SampleDesc、bind 能力和所属前端设备，失败记录实际 desc、frameId/会话/资源代次与 HRESULT。内部契约错误继续明确停止，不静默切成复制帧后声称补帧正常。SRV 缓存仅在成功后更新，并检查视图本身有效。
- [x] **P0-04 保持现有共享同步。** 继续在 keyed-mutex 事务内复制到稳定纹理，随后释放共享 slot；FSR 使用稳定副本采样。不要直接把后端共享槽交给 Presenter 长期持有；不因本次修复破坏源帧与 motion 身份匹配、资源重建及 D3D11/12 fence 顺序。
- [ ] **P0-05 增加可捕获此缺陷的原生资源测试。** 优先复用/提取生产中稳定纹理创建逻辑，测试实际 `CreateTexture2D → CopyResource → CreateShaderResourceView → motion remap`。用 R16G16_FLOAT、不同尺寸、尺寸重建、首帧无 motion/第二帧有效 motion 覆盖边界；验证旧 bind=0 输入被明确识别，新生产路径可创建 SRV且重映射结果正确。避免仅测试一份手写正确 desc。WARP 结果仅证明资源/算法契约，不替代 AMD SDK 实机验收。
- [ ] **P0-06 编译后实机复测。** 经后续编译执行范围授权后，开启 D3D11/12 调试层，用本次 PotPlayer/WGC/1924×1177→2560×1440/AMDOF 条件回归；至少持续 60 秒，越过第一张有效 motion，确认不再出现 SRV 错误，持续提交且非 reset 插值调度增长，并检查实际画面及呈现事件。`initialized`、首帧/reset 复制与 SDK dispatch 计数都不能单独作为有效显示补帧验收。

### P1 — 名称统一与诊断可读性

- [x] **P1-01 统一 FG 显示格式。** 采用已有 DLSS 目录的 `<技术名> FrameGeneration` 风格：`DLSS FrameGeneration`、`XeSS FrameGeneration`、`FSR3 FrameGeneration`。FSR 移除标题内的“实验性”后缀，将“实验性 / 固定 2× / SDR”保留在摘要和详情。`Nvidia VFX VFG` 保留既有系列名称，不在本修复扩大品牌改名范围。
- [x] **P1-02 同步所有名称来源。** 更新审阅目录源数据及 `EffectHelper::GetDisplayName`，再运行目录生成器生成中英文 catalog；检查添加面板、已添加条目、组合名称、冲突提示、参数窗口及目录缺失时的回退。若某入口直接使用文件名，接入同一显示名映射。其余界面语言沿用英文目录回退。
- [x] **P1-03 保持配置兼容。** 不改 `FSR3FG\FSR3_FrameGeneration`、DLSS/XeSS 现有 ID、HLSL 路径、参数 key、倍率或默认值。保留 `FSR3FG`、`FSR 3 FG`、`FSR3_FrameGeneration` 等搜索词；旧配置/导入配置仍能解析，无重建效果组需求。
- [x] **P1-04 透传首个 Presenter 错误。** 保存首个失败阶段、HRESULT/AMD result 和必要资源描述，通过现有报告路径呈现；避免后续 teardown 覆盖首因。此例应能看到“外部运动纹理 SRV 创建失败 / 0x80070057”，而不是仅 `MP-028 / system=0`。
- [ ] **P1-05 名称与目录回归。** 更新现有 `EffectPickerTests.cpp` 的 XeSS 名称断言，增加 DLSS/FSR 稳定 ID 到显示名映射、旧搜索词和 catalog/helper 一致性检查；执行目录 `-Check`、目录校验和配置兼容测试。当前 Beta7 的目录审阅已不同于早期 Beta6，不照搬原交接文档中的历史失败结论。

### P2 — 运行验收与交接

- [ ] **P2-01 外部光流矩阵。** None / AMDOF / 支持硬件上的 NVOF；1:1、缩放、黑边、尺寸变化；首帧、静止、快速运动、光流暂时失效后恢复。记录 reset 原因，确认不会每帧 reset 或只显示复制帧。
- [ ] **P2-02 捕获和界面节奏。** WGC 与至少一个无原生时间戳的捕获方式，验证 capture-admission 备用时钟；仅移动光标/工具栏时不增加基础帧历史。保持当前不限制 WGC 的行为。
- [ ] **P2-03 生命周期和回归。** 连续启停、Alt-Tab、捕获恢复、Resize、遮挡/最小化；回归 XeSSFG 外部运动、DLSSFG、VFX VFG 和普通呈现。已有 DLSS `0xbad00004` 单独跟踪，不能假定本修复会解决它。
- [ ] **P2-04 分层记录验收。** 分开记录源码检查、编译、原生 D3D 资源测试、完整 SDK 运行、实机画质/显示节奏。需要新包时按后续部署授权执行，更新包清单/符号和回滚点；不因本 TODO 自动发布、上传或回复 GitHub。

## 6. 执行顺序与通过条件

顺序：P0 纹理契约 → 原生资源测试 → 同场景完整运行 → P1 名称/错误上下文 → P2 回归与交接。

修复完成的最低证据是：原本失败的 AMDOF 流程能连续通过有效运动帧，D3D 调试层没有对应资源契约错误；正确区分 reset 复制与持续插值；FSR 各 UI 入口名称一致且旧配置可用；XeSS 的共享/复制路径未回归。持续 GPU/显示质量测试仍必须提供独立证据。

前次诊断实际执行仅包括文件/日志/源码读取、4 个部署文件哈希核验、Git 祖先/diff/工作树检查、118 项只读源码审计、Beta7 目录生成 `-Check` / 165 项目录校验，以及本文写入。现有源码审计与目录校验全部通过，Beta7 工作树仍干净；功能工作树仅新增本文。**以上为前次诊断的检查记录，不代表本次修复运行验收。**

## 7. 本次修复执行记录

- 基线重新确认：最新 Beta7 部署清单和 `release/0.6.10-beta7` 均为 `744d78994023e99d82901ef1e3c9b85a3d3b5f02`。独立分支 `fix/beta7-fsr3fg-motion-and-names`；未修改 Beta7 工作树及已部署包。
- P0-02～04：提取生产稳定纹理创建函数，FSR 需要 SRV 绑定，XeSS 保持 copy-only；重建比较覆盖绑定、尺寸、格式、Mip/Array、采样和资源使用属性。资源复制仍处于原共享槽事务内。消费端校验含 COM 设备身份，SRV 缓存仅成功后更新。
- P0-05：**测试源码及 runner 已写入，未编译执行**。复用生产创建、SRV 校验及缓存、原始 HLSL 和 dispatch 代码；覆盖旧 bind=0 负例、缺失/有效/过期运动、重建/复用、错误布局和设备、重映射数值与边界、XeSS copy-only 及首错误保存。该 TODO 保留未勾选，等待实际测试证据。
- P1-01～04：名称和所有现有 helper/catalog 路由已同步；保留稳定 ID、旧搜索词、实验性/2×/SDR 说明。错误报告复制首个错误上下文和 HRESULT；AMD 返回值保留在文本中，系统字段使用 E_FAIL。清理错误仍记日志，但不覆盖首因。
- P1-05：双语目录检查、helper 对照、名称/旧搜索词/限制说明校验已通过，原生 `EffectPickerTests.cpp` 断言已更新。**原生测试与配置/UI 实机验证未运行**，因此整体项目保留未勾选。
- 已运行：130 项只读源码审计、165 项目录校验、3 个包校验夹具、PowerShell runner 语法解析、基线差异核对及 `git diff --check`。这些不是 C++/HLSL 编译或 GPU 运行证明。
- P0-06、P2 全部留待后续编译/运行会话。当前分支没有构建结果；不能宣称已连续成功插值或已解决全部 GPU/SDK 问题。
- 详细交接：[Beta7 FSR3FG 修复交接](../reviews/20261011-beta7-fsr3fg-runtime-fix.md)。
