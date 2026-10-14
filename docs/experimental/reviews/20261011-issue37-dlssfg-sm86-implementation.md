# Issue 37：DLSSFG 社区兼容接入开发交接

日期：2026-10-11。分支：`feature/dlssfg-sm86-unified`。基线：Beta6 `556d4ac245e0fb83762788e57d2ad0eb5a26247f`。

后续更正：本页保留 `7eb2a79d` 的初始源码交接。Beta7 实测暴露了仅映射代理、未注册 bundle 目录的缺口；同分支后续修复见[启动修复源码交接](20261011-issue37-dlssfg-sm86-startup-fix.md)。不能将下文的初版加载流程作为当前实现。

状态：源码候选接入与脚本回归已完成，未编译、未运行代理或 Magpie、未部署、未创建 release、未合并或推送。用户指定由独立会话编译部署，因此原 TODO 的 P0 GPU 原型继续条件仍未验收；本分支的 P1 实现属于待验证候选，不能据此声称 RTX 20／30／40／50 已在 Magpie 中工作或已经可发布。

## 本轮目标与需求

1. 原 `DLSSFG\DLSS_FrameGeneration` 自动使用社区兼容方案，无第二个效果器、选库界面或用户 INI 配置步骤。
2. 仅 2×／3×／4×，最多生成 3 帧；保留旧配置的倍率、光流、重复帧过滤、效果链语义及默认 2×。
3. `Optimized=1`；有损图像近似、跳过真实帧复制、强制生成数量、Streamline 插件补丁和实验性加速关闭。
4. 依赖由开发流程固定，安装包自动携带；用实际渲染适配器选择内部路由，不能用显示名称判断。
5. 加载、能力或身份校验失败时明确失败。保留 NGX Available、Feature 创建、输出格式、插值禁用标志、4× 容量与单 FG 约束。
6. 本轮交付源代码、无需编译的验证及后续验收清单。本轮目标完成不等于原始 TODO 的运行、发布验收全部完成。

## 实现与重要约束

### 加载与路由

`DlssgSm86Runtime.cpp` 在实际 backend 设备选定后、首次 NVIDIA 效果初始化前准备代理；共享 NGX D3D12 Core 和 DLSS SR D3D11 初始化处另有防遗漏调用。普通 shader 缩放不加载 CUDA／代理；非 NVIDIA 适配器不启用此方案。NVIDIA 效果初始化失败不会仅因代理缺失而禁用 SR／NR，FG 自身必须通过准备检查。

通过系统 CUDA Driver API 的 `cuDeviceGetLuid` 对齐 DXGI LUID，并查询物理 compute capability。没有创建 CUDA context，不随包部署 CUDA toolkit。内部路由表目前只覆盖上游目标的 SM75、SM86、SM89、SM120，其他架构报告缺少路由策略；这不是“任意同架构显卡都受支持”的保证，最终仍需 NGX 能力与执行验收。

| 物理架构 | 托管配置 | 候选策略 |
| --- | --- | --- |
| SM75／SM86 | `native.ini` | `Router=Auto`、`KernelImage=Auto`、不强制 SM86、不模拟 Ampere |
| SM89／SM120 | `forward.ini` | `Router=Auto`、PTX、`ForceSM86Route=1`、`SimulateAmpere=1` |

两种配置均设 `SpoofArchToGame=0`，因为 Magpie 直接使用 NGX，没有 Streamline 的游戏侧型号门禁。这是待实测的最小适配候选，并非已经验证关闭伪装就能工作。40／50 的 forced route 也必须单独验收，不能作为现有原生性能无损的证据。

包内只放 `runtimes/dlssg_sm86/version.dll` 和两个配置模板，不放根目录代理。运行时校验后自动缓存至 `%LOCALAPPDATA%/Magpie/DlssgSm86/<proxy-sha>/<config-sha>/`，在那里生成固定的 `dlssg_sm86.ini`，再用绝对路径显式加载。代理本身继续管理 `%LOCALAPPDATA%/DlssgSm86/bundles` 内的运行库／后端缓存。这样可支持只读安装目录，并隔离不同版本和策略。缓存损坏时先复制至临时文件、校验、原子替换；在用文件不强行覆盖。

