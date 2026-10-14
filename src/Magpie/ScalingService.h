#pragma once
#include "Event.h"
#include "ScalingRuntime.h"
#include "EffectParameterPersistence.h"

namespace Magpie {
class ScalingRuntime;
struct ScalingOptions;
struct EffectParametersRequest;
enum class ToolbarDock : uint8_t;
}

namespace Magpie {

struct Profile;

class ScalingService {
public:
	static ScalingService& Get() noexcept;

	ScalingService(const ScalingService&) = delete;
	ScalingService(ScalingService&&) = delete;

	~ScalingService();

	void Initialize();

	void Uninitialize();

	void StartTimer(bool windowedMode);

	void StopTimer();

	bool IsTimerOn() const noexcept {
		return _curCountdownSeconds > 0;
	}

	bool IsTimerOn(bool windowedMode) const noexcept {
		return IsTimerOn() && windowedMode == _isCurCountdownWindowedMode;
	}

	double TimerProgress() const noexcept {
		return SecondsLeft() / _curCountdownSeconds;
	}

	double SecondsLeft() const noexcept;

	bool IsScaling() const noexcept;

	// 强制重新检查前台窗口
	void CheckForeground();
	void OnTaskSwitch();
	void MainUIChanged(HWND window);
	void UpdateScreenshotContent(ScreenshotContent content);
	void EffectParameterEdited(uint32_t modeIdx, uint32_t effectIdx,
		const std::string& parameter, float value);
	void RetryConfigurationSave(std::function<void(bool)> completed);

	Event<bool, bool> IsTimerOnChanged;
	Event<double> TimerTick;
	Event<bool> IsScalingChanged;

private:
	ScalingService() = default;

	void _ShortcutService_ShortcutPressed(winrt::Magpie::ShortcutAction action);

	void _CountDownTimer_Tick(winrt::DispatcherQueueTimer const&, winrt::IInspectable const&);

	void _CheckForegroundTimer_Tick(winrt::DispatcherQueueTimer const&, winrt::IInspectable const&);

	void _ScalingRuntime_StateChanged(ScalingState value);

	void _ScaleForegroundWindow(bool windowedMode);

	void _StartScale(HWND hWnd, const Profile& profile, bool windowedMode, bool force);

	ScalingError _StartScaleImpl(HWND hWnd, const Profile& profile, bool windowedMode, bool force);
	void _SaveToolbarDock(std::weak_ptr<const uint8_t> profileIdentity,
		bool windowed, ToolbarDock dock, uint32_t runId);
	void _HandleEffectParametersRequest(
		ScalingOptions&& sessionOptions,
		EffectParametersRequest&& request
	);

	std::optional<ScalingRuntime> _scalingRuntime;

	winrt::DispatcherQueueTimer _countDownTimer{ nullptr };
	winrt::DispatcherQueueTimer _checkForegroundTimer{ nullptr };

	Event<winrt::Magpie::ShortcutAction>::EventRevoker _shortcutActivatedRevoker;
	Event<winrt::Magpie::ShortcutAction>::EventRevoker _toolbarShortcutsChangedRevoker;
	Event<Profile&>::EventRevoker _frameSyncChangedRevoker;
	Event<>::EventRevoker _frameRefreshModeChangedRevoker;
	Event<>::EventRevoker _duplicateRefreshChangedRevoker;
	Event<Profile&>::EventRevoker _rtssChangedRevoker;
	std::weak_ptr<const uint8_t> _activeFrameSyncProfile;

	std::chrono::steady_clock::time_point _timerStartTimePoint;

	uint32_t _curCountdownSeconds = 0;
	bool _isCurCountdownWindowedMode = false;

	HWND _hwndCurSrc = NULL;
	// Task switching requires an explicit user start, even for automatic profiles.
	bool _isAutoScaleSuspended = false;
	// 1. 避免重复检查同一个窗口
	// 2. 用户使用热键退出全屏后暂时阻止该窗口自动放大
	HWND _hwndChecked = NULL;
	struct PendingEffectParametersSave {
		std::shared_ptr<EffectParametersSaveState> state;
		uint64_t revision;
	};
	void _ScheduleEffectParametersSave(const EffectParametersRequest& request);
	void _FlushEffectParametersSaves(bool synchronous = false);
	winrt::DispatcherQueueTimer _effectParametersSaveTimer{ nullptr };
	std::vector<PendingEffectParametersSave> _pendingEffectParametersSaves;
	std::vector<std::weak_ptr<EffectParametersSaveState>> _parameterSaveStates;
	std::chrono::steady_clock::time_point _firstEffectParametersEdit{};
};

}
