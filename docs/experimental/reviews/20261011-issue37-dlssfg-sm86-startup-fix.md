# DLSSFG sm86 启动修复源码交接

日期：2026-10-11。按用户要求继续 `feature/dlssfg-sm86-unified`，从 `7eb2a79d` 增量开发；没有创建新 release 分支、合并 Beta7、编译、部署或推送。修改后的 Magpie 尚未执行，以下是待编译验证的源码修复。

原故障在 Beta7 `744d78994023e99d82901ef1e3c9b85a3d3b5f02` 的 5070 Ti 会话中表现为 `FeatureNotFound / 0xbad00004`，发生在 Feature 创建之前。初版只加载 `version.dll`，NGX 又只搜索应用目录；当前修改主动加载内嵌 FG DLL，并注册它的实际目录。完整历史证据见[故障 TODO](../todos/20261011-issue37-dlssfg-sm86-startup-failure-TODO.md)。

## 实现变化

- `DlssgSm86Runtime` 区分 Unprepared、Preparing、ProxyMapped、BundleVerified、Failed。只有经过重定向、文件身份与模块配对检查才能返回准备成功；准备过程中失败会保存原因，后续同进程调用继续失败并提示重启。换渲染 LUID 仍要求重启，查询架构或不支持架构时不会加载代理。
- 在受管理代理缓存下新建本次私有空目录，确认 `nvngx_dlssg.dll` 不存在，调用绝对路径 `LoadLibraryExW` 触发 Bundled 重定向。返回路径必须离开触发目录，触发文件在加载后仍须不存在；不能依靠手放原生 DLL 作为入口。
- 验证实际运行库与后端都来自返回的 bundle 目录，文件名、数量与固定 SHA-256 正确；拒绝预先加载的 FG 代理／运行库／后端及后续重复或异配模块。对运行库／后端持有禁止写入／删除的读取句柄，保留运行库引用，并固定后端模块至进程退出。不调用未公开 ABI，不热卸载钩子。
- `NgxFeatureDiscovery` 统一为 D3D11 SR 和 D3D12 共享 Core 提供路径与官方日志回调。搜索顺序为实际 bundle 目录、应用目录；路径字符串、指针列表和 FeatureCommonInfo 在 NGX 会话期间保持有效。SR／NR 原有目录继续保留，FG 准备失败不被其他消费者初始化成功所掩盖。NGX 发生已标记异常或关闭失败时，路径对象与设备一起保留到进程退出。
- FG 在 NGX Init 前、Init 后及获取能力参数后验证配套模块，早于 Available 门禁。NGX 能力块、Available、FeatureInitResult、MFG 各自记录查询结果和 SEH；FeatureCreated 单独记录，明确 GPU 执行尚未确认。日志回调跨线程安全，每进程最多 512 条、每条最多 2048 字符，保留 NGX 自有日志输出。
- MP-032 沿用既有错误捕获和详情机制，三种语言的摘要改为按失败步骤处理运行库／完整安装／重启，移除通用的“关闭光流或降低倍率”建议。
- 打包清单、四个固定依赖文件与配置哈希不变。脚本说明明确根目录 DLL 不是入口，包检查不证明 NGX 可发现；补入 Beta7 已验证的 PowerShell 内置模块按当前宿主路径导入方式，便于独立编译会话使用。

原效果 ID、旧配置、默认 2×、最高 4×、MaxGeneratedFrames=3、Optimized=1 均未改变。未改变运动矢量、队列、HDR 或呈现算法，未强制 Available 为真。

`BundleVerified` 只证明实际映射文件身份及目录；不证明后端 active、选择了正确物理 GPU、优化内核执行或真实图像呈现。后续仍必须核对 loader/backend 日志和真实 NGX 执行，尤其不能把 RTX 40／50 的原生生成误记为社区路由验收通过。

## 验证记录

