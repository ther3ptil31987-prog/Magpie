DLSSNR DLL 可选包

按显卡选择目录

- NVIDIA-Original-RTX50-Series：NVIDIA 原版，RTX 50 系列。
- Community-RTX40-RTX50-Series：社区兼容版，RTX 40／50 系列。
- SF-v2-FP16-RTX30-RTX20-Series：SF-v2 FP16 兼容版，RTX 30／20 系列。

每次只选择一种 nvngx_dlssnr.dll。SF-v2 是该兼容版本的标识，FP16 表示其兼容模式；具体 DLL 文件版本见下方校验表。
SF-v2 在 RTX 30／20 系列上性能有限，主要适合图片和视频场景。

安装或切换

1. 从系统托盘完全退出 Magpie。
2. 备份 Magpie.exe 旁当前使用的 nvngx_dlssnr.dll。
3. 打开与显卡对应的目录，将其中的 nvngx_dlssnr.dll 复制到 Magpie.exe 旁，覆盖同名文件。
4. 保持 DLL 文件名不变，重新启动 Magpie 并启用效果组。

不要把三个目录一起复制到程序目录；每次只替换一个 DLL。初始化失败或运行异常时，完全退出 Magpie 后恢复备份。
本包用于切换 DLSSNR DLL，不是完整的 Magpie 安装包。

------------------------------------------------------------

DLSSNR DLL Options

Choose a folder for your GPU

- NVIDIA-Original-RTX50-Series: NVIDIA original for RTX 50-series GPUs.
- Community-RTX40-RTX50-Series: Community compatibility version for RTX 40/50-series GPUs.
- SF-v2-FP16-RTX30-RTX20-Series: SF-v2 FP16 compatibility version for RTX 30/20-series GPUs.

Select only one nvngx_dlssnr.dll at a time. SF-v2 identifies the compatibility variant; FP16 describes its compatibility mode. Actual DLL file versions are listed below.
SF-v2 performance on RTX 30/20-series GPUs is limited and mainly suited to images and video.

Installation or switching

1. Fully exit Magpie from the system tray.
2. Back up the nvngx_dlssnr.dll currently beside Magpie.exe.
3. Open the folder for your GPU and copy its nvngx_dlssnr.dll beside Magpie.exe, replacing the existing file.
4. Keep the DLL filename unchanged, restart Magpie and enable your effect group.

Do not copy all three folders into the program directory; replace only one DLL at a time. If initialization or operation fails, fully exit Magpie and restore the backup.
This package switches the DLSSNR DLL; it is not a complete Magpie installation.

------------------------------------------------------------

文件校验 / File verification

NVIDIA-Original-RTX50-Series/nvngx_dlssnr.dll
  文件版本 / File version: 310.8.0.0
  字节数 / Bytes: 165840496
  SHA-256: E16BCF15E16E13F527491CDF7845B2FE6521A738D8F7C9C721866A8496E1FC8E

Community-RTX40-RTX50-Series/nvngx_dlssnr.dll
  文件版本 / File version: 310.8.0.0
  字节数 / Bytes: 165840496
  SHA-256: 984BEE0F775C277D5829B8FD6775D53A7B0F75396C852B3AAF06A18375F81014

SF-v2-FP16-RTX30-RTX20-Series/nvngx_dlssnr.dll
  文件版本 / File version: 310.8.SF.0
  字节数 / Bytes: 165830144
  SHA-256: 6EB209E764F39872625DEBD6ABAF45E2BB6322F6F270F781F70C059AE30B3927
