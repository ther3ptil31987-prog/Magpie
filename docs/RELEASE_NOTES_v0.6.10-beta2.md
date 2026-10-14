# Magpie Experimental 0.6.10 Beta2

本地测试版本，继承完整 Beta1，并累计整合截至 2026-10-05 已提交的参数焦点、渲染显卡选择与统一 VRR 开发。公开更新入口仍为 0.6.9。

## 中文

### 参数焦点与窗口协作

- 捕获窗口、已校验根窗口、参数宿主使用统一会话身份规则；独立根窗口不再被参数层当成外部程序。
- 编辑输入同时验证 RunId、真实前台和线程焦点，处理激活失败、外部失焦、源弹窗、旧会话及宿主销毁／重开。
- 参数宿主跟随输出实际层级与可见性；正常持有输入仍等待完整收尾，外部取消和源退役使用取消路径。
- 焦点交接保留内容历史、停止和重建边界。游戏失焦导致的暂停或后台限帧仍需按游戏实测。

### 统一 VRR 与 NVIDIA 前台兼容（实验）

- “配置文件 → 帧率与刷新 → 高级选项”统一为一个默认关闭的“VRR 可变刷新率”开关，重新启用缩放后应用。原 NVIDIA 独立卡片移除；旧配置以原 VRR 开关为准，忽略旧兼容字段，新保存不再写入该字段。
- VRR 开关复用配置文件页面的右对齐紧凑模板（36 DIP 高度）；简体中文状态文字与同类选项统一为“关／开”，英文和繁体中文沿用同类开关译文，保留独立无障碍名称。
- 标准 DXGI VRR 请求和 NVIDIA 前台查询兼容由同一意图驱动；实际非 NVIDIA 输出继续使用标准呈现。按最终呈现路径和交换链实际 HWND／flags 判定资格，接入普通 DXGI、DLSS 前端 DXGI 及 XeSS D3D12 代理，解除补帧与窗口缩放模式的整体排除。
- 目标驱动／图形模块的查询可见最终输出窗口，源应用继续持有真实前台并接收输入；Magpie 自身业务继续取得真实前台。主界面、源弹窗、参数宿主和外部程序分别识别，参数交接、隐藏、最小化及停止暂停或退休兼容状态。
- 兼容状态在整个会话内原子发布，支持 SDK 后台查询。保持 DLSS ring、Reflex、XeLL、SDK 队列和一次输入提交规则；实际绑定不明或失败时撤销资格，保留正常输出和诊断。
- 保留条件 MinHook 静态依赖与许可证，ARM64 排除该依赖；新增 presenter／倍率／LUID／真实前台角色、绑定和多调用来源诊断。标准单独、跟踪及短时真实前台参考对照供开发测试使用。
- 该功能仍待硬件验证。软件回归和构建成功不表示物理 G-SYNC 已激活；DLSS／XeSS 各可用模式、实际调用模块、真实输入、HDR、多输出与最终输出预算仍需实测。

### 渲染显卡选择

- “配置文件 → 性能 → 渲染显卡”常驻显示，单卡也可选择“自动”或指定设备。
- 指定设备暂时缺失时保留选择意图并显示提示；主动选自动才清除指定身份，设备恢复后沿用原匹配规则。
- 无候选时禁用控件并给出提示；保留既有仅显示适配器过滤、配置结构与实际设备创建回退。

## English

This local test version retains Beta1 and accumulates parameter focus, rendering GPU selection and unified VRR developments committed through 2026-10-05. The public update entry remains at 0.6.9.

- **Parameter focus:** share validated capture/root/session identities, verify input ownership using the current run, real foreground and thread focus, follow output visibility/layering, and handle cancellation, host destruction and restart while retaining content-history and retirement boundaries.
- **Unified VRR and NVIDIA foreground compatibility:** use one default-off VRR control under Frame Rate and Refresh → Advanced Options. Retain the old VRR preference, ignore the removed compatibility field and omit it from new saves. Qualify actual swap-chain HWND/flags and the ordinary DXGI, DLSS frontend DXGI or XeSS D3D12 proxy path; remove the blanket frame-generation/windowed-mode exclusion. Selected NVIDIA driver/graphics queries may see the final output while the source retains real foreground/input and Magpie business logic uses the real query. Publish session state for SDK workers, distinguish main UI/parameter/popup/external roles, and suspend or retire during interaction, hiding, minimization and shutdown. Preserve queue, limiter, Reflex, XeLL and single-input-submission ownership. Static MinHook and its license remain included on x86/x64. Hardware G-SYNC activation, actual FG modes and input still require testing.
- **VRR control styling:** reuse the profile page's right-aligned compact toggle template (36 DIP height), match its localized on/off labels, and retain the dedicated accessibility name.
- **Rendering GPU selection:** keep the card in the Performance group even with one GPU; retain explicit device intent during temporary absence, distinguish automatic selection, disable the zero-device control, and preserve existing filtering/configuration/runtime fallback behavior.

## 来源与验收 / Source and acceptance

继承 `release/0.6.10-beta1`（`e6b82596`）；累计整合 UWP `ba1130a5`、VRR `df0e1ac8`、显卡选择 `293b694c` 及本地主线中对应的已提交入口修订记录 `13adddb6`。本轮基于原 Beta2 `96cd7b8a` 增量合入统一 VRR／FG 开发，保留原 DWORD 接口修复、UWP 与显卡选择。前台 C++ 测试夹具的窗口及代次变量改用独立名称，保持生产提取代码和严格警告检查。

The package's `build-manifest.json` records its exact source commit, feature options and file hashes. Local integration audits record regression, full-build and package checks. Native product input, actual application/GPU compatibility and physical G-SYNC/VRR activation still require testing; branch handoff documents retain their historical development-stage results.
