#pragma once
#include "ForegroundCompatibilityState.h"
#include "SourceWindowCoordinator.h"
#include "SourceWindowRelations.h"

namespace Magpie {

// Frontend-owned session control. The process API detour reads only published
// values and live Win32 identity; it never touches ScalingWindow or Renderer.
class NvidiaForegroundCompatibility {
public:
	static constexpr UINT_PTR ReferenceTimer = 0x4D505652;
	static HWND RealForegroundWindow() noexcept;
	static void Prepare(const SourceWindowIdentity& source, HWND output, uint64_t epoch,
		bool standardVRR, bool windowed, const SessionWindowIdentity& mainUI) noexcept;
	static void DeviceInitialized(uint32_t vendor, uint32_t luidLow, int32_t luidHigh,
		ForegroundPresentationPath path, uint32_t multiplier) noexcept;
	static bool BindPresentationWindow(HWND window) noexcept;
	static void FrontendWindows(HWND parameterHost, HWND realForeground) noexcept;
	static void MainUIChanged(const SessionWindowIdentity& identity) noexcept;
	static void PresentationState(bool canCreateTearing, bool canPresentTearing) noexcept;
	static void Reject(const char* reason) noexcept;
	static void Phase(ForegroundQueryPhase phase) noexcept;
	static void FrontendSuspended(bool suspended) noexcept;
	static void OutputShown() noexcept;
	static bool ReferenceOwnsForeground() noexcept;
	static void Poll() noexcept;
	// Retirement does not wait for driver threads or install/remove API hooks.
	static void Retire() noexcept;
	static void ReleaseReferenceFocus() noexcept;
	// Called outside window callbacks, after backend resources have joined.
	static void FinishRetire() noexcept;
};

}
