# FG 设置与空闲重绘：源码交接

日期：2026-10-12。**源码实现完成；未编译、未部署、未进行 SDK/GPU/显示验收。**

## 分支与范围

- 继续使用 `fix/beta7-fsr3fg-motion-and-names`，没有创建新分支。
- 开发前从 `fe31c082` 快进到 Beta7 `34c3d6dd13fe11cd0cae94a2ab23e936a866d95a`，保留之前的 FSR3 motion SRV 修复及最新 DLSS runtime discovery 修复。
- 工作树：`worktrees/beta7-fsr3fg-motion-and-names`；产品分支、用户配置、现有 release 和 SDK/DLL 未修改。
- 执行清单：[合并 TODO](../todos/20261012-fg-input-settings-and-idle-redraw-TODO.md)。先前关于问号字形、完整 FSR3 异步输出统计、Core 名称遗漏的独立任务没有混入本次修复。

## 最终源码行为

1. FSR3 移除 None，保持 `1=AMDOF`、`2=NVOF`；默认 AMDOF Quality。`FrameGenerationParameters.h` 同时处理 App 宽字符模型与 Core 参数：旧 None、缺省、非法方法转完整 `(1,1)` 组合，保留已有合法 AMDOF 性能/质量及 NVOF 选择。新建、复制、加载/导入、导出、运行参数会话及直接 Core 启动接入同一规则。已有迁移备份保存原配置，导入来源文件不被覆盖。
2. 四类 FG 删除 marker、参数结构、解析和资源中的 `duplicateFrameFiltering`；移除捕获层 override API。废弃字段只在迁移器中出现，不反向修改通用设置。简单/高级及 3D 模式与普通效果使用同一捕获策略。
3. FSR3 开启时 AMDOF 未指定则默认开启；显式禁用冲突在 MSBuild 和打包预检报错。预检在构建/暂存之前执行；`-SkipBuild` 检查新 marker 与实际二进制中的 AMDOF 能力标记。SDK 2.3.0 的 DLL hash 和 provider 3.1.6/3.1.7 固定不变。不支持的畸变链提示具体效果 ID，取消 None 绕过指导。
4. `IdlePresentationClock` 在呈现层接收有效空闲率；捕获层仍不伪造旧输入。约 500ms 没有新内容才进入缓存重绘，防止把 24/30 FPS 正常帧间隔当作空闲、持续重置 FG。进入后按有效率重绘，受现有内容上限及显示刷新率约束，不乘 FG 倍率。
5. 有效首帧、无待处理内容、无后端内容在途、捕获健康且未停止/Resize，才允许重绘；失败重试也限速。实际内容、软件 FG FIFO 优先。外层消息泵以空闲期限和既有 UI 期限中的较早者等待，新消息可打断等待。
6. DLSS/VFG 复用最后完成的前端底图，包含 VFG 后置处理结果。FSR3 在 Configure 中关闭生成，只复制/Present，不 Prepare、不运行插帧、不提交新运动；SDK ID 递增而捕获身份不变，恢复时 reset。XeSS 在状态转换时关闭 FG，然后关闭 XeLL 低延迟/限速；恢复先启用 XeLL，再启用 FG。XeLL Sleep/marker 生命周期和独立 SDK present ID 保留，空闲不更新输入时间估计或 motion history。
7. 内容输出、真实输入、已报告生成帧、空闲提交分别统计。工具栏 `R/s` 为成功提交的缓存重绘/秒，不是显示扫描率，也不计入生成 FPS；性能面板提供等待新画面和独立计数。FSR3 内容总数未知时继续保留未知，不以倍率估算。
8. 缓存重绘不会重跑任何前置/后置效果；FG 下依赖 TIME 的效果仍随新内容推进。普通非 FG 的空闲效果处理语义保持原样。

## 本轮验证

以下均不调用 C++/HLSL 编译器，不启动 Magpie 或 SDK：

