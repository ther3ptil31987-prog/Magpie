# DLSSFG Beta7 启用失败诊断与修复 TODO

日期：2026-10-11（Asia/Shanghai）。范围：用户确认的“程序启动或启用 DLSSFG 失败”；定位到的是 DLSSFG 启用失败，Magpie 主程序已经启动。诊断之后，用户授权继续原开发分支完成源码修复，编译部署交给独立会话。

当前状态：下方勾选项表示分支管理／源码实现完成，不表示编译或实机通过。9 项完成本轮源码范围，6 项保留全部或部分运行验收；详见[修复源码交接](../reviews/20261011-issue37-dlssfg-sm86-startup-fix.md)。未编译、未部署修改后的 Magpie，实际补帧仍待验收。

## 结论与证据边界

首要问题在社区运行库的初始化／发现链：已部署 Beta7 实现只加载代理便返回准备成功，却没有主动触发其内嵌运行库／后端加载，也没有把展开后的实际目录加入 NGX 搜索路径。安装脚本同时移除了原来可被 NGX 发现的根目录 FG DLL。这两个接入缺口与实机 `FeatureNotFound` 相吻合，本轮源码修复针对这条链路。

**已通过独立加载探针确认：代理映射成功不等于 bundle 或后端准备完成。** 显式请求 `nvngx_dlssg.dll` 才触发解包、重定向和后端安装。尚未用修复后的 Magpie 完成 NGX 能力查询、Feature 创建或生成帧；NGX 内部具体使用哪种枚举／文件查询系统调用，也未做进程跟踪确认。不能把本轮探针当作产品已修复。

前一轮 18 项打包测试只验证文件与哈希，策略测试只验证选路和倍率；它们没有覆盖“NGX 能否找到并创建 FG Feature”。需要补集成验证，不能继续用文件齐全作为运行就绪的验收。

## 实际失败记录

- 产品：`release/v0.6.10-beta7/Magpie-Experimental-x64`，manifest 构建提交 `744d78994023e99d82901ef1e3c9b85a3d3b5f02`。EXE、代理和两个 INI 的实际哈希均匹配该清单。不要使用早期整合报告中已经过时的构建提交替代此记录。
- 显卡：RTX 5070 Ti，PCI `10DE:2C05`，LUID `0:137cd`，物理 SM120。
- 会话：2026-10-11 **20:04:47–20:04:49**，DLSSFG 4×、AMDOF，1924×1177 输入、2560×1440 输出，SDR。
- 20:04:48.023：`DLSSFG proxy loaded`，托管 forward-PTX 配置与代理哈希正确。
- 20:04:49.436：NGX D3D12 Core 初始化成功。
- 20:04:49.463：`DLSS Frame Generation is unavailable (result=0xbad00004, seh=0x0)`，随后 MP-032。
- `realGroups=0 evaluations=0 generatedPublished=0`。尚未创建 FG Feature，也未到运动矢量处理和补帧输出阶段。
- 本地 SDK `nvsdk_ngx_defs.h:121` 将 `0xbad00004` 定义为 `NVSDK_NGX_Result_FAIL_FeatureNotFound`。此枚举名不是“某个文件必定丢失”的独立证明，需结合发现路径和探针判断。
- 失败后的产品代理缓存只有 `version.dll` 和 INI，无 loader/backend 日志；默认 `%LOCALAPPDATA%/DlssgSm86` bundle 目录不存在。
- 同一日志中的 **20:04:13 FSR3 motion SRV / 0x80070057** 属于前一个 FSR3FG 会话，是独立问题，不作为本次 DLSSFG 的原因。

本地证据保存在工作区 `reports/20261011-dlssfg-sm86-startup-diagnosis/`：`snapshot.json`、`dlssfg-session.log`、`loader_probe.py`、各 case 的 `result.json`／JSONL，以及 `probe-summary.json`。这些材料位于仓库外，不随 Git 发布第三方二进制或用户日志。

## 独立加载探针结果

在三个独立 Python 子进程调用标准 Windows `LoadLibraryExW`／`GetFileAttributesExW`；没有调用未公开 ABI，没有创建 D3D 设备、NGX Feature 或执行 Evaluate。测试副本与 bundle/log 输出均隔离在上述报告目录，未改动产品配置、产品目录、当前 Magpie 进程或其默认缓存。

