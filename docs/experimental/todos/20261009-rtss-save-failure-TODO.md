# RTSS 配置保存失败诊断与修复 TODO

日期：2026-10-09。分支：`feature/069-rtss-target-frame-rate`，诊断基线 `500c0ff2`，修复基线 `58edd39c`。源码修复、事务回归与本机隔离原生验证已完成；产品编译部署交由独立会话，用户现有 RTSS 配置未修改。

已确认当前失败发生在保存前：Magpie 无条件执行 `SetProfileProperty("FrametimeLimit", 0)`，本机 RTSS 7.3.5.28314 拒绝这个值，返回 FALSE。Apply 因而提前退出，未调用 SaveProfile。正常 FPS 模式的无效设置已修复，Restore 使用相同规则。实施结果见[修复与交接](../reviews/20261009-rtss-save-failure-fix.md)。

## 已确认的原因

用户确认“修改全局配置”和“修改目标窗口配置”都失败，两条路径共用 [Apply](../../../src/Magpie/RtssProfileAccess.cpp)。已部署 Beta4 的日志在 11:03:15–11:03:38 记录 5 次 `RTSS write: SetPropertyFailed`；其构建提交为 `f6185c0e`，EXE 哈希与清单一致，该版本的 RTSS 配置访问代码与功能分支一致。

Apply 的实际顺序是设置帧率分母为 1、设置帧时间为 0、设置整数帧率；第二步失败后短路返回。此时 `saveAttempted=false`，保存与重载尚未发生。因此界面中的“保存配置失败”没有准确表达此次失败阶段。

对本机 DLL 进行了原生检查：仅将读取到的当前值原样传回 setter，不调用 SaveProfile、UpdateProfiles、DeleteProfile 或 EnumProfiles。Global、已有 `Magpie.exe` 配置、缺失目标的 Global 回退得到相同结果：

| 属性 | 当前值 | Get 返回 | Set 当前值返回 |
| --- | --- | --- | --- |
| FramerateLimitDenominator | 1 | TRUE | TRUE |
| FrametimeLimit | 0 | TRUE | **FALSE** |
| FramerateLimit | 0 | TRUE | TRUE |
| AppDetectionLevel | 3 | TRUE | TRUE |

三种上下文共 12 组检查。Profiles 下的 Config、Global、Magpie.exe.cfg 在检查前后哈希一致，缺失目标未被创建。

离线核对同一 RTSSHooks64.dll 的 setter 分支确认：FrametimeLimit 是已识别的属性；该版本设置时检查 `value >= 1000 * timeDenominator`，当前分母为 1，0 被拒绝。getter 仍能读出表示未启用帧时间限制的 0。**可读值不等于 setter 接受的值**，本次不是属性名不存在。这个边界结论仅针对已检查的本机版本。

Restore 也无条件设置 `FrametimeLimit=original.rate[2]`。正常 FPS 配置的原值为 0，因此只修复 Apply 后，还会在恢复阶段遇到同一问题。该恢复缺陷在诊断阶段由源码与同一 setter 行为确定；修复阶段已完成隔离真实保存与恢复，见下方结果。

本机证据保存在工作区 `reports/rtss-save-diagnosis-20261009/`：`evidence.json`、`product-failures.txt`、`same-value-probe.json` 和离线 Get/Set 反汇编。依据另包括已安装 RTSS 官方 SDK 的 `RTSSProfileInterface.h` 与帮助 `TEXT_FRAMERATE_LIMIT`。

## 首要修复方案

设置属性前检查已加载的当前值。当前值已等于目标值时保持原值，跳过 setter；确实需要改变时才调用 setter，并保留返回值及读回校验。Apply 和 Restore 共用这个规则。

正常 FPS 模式下，帧时间本来就是 0，保持它即可。整数帧率和分母仍按需求设置；新目标仍从 Global 创建并设置 Low，已有配置保留检测级别。保存后的 API、磁盘读回和冲突检查继续执行。

Restore 时，当前帧时间和原帧时间均为 0，也跳过 setter；帧率、分母和完整原表达仍按快照恢复。原策略保持一次有界尝试，外部修改保护和新建配置删除条件保持现有规则。

不能把修复写成“忽略所有 Set 的 FALSE”，也不能只删掉帧时间验证。需要改变非零帧时间时仍有真实的模式转换问题，必须采用下方兼容方案；诊断阶段未验证完整保存；修复阶段的隔离结果见下方。

## 非零帧时间配置的兼容方案

当已有配置或 Global 采用非零 LimitTime 时，设置 0 仍会被当前接口拒绝，单纯跳过当前值检查无法解决。

优先在隔离配置中验证 RTSS 的模式优先级：如果正的 FramerateLimit 能明确启用 FPS 模式，同时允许保留原 LimitTime，可保留该字段并按实际模式校验及显示状态。该方案必须有 API、落盘、重载和实际模式证据，不能仅凭帧率字段读回成功采用。

若公开接口无法完成转换，再设计明确的配置文件兼容路径：只修改目标文件的限帧字段，保留其余内容、原精度和继承关系，写入前核对快照，并验证保存、重载、恢复及竞争。此路径改变[原设计](../design/20261005-v0.6.9-rtss-target-frame-rate-plan.md)中的接口接入策略，应作为独立改动说明；不在任意 API 失败后自动转为整份文件覆盖。

