# 截图内容选择：源码实现与编译交接

日期：2026-10-09。范围：功能源码、本地开发分支和独立回归；未构建 Magpie 产品，未部署、推送、合并主线或创建新 release。

## 分支与基线

- 分支：`feature/screenshot-content`。
- 开发目录：`C:/Users/81443/Documents/Magpie enhance/worktrees/screenshot-content`。
- 基线：Beta4 `49de3f25478008c962e24015cf81d7e9772b9a2b`，含实际纹理格式和 HDR PNG 导出修复。
- 源码提交：`b442fd59`（`feat(screenshot): add input and output capture modes`）。
- 日常 `source/experimental` 保持 `13adddb6` 和已有未提交工作的状态，仅本任务的 TODO／交接文档更新。
- [需求与实施 TODO](../todos/20261009-screenshot-content-TODO.md)。

## 最终行为

主页 → 工具栏 → 工具栏初始状态与截图保存目录之间加入“截图内容”，默认只截图最终效果。

| 选择 | 保存结果 |
| --- | --- |
| 同时截图原始输入和最终效果 | 同帧 `Magpie_001_input.png`、`Magpie_001_output.png` |
| 只截图最终效果 | `Magpie_002.png` |
| 只截图原始输入 | `Magpie_003.png` |

按钮与快捷键进入相同请求入口。设置持久化为 `overlay.screenshotContent`：0=双图、1=输出、2=输入；旧配置、非法值默认 1。运行中通过 scaling dispatcher 更新下一次请求，已有请求不改档。右键指定效果／通道导出保留独立的 PNG／DDS 行为。

input 是现有采集裁切和捕获解码之后、首个效果之前的原尺寸 `GetPipelineTexture()`；HDR 采用捕获模块的 canonical input 和其颜色元数据。output 是现有成功提交的 processed base 场景，保留最终尺寸适配，不混入工具栏／光标，不受对比开关影响。输入 PNG 也遵守现有 HDR→PNG 颜色编码合同，不等同于 GPU 原始字节／HDR 原始动态范围导出。

含 input 的两档统一经过真实输出成功提交边界，InputOnly 只读回 input，不保存 output。静止内容请求会驱动一次有效真实处理／发布；双图不搭配生成帧。DLSSFG 双图等待下一张可配对真实输出，OutputOnly 延续已有最近场景截图的生成帧支持；XeSSFG／FSRFG 的截图范围沿用现有 base 场景合同，内部生成帧不另行导出。

## 线程、帧标识及保存

- `SceneScreenshotRequest` 固定模式、目录、request ID、run ID、会话、颜色和通知快照；每个 Renderer 同时只接纳一个普通请求，忙时提示重试。
- backend 按待处理请求复制 native input；候选随共享槽和场景元数据在同一 publication mutex 下传递。frontend 成功提交后核对 `frameId`、`captureSequence`、`resourceGeneration` 和非生成标志。序列 0 是合法值。
- 首次异步挂起前复制 input／output 到稳定 staging。GPU 等待在后台，Map／CPU 拷贝回到 frontend dispatcher，PNG 编码及磁盘写入在后台。协程保留设备、上下文和会话，不再读取 Renderer 成员。
- 等待真实配对及 GPU fence 各有 10 秒上限。会话停止、资源重建或采集 epoch 改变不会从新会话补齐旧双图；失败／超时清理候选引用。
- 共用进程内保存锁，双图一次编号并分别提交。目录枚举识别旧名称、双图、PNG／DDS、大小写和目录路径占用；重复编号不会破坏 UINT_MAX 回退。
- WIC／DDS 编码写入私有临时文件，再使用不替换目标的移动提交；外部路径竞争也不会覆盖旧截图。双图部分失败保留成功图片，提示文件名及失败侧，错误详情含路径和阶段。
- 简中、繁中、英文准确新增文案；其他 17 种语言新增键暂使用英文。原有翻译未改动。

## 已通过验证

在上述开发目录运行：

```powershell
pwsh -NoProfile -File tests/Run-ScreenshotContentTests.ps1
```

该脚本只构建独立测试程序，并使用 `/Zs` 检查 Core 源码；不会创建 Magpie 可执行文件或 release 包。依赖现有 VS C++ 工具链、SDK、Conan 包和 WIL 包缓存；当前本机 WIL 缓存可从日常 `source/packages` 读取。

- 模式固定值、缺省／无效值、真实帧标识、生成帧拒绝、序列 0、旧／双图命名及编号溢出。
- 配置恢复局部修复：三档保留、错误类型／负数／越界删除该字段，其他 overlay 项保持；往返序列化。
- 提取实际生产函数的磁盘／WIC fixture：空目录、半对、PNG／DDS、UINT_MAX、八组并发双图、input／output PNG 尺寸、无覆盖提交、路径冲突、部分写入失败、目录错误和临时文件清理。
- 既有 HDR PNG 编码／解码与 DDS 实际 FP16 格式路由回归。
- 既有完整配置恢复回归，包含备份、部分恢复、用户原文件、重试和迁移。
- XAML XML／IDL 声明、卡片顺序、绑定、按钮与快捷键入口、异步保存结构和 20 份语言资源键完整性。
- `RendererScreenshot.cpp`、`Renderer.cpp`、`ScalingRuntime.cpp`、`ScreenshotHelper.cpp`、`TextureHelper.cpp` 五个生产 Core 文件语法检查。该独立检查使用默认编译宏，未覆盖各外部帧生成 SDK 的产品构建组合。
- `git diff --check`。

这些证据属于独立策略／编码／文件系统测试和语法检查；没有执行真实 D3D 截图链路，也没有完成整个 Magpie 的链接、XAML／IDL 生成或原生 UI 验收。

## 后续编译部署会话

1. 从本分支／开发目录或其后续集成结果接手，核验 `git status`、提交与基线。目标分支若不同于 Beta4，须保留已有 HDR 导出和 capture/presentation 修复，不只复制新截图文件。
2. 按项目现有流程执行完整产品构建，检查新增 IDL 绑定、资源生成、`RendererScreenshot.cpp` 项目登记和启用的 SDK 编译组合。由后续会话决定 release 版本与目标目录。
3. 原生 UI 验证三档、默认、重启恢复、缩放中改档、请求中改档、按钮／快捷键、深浅主题、窄宽窗口、DPI 和长文案。
4. 使用带帧编号／颜色标记的动态源，验证双图同帧、原尺寸 input、不同输出尺寸、裁切、对比开关、静止／去重源；核对日志中的 screenshot request/run/frame/sequence/generation。
5. 真实 GPU 验证 SDR／HDR 输入与输出方向、FP16/scRGB/PQ PNG、DLSSFG／XeSSFG／FSRFG、源窗口关闭／尺寸变化、缩放停止、会话重启、GPU 错误、连续点击、显存和截图停顿。
6. 验证成功双图、磁盘空间／权限失败、单侧冲突及部分失败提示；确认旧文件不被覆盖。完成后记录实际构建、部署、运行和验收状态。

本会话没有对任何现有 release 目录、运行进程或用户配置作修改。
