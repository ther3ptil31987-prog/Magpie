#include <deque>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include "RtssSettings.h"
using namespace Magpie;
using hstring = std::wstring;
struct RtssToolInfo { std::filesystem::path executable; bool running = false; };
#include "RtssPresentationTypes.g.h"
enum class RtssInitializationState { Unknown, Uninitialized, Initialized };
struct Dispatch {
    std::deque<std::function<void()>> tasks;
    template<class F> void TryEnqueue(F f) { tasks.emplace_back(f); }
    void Drain() { while (!tasks.empty()) { auto f = std::move(tasks.front()); tasks.pop_front(); f(); } }
};
struct CounterEvent { unsigned count = 0; void Invoke() { ++count; } };
struct Access {};
void SetAccessStatus(Access&, RtssStatus&) {}
struct RtssService {
    struct Session {
        uint64_t generation = 0;
        std::weak_ptr<const uint8_t> identity;
        RtssSettings settings;
        bool cancelled = false, finished = false;
    };
    mutable std::mutex _mutex;
    RtssStatus _status, _sessionStatus, _managementStatus, _inventoryStatus;
    std::shared_ptr<Session> _uiSession, _workerSession;
    uint64_t _generation = 0;
    bool _notify = true, _recheckPending = false, launching = false, initializing = false, preparing = false;
    RtssInitializationState initialization = RtssInitializationState::Unknown;
    unsigned retries = 0, reads = 0, selfReads = 0, healthReads = 0;
    Access _access;
    Dispatch _dispatcher, worker;
    CounterEvent StatusChanged;
    static RtssService& Get() { static RtssService s; return s; }
    void _Queue(std::function<void()> task) { worker.TryEnqueue(task); }
    void Retry(std::weak_ptr<const uint8_t>) { ++retries; }
    void _Prepare(RtssStatus& s) { ++reads; s = _inventoryStatus; s.inventoryKnown = true; s.manualOperation = s.selfInitialization = false; s.sessionGeneration = 0; s.state = RtssState::WaitingTarget; }
    void _RefreshMagpieInitialization(const RtssStatus&) { ++selfReads; }
    void _CheckSession(const std::shared_ptr<Session>&) { ++healthReads; }
    bool IsRechecking() const { return _recheckPending; }
    bool IsLaunching() const { return launching; }
    bool IsInitializingMagpie() const { return initializing; }
    bool IsPreparing() const { return preparing; }
    RtssInitializationState MagpieInitialization() const { return initialization; }
    RtssStatus Status() const;
    RtssStatus InventoryStatus() const;
    RtssStatus SessionStatus() const;
    RtssStatus ManagementStatus() const;
    RtssStatus CurrentSessionStatus(std::weak_ptr<const uint8_t>) const;
    bool HasSession(std::weak_ptr<const uint8_t>) const noexcept;
    bool IsCurrentStatus(const RtssStatus&, std::weak_ptr<const uint8_t>) const noexcept;
    bool IsLatestStatus(const RtssStatus&, std::weak_ptr<const uint8_t>) const noexcept;
    void _Publish(RtssStatus);
    void _PublishSession(const std::shared_ptr<Session>&, RtssStatus);
    void Recheck(bool);
};
struct ScalingService {
    bool scaling = false;
    bool IsScaling() const { return scaling; }
    static ScalingService& Get() { static ScalingService s; return s; }
};
namespace CommonSharedConstants { constexpr auto APP_RESOURCE_MAP_ID = L"fixture"; }
struct ResourceLoader {
    static ResourceLoader GetForCurrentView(const wchar_t*) { return {}; }
    hstring GetString(std::wstring_view key) const { return hstring(key); }
};
namespace fmt {
auto runtime(std::wstring_view key) { return key; }
auto format(std::wstring_view key, double value) { return std::wstring(key) + L":" + std::to_wstring(value); }
}
struct ProfileData { RtssSettings rtss; std::shared_ptr<const uint8_t> runtimeIdentity = std::make_shared<const uint8_t>(uint8_t{}); };
struct ProfileViewModel {
    ProfileData data;
    ProfileData* _data = &data;
    RtssStatus _rtssPreview;
    bool simple = false, source = false;
    unsigned previews = 0;
    bool IsSimpleFrameRefresh() const { return simple; }
    bool IsRtssManaged() const { return EffectiveRtssSettings(data.rtss, simple).scope != RtssScope::None; }
    bool ShowRtssSyncedRate() const { return EffectiveRtssSettings(data.rtss, simple).rateMode == RtssRateMode::ContentSync; }
    bool IsRtssSyncAvailable() const { return !source; }
    void _LoadRtssPreview() { ++previews; }
    RtssStatus _RtssStatus() const;
    hstring RtssStatusTitle() const;
    hstring RtssStatusDescription() const;
    hstring RtssStatusGlyph() const;
    bool IsRtssBusy() const;
    bool ShowRtssManagement() const;
    bool CanRecheckRtss() const;
    bool CanRetryRtss() const noexcept;
    bool ShowRtssDownload() const noexcept;
    hstring RtssProgramStatus() const;
    hstring RtssSelfStatus() const;
    hstring RtssManagementResult() const;
    bool ShowRtssManagementResult() const;
    void RefreshRtss();
};
#include "RtssPresentationCode.g.h"
static unsigned checks = 0;
static void Require(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
static bool Contains(const std::wstring& value, std::wstring_view part) { return value.find(part) != value.npos; }
int main() {
    try {
        auto& service = RtssService::Get();
        ProfileViewModel vm;
        RtssStatus inventory;
        inventory.state = RtssState::WaitingTarget;
        inventory.inventoryKnown = true;
        inventory.rtss.executable = L"RTSS.exe"; inventory.rtss.running = true;
        inventory.readReady = inventory.applyReady = inventory.initializeReady = true;
        service._Publish(inventory);
        vm._rtssPreview = inventory;
        RtssStatus manual = inventory; manual.selfInitialization = true; manual.profile = L"Magpie.exe";
        manual.state = RtssState::MagpieAlreadyUnlimited; manual.rateKnown = true;
        service._Publish(manual);
        Require(vm._RtssStatus().state == RtssState::Disabled, "manual initialization must not replace disabled");
        Require(vm.RtssStatusTitle() == L"Rtss_Status_Disabled" && vm.ShowRtssManagement(), "disabled linkage still allows dependency management");
        Require(!Contains(vm.RtssStatusDescription(), L"Magpie.exe"), "self profile never becomes target description");
        Require(Contains(vm.RtssManagementResult(), L"Rtss_Status_MagpieInitialized"), "manual result remains available separately");
        vm.data.rtss.scope = RtssScope::Target;
        vm.data.rtss.rateMode = RtssRateMode::ContentSync; vm.source = true;
        Require(vm._RtssStatus().state == RtssState::WaitingContent && vm.ShowRtssManagement(), "source waits for fixed target but keeps management available");
        vm.source = false;
        Require(vm._RtssStatus().state == RtssState::WaitingTarget, "no session waits for start");
        auto session = std::make_shared<RtssService::Session>();
        session->identity = vm.data.runtimeIdentity; session->generation = ++service._generation;
        session->settings.scope = RtssScope::Global;
        service._uiSession = service._workerSession = session;
        Require(vm._RtssStatus().state == RtssState::Checking && vm._RtssStatus().scope == RtssScope::Global, "queued start uses captured policy");
        RtssStatus waiting; waiting.state = RtssState::WaitingContent;
        service._PublishSession(session, waiting);
        Require(vm._RtssStatus().state == RtssState::WaitingContent && vm.RtssProgramStatus() == L"Rtss_Inventory_Running", "waiting without discovery must not erase observed tool inventory");
        RtssStatus target = inventory; target.state = RtssState::Written; target.profile = L"Global";
        target.rateKnown = true; target.currentRate = 72;
        service._PublishSession(session, target);
        vm.data.rtss.scope = RtssScope::None; vm.source = true;
        Require(vm._RtssStatus().state == RtssState::Written, "pending off/source edits do not change active state");
        Require(Contains(vm.RtssStatusDescription(), L"Rtss_ProfileRate:72") && Contains(vm.RtssStatusDescription(), L"Rtss_Detail_Global"), "configured rate and actual scope");
        manual.state = RtssState::Failed; manual.failureStage = L"Rtss_Failure_Reload"; manual.profileSaved = true;
        service._Publish(manual);
        Require(vm._RtssStatus().state == RtssState::Written && !vm.CanRetryRtss(), "manual self failure does not replace active target");
        Require(Contains(vm.RtssManagementResult(), L"Rtss_ProfileSaved") && Contains(vm.RtssManagementResult(), L"Rtss_Failure_Reload"), "partial self result preserves save and reload failure");
        manual.selfInitialization = false; manual.manualOperation = true; manual.profile = L"MSI Afterburner";
        manual.profileSaved = false; manual.failureStage.clear(); manual.state = RtssState::ToolStartFailed;
        service._Publish(manual);
        Require(vm._RtssStatus().state == RtssState::Written && Contains(vm.RtssManagementResult(), L"Rtss_Status_AfterburnerStartFailed"), "optional tool result remains separate");
        manual.state = RtssState::ToolStarted; service._Publish(manual);
        Require(Contains(vm.RtssManagementResult(), L"Rtss_Management_Started"), "tool startup completion is explicit");
        inventory.afterburner.running = false; service._Publish(inventory);
        Require(Contains(vm.RtssManagementResult(), L"Rtss_Management_Started"), "later tool exit does not fabricate a startup failure");
        for (auto state : {RtssState::Checking, RtssState::StartingTool, RtssState::InitializingMagpie, RtssState::Applying}) {
            target.state = state; target.rateKnown = false; service._PublishSession(session, target);
            Require(vm.IsRtssBusy() && vm.RtssStatusGlyph().empty() && !vm.CanRecheckRtss() && !vm.CanRetryRtss(), "preparation disables conflicting actions");
        }
        for (auto state : {RtssState::NotInstalled, RtssState::NotRunning, RtssState::Failed, RtssState::ToolStartFailed, RtssState::Incompatible, RtssState::Conflict}) {
            target.state = state; target.profileSaved = true; target.failureStage = L"Rtss_Failure_Reload";
            target.rtss = inventory.rtss;
            if (state == RtssState::NotInstalled) target.rtss.executable.clear();
            if (state == RtssState::NotRunning) target.rtss.running = false;
            service._PublishSession(session, target);
            Require(vm.CanRetryRtss() == (state != RtssState::Conflict), "retry only actionable failures of current session");
            Require(vm.ShowRtssDownload() == (state == RtssState::NotInstalled), "missing dependency exposes download even if pending preference disabled");
            ScalingService::Get().scaling = false;
            Require(!Contains(vm.RtssStatusDescription(), L"Rtss_ScalingContinues"), "not scaling must not claim continued rendering");
            ScalingService::Get().scaling = true;
            Require(Contains(vm.RtssStatusDescription(), L"Rtss_ScalingContinues") && Contains(vm.RtssStatusDescription(), L"Rtss_ProfileSaved"), "partial target failure retains facts while scaling");
        }
        target.state = RtssState::Failed; service._PublishSession(session, target);
        vm.RefreshRtss(); vm.RefreshRtss();
        Require(service.worker.tasks.size() == 1 && !vm.CanRecheckRtss(), "recheck is queued once and busy");
        service.worker.Drain(); service._dispatcher.Drain();
        Require(service.retries == 0 && service.reads == 1 && service.selfReads == 1 && service.healthReads == 1, "recheck never invokes retry or preparation writes");
        Require(vm._RtssStatus().state == RtssState::Failed && vm.CanRetryRtss(), "inventory recheck preserves retryable target result");
        target.state = RtssState::NotInstalled; target.rtss.executable.clear();
        service._PublishSession(session, target);
        service._Publish(inventory);
        Require(vm._RtssStatus().state == RtssState::Failed && !vm.ShowRtssDownload() && vm.CanRetryRtss(), "newly detected installation offers retry without claiming ready");
        Require(Contains(vm.RtssStatusDescription(), L"Rtss_Detail_RetryReady"), "repaired dependency is not still described as missing");
        target.state = RtssState::Failed; service._PublishSession(session, target);
        session->cancelled = true; service._uiSession.reset();
        target.state = RtssState::Written; service._PublishSession(session, target);
        Require(service.SessionStatus().state == RtssState::Failed, "cancelled result rejected");
        target.state = RtssState::RestoreFailed; target.profileIdentity = session->identity;
        target.sessionGeneration = session->generation; service._Publish(target);
        Require(vm._RtssStatus().state == RtssState::Disabled && Contains(vm.RtssStatusDescription(), L"Rtss_Status_RestoreFailed"), "disabled keeps cleanup failure visible");
        vm.data.rtss.scope = RtssScope::Target; vm.source = false;
        Require(vm._RtssStatus().state == RtssState::RestoreFailed && !vm.CanRetryRtss(), "terminal restore failure shown without retrying an ended target");
        auto next = std::make_shared<RtssService::Session>(); next->identity = vm.data.runtimeIdentity;
        next->generation = ++service._generation; next->settings.scope = RtssScope::Target;
        service._uiSession = service._workerSession = next;
        Require(vm._RtssStatus().state == RtssState::Checking, "old result cannot impersonate new generation");
        service._PublishSession(session, target);
        Require(vm._RtssStatus().state == RtssState::Checking, "late cancelled completion cannot replace pending start");
        service._Publish(inventory);
        ProfileViewModel other; other.data.rtss.scope = RtssScope::Target; other._rtssPreview = inventory;
        Require(other._RtssStatus().state == RtssState::WaitingTarget && !other.CanRetryRtss(), "other profile never inherits session result");
        service.initialization = RtssInitializationState::Unknown;
        Require(vm.RtssSelfStatus() == L"Rtss_Inventory_Unknown", "unknown self inventory is not unlimited");
        service.initialization = RtssInitializationState::Uninitialized;
        Require(vm.RtssSelfStatus() == L"Rtss_Inventory_Uninitialized", "limited/missing self profile needs initialization");
        service.initialization = RtssInitializationState::Initialized;
        Require(vm.RtssSelfStatus() == L"Rtss_Inventory_Unlimited", "verified self inventory reports unlimited");
        std::cout << "RTSS production status projection: " << checks << " checks passed (isolated, no RTSS writes).\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