托管配置不读取安装根目录的手工 INI，也不提供用户覆写入口；开发对照须改源码配置及其哈希再构建。上游日志固定为 Level 2，位于对应代理缓存目录的 `logs` 下。日志和缓存不会在会话退出时擅自删除。

代理、配置读取句柄和代理模块保留到进程退出，没有热卸载／进程内切回原生的接口。另一代理已经抢先加载时拒绝含糊的配置。加载后切换渲染 GPU，FG 要求重启 Magpie；同一 LUID 的 Feature 重建继续使用原配置。

### 文件身份与生成边界

`src/Shared/DlssgSm86Package.h` 是开发恢复、打包、Updater 和运行时共用的文件清单。固定上游提交 `9621db573e07ed54f50c15bbb585ed9a7bdfac28`／0.3.5。静态解析代理 PE 资源确认内嵌 NVIDIA 运行库为 **310.9.1.0**，后端和运行库各有独立 SHA-256。DLL 不进入 Git。

首次能力查询后检查已加载运行库／后端的文件身份，拒绝其他版本和未加载配套库的路径。**此检查只证明加载文件身份；后端 active、实际优化内核、生成图像与显示仍需日志和硬件验证。** 未调用无公开 ABI 定义的 `DlssgMod_GetInfo` 等导出，也未凭函数名猜测签名。

倍率为 `min(clamp(requested, 2, 4), reportedMax)`。MFG 查询缺失沿用旧版 2×；明确返回 0 不再被强行抬升为可补帧；超大值封顶 4×；SEH 不作能力缺失继续。没有改动 Renderer 的队列、HDR、生成顺序、历史重置、Reflex 或失败后有界重建策略。

新增日志记录物理 LUID／PCI／SM、CUDA driver API 版本、代理／配套库路径与哈希、请求／有效倍率、Evaluate、插值禁用及生成纹理发布数量。CUDA driver API 版本不是 NVIDIA 显示驱动版本，验收时另录实际驱动版本。`generatedPublished` 仅表示现有发布回调接受，不能充当真实显示计数或端到端延迟。

### 打包与更新

- `Fetch-DlssgSm86.ps1` 固定提交下载代理／声明，并复制托管配置，全部校验后接受；不加载 DLL。
- `DlssgSm86RuntimeDir` 是开发依赖路径。MSBuild 复制目标改用 `Copy-DlssgSm86Runtime.ps1`，移除原官方 FG DLL 的复制，清理该目标拥有的旧 `nvngx_dlssg.dll` 输出。SR／NR DLL 保留。
- `Test-DlssgSm86Package.ps1` 验证配套文件和根目录旧库／代理；`Build-Release.ps1` 的 `-SkipBuild` 同样执行检查，并记 provider 身份。
- Updater 先校验下载内容，再等待主程序结束并备份将被覆盖／删除的文件；复制失败或安装后 FG 清单不匹配会尝试回滚，保留下载目录和回滚副本，并拒绝启动 Magpie。成功后保留 `update-rollback-<pid>-<tick>`，便于交接复核。
- 原生旧 FG DLL 纳入迁移清理；已知同哈希根目录代理及其 INI 会先备份再移除。未知根目录代理不被盲删，会导致完整性检查失败；没有承诺自动迁移任意第三方注入方案。
- `UpdateService` 替换或启动 Updater 失败时不再退出主程序。
- C++ 更新事务尚未运行；断电／进程强杀后的自动恢复不在当前实现保证内。磁盘空间不足、文件占用、只读属性及回滚本身失败必须实测。备份不包含已经由旧进程替换的 Updater 本体。

## 验证记录

