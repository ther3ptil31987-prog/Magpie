# 帧率与刷新 / Frame rate and refresh — 0.6.10

## 设置入口与默认值

默认配置和应用配置各自保存刷新设置。新建配置复制默认配置或所选模板，之后独立保存；主页提供默认配置摘要和编辑入口。

简单／高级是全局选择，默认简单；切换保留高级偏好。内容、空闲重绘和 VRR 为共享设置。高级提供节奏、光标和通用重复检测。RTSS 使用独立的配置接管组，关闭联动或跟随源时仍可打开工具管理。

| 设置 | 选择与新配置默认值 |
| --- | --- |
| 内容帧率 | 跟随源／自定义；默认自定义 60 FPS |
| 静态画面重绘 | 关闭／自定义；默认开启，30 FPS |
| 节奏方式 | Front Edge（默认）／Async／NVIDIA Reflex；高级可选 |
| 光标刷新 | 响应优先／仅随原始内容帧／原始帧优先并补充（默认） |
| 光标补充率 | 自动跟随显示刷新率／自定义；默认自定义 60 FPS |
| VRR（实验） | 默认关闭，重新开始缩放后生效 |
| 通用重复检测 | 从不（默认）／动态／总是；高级可选 |
| RTSS 接管 | 默认不管理；可管理全局或目标应用 |

内容、光标和空闲手动值为 **15–360 FPS、整数、步进 1**；已有有效数值保留。简单使用 Front Edge 与原始帧优先并补充，自定义内容 N FPS 时光标补充目标为 N，跟随源时为 60。简单模式的通用重复检测关闭，高级偏好保留。

修改自动保存并在重新开始缩放时生效。运行时参数面板按当前会话模式显示，“保存并重启”应用更改，单独切换模式也需重启会话。简单模式恢复默认只恢复可见字段，保留高级偏好及其他配置。

## 内容节奏与源应用限帧

跟随源按捕获的新内容工作；自定义控制 Magpie 基础内容目标。旧内容“自动”迁移为跟随源，光标的“自动”仍保留。补帧倍率不会把自定义基础目标反向改成游戏限帧。

Front Edge 在提交前对齐，可能增加等待；Async 在取帧前限制开始间隔，不追赶过期时间网格；NVIDIA Reflex 在支持路径交由驱动控制基础目标，失败时按既有保护回退或停止。XeSS 使用 XeLL 自身限帧，不叠加另一套基础限帧器。源程序、捕获或效果跟不上目标时实际帧率更低。

RTSS 联动在开始缩放时准备已安装工具与 Magpie 自身不限帧配置，再写入当前目标。同步使用有效基础目标；指定值 15–360 FPS、步进 1。跟随源时不使用同步限帧。重新检测只读，重试针对当前会话。默认正常结束时尝试恢复原限帧；崩溃或强制退出不能保证恢复。“已写入”只说明配置核验通过。

## 光标、静态画面与补帧

光标策略控制 Magpie 发布的系统光标；源程序画入图像的光标属于内容。补充率是最低目标，受显示和呈现通道约束，静止光标不要求持续空呈现。

开启空闲重绘后，普通路径按既有缓存规则更新。DLSS／XeSS／FSR3／VFG 约 0.5 秒没有新内容后刷新已完成的缓存，避免把正常 24/30 FPS 内容误判为空闲，不重跑效果或光流。关闭时保持最后画面等待。新内容恢复时重置插帧历史。有效重绘率受内容上限和显示刷新率约束，倍率不乘到重绘率。

工具栏 R/s 单独记录缓存重绘提交。FSR3 Submitted 表示成功提交，不能直接作为显示 FPS 或有效插值画面数。四类 FG 共用通用重复检测；旧效果字段迁移移除，不覆盖已保存的通用偏好。

## VRR 与排查

VRR 默认关闭，开启后按输出路径、显卡和显示器资格请求，重新开始缩放生效。软件开关或成功提交不能证明物理 VRR 工作，请结合显示器指示或显示间隔确认。

