# HDR 工具栏与浮窗亮度：源码交接

日期：2026-10-13。源码及隔离验证完成，未构建应用 EXE/DLL、新 release 或部署运行包。真实 HDR 显示验收交由后续构建/部署会话。

## 分支与基线

- 分支：`feature/hdr-overlay-brightness`。
- 工作树：`C:/Users/81443/Documents/Magpie enhance/worktrees/hdr-overlay-brightness`。
- 基线：Beta8 提交 `cbe2c2cbda103e6cb6f686a5521918ed2501c32e`。
- 计划与验收清单：[TODO](../todos/20261013-hdr-overlay-brightness-TODO.md)。
- 日常 `source` 与其他工作树已有改动均未混入；日常目录仅同步本任务 TODO 状态。

## 已实现的行为

最终输出 HDR 且 UI 目标为线性 FP16 时，工具栏、性能分析、实时参数面板、菜单、tooltip、图表与图标统一做 sRGB→线性 scRGB 转换。UI 参考白随最终输出参考白变化，限制在 **160～300 nits**；缺少可用的输出白点时使用输出窗口所在显示器的 SDR 白点，再回退到 **203 nits**。

HDR 稳态底板不透明，防止天空、白色网页等高亮透过底板压过文字；工具栏淡入淡出、文字覆盖率、禁用态和抗锯齿维持原有语义。普通 SDR 仍使用原着色路径及原透明度。

### 关键实现位置

- `HdrFrame.h` / `HdrComponentRuntime.h`：专用 `overlayReferenceWhiteNits` 随帧传递。原生 SDR→HDR 兼容旧 `whiteNits × exposure`；配对恢复按实际 `pairIndex` 取纸白；普通效果继承；末端 HDR→SDR 清除 UI HDR 白点。没有改写全局 `ColorDescription` 或用户配置参数。
- `OverlayColor.h`：集中处理目标格式、参考白来源、无效值回退和舒适限幅。`referenceWhiteNits=80` 是 scRGB 单位，效果器峰值不会直接成为 UI 亮度。
- `Renderer.cpp`：普通绘制使用对应当前/稳定场景快照；独立叠加层使用成功提交的场景快照。XeSS 场景虽然是 PQ，独立 UI 根据自己的 FP16 目标走 scRGB。参考白变化后，独立 UI 立即重绘；若 BeginOverlayFrame/提交暂时失败，待处理 revision 保留到成功提交。
- 输出显示器白点按显示器缓存，显示/设置消息使缓存失效；不逐帧查询 SDR 白点，不读取参数面板未生效草稿，不跨线程直接读后台参数。
- `ImGuiBackend.cpp` / `ImGuiImplPS.hlsl`：16-byte 像素常量缓冲，重置渲染状态时重新绑定；shader 输出直通 alpha，现有 blend state 完成一次预乘。HDR RGB 不裁剪至 `[0,1]`。
- `OverlayDrawer.cpp`：只提高背景样式不透明度，样式栈在 ImGui EndFrame 前恢复；文本 alpha 和工具栏动画未整体改写。

首版不添加设置项、自动曝光、直方图或物理显示器峰值查询。300-nit 上限是待用户实机校准的舒适上限；现有元数据里的 `displayPeakNits` 可能被效果器改写，因此不用于推断输出显示器能力。FSR3 FG 的 HDR 限制保持原状。

## 已执行验证

证据目录：`C:/Users/81443/Documents/Magpie enhance/reports/20261013-hdr-overlay/`。

| 验证 | 结果与边界 |
| --- | --- |
| HDR 组件及亮度策略 | 通过；覆盖新/旧原生映射、多配对后的最终白点、TrueHDR 约定白点、上下限、无效值、显示器回退、PQ 场景与 FP16 UI、最终 SDR。 |
| 前台提交代码摘取测试 | 通过；覆盖主场景提交失败、稳定帧重播、已生效调整与复位、同白点/限幅不重复刷新、独立 UI 目标暂不可用后的保留与重试。使用生产提交片段，其他时间统计/截图路径不在此夹具内。 |
| WARP 离屏 | **62 个用例通过**，调用生产 ImGuiBackend、生产 HLSL 和真实 Dear ImGui；检查白点数值、sRGB 灰阶/彩色、覆盖率、alpha、常量重绑、1000/4000-nit 背景以及底板样式恢复。WARP 不代表实体显示观感。 |
| HLSL | VS/PS 均通过 FXC `/WX`。 |
| 生产 C++ | `Renderer.cpp`、`OverlayDrawer.cpp`、`ImGuiImpl.cpp`、`ImGuiBackend.cpp` 四个翻译单元通过 MSVC `/Zs /W4 /WX`；未生成产品对象或链接产品。该语法检查不替代完整 SDK 开关组合构建。 |
| 工具栏/参数输入 | 现有回归通过：32 个模式/视口/DPI 组合，128 个 FPS 对齐组合，拖拽、菜单、取消、输入焦点与恢复；补充 SDR→HDR→SDR 状态背景 alpha 检查。布局测试另覆盖 16 个视口/DPI 组合。 |
| FG idle 源码检查 | `validate_fg_settings_idle.py` 的 77 项通过；属于静态检查。 |
| diff | `git diff --check` 通过。 |

日志：`isolated-check.log`、`toolbar-check.log`、`fg-idle-source.log`。已有测试夹具仍输出整数 map 值转 float 的 C4244、参数输入夹具 NOMINMAX 重定义的 C4005；生产变更翻译单元的 `/W4 /WX` 检查无警告。工具栏夹具补齐了 Beta8 已新增的统计接口替身，以便复用现有回归，未改动该统计功能的产品代码。

复跑入口：

```powershell
& tests/Run-HdrOverlayTests.ps1 -OutputDirectory '<isolated-output>'
& tests/Run-ToolbarDragTests.ps1 -OutputDirectory '<toolbar-output>'
python tests/validate_fg_settings_idle.py
```

这些命令只生成隔离测试程序及着色器头文件；不打包、启动或替换 Magpie。

## 后续构建/部署会话

1. 以本功能分支提交为变更来源，先检查目标分支与现有改动；不要从旧日常主线覆盖当前 Beta8 基线。
2. 使用既有正式构建流程生成新应用，确保 ImGuiImplPS 重新编译，并检查当前 SDK 开关配置。此步骤本会话未执行。
3. 获得部署范围后保留运行包与配置恢复点，再部署；本会话未修改已安装包、配置、release 或现有进程。
4. 实机验证原始 HDR 触发链：暗场与天空/雪地/白色网页亮场，工具栏、性能分析、参数面板及菜单/tooltip；动态调节、复位、静止源、窗口/全屏、多屏移动和 HDR/SDR 切换。
5. 验证普通/Composition 输出与 XeSS 独立 UI，按硬件覆盖 DLSS/VFX；FSR3 仅回归 SDR。记录真实观感与亮度适配，不用离屏数据替代显示验收。
6. 与用户校准 160～300 nits 和不透明底板的观感，再决定是否需要少量透明度或更细的输出能力约束。