| 检查 | 本轮结果 |
| --- | --- |
| 固定依赖恢复与 SHA-256 | 通过；仅下载与读取，未执行 DLL |
| `Test-DlssgSm86Packaging.ps1` | 18 项通过：缺文件、等长损坏、配置损坏、6 种根目录旧文件、失败前不改输出、保留 SR、中文与空格路径 |
| PowerShell 语法解析 | 6 个新增／修改脚本通过 |
| 工程／属性 XML | 5 个文件解析通过 |
| `Generate-EffectCatalog.ps1 -Check` | 通过；两种语言仅修改原 DLSSFG 条目 |
| `Validate-EffectCatalog.ps1` | 未全通过：停在基线已有的 XeSSFG 审阅哈希过期；VFX VFG 也有同类过期记录，已用基线 Git blob 复核 |
| DLSSFG 审阅元数据 | 同步 Beta6 真实源码哈希及既有重复帧过滤默认 0；未修改 HLSL 默认值 |
| `DlssgSm86PolicyTests.cpp` | 已写入倍率／路由断言，未编译运行 |
| `UpdateTransactionTests.cpp` | 已写入替换、回滚、占用失败、路径穿越拒绝夹具，未编译运行 |
| 产品构建／实机 GPU／UI／升级 | 按用户范围未执行 |

完整效果目录失败不能写成通过，也没有为通过全局检查去改本功能之外的 XeSSFG／VFX 审阅记录。

## 对应原 TODO 的逐项状态

“源码候选”表示已写入或保留相应代码，不表示其运行验收已通过。

| ID | 当前状态与证据 |
| --- | --- |
| P0-01 | 完成：独立工作树／功能分支，以 Beta6 精确提交为基线 |
| P0-02 | 部分：固定代理、模板、内嵌文件指纹；完整许可／源码材料仍待确认 |
| P0-03 | 待后续会话：直接 NGX 2×／3×／4× 原型 |
| P0-04 | 源码候选：backend、SR、共享 Core 的早期接线；首次加载时序待实测 |
| P0-05 | 源码候选：独立目录＋托管缓存＋绝对路径加载 |
| P0-06 | 源码候选：LUID 和 CUDA 架构路由；各架构 active／内核尚未确认 |
| P0-07 | 候选：关闭游戏侧伪装，待验证直接 NGX 所需最小适配 |
| P0-08 | 部分：能力／文件／Evaluate／发布日志；图像和呈现证据仍缺失 |
| P1-01 | 源码候选：沿用原效果、Frame Guidance 与呈现链 |
| P1-02 | 源码候选：模块与配置按进程保留，不热卸载 |
| P1-03 | 源码候选：保留 Available／Feature 门禁，新增配套身份检查 |
| P1-04 | 源码候选：0／缺失／异常上限处理；C++ 断言待执行 |
| P1-05 | 已复查源码：没有需删除的 40／50 UI 白名单；保留 x64、格式和单 FG 门禁 |
| P1-06 | 源码候选：实际 LUID 选路由；换卡需重启，混合 GPU 待验收 |
| P1-07 | 源码候选：旧 ID／参数不变，中英文说明已更新 |
| P1-08 | 保留既有 drain／重建；代理生命周期变化待反复启停验收 |
| P1-09 | 保留既有帧身份／历史／禁用标志／背压，未扩大容量 |
| P1-10 | 源码候选：失败不视为原生成功；沿用有界恢复与重启错误流程 |
| P1-11 | 部分：增加身份、倍率和阶段计数；物理呈现／实际驱动记录由验收补齐 |
| P1-12 | 源码／脚本完成：MSBuild 与 release 清单接线，实际构建未执行 |
| P1-13 | 部分：中文空格夹具、托管配置版本隔离；升级与缓存修复待实测 |
| P1-14 | 源码候选：快照、失败传播、回滚与禁止启动；Windows 事务测试待运行 |
| P1-15 | 部分：文件指纹已固定；许可、实际升级、可发布状态未通过 |
| P2-01 | 部分：18 项脚本回归通过，C++、旧配置及 NGX 故障回归待编译会话 |
| P2-02 | 待实机：20／30／40／50 × 2／3／4 倍、非 NVIDIA 场景 |
| P2-03 | 待实机：40／50 同运行库原生／社区对照，以及 Beta6 对照 |
| P2-04 | 待实机：GPU 耗时、实际输出、显存、长稳与延迟 |
| P2-05 | 待实机：零 MV／AMDOF／NVOF 与典型画面 |
| P2-06 | 待实机：SDR／HDR、SR／NR／Reflex／RTSS／RTX Video |
| P2-07 | 待实机：非 FG、改尺寸、启停、换卡、设备丢失 |
| P2-08 | 未达到可发布：保留所有未验收项 |

