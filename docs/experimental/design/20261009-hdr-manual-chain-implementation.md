# HDR 手动组装开发交接

日期：2026-10-09。分支：`feature/hdr-compatibility-update`。本次按[手动组装 TODO](../todos/20261009-hdr-manual-chain-adjustment-TODO.md)撤回效果组的 HDR 集成入口和自动组装，保留首轮参数、亮度、导出与 RTX 生命周期修复。源码和独立回归已完成，完整应用构建、原生界面与硬件验收交给后续会话。

## 当前行为

效果列表中的 HDR→SDR、SDR→HDR、RTX HDR 都是独立效果器。用户自行添加、排序、调参和移除；程序检查颜色域与配对，执行实际列表。普通效果的新增、移除、排序和缩放不会生成转换节点。

组级输入、输出、捕获入口及方法切换 API 已移除，能力表、自动分段规划、手动链接管、管理归属和跨配对参数同步也已移除。XAML 和 ImGui 显示各节点的有效参数；恢复配对的节点提示其上游 HDR→SDR 在列表中的序号。修改上游参数会刷新相关显示条件，不改写其他节点的参数。

SDR→HDR 的“自动”仍只决定该节点采用配对恢复还是原生映射。原生映射按 SDR 白色亮度等比例抬高整幅画面；1000/1500 是允许的用户选择，不等于只提高高光峰值。实际观感仍需要用户结合显示器和内容比较，程序不会因此切换 RTX HDR。

| 参数 | 新输入范围 | 普通步进 | Shift 粗调 |
| --- | --- | --- | --- |
| 原生 SDR 白色亮度 | 80–4000，默认 203 | 5／10／50 分段 | 50 |
| HDR→SDR 参考白 | 80–400 | 5 | 50 |
| HDR→SDR 配对峰值 | 400–4000 | 10 | 50 |
| 曝光、肩部 | 0.1–4 | 0.05 | 0.25 |
| RTX 峰值 | SDK 400–2000 | 10 | 50 |
| RTX 对比度、饱和度 | 0–200 | 5 | 10 |
| RTX 中间灰 | 10–100 | 1 | 5 |

保存精度、精确输入与交互刻度继续分离。默认值 203 和端点可达；旧 `whiteNits × exposure`、低于 80 的兼容值和原元数据保留。打开参数窗或保存其他设置不会把旧参数改写为新字段。

PNG 按实际纹理格式处理，HDR 转 SDR sRGB，DDS 保留实际格式；导出读取既有输出，不重复 RTX 推理。RTX 清理失败和异常继续记录阶段、保留故障资源与模块，并阻止不安全重初始化。

## 旧试用组迁移

迁移只读取旧 `hdr` 和 `hdrManagedRole`，校验实际颜色计划与归属后保留物化列表。节点顺序、普通效果、FG、缩放、参数、配对索引和呈现元数据均保持。管理归属不会进入新的保存格式，因此节点恢复正常手动入口。

第一次成功迁移通过现有 `AppSettings` 恢复路径，先保存完整配置的 `.recovery-v6-….original`，再原子写入结果。每个迁移组另存原始组 JSON，含未激活方法偏好：

```json
{
  "name": "原组名称",
  "effects": [],
  "hdrManualMigration": {
    "version": 1,
    "original": "旧组完整 JSON 字符串",
    "captureHdr": true
  }
}
```

这里的空列表只是展示字段位置。实现不会清空或重建实际列表。`captureHdr` 是可选的内部兼容字段：显式转换节点已能给出同一执行合同的组不保存它；无转换节点的直接 HDR 组保留 `true`，原来显式执行 SDR 的组必要时保留 `false`。没有新增的用户入口，也不插入有损转换对。新建普通组没有这些字段，捕获域沿用首个显式转换节点决定的路径。

重载以当前手动列表为准，备份不会重新应用旧设置。只有读取旧组并首次成功展开时标记配置迁移，已迁移组不重复触发写入。未激活偏好只留在备份，不增加额外效果器。

