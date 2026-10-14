Magpie Experimental v0.6.10 x64

安装或升级
备份设置并从托盘完全退出 Magpie，将 Magpie-Experimental-x64.zip 完整解压到新目录，再运行 Magpie.exe。
普通配置：%LOCALAPPDATA%\Magpie\config\v4e\config.json。没有 v4e 时导入旧 v4，后续保存到 v4e。
便携配置：程序目录内的 config\v4e\config.json。保留旧程序和原配置，以便回退。

0.6.10 使用要点
帧率与刷新默认简单模式；高级模式提供节奏、光标和通用重复检测选项。切换保留高级偏好，重新开始缩放后生效。
内容帧率为跟随源或自定义，旧内容自动模式迁移为跟随源。手动值 15–360 FPS、步进 1；新配置内容 60 FPS、空闲重绘开启且为 30 FPS。
RTSS 联动与 VRR 默认关闭，按需开启。RTSS 状态反映配置和工具状态，实际游戏限帧需要确认。
DLSS/XeSS/FSR3/VFG 共用通用重复帧检测，新配置默认关闭。空闲重绘复用缓存画面，不增加真实内容或生成帧。
FSR3 固定 2×、仅 SDR，默认 AMDOF 质量，组合限制见 FSR3FG-README.md。
NVIDIA VFX 提供 Upscale、VSR、VFG 和 TrueHDR。VFG 为 SDR 2–8×，默认 2×、Balanced，旧 High 迁移为 Balanced。
HDR 通过手动添加并排序转换节点配置。截图可选输入＋输出、仅输出（默认）或仅输入。
DLSSNR 状态与分类引导帮助判断实际运行状态；165 个效果的双语说明提供用途与起步建议。

完整更新、安装与限制见 RELEASE-NOTES.md，帧率说明见 FRAME_SYNC_GUIDE.md。
保留 LICENSE-Magpie.txt、THIRD-PARTY-NOTICES.md、组件许可证及 build-manifest.json。
可选附件：DLSSNR-DLL-Options.zip、NGX_OTA_Switch.bat。普通安装无需运行 OTA 工具。

English

Install or upgrade
Back up settings, fully exit Magpie from the tray, extract the complete Magpie-Experimental-x64.zip into a new folder and run Magpie.exe.
Normal settings: %LOCALAPPDATA%\Magpie\config\v4e\config.json. If v4e is absent, import v4 and save subsequent changes to v4e.
Portable settings: config\v4e\config.json under the program folder. Keep the previous program and settings for rollback.

0.6.10 essentials
Frame rate and refresh defaults to Simple. Advanced exposes pacing, cursor and general duplicate-detection options. Switching retains advanced preferences and applies after restarting scaling.
Content rate is Follow source or Custom; legacy content Automatic migrates to Follow source. Manual rates use 15–360 FPS in steps of 1. New defaults: 60-FPS content and enabled 30-FPS idle redraw.
RTSS integration and VRR default to Off; enable as needed. RTSS status describes configuration and tool state; confirm actual in-game limiting separately.
DLSS/XeSS/FSR3/VFG share general duplicate detection, default-off for new settings. Idle redraw reuses cached pictures without adding real content or generated frames.
FSR3 is fixed 2x SDR and defaults to AMDOF Quality. See FSR3FG-README.md for combination restrictions.
NVIDIA VFX provides Upscale, VSR, VFG and TrueHDR. VFG uses SDR 2–8x, defaults to 2x Balanced and migrates retired High to Balanced.
Configure HDR by manually adding and ordering conversion nodes. Screenshots offer input + output, output only (default), or input only.
DLSSNR state and cause-specific guidance help identify actual operation; bilingual descriptions for 165 effects explain purpose and initial setup.

See RELEASE-NOTES.md for updates, installation and limitations, and FRAME_SYNC_GUIDE.md for refresh settings.
Retain LICENSE-Magpie.txt, THIRD-PARTY-NOTICES.md, component licenses and build-manifest.json.
Optional assets: DLSSNR-DLL-Options.zip and NGX_OTA_Switch.bat. Normal installation does not require the OTA tool.