| Case | 实际观察 |
| --- | --- |
| 仅加载代理 | 代理路径和导出存在；没有生成社区日志或 bundle；缺失 FG 路径的属性查询仍返回 `ERROR_FILE_NOT_FOUND` |
| 加载代理后显式请求不存在的完整 `nvngx_dlssg.dll` 路径 | 成功重定向到新展开 bundle 的 NVIDIA DLL；`backend_install.status=0`；后端 `install.active=true` |
| 加载代理后显式请求存在的配套 310.9.1 DLL | 同样成功重定向，安装状态 0，后端 active |

后两种 case 的后端记录了：GPU RTX 5070 Ti、`actual_sm=120`、`selected_luid=0x137cd`、`image=ptx_sm86`、`optimized_tier=1`、`runtime_profile=310.9.1.0`。探针报告显示驱动 **616.92**、HAGS **on**。这说明本机候选路由可以完成后端安装，不能据此宣称 Magpie 已生成帧、内核已执行或有性能收益。退出日志明确 `features=0`、`evaluates=0`。

loader 配置记录 `requested_max=3`。后端安装记录中的 `max_generated_frames=5` 是后端自身能力字段，不能单凭这一字段认定喜鹊改成了 6×；仍须验证 NGX 对外能力与喜鹊 4× 上限。

## 相关代码缺口

| 位置（以 Beta7 `744d7899` 为准） | 问题 |
| --- | --- |
| `src/Magpie.Core/DlssgSm86Runtime.cpp:136–154` | 仅映射 `version.dll`，打印 loaded 后返回 true；没有请求 FG DLL，没有取得 bundle 目录 |
| `src/Magpie.Core/DlssgSm86Runtime.cpp:75–78` | 后续调用依据非空代理句柄直接成功，无法区别 proxy mapped 与 runtime ready |
| `src/Magpie.Core/NgxD3D12Core.cpp:123–127` | 仅将应用目录交给 `PathListInfo`，没有实际 bundle 目录；官方 SDK 字段用于额外 Feature DLL 搜索目录 |
| `src/Magpie.Core/DLSSFrameGenerator.cpp:510–549` | Available 为零先退出，后置的 `VerifyLoadedPair` 根本不会执行，无法在当前失败中给出配套库缺失的阶段诊断 |
| `scripts/Copy-DlssgSm86Runtime.ps1:14–15` | 删除根目录官方 FG 文件，但没有以另一条可发现路径补齐其作用 |
| `scripts/Test-DlssgSm86Package.ps1:6–10` | 把“根目录 FG 文件不存在”作为通过条件，未验证新的 NGX 发现机制 |

## 推荐修复方案

继续保留单一 DLSSFG 入口、用户无需选择库、`Optimized=1` 和最高 4×。不通过强改 Available、降低倍率、关闭光流或全开架构伪装来绕过此启动问题。

推荐初始化顺序：

1. 核对实际渲染适配器，校验并加载托管代理。
2. 在首次 NGX Init／能力查询前，主动发起一次可被代理识别的 `nvngx_dlssg.dll` 加载请求。独立探针已证明绝对路径的缺失文件请求也可触发 Bundled 重定向；正式实现须定义私有触发路径，并拒绝它被意外原生文件占用，避免失败时静默落回原生库。
3. 从返回的模块句柄读取真实加载路径，核对配套运行库与后端的哈希、模块身份及重定向结果；保留句柄至进程退出，不猜测 bundle ID 或路径，不调用未知 ABI。
4. 将实际运行库父目录加入 `NVSDK_NGX_FeatureCommonInfo::PathListInfo`，随后保留应用目录供 SR／NR 等其他 Feature 使用。使 NGX 在自身查找阶段能看到真实 DLL。
5. 再进行 NGX Init、Available／MFG 能力查询、Feature 创建和首帧 Evaluate。每一步记录独立状态，不能再将代理句柄非空作为整个准备过程成功。

如果 NGX 在后续验证中仍不能接受动态展开目录，备用方案是把同哈希的配套 310.9.1 DLL 部署至受管理的 Feature 目录并注册该路径，再验证代理重定向。不要把旧 310.7 随意拷回当前包，也不要混用不同版本后端。正式采用哪一种由直接 NGX 原型结果决定。

## 修复 TODO

### P0 启动闭环

