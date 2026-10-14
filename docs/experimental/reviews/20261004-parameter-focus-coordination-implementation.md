# 参数焦点交接与窗口协作实施记录

日期：2026-10-04。关联 [TODO](../todos/20261004-parameter-focus-coordination-TODO.md)。

P0、P1 源码实施及自动回归完成，继续在用户指定的 `fix/069-uwp-window-owner` 分支执行。基线 `b39fdc45`；P0 提交 `a1a5d459`，P1 提交 `365f8b67`。完整产品构建、打包、部署和游戏／GPU／VRR 验收交给后续独立会话；本轮没有合并、推送、修改版本或改动 release 目录。

## 最终行为

共享 [窗口关系与前台分类](../../../src/Magpie.Core/include/SourceWindowRelations.h) 接受经过 SourceWindowCoordinator 校验的捕获 HWND、GA_ROOT 和本会话窗口。捕获对象与裁剪保持原值。参数入口、预览／Esc、稳定期、源跟踪初始化／更新及 3D 判定使用相同身份规则，独立根窗口不再被参数层当作外部窗口。

源弹窗单独分类，排除输出和其辅助窗口分支。返回目标采用有效、可见且未最小化的根窗口或真实启用弹窗；源被禁用且没有合法弹窗时返回失败。焦点 API 返回后重新查询实际前台和目标资格，不再把“源被禁用”当成恢复成功。参数返回及自动纠正要求前台仍为预期宿主／输出；异步层级回调恢复到前端后重新读取真实前台。

| 消费者 | 使用的语义 |
| --- | --- |
| 窗口层级、非客户区激活绘制、会话广播、窗口模式切换 | `SrcTracker::IsSessionForeground()`，表示当前逻辑会话前台；输出只在已开始的有界交接期间放行 |
| 光标映射、黑边限制及退出捕获的坐标转换 | `ScalingWindow::IsSourceForeground()`，读取真实前台，匹配源捕获／根窗口 |
| 参数消息、输入收尾与停止等待 | 独立输入所有权标志＋当前 RunId＋有效源身份＋真实前台宿主＋本线程 GetFocus；不能凭 Edit 意图认定已接管输入 |

开启时保留关闭／编辑／预览三态、打开即请求编辑、预览首击直接操作控件、编辑区外首击只返回游戏、Esc 逐级退出及继承修饰键收尾。关闭路径、按应用保存、新建／复制默认关闭和下次会话生效保持不变。旧输出窗口的延迟键盘消息不会成为新宿主的按键所有权。

正常持有按键或鼠标仍等待完整输入，没有超时强制释放。外部失焦、源模态状态、捕获被其他目标取走、明确取消、旧 RunId 和源死亡取消本地参数输入；不会注入游戏按键或自动抢回外部前台。待停止不再等待已经失效的宿主。

宿主 `WM_NCDESTROY` 仅记录本地取消、清除所有权和输入等待状态，不执行光标／ImGui 清理、归还焦点或后端关闭。独立宿主销毁后的清理在消息回调返回后执行；退役期间仍由既有 owner 解除／输出销毁完成边界执行 ReleaseParameterInput。立即重新打开也先清理取消状态，旧标志不能关闭新宿主。

透明输入宿主遵循输出实际可见性和 WS_EX_TOPMOST，定位在已排序的输出之前。移动使用 SWP_NOACTIVATE／SWP_NOOWNERZORDER，并复查实际样式及相对位置。激活本身可能改变 Z 序，获得前台／线程焦点后再次校正相对层级。源／外部窗口和输出 owner、样式不由宿主改变。输出隐藏或层级更新失败时隐藏点击层；不可用的宿主拒绝命中／激活。预览区域仍来自最后成功呈现的面板。

重建只保存面板意图和布局，不保存输入所有权。首帧准入前输出尚未显示时，暂存编辑意图；显示后重新验证身份、RunId 和前台，再尝试一次激活。期间切出会取消恢复，没有计时器循环抢焦点。

100 ms 稳定期只随实际交接开始／正常返回设置，外部取消清除稳定期。源身份失效和模态状态不被稳定期掩盖。稳定后的持续几何变化、隐藏或最小化继续使用原来的重建规则。

源真实前台变化和逻辑会话变化分别记录。内部源／根／宿主交接不触发逻辑失焦历史重置；实际捕获中断、源序列变化、FG 作业回收与背压沿用原路径。未修改 Renderer 的内容时钟或 NR／FG 处理算法。日志记录请求来源、RunId、前台分类、捕获／根／宿主、线程焦点、返回目标及失败／取消；只在状态变化／失败时记录。

## 验证及证据边界

结果保存于工作区 [reports/focus-implementation-20261004](../../../../../reports/focus-implementation-20261004)。