损坏元数据、节点、参数、备份版本和无法确认的配对保留整个原组 JSON，显示恢复说明并阻止启动；导入失败不会替换正在使用的列表。通用配置修复不先裁剪这些 HDR 节点的参数。迁移组也避开旧 XeSS、RTX、DLSSNR 与光流默认值迁移，防止展开后普通节点被再次归一化；未参与本次迁移的普通组继续使用原有升级路径。

回滚单个迁移组可使用 `hdrManualMigration.original` 替换该组对象；回滚完整配置使用保存的 `.original` 文件。后续部署会话应先关闭应用并备份当前配置，再操作，避免运行中的异步保存覆盖恢复结果。

## 实现位置

| 文件 | 职责 |
| --- | --- |
| `src/Magpie/HdrManualMigration.h` | 只读旧 schema 校验、完整颜色计划对比、备份读取和最小捕获兼容；不生成节点 |
| `src/Magpie/ScalingModesService.cpp`、`ScalingMode.h` | 手动组读写、一次迁移标记、原组恢复、绕开其他升级改写 |
| `src/Magpie/ConfigRecovery.h`、`src/Magpie.Core/include/OpticalFlowDefaults.h` | 在 HDR 校验前保留原数据；避免已迁移组的普通参数被覆盖 |
| `ScalingModeItem`、`ScalingModeEffectItem`、`ScalingModesPage.xaml` | 撤回组 UI，恢复普通列表与编辑权限，损坏配置显示说明 |
| `EffectParametersViewModel.cpp`、`OverlayDrawer.cpp`、`EffectParameterLocalization.h` | 独立有效参数、配对来源提示、保留步进帮助 |
| `ScalingService`、`ScalingRuntime`、`ScalingWindow`、`Renderer` | 删除组切换路径，保留手动参数会话和捕获兼容传递 |

没有整体回退 `e0d95734`；`5eadefb8` 的基础修复和后续分段步进提示保留。

## 验证与后续验收

独立测试通过以下回归，证据保存在当前工作树 `obj/tests`：

- `Run-HdrCompatibilityUpdateTests.ps1`：精度、步进、颜色计划、生产组 writer／loader／完整 import、首次迁移与重载、多个配对独立、纯 HDR／SDR、旧 8／1600、未激活偏好、损坏恢复、手动列表与缩放、PNG／DDS、RTX 普通失败和 SEH。读取了本机试用组的副本验证迁移，没有写入正在使用的配置。
- `Run-HdrRegression.ps1`：CPU 颜色合同、Bicubic 缓存与边界、生产着色器 D3D11 WARP 的 9 条路线／54 组渐变，以及原生 203／1000／1500／4000 和旧亮度、元数据、alpha。
- `Validate-HdrUpdateSource.ps1`：重新生成独立 WinRT 投影，检查 17 个 C++ 源文件（16 项主检查及 AppSettings 补测，最新 loader 另行复查）；不生成应用 EXE／DLL、不链接或部署。
- 配置恢复、Profile 帧同步、组身份与同步绑定、参数输入测试通过。恢复策略升级为 v6；普通组的修复与升级继续工作。
- 效果目录生成检查与验证通过；20 份语言资源键唯一且 XML 有效，组专用资源已删除，XAML 语法有效。

主要日志：`obj/tests/hdr-manual-migration/results.txt`、`warp-results.txt`、`config-recovery-results.txt`、`profile-results.txt`、`group-identity-results.txt`、`input-results.txt`；源码检查日志在 `obj/tests/hdr-source-validation/manual-*.log`。这些构建产物不提交。

后续会话从当前分支构建并部署试用版，再验收实际 XAML／ImGui 布局、五类手动路线、错误定位、输入释放和重启交互、HDR 显示器与真实 RTX Create/Evaluate/Destroy。独立源码、WARP 和模拟 SDK 测试不代表实际界面、HDR／RTX 硬件、游戏画质、延迟或性能已验收。
