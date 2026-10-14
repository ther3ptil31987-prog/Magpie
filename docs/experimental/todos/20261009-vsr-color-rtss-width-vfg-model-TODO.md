# VSR 通道、RTSS 按钮宽度与 VFG 两档模型 TODO

日期：2026-10-09（Asia/Shanghai）。初稿只调查和整理方案；用户随后授权从原功能分支实施，现已完成下述源码候选和独立 SDK 合成验证，未编译、打包或部署产品。用户补充 VSR 的异常“似乎是通道互换”；尚未获得具体分类、档位、捕获方式和完整效果链，不将疑似原因记为已复现的根因。

## 调查阶段基线和结论（实施进度见下方）

- 开发分支：`feature/nvidia-vfx-optimization`，源码提交 `970dd5f9d9f0e43645a6175935d86e9ebaeee100`。调查开始时工作树干净。
- 当前 Beta5 的 `build-manifest.json`：`c640f9c2ce8ccaa19befab018980d80fbb9967f1`，声明 `EnableNvidiaVfx=true`、`Minimal`，Async/CudaHandoff 均为 false。与开发提交比较，只新增运行库验证脚本的 PowerShell 模块导入和发布说明；本轮涉及的 VSR、RTSS、VFG 源码一致。此项是包清单与源码核对，未核实当前正在运行的进程、未重新验收产品。
- VSR：优先排查 RGBA/BGRA 通道解释及导入／模型／导出边界。当前 8-bit 路径没有发现显式取负或 signed 8-bit 缓冲；不能据此排除所有运行时错误。
- RTSS：初始化成功后的隐藏行为已有；按钮宽度恢复没有明确的状态布局合同。WrapPanel 已跳过 Collapsed 子项，不将“隐藏项继续占位”作为已确认原因。
- VFG：产品只保留 **性能（Low，值 0）／均衡（Medium，值 1）**，默认仍为均衡；移除用户可选 High，并迁移旧 `model=2` 为 1。用户认为 High 的性能开销不可接受是本次产品决策，本轮没有测量三个模型的性能差值。

## C：VSR 通道问题，P0

### 已确认的源码与 SDK 证据

1. [图像后端](../../../src/Magpie.Core/NvidiaVfxImageBackend.cpp)根据实际 D3D 输入、输出格式分别分配 `NVCV_RGBA` 或 `NVCV_BGRA`，均为 `NVCV_U8`／interleaved。它允许 SDK 输入与输出采用不同通道顺序。例如 SDR 捕获为 BGRA 而普通效果输出为 RGBA 时，两者编码不同；HDR 兼容路由又可能采用 BGRA→BGRA。这是可达路线，未证明用户这次使用的正是该路线。
2. [公共传输层](../../../src/Magpie.Core/NvidiaVfxRuntime.cpp)通过 `NvCVImage_InitFromD3D11Texture`、Map、`NvCVImage_Transfer(..., 1.f)`、Unmap 导入导出，没有对运行时 wrapper 的格式、类型、layout、pitch 做明确合同校验或颜色读回。函数成功不能证明实际红蓝顺序正确。
3. 实际调用本地匹配的 `NVCVImage.dll` 的 **无状态格式翻译与分量偏移查询**，得到下表。没有创建 D3D 纹理、分配 CUDA 缓冲、执行 VSR 或启动 Magpie。DLL SHA-256 为 `AFD9B7AD335D78663126B315C50C2A8820C474CE6DEBEDA414DD633ACEAD949D`。

   | DXGI 格式 | 返回值 | NvCV pixelFormat | componentType | layout | R/G/B/A 分量偏移 |
   | --- | --- | --- | --- | --- | --- |
   | R8G8B8A8_UNORM (28) | 0 | RGBA (6) | U8 (1) | interleaved (0) | 0/1/2/3 |
   | B8G8R8A8_UNORM (87) | 0 | BGRA (7) | U8 (1) | interleaved (0) | 2/1/0/3 |

   普通格式翻译正常；这不证明 Map 后的 CUDA array、实际像素传输或模型执行正常。
