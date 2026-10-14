# VFG 性能与运行库精简：源码交接

2026-10-09，Asia/Shanghai。开发分支 `feature/nvidia-vfx-optimization`，工作目录 `C:\Users\81443\Documents\Magpie enhance\worktrees\nvidia-vfx-optimization`，基于 Beta5 后续修复 `7b10961c7d82f37e38160af94d60ca4e36561293`。继续刚创建的开发分支，没有重新创建另一条分支。此前的功能接入提交 `999e94e2` 已在基线中。

本阶段只完成源码候选与非编译验证，没有编译 C++／HLSL，没有生成 ZIP、创建／部署 release，没有启动 Magpie 或对修改后的产品进行 GPU／画质／性能验收。既有 Beta5、旧恢复包及完整 SDK 保留。此前 26 项直接 SDK GPU 依赖用例属于依赖调查，不是本次 C++ 修改的执行证明。

## 已写入的源码

- 比较着色器按线程组归约后才写全局标志；边缘线程通过两次屏障，不提前退出。继续比较全部四通道的完全相等结果。比较用 SRV 与对应纹理一起缓存，纹理更换时创建候选并替换。
- VFG 上一帧／当前帧的拥有描述符保持原地址，只轮换缓冲角色。每对帧重新绑定 A／B；成功消费当前真实帧后提交轮换，移除原先的 B→历史整图复制。首帧、场切旁路、错误回退和历史 revision 合同保留。
- VFG／图像效果的 CUDA 输入匹配 RGBA／BGRA，SDR 输出匹配输出编码；TrueHDR 仍输出 RGB10A2／P32，再显式解码到既有 FP16/scRGB 路线。
- 详细计时使用原 `EnableNativeBackendTiming` 开关。CUDA 事件与 D3D11 查询使用固定池、非阻塞轮询；池满跳过计时样本，记录跳过数。记录导入／SDK／导出 API 墙钟时间、CUDA GPU span、显式同步等待、比较与读回、后处理 GPU span、发布／环队列／互斥等待，以及前端接受呈现的间隔。每 120 个新捕获输入汇总；重复的非新捕获调用不计作新组。
- CPU API 时间包含等待，与 GPU span 重叠，不相加为总耗时。CUDA／D3D span 可能含调度与空闲，不能标成隔离内核耗时。前端日志记录成功接受的应用呈现，不能当作扫描输出或端到端延迟。延迟验收仍需已有 FrameTrace／呈现工具和真实显示器。

## 两个默认关闭的验证路线

`EnableNvidiaVfxAsync=false`，`EnableNvidiaVfxCudaHandoff=false`。它们只改变开发构建路径，不新增用户控件。

- Async 路线使用 `NvVFX_Run(..., 1)`，同一 stream 排序 map／transfer／unmap，并保留资源重新绑定、参数事务、销毁／重建的 drain；事件检测既有异步失败。默认路线保留同步 Run 与传输后的 CPU 等待。后续必须在 D3D11 下验证 SDK 包装层的排序和失败传播，才评估启用。
- CUDA 直连仅用于第一项后置原生 TrueHDR，且实际 SDR 来源的尺寸／编码与已加载 HDR 输入一致、无需 bounded 映射。编码或路线不匹配时选择原通用 D3D 路径，避免改变模型加载后的编码合同。
- 生产者 ready 事件使 TrueHDR 等待 VFG；消费者完成读取后反向事件使 VFG 等待 TrueHDR，防止共享生成缓冲被覆盖。SDK 失败也建立反向依赖，并恢复 TrueHDR 的拥有输入绑定。第一张输出的精确场切比较仍导出 D3D；其他兼容输出可直接交给 CUDA 消费者。每张真实帧与生成帧仍经过 TrueHDR 和既有 canonical 颜色转换。
- 两种路线及它们的组合尚未编译／执行，保持关闭。先完成默认路线回归与计时基线，再分别比较 Async、直连、两者同时启用；不能用源码检查作为启用依据。

F17 的过期生成帧省算仍待运行数据决定。本阶段没有自动跳帧、降低倍率／模型或扩容四槽队列。

## 精简分发

`src/NvidiaVfxRuntime.props` 是统一套件的唯一 DLL 白名单：匹配的四个 Core DLL 加六个功能 DLL，共十个，含 SHA-256；另列十七个排除 Core 名称及四份许可证／第三方通知的哈希。四项功能全部保留。

