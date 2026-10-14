# Magpie Experimental 0.6.10 Beta4

本地测试版本，继承完整 Beta3 `3c071cc6`，整合截至 2026-10-09 已提交的 HDR、RTSS 与 VRR 更新。公开更新入口仍为 0.6.9。

## 中文

- **HDR 恢复手动组装：** 移除效果组的 HDR 输入／输出入口和自动规划，HDR→SDR、SDR→HDR、RTX HDR 由用户自行添加、排序、调参和移除。有效的 Beta3 试用组保持实际节点、顺序、参数、捕获域与配对，保存原始组数据供恢复；损坏或无法确认的组保留原文并阻止启动。首次迁移沿用配置恢复备份路径，重载不重复展开。
- **HDR 基础修复保留：** 原生白色新编辑仍为 80–4000 尼特、默认 203，旧低亮度和曝光语义保留；各节点独立参数与配对来源提示、精度和分段／Shift 步进、实际格式 PNG／DDS 导出及 RTX 清理失败保护继续生效。
- **RTSS 固定布局：** 停止处理直接放在限制帧率下方，移除 RTSS 内层高级区。同步模式显示只读整数目标，未开始／停止／跟随源或无目标时显示“—”；手动值独立保存。同步目标支持 1–1000 FPS，手动输入保持 15–360 FPS。精简状态文字并对齐工具按钮。
- **RTSS 保存与恢复：** 按当前值跳过未变化的属性，避免普通 FPS 配置因重复设置已为 0 的帧时间而失败；变化值必须设置成功且即时读回一致，保存后核对实际文件再请求重载。新增设置、核对、重载和帧时间模式失败文案，明确检测当前调用者的写入权限，停止状态清除旧失败说明。非零帧时间切换、字段缺失表达及按需提权仍保留兼容边界，真实目标限帧需实测。
- **VRR 开销与诊断：** 精简前台查询热路径、身份检查、发布快照及默认统计，缓存模块和输出资格；完整画面复制前省略冗余清空，区域复制仍保留清空。默认关闭详细工作量统计，显式 `MAGPIE_VRR_PERFORMANCE=1` 或相关诊断模式可提供有界计数与聚合日志。保持源输入、退休、后台 SDK 查询和单一限帧所有者。
- GPU 占用升高的根因、等帧率 GPU 成本与实际 G-SYNC 仍需实测；CPU 开销改进和工作量计数不代表该硬件问题已解决。本轮没有新增独立 sleep 或自动 VRR 输出预算。

## English

This local Beta4 retains Beta3 (`3c071cc6`) and integrates the committed HDR, RTSS and VRR updates. The public update entry remains at 0.6.9.

- **Manual HDR chains:** remove group-level HDR controls and automatic planning. Add, reorder, edit and remove conversion effects manually. Safely expand valid trial groups while retaining their materialized chain, capture intent, parameters and pair semantics, with original data saved for recovery. Preserve malformed groups and block unsafe startup. Keep native-white editing, legacy dim values, precision/step controls, PNG/DDS export and RTX cleanup protection.
- **RTSS UI and rate display:** use fixed rows with a direct stop-policy card. Show the current synchronized target as a disabled integer control, or a dash when unavailable. Retain the independent manual value and 15–360 FPS editing range; synchronized targets may span 1–1000 FPS. Simplify text and align tool buttons.
- **RTSS save and restore:** skip unchanged properties, verify changed values immediately, and read back saved files before requesting reload. Report permission, setting, save, verification, reload and time-mode failures separately; clear stale errors when stopping. Nonzero frametime conversion, missing-field expressions and on-demand elevation remain compatibility work. Actual target-process limiting still requires measurement.
- **VRR overhead and diagnostics:** reduce foreground-query and snapshot work, disable detailed statistics by default, cache module/output qualification, and omit redundant clears before full scene copies. Opt-in work counters provide bounded aggregate diagnostics. Preserve real source input, retirement, background SDK access and existing limiter ownership. GPU usage, physical G-SYNC and equal-rate performance still need hardware validation.

## 来源与验收 / Source and acceptance

功能头：HDR `fac29926`、RTSS `50144ea0`、VRR `9e5f89f7`。合并保留原 UWP／参数焦点、显卡选择及 VRR 开关样式；原前台回归夹具的全局窗口和会话代次变量保持独立名称，使扩展回归继续满足严格警告检查。窗口切换回归夹具同步适配当前双窗口前台接口。

包内 `build-manifest.json` 记录准确源码、构建选项与逐文件哈希；初次整合记录位于工作区 `reports/0610-beta4-20261009/`；本次 RTSS 增量、候选构建、备份与覆盖记录位于 `reports/0610-beta4-update-20261009-rtss-save/`。原生整页／DPI、实际迁移交互、RTSS 事务、HDR／RTX／FG 画质与性能、物理 VRR 另行验收。Beta3 与替换前的 Beta4 均保留供回退。