- [x] FIX-01 按用户后续指令改为从原 `feature/dlssfg-sm86-unified`／`7eb2a79d` 继续。保留 Beta7 实际包与会话证据；已核对四个相关 C++ 实现的修复前内容一致，后续只整合修复增量，不用旧分支文件树覆盖最新产品。
- [x] FIX-02 源码：明确准备阶段和失败状态；BundleVerified 才返回成功，NGX 目录注册、Available、FeatureCreated 分别记录，不能由代理句柄替代。实际可发现性仍由 NGX 验收。
- [x] FIX-03 源码：NGX 前显式请求私有缺失 FG 路径，核对实际重定向、单一 bundle 配对和哈希，保留模块／文件句柄；拒绝请求失败、路径被占用、预加载和异配模块。四种上游 loader 场景通过，产品代码待编译。
- [x] FIX-04 源码：共享 NgxFeatureDiscovery 先注册真实 bundle 目录再保留应用目录，同时接入 D3D11 SR／D3D12 Core；路径对象持续到会话结束。SR／NR 与 FG 的实机切换留在 FIX-13。
- [x] FIX-05 源码：配套检查提前到 NGX 初始化之前，初始化与能力块查询之后再次验证；保持 Available、Feature 创建和资源门禁。
- [ ] FIX-06 在本机 5070 Ti 的独立产品副本完成冷缓存／热缓存 NGX 原型：实际重定向 → 后端安装成功／active → Available → Feature 创建 → Evaluate → 插值未禁用 → 有效生成纹理发布与呈现。只通过加载阶段不得标为完成。

### P1 诊断和失败处理

- [x] FIX-07 源码：两种 NGX 初始化共用官方 LoggingInfo 回调，限制每进程 512 条／每条 2048 字符，保留 NGX 自有日志。记录搜索路径及各查询结果／SEH；若实测仍失败再做进程跟踪。
- [x] FIX-08 源码：准备／发现初始化／能力／Feature 创建均有独立错误详情，Evaluate 保留独立错误。三语 MP-032 摘要改为按失败阶段处理运行库、完整安装与重启；实际 UI 待验证。
- [x] FIX-09 源码：准备失败保存原因，同进程不重新尝试加载；代理、运行库和后端保留至退出。共享 Core 未注册 FG 目录时不能接收 FG 消费者；NGX 异常后的路径对象随设备一起保留。反复启停留在 FIX-13。
- [x] FIX-10 源码／脚本：固定四文件和内嵌载荷身份不变，明确 bundle 注册才是发现入口；保留混装拒绝及更新事务。包脚本在 PS5.1／7 各 18 项通过；更新实际运行仍需后续编译会话验收。

### P2 回归与验收

- [ ] FIX-11 部分完成：可复用 Python loader 探针涵盖原三场景及缺失请求热缓存，中文空格目录、实际 bundle 目录归属、加载前后路径状态、读取锁、模块固定和配套哈希；4 子进程通过。仍需产品级只读、损坏／占用缓存、原始路径被占用、预加载／异配模块的拒绝和恢复测试。
- [ ] FIX-12 后续编译会话补真实 NGX 能力／Feature 测试。必须包含“哈希全正确但 FeatureNotFound”的失败断言；现有 loader 探针未调用 NGX，不能替代。
- [ ] FIX-13 在 5070 Ti 分别验证 2×／3×／4×、None／AMDOF／NVOF，以及首次启动、启停、Resize、SR／NR→FG、FG→SR／NR、换卡后重启；保留帧身份和队列检查。
- [ ] FIX-14 将初始化闭环推广到 RTX 20／30／40 代表硬件，逐卡记录实际 LUID／路由、驱动、HAGS 与执行证据；不得用本次 SM120 后端安装结果代替全部架构。
- [ ] FIX-15 启动与实际生成通过后，再执行 SDR/HDR、画质、延迟和同版本性能对照；继续限制最高 4×。同步原始 TODO、开发交接和发布状态。

完成判据：冷启动和重复启动均能发现正确配套库并完成实际补帧链；不依靠手放 DLL／选库；失败时有准确诊断且无静默原生回落；原配置沿用；原始硬件／性能验收仍逐项保留。

## 参考

- [原始接入 TODO](20261011-issue37-dlssfg-sm86-unified-TODO.md)
- [上一轮源码交接](../reviews/20261011-issue37-dlssfg-sm86-implementation.md)
- [上游固定提交配置说明](https://github.com/sdli1995/dlssg_for_sm86/blob/9621db573e07ed54f50c15bbb585ed9a7bdfac28/docs/INSTALL.en.md)：确认 Bundled 重定向与日志语义；本轮实际触发时序以独立探针为证。