先使用简单模式和简单效果链，再逐项开启 RTSS、VRR 或补帧。比较时保持同一源上限、场景和参数。遇到问题先关闭相关选项，记录日志、捕获方式、驱动和复现步骤。完整变化和组合限制见包内 RELEASE-NOTES.md 与 FSR3FG-README.md。

---

## Settings and defaults

Default and application profiles store their own refresh settings. New profiles copy the default or chosen template, then save independently. Home offers a default-profile summary and editing entry.

Simple / Advanced is global, defaults to Simple and retains advanced preferences. Content, idle redraw and VRR are shared. Advanced adds pacing, cursor and general duplicate detection. RTSS has a separate management group accessible when integration is off or content follows the source.

| Setting | Options and new-profile default |
| --- | --- |
| Content rate | Follow source / Custom; default Custom 60 FPS |
| Static-picture redraw | Off / Custom; enabled at 30 FPS |
| Pacing | Front Edge (default) / Async / NVIDIA Reflex, selectable in Advanced |
| Cursor | Response first / Original frames only / Prefer original frames with supplementation (default) |
| Cursor supplementation | Automatic display refresh / Custom; default Custom 60 FPS |
| VRR (experimental) | Off; restart scaling to apply |
| General duplicate detection | Never (default) / Dynamic / Always, selectable in Advanced |
| RTSS management | Off; global or target-application management available |

Manual content, cursor and idle rates use **15–360 FPS, integers, 1-FPS steps**; valid existing values remain. Simple uses Front Edge and supplemental cursor refresh at N FPS for Custom N content or 60 FPS for Follow source. Simple disables general duplicate detection while retaining Advanced preferences.

Edits save automatically and apply on restart. Runtime controls follow the current session's mode; Save and restart applies changes, including mode-only changes. Reset in Simple restores visible fields while retaining advanced preferences and other profiles.

## Content pacing and source-application limiting

Follow source processes new captures; Custom controls Magpie's base content target. Legacy content Automatic migrates to Follow source; automatic cursor supplementation remains. The FG multiplier does not turn the custom base target into a different game limit.

Front Edge aligns before submission and can add waiting. Async limits processing starts before capture without catching up to an expired grid. NVIDIA Reflex uses driver control on supported paths with protected fallback or stopping on failure. XeSS uses XeLL without stacking another base limiter. Actual rate falls below target when the source, capture or effects cannot keep up.

RTSS prepares the installed tool and Magpie's unlimited profile at scaling start, then writes the target. Synchronization uses the effective base rate; custom values use 15–360 FPS in 1-FPS steps. Follow source does not use synchronized limiting. Recheck is read-only and retry belongs to the current session. Normal exit attempts restoration by default; crashes or forced exits cannot guarantee it. A verified write confirms configuration only.

## Cursor, static pictures and frame generation

Cursor policy controls Magpie's system cursor; a cursor drawn into the source picture is content. Supplementation is a minimum target subject to display and presentation capacity. A stationary cursor does not require continuous empty presentation.

With idle redraw enabled, ordinary paths use existing cache rules. DLSS / XeSS / FSR3 / VFG refresh completed cached pictures after about 0.5 seconds without content, avoiding false idle transitions at normal 24/30 FPS, without rerunning effects or optical flow. Disabling holds the last picture. Resumed content resets FG history. Effective redraw is constrained by content and display rates and is not multiplied by FG.

Toolbar R/s counts cached redraw submissions separately. FSR3 Submitted counts successful submissions, not display FPS or unique interpolated images. All four FG types share general duplicate detection; old effect fields are removed without overriding general preferences.

## VRR and troubleshooting

VRR defaults to Off and requests variable refresh according to output path, GPU and display qualification after restarting scaling. A toggle or successful submission alone cannot prove physical VRR; check the display indicator or display intervals.

Start with Simple and a simple chain, then add RTSS, VRR or FG individually. Compare with the same source limit, scene and parameters. Disable the relevant option first if issues occur; retain logs, capture method, driver information and steps. See packaged RELEASE-NOTES.md and FSR3FG-README.md for changes and restrictions.
