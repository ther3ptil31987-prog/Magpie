// Reuse the faithful profile-export fixture with the actual service transaction bodies.
#define wmain RtssProfileUnitMain
#include "RtssProfileTransactionTests.cpp"
#undef wmain
#include "RtssSettings.h"
#include <atomic>
#include <memory>
#include <thread>
#include <chrono>
namespace wil { template<class F> struct Exit { F callback; ~Exit() { callback(); } }; template<class F> auto scope_exit(F f) { return Exit<F>{f}; } }

namespace Magpie {
bool installed = true, launchSuccess = false;
unsigned launches = 0;
std::function<void()> launchHook;
RtssToolInfo RtssProfileAccess::DetectTool(bool afterburner) {
	if (afterburner || !active || !installed) return {};
	return {active->directory / L"RTSS.exe", active->access._version, active->access._running};
}
bool RtssProfileAccess::LaunchTool(const RtssToolInfo&, std::wstring& error) {
	++launches;
	if (launchHook) launchHook();
	if (launchSuccess) { active->access._running = true; return true; }
	error = L"TestLaunchFailure"; return false;
}
#include "RtssSessionCases.g.h"
namespace {
struct TargetIdentity {
	std::string profile = "game.exe";
	std::wstring exe = L"game.exe";
	HWND source = nullptr;
	bool valid = true;
	bool Valid() const { return valid; }
	bool SameProcess(const TargetIdentity&) const { return true; }
};
std::shared_ptr<TargetIdentity> ResolveTarget(HWND) { return {}; }
std::string ProfileName(const std::wstring& name) { return RtssProfileAccess::ProfileName(name); }
namespace Win32Helper { std::filesystem::path GetExePath() {
	wchar_t path[32768]{}; GetModuleFileNameW(nullptr, path, 32768); return path;
} }
void LogFailure(const char*, const std::wstring&) {}
RtssProfileFailure loggedFailure;
void LogProfileFailure(const char*, const std::wstring&, const RtssProfileFailure& failure) { loggedFailure = failure; }
}
enum class RtssInitializationState : uint8_t { Unknown, Uninitialized, Initialized };
class RtssService {
public:
	struct Session;
	RtssProfileAccess& _access;
	RtssStatus _status;
	std::shared_ptr<Session> _workerSession;
	unsigned publications = 0;
	std::atomic<bool> _notify = true, _autoPreparing = false;
	struct Dispatcher { template<class F> void TryEnqueue(F f) { f(); } } _dispatcher;
	struct Event { void Invoke() {} } StatusChanged;
	std::atomic<RtssInitializationState> _magpieInitialization = RtssInitializationState::Unknown;
	bool IsMagpieInitialized() const { return _magpieInitialization == RtssInitializationState::Initialized; }
	explicit RtssService(RtssProfileAccess& access) : _access(access) {}
	bool _Prepare(RtssStatus& status);
	void _RefreshMagpieInitialization(const RtssStatus& status);
	void _Publish(RtssStatus value) { SetCapabilities(value); _status = std::move(value); ++publications; }
	void SetCapabilities(RtssStatus& status) {
		status.readReady = _access.CanRead(); status.applyReady = _access.CanApply(); status.initializeReady = _access.CanInitializeUnlimited();
	}
	RtssStatus Status() const { return _status; }
	RtssStatus SessionStatus() const { return _status; }
	RtssStatus InventoryStatus() const { return _status; }
	void _CheckSession(const std::shared_ptr<Session>& session);
	void _TryApply(const std::shared_ptr<Session>& session);
	bool _PrepareSession(const std::shared_ptr<Session>& session, RtssStatus& status);
	void _PublishSession(const std::shared_ptr<Session>& session, RtssStatus status);
	void StartApply(const std::shared_ptr<Session>& session) { _workerSession = session; _TryApply(session); }
	void _Finish(const std::shared_ptr<Session>& session);
	void _InitializeMagpie();
};
#include "RtssSessionTransactions.g.h"
}