在兼容方案验证完成前，非零帧时间转换应给出具体失败原因，不把未转换的状态标记为整数 FPS 已成功应用。正常 FPS 模式的修复已完成。非零模式的验证与字段级兼容设计见[兼容方案](../design/20261009-rtss-time-mode-compatibility-plan.md)，文件路径实现仍为后续 C01。

## 修复清单

- [x] **P01** 建立 Apply/Restore 共用的按当前值设置规则；相同值不调用 setter，不同值仍检查返回和读回。保留接口异常隔离，不把 native fault 当作普通参数失败。
- [x] **P02** 修复 Apply 的 `FrametimeLimit=0` 调用：正常 FPS 模式保留现有 0；整数目标、分母、新建 Low、已有配置非限帧字段及保存验证沿用现实现。
- [x] **P03** 修复 Restore 的同类调用，覆盖原本不限帧、整数和小数 FPS、保持原分母、已有配置恢复及新建配置删除；未能恢复时结束一次尝试。
- [x] **P04** 拆分设置、保存、核对、重载和恢复失败阶段。界面使用“设置帧率失败”“保存配置失败”“核对配置失败”“重载配置失败”等简短结果，保持最近的文案风格及 20 份资源一致。
- [x] **P05** 日志记录具体失败属性、请求值、读取值、配置对象、RTSS 版本、saveAttempted/readbackComplete 和接口异常；文件访问失败另外记录对应错误码。RTSS 的 BOOL 失败不依赖未声明有效的 GetLastError。现有对象的无写入权限探测覆盖 Global/目标创建，拒绝时显示权限结果。
- [x] **P06** 验证非零 LimitTime 的接口边界与原值恢复：本机 setter 无法清零，正 FPS 和非零帧时间可同时保存，不能据此认定模式优先级。逆向帧时间/分母恢复通过；独立字段级兼容方案及原功能文档已更新，前向转换文件实现留在 C01。
- [x] **P07** 增加实际 Apply/Restore 方法的事务回归，测试接口必须模拟“读 0 成功、Set FrametimeLimit=0 返回 FALSE”；覆盖 Global、已有目标、缺失目标、正常 FPS、真实 setter 拒绝、取消、部分保存和冲突，不能让 mock 恒定返回成功。
- [x] **P08** 在隔离 RTSS 配置目录完成完整原生事务：设置、SaveProfile、API/磁盘读回、重载、保留/恢复、目标创建/删除及并发保护。用户原有配置前后哈希一致；本机 7.3.5.28314 通过；后续支持版本的验证单列 C02。
- [x] **P09** 更新资源和源码合同、实施记录及交接；把“只读/设置/资源检查通过”与“真实保存恢复通过”分别记录。
- [ ] **P10** 修复提交后，由独立编译会话构建并核对两种管理范围、自定义/同步、停止策略及简短失败状态，再执行相应部署和目标应用验收。

P01–P09 已完成本轮源码、验证与兼容设计，P10 交给独立编译部署会话。验证结果：311 项源码/资源合同、176 项设置、54 项显示、216 项配置事务、64 项会话事务、55 项本机隔离原生事务、358 项原生资源读取通过。隔离测试前后，用户现有 Profiles 的 3 个文件哈希相同。实际源进程 FPS 和完整产品界面仍由 P10 验收。

## 后续兼容

- [ ] **C01** 按[独立兼容方案](../design/20261009-rtss-time-mode-compatibility-plan.md)实现并验证非零帧时间转 FPS 和原限帧字段缺失关系恢复。Magpie 初始化已完成 7.3.5.28314 的独立帧时间清零入口，见[实施记录](../reviews/20261009-rtss-status-magpie-init-and-admin.md)；本项的 Global/目标转正 FPS 和缺失关系恢复仍未完成。当前不在 API 失败后静默改文件；非零转换显示具体失败原因，SDK 补齐字段后的恢复不报告为原表达精确恢复。
- [ ] **C02** 对实际可取得的 RTSS 样本运行隔离能力与事务验证，记录实际支持的操作和字段，不建立版本准入白名单或最低版本门槛。已有原生结论覆盖 7.3.5.28314；能力拆分与门槛整改已按[新 TODO](20261009-rtss-capability-checks-TODO.md)完成第一阶段，见[实施记录](../reviews/20261009-rtss-capability-checks.md)；未取得的实际样本保留验证待补。

- [x] **C03** 按用户已确认的新方案，Magpie 主程序启动时默认请求管理员权限，复用既有管理员启动链路并覆盖默认值、旧配置迁移、取消授权、单实例与自启动。此项由[状态与 Magpie 初始化 TODO 的 A01–A04](20261009-rtss-status-magpie-init-and-admin-TODO.md)统筹，已完成源码/模拟启动与配置迁移，真实 UAC、自启动和单实例验收列于 V03；替代原先单独为 RTSS 按需提权的候选方案。保留真实文件权限失败状态，不自动改变目录 ACL。

关联：[原功能开发清单](20261005-v0.6.9-rtss-target-frame-rate-TODO.md)、[界面调整 TODO](20261009-rtss-ui-text-and-style-TODO.md)、[配置访问实现](../../../src/Magpie/RtssProfileAccess.cpp)、[会话失败映射](../../../src/Magpie/RtssService.cpp)。
