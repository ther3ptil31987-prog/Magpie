#pragma once
#include "Event.h"
#include "RtssSettings.h"
#include "RtssProfileAccess.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <memory>
#include <functional>

namespace Magpie {

enum class RtssState {
	Checking, NotInstalled, NotRunning, Incompatible, Failed, WaitingTarget, Read, MissingProfile,
	WaitingContent, Applying, Written, Conflict, Kept, Restored, RestoreFailed, StartingTool, ToolStartFailed,
	InitializingMagpie, MagpieInitialized, MagpieAlreadyUnlimited, Disabled, ToolStarted
};

struct RtssStatus {
	RtssState state = RtssState::Checking;
	uint64_t sessionGeneration = 0;
	RtssToolInfo rtss, afterburner;
	std::wstring profile;
	double currentRate = 0;
	bool rateKnown = false;
	uint32_t desiredRate = 0;
	bool timeLimit = false;
	bool detectionDisabled = false;
	bool targetOverridesGlobal = false;
	bool contentConstrained = false;
	bool readReady = false, applyReady = false, initializeReady = false;
	bool reloaded = false, profileSaved = false, rateUnavailable = false;
	bool selfInitialization = false;
	bool manualOperation = false;
	bool inventoryKnown = false;
	RtssScope scope = RtssScope::None;
	std::weak_ptr<const uint8_t> profileIdentity;
	std::wstring failureStage;
};

// UI commands and immutable status snapshots; external operations run on one
// dedicated worker, never on the render/dispatcher threads.
enum class RtssInitializationState : uint8_t { Unknown, Uninitialized, Initialized };

class RtssService {
public:
	static RtssService& Get();
	void Initialize(winrt::DispatcherQueue dispatcher);
	void Uninitialize();
	uint64_t Begin(HWND target, std::weak_ptr<const uint8_t> identity, RtssSettings settings);
	void ContentTargetChanged(uint64_t session, HWND source, bool fixed, double baseRate, bool constrained);
	void End();
	void ReleaseProfile(std::weak_ptr<const uint8_t> identity);
	void Recheck(bool retry = true);
	bool HasSession(std::weak_ptr<const uint8_t> identity) const noexcept;
	void Retry(std::weak_ptr<const uint8_t> identity);
	bool IsCurrentStatus(const RtssStatus& status, std::weak_ptr<const uint8_t> identity) const noexcept;
	bool IsPreparing() const noexcept { return _autoPreparing; }
	void Launch(bool afterburner);
	void InitializeMagpie();
	bool IsInitializingMagpie() const noexcept { return _initializationPending; }
	RtssInitializationState MagpieInitialization() const noexcept { return _magpieInitialization.load(); }
	bool IsMagpieInitialized() const noexcept { return MagpieInitialization() == RtssInitializationState::Initialized; }
	bool IsLaunching() const noexcept { return _launchPending; }
	void Preview(RtssSettings settings, std::wstring executable,
		std::function<void(RtssStatus)> completed);
	RtssStatus Status() const;
	RtssStatus InventoryStatus() const;
	RtssStatus SessionStatus() const;
	RtssStatus ManagementStatus() const;
	// UI thread: snapshot of the actual running policy, including queued startup.
	RtssStatus CurrentSessionStatus(std::weak_ptr<const uint8_t> identity) const;
	bool IsLatestStatus(const RtssStatus& status, std::weak_ptr<const uint8_t> identity) const noexcept;
	bool IsRechecking() const noexcept { return _recheckPending; }
	// UI thread only; the current session's resolved sync target, or 0 if unknown.
	uint32_t ContentSyncRate(std::weak_ptr<const uint8_t> identity) const noexcept;
	Event<> StatusChanged;

private:
	struct Session;
	RtssService() = default;
	~RtssService();
	void _Queue(std::function<void()> task);
	void _Worker();
	void _Publish(RtssStatus status);
	bool _PrepareSession(const std::shared_ptr<Session>& session, RtssStatus& status);
	void _PublishSession(const std::shared_ptr<Session>& session, RtssStatus status);
	void _StartSessionWatch(const std::shared_ptr<Session>& session);
	void _CheckSession(const std::shared_ptr<Session>& session);
	void _TryApply(const std::shared_ptr<Session>& session);
	void _Finish(const std::shared_ptr<Session>& session);
	void _InitializeMagpie();
	void _RefreshMagpieInitialization(const RtssStatus& status);
	bool _Prepare(RtssStatus& status);
	std::shared_ptr<Session> _uiSession;
	std::shared_ptr<Session> _workerSession;
	RtssProfileAccess _access;
	winrt::DispatcherQueue _dispatcher{ nullptr };
	std::thread _thread;
	mutable std::mutex _mutex;
	std::condition_variable _wake;
	std::condition_variable _done;
	std::deque<std::function<void()>> _tasks;
	RtssStatus _status;
	RtssStatus _sessionStatus, _managementStatus, _inventoryStatus;
	uint64_t _generation = 0;
	bool _closing = false;
	bool _exited = false;
	std::atomic<bool> _notify = false;
	std::atomic<bool> _autoPreparing = false;
	std::atomic<RtssInitializationState> _magpieInitialization = RtssInitializationState::Unknown; // worker writes, UI reads
	bool _launchPending = false; // UI thread
	bool _initializationPending = false; // UI thread
	bool _recheckPending = false; // UI thread
	winrt::DispatcherQueueTimer _launchTimer{ nullptr };
	winrt::DispatcherQueueTimer _sessionTimer{ nullptr };
};

}