## 后续会话的执行顺序

1. 使用本功能分支和工作树，确认所需其他并行修复的合并范围；不要回到脏的日常源码目录直接编译。不要把本次基线误写成 experimental 最新主线。
2. 设置 `DlssgSm86RuntimeDir` 指向经 `Fetch-DlssgSm86.ps1` 校验的依赖；继承 Beta6 所需其他 SDK 路径。没有替下一会话写本地 `BuildOptions.props.user`。
3. 先编译 C++ 策略与更新事务夹具（C++20，Windows SDK），再做完整产品构建。检查首次链接的 ShlObj／bcrypt／Kernel32 接口、WIL 类型、代理加载错误和默认关闭 FG 的构建。
4. 先在独立可恢复产品副本跑 P0，重点检查显式加载 cached `version.dll` 的接管时机、`SpoofArchToGame=0`、配套模块名称及 capability 后是否仍常驻。文件身份检查若拒绝，应定位加载链，不可删掉检查强行宣称成功。
5. 从 loader/backend 日志证明 active 和优化内核实际路径，再确认 Feature／Evaluate／插值标志与生成图像。40／50 forced route 若不工作，继续开发；不把旧路径作为统一接入完成。
6. 每张代表卡跑 2×／3×／4×、现有配置导入、GPU 切换、重建和 SDR/HDR；随后进行同版本对照和 Beta6 对照。所有性能、画质结论记录设置和样本，不使用社区自述代替。
7. 做完整升级、文件占用／损坏／磁盘不足／回滚失败试验。确认旧配置保留、无根目录旧 FG DLL、包指纹一致、失败时不启动混装进程。
8. 清理或补齐基线已有的效果审阅哈希问题，核实第三方分发材料，然后单独决定 release 状态。

依赖恢复与脚本回归的可复用命令（仓库根目录）：

```powershell
./scripts/Fetch-DlssgSm86.ps1 -OutputDirectory <开发依赖目录>
./tests/Test-DlssgSm86Packaging.ps1 -RuntimeDirectory <开发依赖目录>
./scripts/Generate-EffectCatalog.ps1 -Check
./tests/Validate-EffectCatalog.ps1
```

## 上游材料缺口

固定提交公开树没有文档引用的完整 C++ 源码、构建接口及 `LICENSE.md`。实际 `THIRD_PARTY_NOTICES.txt` 仍描述 310.1 的运行库指纹，而根目录代理静态资源已是 310.9.1。此处保留原始声明，不改写成已经获得再分发许可；完整材料与条件仍需确认。仓库没有提交第三方二进制。

- [固定上游提交](https://github.com/sdli1995/dlssg_for_sm86/tree/9621db573e07ed54f50c15bbb585ed9a7bdfac28)
- [配置与路由说明](https://github.com/sdli1995/dlssg_for_sm86/blob/9621db573e07ed54f50c15bbb585ed9a7bdfac28/docs/INSTALL.en.md)
- [原始第三方声明](https://github.com/sdli1995/dlssg_for_sm86/blob/9621db573e07ed54f50c15bbb585ed9a7bdfac28/THIRD_PARTY_NOTICES.txt)
- [CUDA Driver API 的设备与 LUID 接口](https://docs.nvidia.com/cuda/archive/12.8.2/cuda-driver-api/group__CUDA__DEVICE.html)
