#pragma once
#include "HomeViewModel.g.h"
#include "Event.h"

namespace Magpie { struct Profile; }

namespace winrt::Magpie::implementation {

struct HomeViewModel : HomeViewModelT<HomeViewModel>, wil::notify_property_changed_base<HomeViewModel> {
	HomeViewModel();
	bool ShowDlssNrStatus() const;
	hstring DlssNrStatusSummary() const;
	void OpenDlssNrEffects();
	bool ShowRecentIssue() const noexcept;
	void ShowRecentIssue(bool value);
	hstring RecentIssueSummary() const noexcept;
	MUXC::InfoBarSeverity RecentIssueSeverity() const noexcept;
	bool ShowIssueHistory() const noexcept;
	fire_and_forget ShowRecentIssueDetails() noexcept;

	hstring TimerDescription() const noexcept;

	bool IsTimerOn() const noexcept;

	double TimerProgressRingValue() const noexcept;

	hstring TimerLabelText() const noexcept;

	bool IsNotRunning() const noexcept;

	hstring TimerButtonText(bool windowedMode) const noexcept;

	void ToggleTimerFullscreen() const noexcept;

	void ToggleTimerWindowed() const noexcept;

	uint32_t Delay() const noexcept;
	void Delay(uint32_t value);

	hstring DelayText() const noexcept;

	bool ShowUpdateCard() const noexcept {
		return _showUpdateCard;
	}

	void ShowUpdateCard(bool value) noexcept;

	hstring UpdateCardTitle() const noexcept;

	bool IsAutoCheckForUpdates() const noexcept;
	void IsAutoCheckForUpdates(bool value) noexcept;

	void DownloadAndInstall();

	void ReleaseNotes();

	void RemindMeLater();

	hstring InitialToolbarStateDescription() const noexcept;

	int FullscreenInitialToolbarState() const noexcept;
	void FullscreenInitialToolbarState(int value);

	int WindowedInitialToolbarState() const noexcept;
	void WindowedInitialToolbarState(int value);

	hstring ScreenshotSaveDirectory() const noexcept;
	int ScreenshotContent() const noexcept;
	void ScreenshotContent(int value);

	void OpenScreenshotSaveDirectory() const noexcept;

	fire_and_forget ChangeScreenshotSaveDirectory() noexcept;

	bool IsTouchSupportEnabled() const noexcept;
	fire_and_forget IsTouchSupportEnabled(bool value);

	Uri TouchSupportLearnMoreUrl() const noexcept;

	bool IsShowTouchSupportInfoBar() const noexcept;

	bool IsAllowScalingMaximized() const noexcept;
	void IsAllowScalingMaximized(bool value);

	bool IsSimulateExclusiveFullscreen() const noexcept;
	void IsSimulateExclusiveFullscreen(bool value);

	bool IsInlineParams() const noexcept;
	void IsInlineParams(bool value);

	bool IsStopEffectsOnTaskSwitchEnabled() const noexcept;
	void IsStopEffectsOnTaskSwitchEnabled(bool value);
	bool IsVRREnabled() const noexcept;
	void IsVRREnabled(bool value);

	hstring DefaultFrameRefreshSummary() const;
	void EditDefaultFrameRefresh();

	bool IsDeveloperMode() const noexcept;
	void IsDeveloperMode(bool value);

	void LocateMagpieLogs() noexcept;
	void LocateTouchHelperLogs() noexcept;
	void LocateUpdaterLogs() noexcept;

	bool IsDebugMode() const noexcept;
	void IsDebugMode(bool value);

	bool IsBenchmarkMode() const noexcept;
	void IsBenchmarkMode(bool value);

	bool IsTopmostDisabled() const noexcept;
	void IsTopmostDisabled(bool value);

	bool IsEffectCacheDisabled() const noexcept;
	void IsEffectCacheDisabled(bool value);

	bool IsFontCacheDisabled() const noexcept;
	void IsFontCacheDisabled(bool value);

	bool IsSaveEffectSources() const noexcept;
	void IsSaveEffectSources(bool value);

	bool IsWarningsAreErrors() const noexcept;
	void IsWarningsAreErrors(bool value);

	bool IsFP16Disabled() const noexcept;
	void IsFP16Disabled(bool value);

	int DuplicateFrameDetectionMode() const noexcept;
	void DuplicateFrameDetectionMode(int value);

	bool IsDynamicDection() const noexcept;

	bool IsStatisticsForDynamicDetectionEnabled() const noexcept;
	void IsStatisticsForDynamicDetectionEnabled(bool value);

private:
	::Magpie::Event<>::EventRevoker _duplicateFrameDetectionModeChangedRevoker;
	::Magpie::Event<::Magpie::Profile&>::EventRevoker _frameRefreshChangedRevoker;
	::Magpie::Event<>::EventRevoker _frameRefreshModeChangedRevoker;
	void _ScalingService_IsTimerOnChanged(bool value, bool windowedMode);

	void _ScalingService_TimerTick(double);

	void _ScalingService_IsScalingChanged(bool);

	void _ToggleTimer(bool windowedMode) const noexcept;

	::Magpie::Event<bool, bool>::EventRevoker _isTimerOnRevoker;
	::Magpie::Event<>::EventRevoker _issueChangedRevoker;
	::Magpie::Event<double>::EventRevoker _timerTickRevoker;
	::Magpie::Event<bool>::EventRevoker _isScalingChangedRevoker;
	::Magpie::Event<bool>::EventRevoker _isShowOnHomePageChangedRevoker;

	bool _showUpdateCard = false;
};

}
