#include <atomic>
#include <deque>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include "RtssSettings.h"
#include "FrameRefreshSettings.h"

using namespace Magpie;
using HWND = uintptr_t;

// Fixtures replace process discovery, dispatch, persistence and external I/O.
// The generated header supplies the unchanged production method bodies.
struct TargetIdentity {
	HWND source;
	std::string profile = "target.exe";
	bool Valid() const { return true; }
	bool SameProcess(const TargetIdentity& other) const { return source == other.source; }
};
static auto ResolveTarget(HWND source) {
	return source ? std::make_shared<TargetIdentity>(TargetIdentity{ source }) : nullptr;
}
enum class RtssState { Failed };
struct RtssStatus {
	bool selfInitialization = false, rateKnown = false, reloaded = false, profileSaved = false, rateUnavailable = false;
	bool manualOperation = false;
	uint64_t sessionGeneration = 0;
	RtssScope scope = RtssScope::None;
	std::wstring profile;
	std::weak_ptr<const uint8_t> profileIdentity;
	RtssState state{};
	std::wstring failureStage;
};
struct Dispatch {
	std::deque<std::function<void()>> tasks;
	void TryEnqueue(std::function<void()> task) { tasks.push_back(std::move(task)); }
	void Drain() { while (!tasks.empty()) { auto task = std::move(tasks.front()); tasks.pop_front(); task(); } }
};
struct CounterEvent { unsigned count = 0; void Invoke() { ++count; } };
struct RtssService {
	struct Session {
		uint64_t generation = 0;
		std::weak_ptr<const uint8_t> identity;
		RtssSettings settings;
		std::shared_ptr<TargetIdentity> initialTarget;
		std::atomic<std::shared_ptr<TargetIdentity>> target;
		std::string profile;
		std::atomic<bool> cancelled = false, updateQueued = false;
		std::atomic<uint32_t> desiredRate = 0;
		std::atomic<bool> contentConstrained = false;
	};
	std::shared_ptr<Session> _uiSession, _workerSession;
	uint64_t _generation = 0;
	bool _notify = true;
	Dispatch _dispatcher, worker;
	CounterEvent StatusChanged;
	unsigned applyCalls = 0, finishCalls = 0;
	RtssSettings finishedSettings;
	static RtssService& Get() { static RtssService instance; return instance; }
	void _Queue(std::function<void()> task) { worker.TryEnqueue(std::move(task)); }
	struct Timer { void Stop() {} Timer& operator=(std::nullptr_t) { return *this; } explicit operator bool() const { return false; } } _sessionTimer;
	void _StartSessionWatch(const std::shared_ptr<Session>&) {}
	void _TryApply(const std::shared_ptr<Session>&) { ++applyCalls; }
	void _Finish(const std::shared_ptr<Session>& session) { ++finishCalls; finishedSettings = session->settings; }
	RtssStatus Status() const { return {}; }
	RtssStatus SessionStatus() const { return {}; }
	void _Publish(RtssStatus) { StatusChanged.Invoke(); }
	uint64_t Begin(HWND target, std::weak_ptr<const uint8_t> identity, RtssSettings settings);
	void ContentTargetChanged(uint64_t session, HWND source, bool fixed, double baseRate, bool constrained);
	void End();
	uint32_t ContentSyncRate(std::weak_ptr<const uint8_t> identity) const noexcept;
};
struct ProfileData {
	RtssSettings rtss;
	FrameRefreshSettings frameRefresh;
	std::shared_ptr<const uint8_t> runtimeIdentity = std::make_shared<const uint8_t>(uint8_t(0));
};
struct ProfileViewModel {
	ProfileData data;
	ProfileData* _data = &data;
	unsigned saves = 0, notifications = 0;
	bool simple = false;
	bool IsSimpleFrameRefresh() const noexcept { return simple; }
	bool IsAdvancedFrameRefresh() const noexcept { return !simple; }
	bool ShowRtssOptions() const noexcept;
	void _SaveRtss() { ++saves; }
	void RaisePropertyChanged(const wchar_t*) { ++notifications; }
	double RtssCustomRate() const noexcept;
	void RtssCustomRate(double value);
	double RtssSyncedRate() const noexcept;
	bool IsRtssManaged() const noexcept;
	bool ShowRtssCustomRate() const noexcept;
	bool ShowRtssSyncedRate() const noexcept;
	bool IsRtssSyncAvailable() const noexcept;
};
#include "RtssRateCode.g.h"

static unsigned checks = 0;
static void Require(bool value, const char* message) {
	++checks;
	if (!value) throw std::runtime_error(message);
}