默认 `NvidiaVfxRuntimeLayout=Minimal`。`Full` 用于私有完整 Core 验证；无统一套件的旧回退构建继续使用其原完整依赖路径。公开最小清单仅代表当前四项 1.3.0.0 套件，增加其他 SDK 效果时须重新审查，不复用为整个 SDK 的通用依赖表。

- `Validate-NvidiaVfxRuntime.ps1` 在复制前核对 x64、十个 SHA-256、NVIDIA 签名、许可证同名内容；Minimal 输出有十七个 Core 残留时拒绝并要求干净候选目录，不删除文件。复制后再次校验，生成不含本地 SDK 绝对路径的 `nvidia-vfx-runtime-manifest.json`。
- `Test-NvidiaVfxPackage.ps1` 在封包前核对十个组件、固定许可证、布局及输出已验证状态；`Build-Release.ps1` 的 `-SkipBuild` 也经过该检查，防止把原来的全 Core 输出直接当成精简包。
- 保留 `ThirdPartyLicenses.txt` 和官方三份 PDF。空旧许可目录不会通配到驱动器根目录。源码没有改变 SDK 二进制，没有保存 NGC key，没有新增组件在线下载器。
- 约 487 MB 是此前 ZIP 条目的测算。本次增加第三方通知／运行库 manifest、修改后的程序二进制以及实际 ZIP 元数据会改变精确大小；实际成包后另行核对，不宣称已有 487 MB 可用包。

## 本阶段验证

- `Validate-NvidiaVfxSource.py`：91 项 SDK／工程／资源／TrueHDR 合同静态检查。
- `Validate-NvidiaVfxOptimization.py`：61 项结构、白名单、哈希、默认开关、资源与排序约束检查。
- `Run-NvidiaVfxRuntimeManifestTests.ps1`：7 项文件完整性用例，包括有效最小清单、完整 Core 残留、缺失／损坏组件、缺失第三方通知及恢复后再次通过。仅操作报告目录中的组件夹具，不运行 DLL、不包含 Magpie 程序。
- Windows PowerShell 5.1 实际运行最小源清单验证通过；四份 PowerShell 脚本解析通过。最小十个 DLL 总原始字节数为 275,483,232。
- `git diff --check` 通过。本阶段未运行需要编译的策略／WARP／C++ 回归，也未编译比较着色器。

证据目录：`C:\Users\81443\Documents\Magpie enhance\reports\nvidia-vfx-optimization-source-20261009`。本次源清单与组件夹具测试不需要或读取用户 NGC 凭据。

## 后续编译会话

1. 从本分支本地提交开始，先核对最新产品修复是否已包含。显式设置 `EnableNvidiaVfx=true`、准确的 `VFXSdkDir`；清除旧 Include／Proxy／Runtime 覆盖，或使其全部指向 1.3.0.0。不要因使用新工作树而假定未跟踪的 `.user` 设置已被复制。
2. 默认候选先关闭 Async／CudaHandoff，使用 Minimal；单独诊断构建启用 `EnableNativeBackendTiming=true` 和按需 FrameTrace。编译默认、无统一套件回退及两条验证路线／组合；执行已有 VFX 策略、R1、HDR 精确参数、保存迁移、FG／呈现和颜色回归。
3. 测比较边缘尺寸、RGBA／BGRA、首帧／场切／静态／重复、失败后的缓冲角色、2–8×、模型／参数／捕获 revision、切屏／重建／停止。重点确认缓冲轮换和 CUDA 借用未改变 A／B、输出顺序、alpha 或 101／1003 参数。
4. 以相同素材测默认路线的 1080p／1440p／4K，再逐项开启验证开关。检查事件缺失、异步错误、候选加载失败、三次 VFG 失败、队列背压和退出；GPU 查询丢样不得使内容等待。未通过前保持实验开关关闭。
5. 在干净目录运行完整 Magpie 精简依赖验收，确认真实加载路径及其他支持 GPU／驱动、多 GPU。既有其他 SDK 功能也要回归；若出现额外动态依赖，按实际证据增加清单并重算体积。
6. 得到呈现／延迟／预算数据后再决定 F17，不先改固定倍率或以队列深度代替算法优化。通过后仅按维护者后续授权创建／部署新 release，保留当前 Beta5 完整包作为回退。

工具依据：[SDK 同步／异步执行](https://docs.nvidia.com/maxine/vfx/1.3.0/API/Architecture/RunaVideoEffectFilter.html)、[CUDA 事件](https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__EVENT.html)、[CUDA／图形互操作](https://docs.nvidia.com/cuda/cuda-runtime-api/cuda_runtime_api/group__CUDART__INTEROP.html)。