| 检查 | 结果与边界 |
| --- | --- |
| `Test-DlssgSm86Packaging.ps1` | PowerShell 7、Windows PowerShell 5.1 各 18 项通过；仅包和文件检查 |
| 修改的 PowerShell 语法 | 4 个脚本解析通过 |
| 工程与本地化 XML | Magpie.Core 项目、3 个 resw 解析通过 |
| `Probe-DlssgSm86Loader.py` | 最终 4 个独立子进程场景通过：proxy-only、缺失请求冷缓存／热缓存、已有配套 DLL 的请求；未调用 NGX |
| 上游 PE 资源 | 探针静态验证 30001／30002 内容与共享清单匹配 |
| 中文空格路径、文件锁与模块固定 | 最终探针通过，配套模块都落在预期私有缓存；仍非产品只读目录／中文用户配置验收 |
| 源码人工复查 | 检查 SDK 回调签名与 FeatureCommonInfo 字段、生命周期、无 SDK 分支、失败状态、门禁顺序及 4× 上限；不是 C++ 编译检查 |
| 产品与 C++ 测试 | 按用户范围未编译、未部署、未运行修改后的 Magpie |

证据目录位于工作区仓库外 `reports/20261011-dlssfg-sm86-startup-fix/loader-final/`，包含逐进程结果和上游日志。5070 Ti 的请求场景记录了 backend install status=0、active=true、SM120、PTX sm86；这些进程没有 NGX Feature 或 Evaluate。

第一次探针把含中文的私有 CacheDirectory 写为 UTF-8，发现上游按本地代码页读取后生成了乱码目录。最终探针改用带 BOM 的 UTF-16 INI，并增加“实际 bundle 必须位于预期私有目录”检查后重新通过。产品模板是 ASCII，CacheDirectory 保持空值，未修改其编码或哈希。最早 `loader/` 的结果不可用来证明中文缓存路径正确。

探针复跑示例（输出目录必须不存在，文件和日志保留）：

```powershell
python tests/Probe-DlssgSm86Loader.py --runtime-directory '<固定依赖目录>' --route forward --output '<新的私有报告目录>'
```

需要 Windows x64 Python 和 `pefile`；SM75／86 使用 `--route native`，SM89／120 使用 `forward`。该测试只执行固定上游 DLL 的加载，不编译 Magpie，也不操作当前产品进程或默认缓存。

## 下一会话的编译与运行验收

1. 在整合分支接入本分支的新修复提交，保留 Beta7 或后续版本的其他功能。与已诊断 Beta7 相比，四个相关 C++ 实现的修复前内容一致；资源文件和工程 XML 可能同时包含其他变化，合并时保留双方增量。不要把原分支的整棵 Beta6 文件树覆盖到新包。
2. 首先完整编译修改的 C++ 及无 DLSS SDK 变体，执行既有策略／更新事务测试。在独立产品副本测试，记录构建 SHA、EXE／依赖 SHA、GPU LUID、驱动、HAGS、配置及新进程 PID。
3. 使用隔离用户缓存或完整备份后的测试环境做冷／热缓存。按顺序确认 BundleVerified → NGX 搜索目录 → Init → Available → FeatureCreated → Evaluate → 插值未禁用 → 生成纹理发布 → 实际呈现。后端 active 与物理 LUID 必须和该进程、该渲染 GPU 对齐。仅加载成功不能关闭 FIX-06／12。
4. **负例必须失败**：文件哈希都正确但 NGX 返回 FeatureNotFound；只有代理没有配套后端；混装或异路径模块；请求路径意外被占用；缓存损坏／被占用／不可写。确认失败后重试保留原原因，重启后才重新准备，不把后续 SR／NR 初始化当成 FG 成功。
5. 若仍为 FeatureNotFound，先查看新增查询返回值及 NGX SDK 日志，必要时跟踪实际搜索路径。以同哈希 310.9.1 的受管理 Feature 目录注册作备用原型；不要强开 Available 或混入旧 310.7。
6. 5070 Ti 验证 2×／3×／4×、None／AMDOF／NVOF、启停／Resize、SR／NR 双向切换、换卡后重启；再进行 RTX 20／30／40、SDR／HDR、图像与性能验收。多 GPU 的实际后端选卡、只读用户缓存、并发进程及模块冲突尚未验证。

完整待办状态以[故障 TODO](../todos/20261011-issue37-dlssfg-sm86-startup-failure-TODO.md)为准。本次交付源码修复和可复用 loader 探针，不声明 Magpie 运行故障已经实机解决。