| 检查 | 结果与范围 |
| --- | --- |
| [参数输入](../../../../../reports/focus-implementation-20261004/final-parameter.log) | 开／关双路径、真实 ImGui 控件、首击、Esc／下拉／数值、修饰键、队列、按住等待、延迟停止与恢复通过；新增独立根、3D 重叠、模态／禁用源、返回失败、线程焦点拒绝、激活中前台变化、旧会话、源死亡、捕获／取消、宿主销毁／重开及首帧前恢复用例通过。焦点、窗口、捕获 API 使用确定性替身 |
| [工具栏](../../../../../reports/focus-implementation-20261004/final-toolbar.log) | 真实 ImGui 拖动、按钮／取消、32 个模式／视口／DPI 场景、128 个 FPS 排列、配置与会话恢复通过；包含参数／布局回归，原生 UI／GPU 使用替身 |
| [原生窗口及完整停止](../../../../../reports/focus-implementation-20261004/final-owner.log) | 实际 USER32 根／子／宿主／外部窗口；原生 owner 和显式协作、置顶关→开→关、外部窗口遮挡、保持根样式与输出 owner、隐藏输出、真实源弹窗及辅助分支排除通过。26 个双进程退役场景、捕获关闭 12 场景＋1000 重建、NGX 故障锁存及准入回归通过 |
| [源状态／呈现队列](../../../../../reports/focus-implementation-20261004/final-transitions.log) | 提取的生产路径；有界输出前台、根／宿主切换不重置逻辑历史、临时几何变化不改基线、持续变化重建、1000 次持有输入重试、停止优先、FG 队列／slot 回收、x2／x3／x4 背压通过。GPU／SDK 为替身 |
| [光标策略／配置](../../../../../reports/focus-implementation-20261004/final-cursor.log) | 策略 181 项、生产配置 195 项、输入／活动采样 15 项通过；没有实际 FG／GPU 证明 |
| [生产语法](../../../../../reports/focus-implementation-20261004/final-syntax.log) | ScalingWindow、ScalingRuntime、Renderer、GraphicsCaptureFrameSource、SrcTracker、ParameterInputHost、CursorManager，默认功能开关 MSVC /Zs /W4 /WX；没有生成产品对象／库／EXE |

原生层级测试创建测试自有窗口，不操作现有游戏窗口。测试发现桌面其他窗口可能位于测试外部窗口与宿主之间，因此验收的是外部窗口仍位于宿主之前，以及宿主紧邻输出；不要求外部窗口与宿主相邻。

生产初始化／前台策略修复了旧回归 fixture 的接口依赖；源状态 fixture 接入新的身份分类和逻辑焦点方法。工具栏 fixture 同步面板恢复字段。Run-Beta6RegressionTests 增加输出目录参数与已有 include 路径，便于在报告目录运行。

没有强制 Windows 真实句柄数值复用，旧身份／代次与销毁锁存由受控场景验证。原生层级测试没有运行生产参数宿主的真实键盘输入、前台授权拒绝、跨屏 DPI 或真实游戏。驱动／SDK、部分捕获／状态查询／重启适配器使用替身；不会把这些结果写成 GPU、照片／Forza、后台原始输入、IME 或 VRR 验收通过。

## 独立会话交接

源码位于 `C:/Users/81443/Documents/Magpie enhance/worktrees/069-uwp-window-owner`，继续该分支或将 P0／P1 提交整合到实际产品基线。当前功能分支未合入 0.6.10 整合分支的 VRR 配置改动；后续整合应保留目标分支的 UWP／VRR 已有逻辑。不能直接用旧主工作区覆盖整合版本。

后续执行 TODO V04–V08：真实前台／线程焦点／捕获及键盘鼠标、两种 owner 模式、实际置顶开关、源弹窗、跨屏 DPI、普通／3D／UWP 游戏、后台原始输入、四种捕获、NR／各 FG／HDR、至少 10 次启停和 VRR 单独对照，再做完整功能开关下的 Release 构建与按实际授权部署。游戏真实失焦导致的暂停／降帧／最小化仍是兼容边界。

复查命令在上述功能工作树运行，输出目录选报告目录；均复用已有依赖：

```powershell
pwsh -NoProfile -File tests/Run-ParameterInputTests.ps1 -OutputDirectory <目录>
pwsh -NoProfile -File tests/Run-ToolbarDragTests.ps1 -OutputDirectory <目录>
pwsh -NoProfile -File tests/Run-ScalingWindowOwnerTests.ps1 -OutputDirectory <目录>
pwsh -NoProfile -File tests/Run-Beta6RegressionTests.ps1 -OutputDirectory <目录>
pwsh -NoProfile -File tests/Run-CursorRefreshTests.ps1 -OutputDirectory <目录>
pwsh -NoProfile -File tests/Check-WindowCoordinationSyntax.ps1 -OutputDirectory <目录>
```

API 依据：[SetForegroundWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow)、[GetFocus](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getfocus)、[SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos)。输入所有权及层级结论同时依赖当前源码和上述分级测试，API 文档本身不是实际游戏验证。