int main() {
	try {
		auto& service = RtssService::Get();
		ProfileViewModel vm, other;
		vm.data.rtss.scope = other.data.rtss.scope = RtssScope::Target;
		vm.data.rtss.rateMode = other.data.rtss.rateMode = RtssRateMode::ContentSync;
		vm.data.rtss.customRate = 83;
		Require(std::isnan(vm.RtssSyncedRate()), "Before scaling, sync is unknown");
		Require(!vm.ShowRtssCustomRate() && vm.ShowRtssSyncedRate(), "Only synced number is shown");
		const auto session = service.Begin(1, vm.data.runtimeIdentity, vm.data.rtss);
		service.worker.Drain();
		Require(std::isnan(vm.RtssSyncedRate()), "Starting without a content target is unknown");
		for (double base : { 1., 14., 59.94, 72., 360., 600., 1000. }) {
			const auto events = service.StatusChanged.count;
			service.ContentTargetChanged(session, 1, true, base, true);
			service._dispatcher.Drain();
			Require(service.StatusChanged.count == events + 1, "Changed target refreshes the display");
			Require(vm.RtssSyncedRate() == std::round(base), "Actual resolved integer is displayed without manual clipping");
			Require(std::isnan(other.RtssSyncedRate()), "Other profile cannot display this session's rate");
			vm.RtssCustomRate(vm.RtssSyncedRate());
			Require(vm.RtssCustomRate() == 83 && vm.saves == 0, "Readonly refresh preserves manual preference");
			service.worker.Drain();
		}
		const auto events = service.StatusChanged.count;
		service.ContentTargetChanged(session, 1, true, 1000., false);
		service._dispatcher.Drain();
		Require(service.StatusChanged.count == events, "Same target has no redundant display notification");
		for (double invalid : { 0., 1001., std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() }) {
			service.ContentTargetChanged(session, 1, true, invalid, false);
			Require(std::isnan(vm.RtssSyncedRate()), "Invalid fixed target is unknown");
		}
		service.ContentTargetChanged(session, 1, true, 72., false);
		service.ContentTargetChanged(session, 1, false, 72., false);
		Require(std::isnan(vm.RtssSyncedRate()), "Source-follow has no fixed target");
		service.ContentTargetChanged(session, 1, true, 72., false);
		vm.data.frameRefresh.contentMode = ContentFrameRateMode::Source;
		Require(std::isnan(vm.RtssSyncedRate()), "Saved source-follow mode hides an old numeric target");
		vm.data.frameRefresh.contentMode = ContentFrameRateMode::Custom;
		vm.data.rtss.rateMode = RtssRateMode::Custom;
		Require(vm.ShowRtssCustomRate() && !vm.ShowRtssSyncedRate() && vm.RtssCustomRate() == 83,
			"Switching custom restores manual preference");
		Require(std::isnan(vm.RtssSyncedRate()), "Custom UI does not present a sync number");
		vm.RtssCustomRate(99.6);
		Require(vm.RtssCustomRate() == 100 && vm.saves == 1, "Manual integer editing still saves");
		for (double invalid : { 14., 361., std::numeric_limits<double>::quiet_NaN() }) vm.RtssCustomRate(invalid);
		Require(vm.RtssCustomRate() == 100 && vm.saves == 1, "Invalid manual edits do not save");
		vm.data.rtss.rateMode = RtssRateMode::ContentSync;
		Require(vm.RtssSyncedRate() == 72, "Returning sync uses actual session target");
		vm.data.rtss.scope = RtssScope::None;
		vm.RtssCustomRate(120);
		Require(std::isnan(vm.RtssSyncedRate()) && vm.saves == 1, "Unmanaged controls do not save or show a sync target");
		vm.data.rtss.scope = RtssScope::Target;
		service.worker.Drain(); service._dispatcher.Drain();
		const auto stoppedEvents = service.StatusChanged.count;
		vm.data.rtss.exitPolicy = RtssExitPolicy::KeepChanges;
		service.End();
		Require(std::isnan(vm.RtssSyncedRate()), "Stop clears sync before restoration finishes");
		service._dispatcher.Drain();
		Require(service.StatusChanged.count == stoppedEvents + 1, "Stop notifies even without a published restoration result");
		service.worker.Drain();
		Require(service.finishedSettings.exitPolicy == RtssExitPolicy::RestoreOriginal, "Editing stop policy leaves the captured default restore policy intact");
		service.ContentTargetChanged(session, 1, true, 600., false);
		Require(std::isnan(vm.RtssSyncedRate()), "Old session callback after stop cannot resurrect the rate");
		vm.data.rtss.rateMode = RtssRateMode::Custom;
		const auto customSession = service.Begin(1, vm.data.runtimeIdentity, vm.data.rtss);
		vm.data.rtss.rateMode = RtssRateMode::ContentSync;
		Require(std::isnan(vm.RtssSyncedRate()), "Editing sync preference does not relabel an active custom session");
		service.End(); service.worker.Drain(); service._dispatcher.Drain();
		const auto next = service.Begin(1, vm.data.runtimeIdentity, vm.data.rtss);
		service.ContentTargetChanged(customSession, 1, true, 600., false);
		Require(std::isnan(vm.RtssSyncedRate()), "Old generation cannot supply a new session target");
		service.ContentTargetChanged(next, 1, true, 60., false);
		Require(vm.RtssSyncedRate() == 60, "New generation supplies its own target");
		service._uiSession->cancelled = true;
		Require(std::isnan(vm.RtssSyncedRate()), "Cancelled session has no display rate");
		service.End(); service.worker.Drain(); service._dispatcher.Drain();
		std::weak_ptr<const uint8_t> expired;
		{ auto identity = std::make_shared<const uint8_t>(uint8_t(0)); expired = identity; service.Begin(1, identity, vm.data.rtss); }
		service.ContentTargetChanged(service._generation, 1, true, 60., false);
		Require(service.ContentSyncRate(expired) == 0, "Two expired identities never compare as the same profile");
		service.End(); service.worker.Drain(); service._dispatcher.Drain();
		Require(vm.saves == 1 && vm.RtssCustomRate() == 100, "All display/lifecycle changes leave manual settings intact");
		std::cout << "RTSS rate display: " << checks << " production-code checks passed. No RTSS I/O or XAML.\n";
		return 0;
	} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