- `tests/validate_fg_settings_idle.py`：77 项通过，跨文件设置、迁移入口、缓存路径、SDK 状态次序、计数和打包接线检查。
- `tests/validate_fsr3fg_source.py --sdk-directory <FSR-SDK-v2.3.0>`：129 项通过，固定 SDK 公共描述符、符号、资源与包装契约检查。
- `tests/check_frame_refresh_bindings.py`：既有设置布局、简单/高级模式、运行参数重启、前端调度规则检查。
- `tests/Validate-EffectCatalog.ps1`：165 个效果的源 hash、双语目录、显示名称及搜索别名检查。
- `tests/Test-FGFeaturePreflight.ps1 -SdkDirectory <FSR-SDK-v2.3.0>`：10 个实际 PowerShell 预检用例；覆盖 FG/SR/AMDOF 默认与显式覆盖、冲突拒绝，以及新旧包策略。使用临时夹具和仅作数据的能力标记文件，不执行二进制、不创建 release。
- PowerShell 语法解析、`git diff --check`；生产捕获策略 C++ harness 提取成功，**未编译执行 harness**。

检查记录保存在工作区 `reports/fg-settings-idle-source-20261012/validation.json`，提取的 C++ harness 位于同目录 `native-harness/`。

原生回归源码已提供：`FGSettingsIdleTests.cpp` 直接调用生产规范化函数、空闲时钟、FSR history 与统计类，覆盖非法/缺省值、幂等、旧选择、跨宽窄字符模型、24/30/60 FPS、首帧、失败重试、恢复和独立计数；队列优先、中断、Resize/Stop 的 Renderer 接线由源码审计检查，真实生命周期回归仍待执行。`test_fg_duplicate_filter.py` 提取实际捕获函数，验证通用 Always/Dynamic/Never、3D、恢复与读回失败。**这些原生测试本轮没有运行，不能算通过。**

## 后续编译与验收入口

在本分支的 x64 VS 开发者终端执行（本轮未执行）：

```powershell
./tests/Run-FGSettingsIdleTests.ps1
./tests/Run-FSR3FGPolicyTests.ps1
./tests/Run-FSR3FGMotionResourceTests.ps1
./tests/Run-FrameRefreshTests.ps1
./tests/Run-DuplicateFrameTests.ps1
```

随后编译 FG 开启/关闭两种配置；验证 `EnableFSR3FrameGeneration=true`、`EnableFSR3ZeroMV=false`、未指定 AMDOF 时实际包含 AMDOF，显式 AMDOF=false 能清楚拒绝；正常 NVOF 构建能力保持可选。确认编译与原生回归后，再由构建会话按项目既有流程准备包、部署及回滚。

必须在真实 SDK/GPU/显示环境完成：

- 用户原素材及已知 0.5/1/4 像素平移源的 AMDOF Quality/NVOF 图像验收，确认中间位置；不能用 dispatch 次数替代。
- 四类 FG × 通用 Always/Dynamic/Never × 空闲开/关；24/30/60 FPS、AABB/AAABB、静止 10/60 秒再恢复。确认 500ms 之前不会因低帧率源反复切换 FG；静止后按有效率提交缓存；恢复重新预热后持续插帧。
- 第一帧、尺寸变化、HDR 适用路径、窗口切换、最小化、工具栏/比较/鼠标、停止与故障恢复；确认新内容优先、无旧图插队或无效缓存读取。
- XeSS 2×/3×/4×（尤其非 Intel 兼容路径）和 XeLL 切换，FSR disabled-present 的 fence/allocator 使用，DLSS Reflex 与 VFG 后置链各自验收。
- 记录 PresentMon/SDK 可用统计、实际显示间隔、CPU/GPU 空闲成本；`R/s` 是提交统计。AMD SDK 状态切换内部包含其自身等待机制，宿主的容量/fence 超时不等于能打断 SDK 内部所有等待。

上述实机项仍为 TODO G15/G16；G13/G14 的原生执行也留待该会话。回滚起点为干净 Beta7 `34c3d6dd` 及迁移前原配置备份。
