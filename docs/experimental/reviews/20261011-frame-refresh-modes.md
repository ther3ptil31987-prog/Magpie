# 帧率与刷新简单 / 高级模式实施记录

日期：2026-10-11。功能分支：`feature/0610-frame-refresh-modes`。基线为 Beta6 `556d4ac2`，已经包含原 RTSS 功能分支 `5a1b51c1`。先提交完整 [TODO](../todos/20261011-frame-refresh-modes-TODO.md)（`a0d427c1`），再实施功能。本轮未合入主线或 Release，未部署、推送或改写用户配置。

## 实际行为

设置页增加全局记忆、默认简单的模式选择。简单只有内容帧率和空闲重绘两个设置项，各自包含选择与按需出现的 FPS 输入。内容允许跟随源 / 自定义，空闲允许关闭 / 自定义。高级显示目标窗口限帧、内容、光标、VRR、重复帧检测、空闲重绘六个同级条目，移除本组原高级容器；各条目的必要子控件保留。恢复默认入口留在高级视图。

简单模式不覆盖保存的高级字段，而是在启动和参数重启时生成实际运行参数：Front Edge、光标原始帧优先并补充、VRR 关闭、本组的全局重复帧检测 Never、RTSS 不接管。固定内容 N FPS 时光标补充 N FPS；跟随源时补充 60 FPS。既有显示刷新率约束仍适用。两种模式共享内容与空闲字段；返回高级使用保留的节奏、光标、VRR、RTSS 及全局重复检测偏好。

全局 JSON 新增 `simpleFrameRefresh`，缺失或类型无效时默认 true；既有按应用保存的数值仍保留。内容枚举历史 Auto 值保留编号供迁移识别，读取和保存时转为 Source 并释放该自动模式的兼容上限；Custom 历史编号不变。设置页和参数浮层不再提供内容 Auto，光标补充的 Auto 仍保留。

工具栏参数面板不增加模式选择，编辑项依据当前运行会话的模式：简单仅编辑共享的内容/空闲，高级保留原有详细编辑能力。设置页修改全局模式后，当前会话仍使用原参数；待重启状态单独同步到参数会话，确保只切换模式也会启用“保存并重启”。该操作在模式变化时重建完整缩放会话，重新计算重复检测和 RTSS 策略；普通帧率编辑沿用原参数重启路径。

## 实现位置

- `AppSettings.h/.cpp`、`ConfigRecovery.h`：模式持久化、缺省处理、全局事件。
- `FrameRefreshSettings.h`、`ProfileFrameRefresh.h`：自动迁移与简单模式有效值投影；不把投影回写成高级偏好。
- `ScalingService.cpp`、`ScalingOptions.h`、`ScalingRuntime.cpp`、`ScalingWindow.cpp`：启动、参数同步、待重启通知、完整模式重启与 RTSS 生命周期。
- `ProfilePage.xaml`、`ProfileViewModel.*`、`HomeViewModel.*`：设置布局、两项共享数值、隐藏控件写入保护、有效值摘要。
- `OverlayDrawer.cpp`：无模式选择的参数界面、内容两种选择、模式单独变化的重启计数。
- 20 份 `.resw` 添加模式名称/说明/无障碍名称；英、简中、繁中更新内容与空闲说明，其余语言沿用既有资源回退。

## 已完成验证

输出目录：`C:/Users/81443/Documents/Magpie enhance/reports/20261011-frame-refresh-modes`。测试使用独立输出目录与模拟/隔离依赖，没有操作正在运行的用户 RTSS。

| 验证 | 结果与范围 |
| --- | --- |
| `Run-FrameRefreshTests.ps1` | 1,972 项模型/迁移/事务检查通过；生产 ViewModel 提取的 72 种组合通过；新增生产全局模式读写、事件、启动参数、RTSS 范围与模式单独变化的待重启测试通过 |
| `Run-CursorRefreshTests.ps1` | 光标策略 181、配置/复制 195、输入采样 15 项通过 |
| `Run-FrameRefreshRateSliderTests.ps1` | 真实 ImGui 滑条测试通过，2,076 次整数编辑，15–360 边界及无隐式小数回写通过 |
| `Run-RtssTests.ps1 -Compile` | 367 项结构/20 语言资源合同通过；设置 216、生产数值显示 54、配置事务 309、会话事务 126、初始化 87、布局/启动 22 项隔离原生检查通过 |
| VRR / NVIDIA 前台合同 | 两项脚本通过；只证明源码连接、保存和生命周期边界 |
| MSVC `/Zs /W4 /WX` | `ScalingWindow`、`ScalingRuntime`、`Renderer`、`GraphicsCaptureFrameSource`、`SrcTracker`、`ParameterInputHost`、`CursorManager`、`OverlayDrawer`、`ScalingOptions` 共 9 个 Core 翻译单元通过，无产品链接 |
| MIDLRT | 修改后的 `ProfileViewModel.idl` 单独编译成功，输出在 reports，未生成或替换产品包 |
| XAML / IDL / 资源与 diff | XML、属性绑定、简单两项/高级六项、无旧高级容器、无工具栏模式选择、20 语言模式资源及 `git diff --check` 通过 |

旧测试中依赖原高级容器的断言已改为检查六项同级；CursorConfig 夹具补上基线已使用的 `ProfileRtss.h`，RTSS 布局合同对齐基线已有的 Tools 按钮样式。生产对应行为未因测试适配而修改。

## 后续产品验收

- 未构建/链接完整 Magpie 产品、未部署、未运行修改后的真实设置页，DPI、滚动高度、焦点和控件展开的视觉表现待实际 UI 验收。
- 未进行真实游戏 FPS、光标延迟、VRR/G-SYNC、GPU 功耗或实际外部 RTSS 写入验收。
- 空闲重绘沿用现有补帧禁用及内容目标上限；本轮没有改变补帧算法。
- 本组的全局重复帧检测被简单模式关闭。补帧效果自身独立的 `duplicateFrameFiltering` 参数沿用现有覆盖规则，未擅自改写效果参数。
- RTSS 不接管不会清空外部工具原有配置；已有 Magpie 管理会话在重新开始时按旧会话退出策略结束，再按新模式开始。
