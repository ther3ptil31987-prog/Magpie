# Beta7 FSR3FG 运行错误修复与名称统一交接

日期：2026-10-11。状态：**源码修改和静态验证完成；未编译、未部署、未运行新二进制**。

## 基线和范围

- 基线：最新 Beta7 `744d78994023e99d82901ef1e3c9b85a3d3b5f02`，与当前部署 `build-manifest.json` 一致。
- 修复分支：`fix/beta7-fsr3fg-motion-and-names`。
- 工作目录：`C:\Users\81443\Documents\Magpie enhance\worktrees\beta7-fsr3fg-motion-and-names`。
- 原始证据、完整 TODO：[运行失败和名称 TODO](../todos/20261011-fsr3fg-runtime-failure-and-naming-TODO.md)。
- 没有修改 SDK、效果 ID、HLSL 标记路径、倍率、参数默认值、捕获时间策略、AMD/XeSS DLL 或已部署包；没有推送、发布或回复 GitHub。

## 改动

2026-10-11 20:04:13 的 Beta7 日志在第一张有效 AMDOF 运动输入上报告 `create source motion SRV / 0x80070057`。Renderer 创建稳定运动副本时将 `BindFlags` 清零，FSR 随后要创建 SRV；此前首帧仅走 reset 复制，未触发该路径。

1. `FrontendMotionTexture.h` 是 Renderer 实际调用的稳定纹理创建函数。FSR 副本使用 `D3D11_BIND_SHADER_RESOURCE`，XeSS 仍为 copy-only。绑定、尺寸、格式、Mip/Array、采样和使用属性变化时重建，成功后才替换旧纹理。复制和元数据读取仍在原 keyed-mutex 事务内。
2. `FSR3FGMotion.h` 复用原始 HLSL 和 dispatch，未改变向量映射算法。消费者验证单 mip/单 array、R16G16_FLOAT、单采样、SRV 绑定以及所属设备的 COM 身份。缺失/过期运动仍走 reset；匹配帧上的资源契约错误停止缩放并报告实际 desc。创建 SRV 成功后才更新缓存。
3. `PresenterFailure` 保存首个阶段/错误码。Renderer 在异步报告前复制错误，避免依赖已销毁 Presenter。AMD result 放入上下文、系统错误字段用 E_FAIL；清理失败仍输出日志，不覆盖首因。无 FSR SDK 构建也实现了诊断接口的空返回。
4. Catalog 审阅源、生成的中英文目录和 `EffectHelper` 同步为 `DLSS FrameGeneration` / `XeSS FrameGeneration` / `FSR3 FrameGeneration`。添加面板、条目、组合名、FG 冲突提示沿用现有 helper/catalog 路由。旧搜索词保留，新名称加入搜索；实验性、2×、SDR 继续出现在摘要/说明。`Nvidia VFX VFG` 保持原名。

## 本轮验证

| 检查 | 结果 |
| --- | --- |
| `tests/validate_fsr3fg_source.py --sdk-directory ...` | 130 项通过；只读接线/SDK 符号检查 |
| `tests/Validate-EffectCatalog.ps1` | 165 个效果器通过，包含生成器 `-Check`、FG 名称/搜索词/helper 一致性 |
| `tests/test_fsr3fg_package.py --sdk-directory ...` | 3 个夹具通过：完整包接受、缺失 provider 拒绝、错误 provider 拒绝；不加载 DLL |
| PowerShell AST 解析新 runner | 通过，未执行 runner |
| 与 Beta7 对照 | 原 motion HLSL 字符串完全一致；catalog ID/顺序/非名称字段未变；FG 参数、标记与历史策略未变 |
| `git diff --check` | 通过 |

本轮没有调用 C++/HLSL 编译器。新增原生测试仅完成源码编写及人工审阅，不能计为通过。

## 后续编译会话

在本修复分支中，用已配置 Windows SDK 的 **x64 VS 开发者 PowerShell** 执行：

```powershell
pwsh -NoProfile -File tests/Run-FSR3FGMotionResourceTests.ps1
pwsh -NoProfile -File tests/Run-FSR3FGPolicyTests.ps1
```

第一项会编译独立资源测试并在 WARP 上运行，也会编译生产 HLSL；本轮未执行。它实际运行 `CreateTexture2D → CopyResource → CreateShaderResourceView → remap → staging readback`，验证负例、重建/缓存、跨设备/布局拒绝、数值缩放/方向、非有限输入和边界。若机器缺少 D3D11 debug layer，会显式标明该项跳过，不能将跳过记录为通过。WARP 仍不包含 FSR SDK/实际显示验收。

更新的 `EffectPickerTests.cpp` 也需在后续会话编译执行；使用现有测试入口并显式提供本次构建的 CoreLibrary，避免采用历史 runner 默认旧包路径。需分别覆盖 `EnableFSR3FrameGeneration=true/false` 的构建，确认新头文件和虚函数接线有效。

实机首先回归本次失败条件：PotPlayer / WGC / AMDOF / 1924×1177 → 2560×1440 / RTX 5070 Ti，持续至少 60 秒、越过首个有效 motion。检查调试层、错误详情、reset 原因和非 reset 插值调度，并用实际画面/呈现事件确认输出。然后覆盖 None、支持硬件上的 NVOF、尺寸/黑边、无原生时间戳捕获、连续启停/Alt-Tab，以及 XeSS 外部运动和其他 FG。

新增测试和静态审计不能替代这些实机步骤。此前另一轮 DLSSFG 的 `0xbad00004` 属独立问题，本修复没有声称解决它。