int wmain(int argc, wchar_t** argv) {
	try {
		if (argc != 2) throw std::runtime_error("Expected output directory");
		const auto selfExe = Win32Helper::GetExePath().filename().native();
		const auto selfName = RtssProfileAccess::ProfileName(selfExe);
		const auto output = std::filesystem::path(argv[1]) / ("session-run-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
		{
			Fixture f(output); RtssService service(f.access);
			RtssStatus status;
			Require(service._Prepare(status), "Prepare initialization inventory");
			WriteBytes(f.Path(""), Profile(0));
			WriteBytes(f.Path("game.exe"), Profile(0));
			const auto global = ReadBytes(f.Path("")), game = ReadBytes(f.Path("game.exe"));
			service._RefreshMagpieInitialization(status);
			Require(!service.IsMagpieInitialized(), "Global/game unlimited do not initialize missing self profile");
			WriteBytes(f.Path(selfName), Profile(0));
			service._RefreshMagpieInitialization(status);
			Require(service.IsMagpieInitialized(), "Existing independent unlimited profile is detected");
			RtssService reopened(f.access); reopened._RefreshMagpieInitialization(status);
			Require(reopened.IsMagpieInitialized(), "Fresh service detects persisted initialization without cached success");
			WriteBytes(f.Path(selfName), Profile(60));
			service._RefreshMagpieInitialization(status);
			Require(!service.IsMagpieInitialized(), "External frame cap restores initialization action");
			WriteBytes(f.Path(selfName), Profile(0, 1, 16));
			service._RefreshMagpieInitialization(status);
			Require(!service.IsMagpieInitialized(), "Time cap is not treated as unlimited");
			WriteBytes(f.Path(selfName), "[Framerate]\nLimit=0\n"); f.getFailure = "FrametimeLimit";
			service._RefreshMagpieInitialization(status);
			Require(!service.IsMagpieInitialized(), "Unknown time cap is distinct from known initialization");
			f.getFailure.clear(); WriteBytes(f.Path(selfName), Profile(0));
			service._RefreshMagpieInitialization(status);
			Require(service.IsMagpieInitialized(), "Repaired self profile hides initialization again");
			status.readReady = false; service._RefreshMagpieInitialization(status);
			Require(!service.IsMagpieInitialized(), "Unavailable read capability clears stale initialization");
			Require(f.saves == 0 && f.reloads == 0 && f.deletes == 0 && f.setters.empty() &&
				ReadBytes(f.Path("")) == global && ReadBytes(f.Path("game.exe")) == game,
				"Initialization inventory is read-only and leaves target/global profiles unchanged");
		}
		auto session = [](RtssScope scope, RtssExitPolicy exit) {
			auto s = std::make_shared<RtssService::Session>();
			s->settings.scope = scope; s->settings.exitPolicy = exit;
			s->initialTarget = std::make_shared<TargetIdentity>(); s->target = s->initialTarget;
			s->profile = scope == RtssScope::Global ? "" : "game.exe";
			s->desiredRate = 60;
			// Legacy transaction cases start after preparation; auto lifecycle cases below do not.
			s->prepared = true;
			static const auto owner = std::make_shared<const uint8_t>(uint8_t(0));
			s->identity = owner;
			return s;
		};
		for (const auto scope : {RtssScope::Global, RtssScope::Target}) for (const bool existing : {false, true})
			for (const auto exit : {RtssExitPolicy::KeepChanges, RtssExitPolicy::RestoreOriginal}) {
				Fixture f(output);
				WriteBytes(f.Path(""), Profile(30));
				if (existing) WriteBytes(f.Path("game.exe"), Profile(30));
				RtssService service(f.access);
				const auto s = session(scope, exit);
				service.StartApply(s);
				Require(s->modified && service._status.state == RtssState::Written && s->original.rate[0] == 30, "Session actual write");
				s->settings.rateMode = RtssRateMode::ContentSync; s->desiredRate = 72;
				service.StartApply(s);
				Require(service._status.state == RtssState::Written && s->original.rate[0] == 30 && s->lastWritten.rate[0] == 72, "Sync update preserves first baseline");
				s->cancelled = true;
				service._Finish(s);
				const auto saves = f.saves, deletes = f.deletes, publications = service.publications;
				service._Finish(s);
				Require(s->finished && f.saves == saves && f.deletes == deletes && service.publications == publications, "One cleanup attempt only");
				const auto actual = f.Snapshot(s->profile);
				if (exit == RtssExitPolicy::KeepChanges) Require(service._status.state == RtssState::Kept && actual.exists && actual.rate[0] == 72, "Keep actual saved state");
				else Require(service._status.state == RtssState::Restored && actual.exists == (scope == RtssScope::Global || existing) && actual.rate[0] == 30, "Restore actual baseline/inheritance");
			}
		for (int kind = 0; kind < 7; ++kind) {
			Fixture f(output);
			RtssService service(f.access);
			const auto s = session(kind == 2 ? RtssScope::Target : RtssScope::Global, RtssExitPolicy::RestoreOriginal);
			if (kind == 0) s->cancelled = true;
			if (kind == 1) f.saveMode = SaveMode::Noop;
			if (kind == 2) f.saveMode = SaveMode::Partial;
			if (kind == 3) f.saveMode = SaveMode::BadFile;
			if (kind == 4) f.saveMode = SaveMode::Fault;
			if (kind == 5) f.reject = "FramerateLimit";
			if (kind == 6) f.reloadFault = true;
			service.StartApply(s);
			if (kind == 0) Require(f.saves == 0 && !s->modified, "Cancelled session never writes");
			if (kind == 1) Require(!s->modified && service._status.failureStage == L"Rtss_Failure_Write", "Void unchanged save has no fabricated ownership");
			if (kind == 2) Require(s->modified && service._status.failureStage == L"Rtss_Failure_Verify", "Partial new save retains cleanup ownership");
			if (kind == 6) Require(service._status.profileSaved && service._status.rateKnown && !service._status.initializeReady, "Reload fault keeps verified persisted result and revokes module capability");
			if (kind == 3 || kind == 4 || kind == 6) Require(s->modified, "Ambiguous write retains guarded cleanup");
			if (kind == 5) Require(!s->modified && service._status.failureStage == L"Rtss_Failure_Set" && loggedFailure.property == "FramerateLimit", "Service setter failure mapped and logged");
			service._Finish(s);
			if (kind == 2) Require(service._status.state == RtssState::Restored && !f.Snapshot("game.exe").exists, "Owned partial new profile removed");
			if (kind == 3 || kind == 4 || kind == 6) Require(service._status.state == RtssState::RestoreFailed, "Unverifiable/faulted cleanup reports failure");
			const auto saves = f.saves, deletes = f.deletes;
			service._Finish(s);
			Require(f.saves == saves && f.deletes == deletes, "Failed cleanup is not retried");
		}
		{
			Fixture f(output);
			RtssService service(f.access);
			const auto s = session(RtssScope::Global, RtssExitPolicy::RestoreOriginal);
			service.StartApply(s);
			Require(SetFileAttributesW(f.Path("").c_str(), FILE_ATTRIBUTE_READONLY) != FALSE, "Session restore permission fixture");
			service._Finish(s);
			SetFileAttributesW(f.Path("").c_str(), FILE_ATTRIBUTE_NORMAL);
			Require(service._status.state == RtssState::RestoreFailed && service._status.failureStage == L"Rtss_Failure_Permission" && f.saves == 1,
				"Restore permission failure is precise and performs no save");
			service._Finish(s);
			Require(f.saves == 1 && s->finished, "Permission cleanup is not retried");
		}
		{
			Fixture f(output);
			RtssService service(f.access);
			const auto s = session(RtssScope::Global, RtssExitPolicy::RestoreOriginal);
			service.StartApply(s);
			WriteBytes(f.Path(""), Profile(59));
			service._Finish(s);
			Require(service._status.state == RtssState::Conflict && f.Snapshot("").rate[0] == 59 && f.saves == 1, "Service restore conflict preserves external value");
		}
		for (auto scope : {RtssScope::Global, RtssScope::Target}) for (auto exit : {RtssExitPolicy::KeepChanges, RtssExitPolicy::RestoreOriginal}) {
			Fixture f(output); f.access._version = L"7.3.5.28314";
			WriteBytes(f.Path(""), Profile(30));
			RtssService service(f.access);
			const auto s = session(scope, exit);
			service.StartApply(s);
			const auto original = s->original;
			const auto lastWritten = s->lastWritten;
			const bool created = s->created;
			service._InitializeMagpie();
			Require(service._status.selfInitialization && service._status.profile == selfExe && service._status.state == RtssState::MagpieInitialized, "Initialization result labels its own profile");
			Require(service.IsMagpieInitialized(), "Successful worker initialization hides action immediately");
			Require(s->original == original && s->lastWritten == lastWritten && s->created == created && !s->finished && s->settings.exitPolicy == exit, "Initialization leaves game transaction ownership and policy intact");
			const auto initialized = f.Snapshot(selfName).contents;
			service._InitializeMagpie();
			Require(service._status.state == RtssState::MagpieAlreadyUnlimited, "Worker no-op result");
			s->desiredRate = 72; service.StartApply(s);
			Require(!service._status.selfInitialization && service._status.profile != selfExe, "Next sync update identifies the game transaction");
			service._Finish(s);
			Require(!service._status.selfInitialization && f.Snapshot(selfName).contents == initialized, "Stopping game scaling never restores/deletes self initialization");
		}
		for (int missing = 0; missing < 3; ++missing) {
			Fixture f(output); RtssService service(f.access);
			const auto s = session(missing == 0 ? RtssScope::Target : RtssScope::Global, RtssExitPolicy::RestoreOriginal);
			if (missing == 0) f.access._delete = nullptr;
			service.StartApply(s);
			Require(s->modified && service._status.state == RtssState::Written, "Begin never requires guaranteed future restore");
			if (missing == 1) f.access._save = nullptr;
			if (missing == 2) f.access._set = nullptr;
			service._Finish(s);
			Require(s->finished && s->settings.exitPolicy == RtssExitPolicy::RestoreOriginal && service._status.state == RtssState::RestoreFailed &&
				service._status.failureStage == L"Rtss_Failure_MissingInterface" && f.saves == 1 && f.deletes == 0, "Missing restore interface attempts once without changing policy");
			const auto publications = service.publications;
			service._Finish(s); service.StartApply(s);
			Require(f.saves == 1 && f.deletes == 0 && publications == service.publications, "No delayed restore or reapplication after cleanup");
		}
		{
			Fixture f(output); f.access._running = false; f.access._update = nullptr;
			RtssService service(f.access); RtssStatus prepared;
			Require(service._Prepare(prepared) && prepared.readReady && prepared.initializeReady && !prepared.applyReady && prepared.state == RtssState::NotRunning,
				"Actual prepare exposes offline read/self capabilities independently");
			const auto s = session(RtssScope::Global, RtssExitPolicy::RestoreOriginal);
			service.StartApply(s);
			Require(!s->modified && f.saves == 0 && service._status.state == RtssState::ToolStartFailed, "Failed automatic launch leaves target unchanged");
			service._InitializeMagpie();
			Require(service._status.state == RtssState::MagpieInitialized && service._status.rtss.running == false && !service._status.reloaded && f.reloads == 0,
				"Offline worker self initialization reports saved without claiming reload");
			s->cancelled = true; service._Finish(s); f.access._running = true; f.access._update = Fixture::Update;
			service.StartApply(s);
			Require(!s->modified && f.saves == 0, "Launching tools cannot revive cancelled cap request");
		}
		{
			Fixture f(output); RtssService service(f.access);
			const auto s = session(RtssScope::Global, RtssExitPolicy::RestoreOriginal);
			service.StartApply(s); f.access._running = false; f.access._update = nullptr;
			service._Finish(s);
			Require(service._status.state == RtssState::Restored && !service._status.rtss.running && !service._status.reloaded && f.reloads == 1,
				"Offline stop restores persistent settings once without reload");
		}
		for (auto scope : {RtssScope::Global, RtssScope::Target}) {
			Fixture f(output); RtssService service(f.access);
			const auto s = session(scope, RtssExitPolicy::RestoreOriginal);
			s->initialTarget->exe = selfExe; s->initialTarget->profile = selfName;
			if (scope == RtssScope::Target) s->profile = selfName;
			service._workerSession = s;
			service.StartApply(s);
			Require(!s->modified && f.saves == 0 && service._status.failureStage == L"Rtss_Failure_SelfProfileConflict", "Shared basename conflict is distinct from process identity");
			service._InitializeMagpie();
			Require(service._status.failureStage == L"Rtss_Failure_SelfProfileConflict" && !std::filesystem::exists(f.Path(selfName)), "Self initialization does not override active shared target profile");
		}
		{
			Fixture f(output); RtssService service(f.access);
			const auto s = session(RtssScope::Global, RtssExitPolicy::RestoreOriginal); service.StartApply(s);
			const auto original = ReadBytes(f.Path("")); s->installation = f.directory / "different-installation";
			service._Finish(s);
			Require(service._status.state == RtssState::RestoreFailed && service._status.failureStage == L"Rtss_Failure_InstallationChanged" && f.saves == 1 && original == ReadBytes(f.Path("")),
				"Cleanup never restores another installation");
		}
		// Full automatic preparation uses the production worker path and file transactions.
		for (int selfState : {0, 1, 2}) for (bool running : {false, true}) {
			Fixture f(output); RtssService service(f.access);
			f.access._running = running; launchSuccess = true; launches = 0;
			if (selfState) WriteBytes(f.Path(selfName), Profile(selfState == 1 ? 60 : 0));
			const auto s = session(RtssScope::Target, RtssExitPolicy::RestoreOriginal); s->prepared = false;
			service.StartApply(s);
			Require(s->modified && s->prepared && service._status.state == RtssState::Written, "Auto start/init/apply completes");
			Require(launches == unsigned(!running), "Only absent RTSS process is launched");
			Require(f.Snapshot(selfName).rate[0] == 0 && f.Snapshot("game.exe").rate[0] == 60, "Self unlimited and target cap are separate");
			const auto saves = f.saves, reloads = f.reloads;
			service.StartApply(s);
			Require(f.saves == saves && f.reloads == reloads, "Prepared unchanged session does not rewrite/reload");
			s->cancelled = true; service._Finish(s);
			Require(!std::filesystem::exists(f.Path("game.exe")) && f.Snapshot(selfName).rate[0] == 0, "Stop restores only owned target cap");
		}
		{
			Fixture f(output); RtssService service(f.access); installed = false;
			const auto s = session(RtssScope::Target, RtssExitPolicy::RestoreOriginal); s->prepared = false;
			service.StartApply(s);
			Require(s->suspended && !s->modified && service._status.state == RtssState::NotInstalled, "Missing install suspends cap once");
			const auto publications = service.publications;
			installed = true; service.StartApply(s);
			Require(service.publications == publications && f.saves == 0, "Failure does not retry from repeated notification");
			s->suspended = false; service.StartApply(s);
			Require(s->modified && service._status.state == RtssState::Written, "Explicit retry after installation succeeds");
		}
		{
			Fixture f(output); RtssService service(f.access); f.access._running = false; launchSuccess = true;
			const auto s = session(RtssScope::Target, RtssExitPolicy::RestoreOriginal); s->prepared = false;
			launchHook = [s] { s->cancelled = true; };
			service.StartApply(s); launchHook = {};
			Require(!s->modified && f.saves == 0 && !std::filesystem::exists(f.Path(selfName)), "Cancelled launch cannot initialize or apply");
			const auto publications = service.publications;
			RtssStatus stale; stale.state = RtssState::Written; service._PublishSession(s, stale);
			Require(service.publications == publications, "Cancelled result is never published");
		}
		{
			Fixture f(output); RtssService service(f.access);
			const auto s = session(RtssScope::Target, RtssExitPolicy::RestoreOriginal); s->prepared = false; s->desiredRate = 0;
			service.StartApply(s);
			Require(!s->prepared && f.saves == 0 && f.reloads == 0 && service._status.state == RtssState::WaitingContent, "Source sync waits without preparation or writes");
		}
		for (bool selfFailure : {false, true}) {
			Fixture f(output); RtssService service(f.access);
			const auto s = session(RtssScope::Target, RtssExitPolicy::RestoreOriginal);
			s->prepared = !selfFailure; f.reloadFault = true;
			service.StartApply(s);
			Require(s->suspended && service._status.profileSaved && service._status.state == RtssState::Failed,
				"Verified disk save with reload failure is partial completion");
			const auto saves = f.saves;
			f.reloadFault = false; f.access._faulted = false;
			s->suspended = false; s->prepared = false;
			service.StartApply(s);
			Require(service._status.state == RtssState::Written && service._status.reloaded &&
				!s->reloadPending && !s->selfReloadPending, "Explicit retry reloads and reaches ready");
			Require(f.saves == saves + unsigned(selfFailure), "Retry does not resave an already written target");
			const auto published = service.publications;
			service._workerSession = session(RtssScope::Target, RtssExitPolicy::RestoreOriginal);
			service._PublishSession(s, service._status);
			Require(service.publications == published, "Old generation cannot publish into a newer session");
		}
		{
			Fixture f(output); RtssService service(f.access);
			const auto s = session(RtssScope::Target, RtssExitPolicy::RestoreOriginal);
			service.StartApply(s); const auto saves = f.saves;
			f.access._running = false; launches = 0;
			service._CheckSession(s);
			Require(s->suspended && !s->prepared && service._status.state == RtssState::NotRunning && f.saves == saves && launches == 0,
				"Read-only health watch detects RTSS exit without restarting or rewriting");
			const auto publications = service.publications; s->cancelled = true;
			service._CheckSession(s);
			Require(publications == service.publications, "Cancelled health check is silent");
		}
		launchSuccess = false;
		std::cout << "RTSS production session transactions: " << checks << " checks passed.\n";
		return 0;
	} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
