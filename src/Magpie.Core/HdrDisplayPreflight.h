#pragma once
#include "ScalingOptions.h"
#include "Win32Helper.h"
#include "Logger.h"
#include "StrHelper.h"

namespace Magpie {

// DisplayConfig queries are CPU/OS capability checks; no graphics device is created.
inline bool IsHdrMonitorActive(HMONITOR monitor) noexcept {
	MONITORINFOEXW info{};
	info.cbSize = sizeof(info);
	if (!monitor || !GetMonitorInfoW(monitor, &info)) return false;
	for (unsigned retry = 0; retry < 3; ++retry) {
		UINT32 pathCount = 0, modeCount = 0;
		if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS) return false;
		std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
		std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
		const LONG status = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
		if (status == ERROR_INSUFFICIENT_BUFFER) continue;
		if (status != ERROR_SUCCESS) return false;
		for (UINT32 i = 0; i < pathCount; ++i) {
			const auto& path = paths[i];
			DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
			source.header = { DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME, sizeof(source), path.sourceInfo.adapterId, path.sourceInfo.id };
			if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS ||
				_wcsicmp(source.viewGdiDeviceName, info.szDevice) != 0) continue;
			DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO_2 advanced{};
			advanced.header = { DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO_2, sizeof(advanced), path.targetInfo.adapterId, path.targetInfo.id };
			if (DisplayConfigGetDeviceInfo(&advanced.header) == ERROR_SUCCESS)
				return advanced.activeColorMode == DISPLAYCONFIG_ADVANCED_COLOR_MODE_HDR;
			DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO legacy{};
			legacy.header = { DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO, sizeof(legacy), path.targetInfo.adapterId, path.targetInfo.id };
			return DisplayConfigGetDeviceInfo(&legacy.header) == ERROR_SUCCESS && legacy.advancedColorEnabled;
		}
		return false;
	}
	return false;
}

inline ScalingError CheckHdrComponentPrerequisites(HWND source, const ScalingOptions& options) noexcept {
	const auto& plan = options.hdrComponents;
	if (!plan.enabled) return ScalingError::NoError;
	switch (plan.error) {
	case HdrComponentError::ExpectedHdr: return ScalingError::HdrComponentExpectedHdr;
	case HdrComponentError::ExpectedSdr: return ScalingError::HdrComponentExpectedSdr;
	case HdrComponentError::MissingPair: return ScalingError::HdrComponentMissingPair;
	case HdrComponentError::InvalidParameters: return ScalingError::HdrComponentInvalidParameters;
	default: break;
	}
	for (const auto& stage : plan.stages) {
		if (stage.kind != HdrComponentKind::RtxVideoHdr) continue;
		const auto directory = Win32Helper::GetExePath().parent_path();
#ifdef MP_ENABLE_NVIDIA_VFX
		constexpr const wchar_t* required[] = { L"nvngxruntime.dll", L"NVCVImage.dll",
			L"NVVideoEffects.dll", L"nvngx_truehdr.dll", L"nvVFXTrueHDR.dll" };
#else
		constexpr const wchar_t* required[] = { L"Magpie.RtxVideo.dll", L"nvngx_truehdr.dll" };
#endif
		for (const auto* component : required) {
			if (Win32Helper::FileExists((directory / component).c_str())) continue;
			Logger::Get().Error("HDR component missing: " + StrHelper::UTF16ToUTF8(component));
			return ScalingError::RtxHdrUnavailable;
		}
		break;
	}
	const HMONITOR sourceMonitor = MonitorFromWindow(source, MONITOR_DEFAULTTONEAREST);
	if (plan.captureHdr) {
		if (options.captureMethod != CaptureMethod::GraphicsCapture) return ScalingError::HdrCaptureMethodRequired;
		if (!IsHdrMonitorActive(sourceMonitor)) return ScalingError::HdrCaptureRequired;
	}
	if (!plan.outputHdr) return ScalingError::NoError;
	if (options.IsWindowedMode() || options.multiMonitorUsage == MultiMonitorUsage::Closest)
		return IsHdrMonitorActive(sourceMonitor) ? ScalingError::NoError : ScalingError::HdrDisplayRequired;
	RECT sourceRect{};
	if (!GetWindowRect(source, &sourceRect)) return ScalingError::DisplayLayoutFailed;
	bool checked = false;
	for (const auto& monitor : Win32Helper::GetDisplayMonitors()) {
		bool selected = options.multiMonitorUsage == MultiMonitorUsage::All;
		if (options.multiMonitorUsage == MultiMonitorUsage::Specific)
			selected = _wcsicmp(monitor.deviceId.c_str(), options.preferredMonitorId.c_str()) == 0;
		if (options.multiMonitorUsage == MultiMonitorUsage::Intersected)
			selected = Win32Helper::IsRectOverlap(monitor.rect, sourceRect);
		if (!selected) continue;
		checked = true;
		if (!IsHdrMonitorActive(monitor.handle)) return ScalingError::HdrDisplayRequired;
	}
	return checked ? ScalingError::NoError : ScalingError::DisplayLayoutFailed;
}

}
