#pragma once
#include "ScalingOptions.h"
#include "RtssSettings.h"

namespace Magpie {

enum class InitialWindowedScaleFactor {
	Auto = 0,
	x1_25 = 1,
	x1_5 = 2,
	x1_75 = 3,
	x2 = 4,
	x3 = 5,
	Custom = 6,
	// 配置文件直接保存枚举值，新选项必须追加以保持旧配置兼容
	x1 = 7,
	COUNT
};

// 下拉框按倍率排序，但持久化枚举不能重排，因此在 UI 索引和枚举值之间显式转换。
constexpr int InitialWindowedScaleFactorToSelectedIndex(InitialWindowedScaleFactor factor) noexcept {
	switch (factor) {
	case InitialWindowedScaleFactor::Auto:
		return 0;
	case InitialWindowedScaleFactor::x1:
		return 1;
	case InitialWindowedScaleFactor::x1_25:
		return 2;
	case InitialWindowedScaleFactor::x1_5:
		return 3;
	case InitialWindowedScaleFactor::x1_75:
		return 4;
	case InitialWindowedScaleFactor::x2:
		return 5;
	case InitialWindowedScaleFactor::x3:
		return 6;
	case InitialWindowedScaleFactor::Custom:
		return 7;
	default:
		return 0;
	}
}

constexpr InitialWindowedScaleFactor InitialWindowedScaleFactorFromSelectedIndex(int index) noexcept {
	switch (index) {
	case 0:
		return InitialWindowedScaleFactor::Auto;
	case 1:
		return InitialWindowedScaleFactor::x1;
	case 2:
		return InitialWindowedScaleFactor::x1_25;
	case 3:
		return InitialWindowedScaleFactor::x1_5;
	case 4:
		return InitialWindowedScaleFactor::x1_75;
	case 5:
		return InitialWindowedScaleFactor::x2;
	case 6:
		return InitialWindowedScaleFactor::x3;
	case 7:
		return InitialWindowedScaleFactor::Custom;
	default:
		return InitialWindowedScaleFactor::Auto;
	}
}

enum class CursorScaling {
	x0_5,
	x0_75,
	NoScaling,
	x1_25,
	x1_5,
	x2,
	Source,
	Custom,
	COUNT
};

enum class AutoScale {
	Disabled,
	Fullscreen,
	Windowed,
	COUNT
};

struct Profile {
	void Copy(const Profile& other) noexcept {
		// A newly created application profile opts in independently, even when copied.
		isParameterFocusSwitchingEnabled = false;
		scalingMode = other.scalingMode;
		autoScale = other.autoScale;
		initialWindowedScaleFactor = other.initialWindowedScaleFactor;
		customInitialWindowedScaleFactor = other.customInitialWindowedScaleFactor;
		cursorScaling = other.cursorScaling;
		customCursorScaling = other.customCursorScaling;
		frameRefresh = other.frameRefresh;
		rtss = other.rtss;
		autoHideCursorDelay = other.autoHideCursorDelay;
		cropping = other.cropping;
		captureMethod = other.captureMethod;
		graphicsCardId = other.graphicsCardId;
		multiMonitorUsage = other.multiMonitorUsage;
		preferredMonitorId = other.preferredMonitorId;
		preferredMonitorName = other.preferredMonitorName;
		cursorInterpolationMode = other.cursorInterpolationMode;
		launchParameters = other.launchParameters;
		destAlignment = other.destAlignment;
		scalingFlags = other.scalingFlags;
		toolbarDocks = other.toolbarDocks;
		
		isCroppingEnabled = other.isCroppingEnabled;
		isAutoHideCursorEnabled = other.isAutoHideCursorEnabled;
	}

	DEFINE_FLAG_ACCESSOR(Is3DGameMode, ScalingFlags::Is3DGameMode, scalingFlags)
	DEFINE_FLAG_ACCESSOR(IsCaptureTitleBar, ScalingFlags::CaptureTitleBar, scalingFlags)
	DEFINE_FLAG_ACCESSOR(IsAdjustCursorSpeed, ScalingFlags::AdjustCursorSpeed, scalingFlags)
	DEFINE_FLAG_ACCESSOR(IsDirectFlipDisabled, ScalingFlags::DisableDirectFlip, scalingFlags)
	DEFINE_FLAG_ACCESSOR(IsHdrCompatibilityEnabled, ScalingFlags::EnableHdrCompatibility, scalingFlags)

	// 默认规则 name、pathRule 和 classNameRule 均为空
	std::wstring name;

	// 对于打包应用，pathRule 存储 AUMID
	std::wstring pathRule;
	std::wstring classNameRule;

	// 允许 exe 和 lnk
	std::filesystem::path launcherPath;

	bool isParameterFocusSwitchingEnabled = false;
	ToolbarDockSettings toolbarDocks;
	FrameRefreshSettings frameRefresh;
	RtssSettings rtss;
	// Not serialized/copied by Copy(): survives vector moves and renames, but
	// a deleted/recreated profile cannot receive an older toolbar save.
	std::shared_ptr<const uint8_t> runtimeIdentity = std::make_shared<const uint8_t>(uint8_t{0});
	AutoScale autoScale = AutoScale::Disabled;

	InitialWindowedScaleFactor initialWindowedScaleFactor = InitialWindowedScaleFactor::Auto;
	float customInitialWindowedScaleFactor = 1.25f;

	CursorScaling cursorScaling = CursorScaling::NoScaling;
	float customCursorScaling = 1.0;

	// 0.1~5
	float autoHideCursorDelay = 3.0f;

	Cropping cropping{};
	// -1 表示原样
	int scalingMode = -1;
	CaptureMethod captureMethod = CaptureMethod::GraphicsCapture;
	GraphicsCardId graphicsCardId;
	MultiMonitorUsage multiMonitorUsage = MultiMonitorUsage::Closest;
	std::wstring preferredMonitorId;
	std::wstring preferredMonitorName;
	CursorInterpolationMode cursorInterpolationMode = CursorInterpolationMode::NearestNeighbor;

	std::wstring launchParameters;
	DestAlignment destAlignment = DestAlignment::Center;

	uint32_t scalingFlags = ScalingFlags::AdjustCursorSpeed;

	bool isPackaged = false;
	bool isCroppingEnabled = false;
	bool isAutoHideCursorEnabled = false;
};

}
