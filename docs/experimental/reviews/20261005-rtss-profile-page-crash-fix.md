# RTSS 配置页闪退定位与修复

日期：2026-10-05（Asia/Shanghai）。修复分支：`feature/069-rtss-target-frame-rate`，修复前提交 `f5f534c2`。

## 症状与证据

用户反馈本地 0.6.10 Beta3 可以启动，点击具体配置文件后立即退出。对应发布分支 `release/0.6.10-beta3`，构建提交 `87f399d23a4cdfad12a217ad13033142716315ac`。

- 产品日志两次记录：14:04:40.059 和 14:04:44.688，`App.cpp` 的 XAML 未处理异常处理器收到 `HRESULT 0x80073B1F`，详情“未找到 ResourceMap”。
- Application 事件 1000 的路径均为 `release/v0.6.10-beta3/Magpie-Experimental-x64/Magpie.exe`，故障模块 `Windows.UI.Xaml.dll`，最终异常 `0xc000027b`。
- WER 转储 `Magpie.exe.15040.dmp` 和 `Magpie.exe.45296.dmp` 与这两次退出对应。使用 Beta3 的本地符号读取后，第二份转储显示 XAML 经 `RoFailFastWithErrorContextInternal2` 终止进程；小型转储没有保存 stowed exception 所指的堆内容，无法直接从该转储还原四个 getter 的原始栈。
- 已编译 `resources.pri` 的资源表是 `Magpie/Resources`。四个新增动态文字 getter 却调用无参数 `ResourceLoader::GetForCurrentView()`，请求默认资源表。原有页面文字读取显式传入 `CommonSharedConstants::APP_RESOURCE_MAP_ID`。

四个 getter 是 `RtssStatusText`、`RtssSyncText`、`RtssLaunchText` 和 `AfterburnerLaunchText`。配置页的 OneWay 绑定在加载或状态刷新时读取它们，缺失资源表异常传回 XAML。该路径在“不管理 RTSS 配置”时也存在，与实际写入 RTSS 配置无关。

根因依据是产品日志、实际 PRI 的表结构、四处错误调用及原生资源读取复现的相同 HRESULT；没有把最终 XAML fail-fast 栈当作 getter 原始调用栈。

## 修复

[ProfileViewModel.cpp](../../../src/Magpie/ProfileViewModel.cpp) 中四处调用均显式传入 `CommonSharedConstants::APP_RESOURCE_MAP_ID`，与应用其他资源读取入口一致。沿用现有状态、同步、启动菜单文字和语言资源，不吞掉异常来掩盖资源表选择错误。

新增的源码合同逐项约束这四个 getter 的资源表参数。[原生测试](../../../tests/RtssResourceTextTests.cpp)执行从生产源码提取的四个 getter，使用发布包真实 PRI；状态与 Profile 输入采用独立夹具，不运行 RTSS 服务、目标进程或配置写入。

原生测试是无界面的资源回归：一个明确的工厂适配器保留生产代码的 overload 和 map 参数，将 `GetForCurrentView` 转到 WinRT 的 `GetForViewIndependentUse`。因此验证的是实际 PRI 解析、资源表选择与文字分支，不是原生配置页导航或完整 ViewModel 生命周期。尝试使用独立 XAML Application 的初始夹具因缺少产品元数据而失败，最终回归不依赖该夹具。

## 验证结果

- 旧无参数工厂读取同一 PRI，复现 `0x80073B1F`。
- 修复后提取的四个 getter：325 项原生资源检查通过，覆盖 17 种状态、三种管理范围、同步与启动文字、41 个动态资源键，英文/简体/繁体；编译 `/W4 /WX`。
- `Run-RtssTests.ps1` 的无需编译检查通过：209 项 RTSS 源码/UI/资源合同、20 份语言资源、原有统一刷新合同及显卡选择合同。
- `git diff --check` 通过；整个 `src/Magpie` 不再存在无参数 `ResourceLoader::GetForCurrentView()` 或 `GetForViewIndependentUse()` 调用。
- 本轮仅编译独立回归程序，没有编译、打包、部署产品，没有合并发布分支，没有改动用户配置或 RTSS 配置。
- Beta3 EXE SHA-256 仍为 `25A5BB7D3A3AFEA535FFAE48879D0DBB332BE74545681C0E678CDC5865737995`；PRI SHA-256 为 `33FCFF3578578F7DA9F5E989BF5DDB660D037DE2A00AE1B2D1033695F048044A`。发布分支仍停在原提交，工作树干净。

本地诊断证据目录：`C:/Users/81443/Documents/Magpie enhance/reports/rtss-profile-crash-20261005`，包含日志摘取、转储分析、原生回归日志和源码检查日志。

## 编译会话交接

将本次功能分支修复提交合入 Beta3 或后续发布分支，然后重新完整构建。原 Beta3 已含 `f5f534c2`，本次是其上的后续修复。

可在编译后使用新回归入口（路径替换为对应发布包与生成目录）：

```powershell
./tests/Run-RtssResourceTests.ps1 -RuntimeDirectory <package-directory> -GeneratedDirectory <obj/Release/Magpie/Generated-Files> -OutputDirectory <diagnostic-directory>
```

产品验收需要实际打开默认配置、普通应用配置和打包应用配置，反复切换页面，检查 RTSS 状态与启动菜单文字，并确认没有新的未处理异常。随后继续原功能文档中的 RTSS 写入/恢复与真实目标帧率验收；本次资源回归不替代这些验收。