4. [官方 VSR 文档](https://docs.nvidia.com/maxine/vfx/1.3.0/Filters/VideoSuperResolution.html)说明 8-bit RGBA/BGRA 的分量是无符号整数。[官方示例](https://github.com/NVIDIA-Maxine/VFX-SDK-Samples/blob/52011f89c1741d06b40ea312af1f20be8be9ec62/apps/VideoSuperResEffectApp/VideoSuperResEffectApp.cpp#L630)的 8-bit GPU 输入、输出均用 RGBA；它没有验证任意 RGBA/BGRA 混合 I/O。
5. [NvCVImage 传输说明](https://docs.nvidia.com/maxine/nvcvimage/latest/functions.html#nvcvimage-transfer)区分线性图像转换与映射 CUDA array 的有限转换能力。本地 `nvCVImage.h` 也列出 BGRA 纹理与 BGRA chunky 同编码复制，不能把线性缓冲可转换推导为任意纹理方向都同样受支持。
6. [HDR 兼容路由](../../../src/Magpie.Core/EffectProtocolCatalogC.h)为 VSR 声明 BGRA8／sRGB；[HDR 转换](../../../src/Magpie.Core/HdrSurfaceAdapter.cpp)以逻辑 RGB 做采样和颜色运算。色域矩阵中的负系数是正常数学项，不因本次通道现象统一取绝对值、反号或替换颜色矩阵。

### 处理方案与待办

- [ ] **C01／P0** 记录复现配方：运行包／组件加载路径与版本、实际渲染 GPU、捕获方式及 D3D 格式、VSR 分类／档位、缩放尺寸、HDR 输入输出状态和完整效果链。用大面积红／绿／蓝／黑／白块、非对称 RGB 值和灰阶区分 R/B 互换、负片、范围错误与空间错位；保存原始输入和效果输出配对。
- [ ] **C02／P0** 建立逐边界颜色诊断：D3D 输入 → CUDA 导入结果 → SDK 输出 → D3D 导出 → HDR 适配／最终输出。分别执行无模型的导入导出往返，以及有模型的 VSR，用第一次出现通道互换的位置选择修复点。核对 Map 前后 NvCV 格式、类型、layout、pitch、尺寸及 wrapper 缓存随纹理更换的更新。颜色断言检查色块内部，模型输出允许数值变化，但主通道身份不能互换；灰阶不能单独证明颜色正确。
- [x] **C03／P0** VSR 候选先统一 SDK 内部输入／输出为同一 **RGBA/U8** 编码，参照官方示例；D3D 边界按真实纹理编码转换。若映射 array 的跨编码传输未获验证，采用“BGRA 纹理↔BGRA 线性缓冲同编码复制，BGRA↔RGBA 在线性 GPU 缓冲转换”，避免让 SDK 模型承担混合通道顺序或假设任意 array 转换可用。本次已由合成对照选定 RGBA 内部编码与线性转换路线；必要缓冲按尺寸／编码缓存，同编码不额外分配。额外转换的实际 GPU 成本留在后续产品预算验收。
- [x] **C04／P0** 增加格式合同校验和清晰错误；不在最终画面全局加 `.bgra` 或反号兜底，否则可能对原本正确的路线再次交换。诊断开关允许暂时分段读回，正常执行不每帧读回、额外同步或记录像素内容。
- [ ] **C05／P0** 验收 RGBA/BGRA 来源与输出、五类 VSR 的合法档位（降噪／去模糊同尺寸）、前置 HLSL、单独 VSR、VSR→TrueHDR、VSR→VFG→TrueHDR、合法 HDR→SDR 输入路线。覆盖参数切换、尺寸／纹理重建和停止重启；VFG 真实帧与生成帧分别检查。先修颜色，再接受 F14 的编码优化；扩展到 10-bit/PQ 仍保持原独立验收要求。

初稿调查时最强的候选原因是 **混合 I/O 编码或 D3D/CUDA 传输边界的通道解释不一致**。当时尚未执行 C01/C02，未确认交换边界；本次实施后的合成对照已定位到混合 I/O 的模型输出，见下方新增证据。此前直接 SDK 的非零读回冒烟测试不包含本次 D3D 往返和通道身份断言。

## R：RTSS 工具按钮宽度恢复，P1

[ProfilePage.xaml](../../../src/Magpie/ProfilePage.xaml)使用 WrapPanel 放置初始化、启动工具、重新检测三个按钮；初始化只绑定可见性。启动工具没有宽度状态绑定，其内容模板使用 `* + Auto` 列和可换行文本。[WrapPanel](../../../src/Magpie/WrapPanel.cpp)把全部可用尺寸用于测量每个子项，再按 DesiredSize 排行，已跳过 Collapsed 项。

现有布局测试只使用给定 DesiredSize 的模拟子项，包含隐藏项不占间距的检查，没有执行真实 Button/DataTemplate 的 WinUI 测量或初始化完成后的宽度恢复。因此可确认缺少该状态合同和验收，实际压缩宽度为何保留仍需真实 UI 测量。

- [ ] **R01／P1** 在真实页面记录初始化显示、处理中、成功隐藏，以及重新进入页面时按钮的 DesiredSize／ActualWidth、父区域可用宽度和换行行数；检查可见性变化是否引发重新测量，区分内容模板星号列的约束与缓存排列问题。
- [x] **R02／P1** 在 RTSS 操作区域落实局部响应布局：初始化隐藏后，启动工具重新测量并恢复“文字＋下拉箭头”的常规内容宽度；不沿用三按钮状态下的压缩 Width/MaxWidth。首选内容模板按 Auto 宽度测量，并在状态切换清除临时约束；初始化的间距与其可见性一起消失。窄窗口按现有内容适配规则换行，右侧重新检测保持 32 DIP 最小宽度和 8 DIP 间距。其他输入行原有 225 DIP 列宽保留，不把整个操作区域重新强制限制到该列。
- [ ] **R03／P1** 验收无需重新打开页面即可恢复宽度，隐藏→显示也正确；覆盖浅／深主题、100%／150%／200% DPI、窄窗、中英文及长翻译、菜单、禁用／处理中状态与键盘焦点。只改变布局，不自动初始化、重启或修改 RTSS 配置；如最终需要修改通用 WrapPanel，另行覆盖其全部调用者。

此处“恢复”按现有 RTSS 操作区域的内容适配设计解释为恢复正常文字宽度，不新增固定全列拉伸要求。

## M：VFG 仅性能／均衡，P1

[VFG 参数声明](../../../src/Effects/NvidiaVFX/VFG.hlsl)目前暴露 0/1/2；[执行计划](../../../src/Magpie.Core/include/NvidiaVfxExecutionPlan.h)及[设置校验](../../../src/Magpie.Core/include/NvidiaVfxParameters.h)也接受 2。[运行中模型更新](../../../src/Magpie.Core/Renderer.cpp)通过 `ApplyModel` 创建候选实例。因此只删下拉选项不能阻止旧配置继续执行 High。

| 用户可见项 | SDK 模型 | 保存值 | 默认 |
| --- | --- | --- | --- |
| 性能 / Performance | Low | 0 | 否 |
| 均衡 / Balanced | Medium | 1 | 是 |

- [x] **M01／P1** 删除用户可选 High，Low/Medium 显示为性能／均衡，保留 `model` 参数键和 0/1 编号、默认 1。更新 HLSL 注解、中英文及既有语言回退、编辑面板、运行中面板、预览和说明；明确两档映射，不影响 VSR 的 High/Ultra 档位。
- [x] **M02／P1** 增加统一兼容迁移：旧 VFG `model=2` 在配置加载／导入时转为 1，保存／导出反映实际均衡值；0/1 不变、缺省仍 1，迁移幂等且仅匹配 VFG 效果 ID。不依赖 UI 显示默认值代替真实参数迁移；保留配置恢复原文和其他参数／链顺序。
- [x] **M03／P1** 执行计划、初始化和运行中 ApplyModel 均限制为 0/1；合法旧值 2 经兼容入口迁移，未知值、非整数、非有限值按现有非法参数策略处理。迁移和白名单使用共同规则，候选加载失败保留旧实例／参数，成功切换按现有合同清历史。
- [ ] **M04／P1** 验证旧 High 加载／导入／导出往返、缺省与 0/1、两面板显示一致、运行中候选切换和失败回滚；确认没有 UI 或正常配置路径运行 SDK 模型 2。固定 2–8×、SDR 补帧和逐帧 TrueHDR 保持既定要求，性能比较只对产品两档进行。官方组件 DLL 保持原样，隐藏档位不会单独缩小运行库文件。

官方映射见 [VFG Model Modes](https://docs.nvidia.com/maxine/vfx/1.3.0/Filters/VideoFrameGeneration.html#model-modes)。本轮采用用户要求的两档产品策略，不把 SDK 的三档支持删改为两档能力。

## 实施顺序与交付边界

本次已完成 C03/C04、R02、M01–M03 的源码实现，并纳入 Beta5 的 PowerShell 构建验证模块修复。已勾选项表示源码候选完成；未编译的 C++／XAML／HLSL 与完整产品运行不能据此记为验收通过。C01/C02/C05、R01/R03、M04 的产品配方与验收继续留给编译会话。

初稿调查没有执行 VSR／D3D 像素往返。随后实施阶段用独立探针复现了混合 I/O 在 CUDA 模型输出的红蓝互换，显式 RGB8 的旧路线仍复现。RGBA 候选覆盖四格式组合、18 个合法档位和往返，共 76/76 通过；另有非对齐行距、wrapper 合同与 packed 字节导出验证。探针只使用合成色块，没有运行新编译的 Magpie、捕获窗口内容、WinUI 或测量产品性能。

## 实施阶段补充：TrueHDR 导出（同一颜色链）

- [x] **H01／P0** 独立验证旧 R10G10B10A2 纹理包装返回 -9；确认 CUDA D3D11 支持格式表未列出该格式，保存原失败报告。
- [x] **H02／P0** SDK 保持 RGB10A2/P32，新增非拥有 RGBA/U8 字节视图，使用 RGBA8 纹理承载原始四字节，并在 HDR 适配模式 9 重建原始 10/10/10/2 位、继续 PQ／Rec.2020→scRGB 解码。4096 像素往返逐位一致，CPU float32 字节重建一致；不改变 TrueHDR 参数范围、默认值、步进或原 SDK 图像描述符。
- [ ] **H03／P0** 编译并运行新增 WARP 模式 9 与原生 R10 模式 6 的 FP32／FP16 对照；实际 GPU 运行 TrueHDR 以及 VSR→VFG→TrueHDR，分别验证真实／生成帧颜色、alpha、峰值／中灰／对比度／饱和度／去色带和最终呈现。本次未执行新 HLSL 或完整产品 HDR 链。

完整实现、证据报告、已执行检查与独立编译会话的继续事项见[源码交接](../reviews/20261009-vsr-color-rtss-vfg-source-handoff.md)。后续编译、打包和部署继续交给独立会话。

关联：[VFX 总 TODO](20261009-nvidia-vfx-suite-upgrade-TODO.md)、[优化源码交接](../reviews/20261009-nvidia-vfx-optimization-source-handoff.md)、[RTSS 初始化与按钮设计](20261009-rtss-status-magpie-init-and-admin-TODO.md)。
