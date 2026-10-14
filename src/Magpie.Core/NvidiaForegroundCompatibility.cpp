#include "pch.h"
#include "NvidiaForegroundCompatibility.h"
#include "Logger.h"
#include "StrHelper.h"
#include "VRRPerformance.h"
#include <psapi.h>
#include <intrin.h>
#include <iterator>
#if defined(_M_X64) || defined(_M_IX86)
#include <MinHook.h>
#endif

namespace Magpie {
namespace {
using ForegroundFunction = HWND(WINAPI*)();
std::atomic<ForegroundFunction> originalForeground = &::GetForegroundWindow;
thread_local unsigned realQueryDepth = 0;
PublishedForegroundState published;
std::atomic<ForegroundQueryPhase> queryPhase = ForegroundQueryPhase::Preparing;
std::atomic<uint64_t> targetQueries = 0, appliedQueries = 0;
std::array<std::atomic<uint32_t>, size_t(ForegroundQueryPhase::Count)> observedPhases{};

struct UnknownQueryObservation {
	std::atomic<uintptr_t> page = 0;
	std::atomic<uint32_t> phases = 0;
};
std::array<UnknownQueryObservation, 16> unknownQueries;
struct QueryRecord {
	uint64_t epoch = 0;
	uintptr_t caller = 0, real = 0, result = 0;
	DWORD thread = 0;
	ForegroundQueryPhase phase{};
	bool target = false;
	const char* role = "unknown";
};
struct QuerySlot {
	// Empty -> writer -> ready -> consumer -> empty; no payload data races.
	std::atomic<unsigned> state = 0;
	QueryRecord value{};
};
std::array<QuerySlot, 64> records;
// Frontend-only process cache. Pinned modules cannot unload or change ranges.
std::array<ForegroundCallerRange, 5> pinnedCallers{};

// Frontend-only control. The detour must never read this object.
struct Control {
	ForegroundCompatibilitySnapshot snapshot{};
	bool requested = false, standardVRR = false, windowed = false;
	bool deviceReady = false, nvidia = false;
	ForegroundPresentationPath path = ForegroundPresentationPath::Unknown;
	uint32_t multiplier = 1, luidLow = 0;
	int32_t luidHigh = 0;
	bool potential = false, present = false, suspended = false, retired = true;
	const char* reason = "disabled";
	uint64_t nextPoll = 0, nextModules = 0;
	HWND lastForeground = nullptr;
	bool lastArmed = false;
	uint64_t lastApplied = 0, lastTargets = 0;
	uint64_t performanceSampleTick = 0;
	VRRPerformance::Sample performanceSample{};
	bool referenceActive = false, referenceRestore = false;
	HWND referenceReturn = nullptr;
	uint64_t referenceDeadline = 0;
} control;
bool hookEnabled = false;
#if defined(_M_X64) || defined(_M_IX86)
void* hookTarget = nullptr;
bool hookCreated = false, hookInitialized = false;
#endif

ForegroundWindowIdentity Identity(HWND window) noexcept {
	ForegroundWindowIdentity result;
	result.window = uintptr_t(window);
	DWORD processId = 0;
	result.tid = QueryWindowThreadProcessId(window, processId);
	result.pid = processId;
	result.atom = GetClassLongPtrW(window, GCW_ATOM);
	return result;
}
bool Matches(const ForegroundWindowIdentity& value) noexcept {
	const HWND window = HWND(value.window);
	DWORD pid = 0;
	return window && IsWindow(window) && value.tid && value.pid &&
		QueryWindowThreadProcessId(window, pid) == value.tid && pid == value.pid &&
		GetClassLongPtrW(window, GCW_ATOM) == value.atom;
}
bool MatchesPair(const ForegroundWindowIdentity& first, const ForegroundWindowIdentity& root) noexcept {
	return Matches(first) && (first == root || Matches(root));
}
bool SourceForeground(const ForegroundQuerySnapshot& value, HWND foreground) noexcept {
	// Capture and root can have different PIDs (UWP). A source popup or our
	// parameter host pauses the presentation override while it owns input.
	return foreground && (uintptr_t(foreground) == value.source.window || uintptr_t(foreground) == value.root.window) &&
		MatchesPair(value.source, value.root) &&
		GetAncestor(HWND(value.source.window), GA_ROOT) == HWND(value.root.window) &&
		IsWindowVisible(HWND(value.root.window)) && !IsIconic(HWND(value.root.window)) &&
		IsWindowEnabled(HWND(value.root.window));
}
bool OutputUsable(const ForegroundQuerySnapshot& value) noexcept {
	// Hidden startup presents are allowed. Once shown, background SDK workers
	// must also observe hiding/minimization without waiting for a frontend poll.
	return MatchesPair(value.output, value.outputRoot) &&
		GetAncestor(HWND(value.output.window), GA_ROOT) == HWND(value.outputRoot.window) &&
		(!value.outputShown || (IsWindowVisible(HWND(value.output.window)) &&
			!IsIconic(HWND(value.outputRoot.window))));
}

const char* PathName(ForegroundPresentationPath path) noexcept {
	switch (path) {
	case ForegroundPresentationPath::OrdinaryDXGI: return "dxgi";
	case ForegroundPresentationPath::DLSSFrontendDXGI: return "dlss-frontend-dxgi";
	case ForegroundPresentationPath::XeSSProxy: return "xess-proxy";
	case ForegroundPresentationPath::FSR3Proxy: return "fsr3-proxy";
	case ForegroundPresentationPath::Composition: return "composition";
	default: return "unknown";
	}
}

const char* ForegroundRole(const ForegroundCompatibilitySnapshot& value, HWND foreground) noexcept {
	if (!foreground) return "none";
	if (foreground == HWND(value.source.window) || foreground == HWND(value.root.window)) {
		if (MatchesPair(value.source, value.root) &&
			GetAncestor(HWND(value.source.window), GA_ROOT) == HWND(value.root.window)) return "source";
		return "invalid-source";
	}
	if (Matches(value.outputRoot) && foreground == HWND(value.outputRoot.window)) return "output";
	if (Matches(value.parameterHost) && foreground == HWND(value.parameterHost.window) &&
		GetWindow(foreground, GW_OWNER) == HWND(value.outputRoot.window)) return "parameter-host";
	if (Matches(value.mainUI) && IsWindowInOwnerChain(foreground, HWND(value.mainUI.window)) &&
		!IsWindowInOwnerChain(foreground, HWND(value.outputRoot.window))) return "magpie-main-ui";
	if (Matches(value.root) && IsWindowInOwnerChain(foreground, HWND(value.root.window)) &&
		!IsWindowInOwnerChain(foreground, HWND(value.outputRoot.window))) return "source-popup";
	return "external";
}
#if defined(_M_X64) || defined(_M_IX86)
QuerySlot* Record(const ForegroundQuerySnapshot& value, uintptr_t caller, unsigned index, HWND real) noexcept {
	const auto phase = queryPhase.load(std::memory_order_relaxed);
	if (index < value.callers.size()) {
		const uint32_t bit = uint32_t(1) << index;
		if (observedPhases[size_t(phase)].load(std::memory_order_relaxed) & bit) return nullptr;
		if (observedPhases[size_t(phase)].fetch_or(bit, std::memory_order_relaxed) & bit) return nullptr;
	} else {
		// Keep several unknown call sites: an overlay hit must not mask a later
		// D3D12/SDK hit in the same phase. Observation does not admit the caller.
		const uintptr_t page = caller | uintptr_t(0xfff);
		UnknownQueryObservation* observation = nullptr;
		for (auto& slot : unknownQueries) {
			uintptr_t key = slot.page.load(std::memory_order_relaxed);
			if (!key && slot.page.compare_exchange_strong(key, page)) key = page;
			if (key == page) { observation = &slot; break; }
		}
		const uint32_t bit = uint32_t(1) << size_t(phase);
		if (!observation || (observation->phases.load(std::memory_order_relaxed) & bit) ||
			(observation->phases.fetch_or(bit, std::memory_order_relaxed) & bit)) return nullptr;
	}
	ForegroundCompatibilitySnapshot diagnostic;
	uint64_t diagnosticVersion = 0;
	if (!published.Read(diagnostic, diagnosticVersion) || diagnostic.epoch != value.epoch) return nullptr;
	for (auto& slot : records) {
		unsigned empty = 0;
		if (!slot.state.compare_exchange_strong(empty, 1, std::memory_order_acquire)) continue;
		slot.value = { value.epoch, caller, uintptr_t(real), 0, GetCurrentThreadId(), phase,
			index < value.callers.size(), ForegroundRole(diagnostic, real) };
		return &slot;
	}
	return nullptr;
}
HWND WINAPI DetourForegroundWindow() noexcept {
	const HWND real = originalForeground.load(std::memory_order_acquire)();
	const DWORD error = GetLastError();
	if (realQueryDepth || !published.QueriesEnabled()) return real;
	++realQueryDepth;
	ForegroundQuerySnapshot value;
	uint64_t version = 0;
	unsigned index = 0;
	HWND result = real;
	const uintptr_t caller = uintptr_t(_ReturnAddress());
	if (published.ReadQuery(caller, value, version, index)) {
		const bool target = index < value.callers.size();
		if (value.diagnostics && target) targetQueries.fetch_add(1, std::memory_order_relaxed);
		if (value.mode == ForegroundCompatibilityMode::Query && value.armed && target &&
			SourceForeground(value, real) && OutputUsable(value) && published.Current(version))
			result = HWND(value.output.window);
		QuerySlot* record = value.diagnostics ? Record(value, caller, index, real) : nullptr;
		// A retiring/rebuilding frontend invalidates an in-flight decision.
		if (!published.Current(version) || (result != real &&
			(originalForeground.load(std::memory_order_acquire)() != real || !published.Current(version))))
			result = real;
		if (value.diagnostics && result != real) appliedQueries.fetch_add(1, std::memory_order_relaxed);
		if (record) {
			record->value.result = uintptr_t(result);
			record->state.store(2, std::memory_order_release);
		}
	}
	--realQueryDepth;
	SetLastError(error);
	return result;
}
#endif
bool InstallHook() noexcept {
#if defined(_M_X64) || defined(_M_IX86)
	MH_STATUS status = MH_OK;
	if (!hookInitialized) {
		status = MH_Initialize();
		if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
			Logger::Get().Warn(fmt::format("G-SYNC foreground: initialize hook failed: {}", MH_StatusToString(status)));
			return false;
		}
		hookInitialized = true;
	}
	if (!hookCreated) {
		void* original = nullptr;
		status = MH_CreateHookApiEx(L"user32.dll", "GetForegroundWindow",
			reinterpret_cast<void*>(&DetourForegroundWindow), &original, &hookTarget);
		if (status != MH_OK) {
			Logger::Get().Warn(fmt::format("G-SYNC foreground: create hook failed: {}", MH_StatusToString(status)));
			return false;
		}
		originalForeground.store(reinterpret_cast<ForegroundFunction>(original), std::memory_order_release);
		hookCreated = true;
	}
	if (!hookEnabled) {
		status = MH_EnableHook(hookTarget);
		if (status != MH_OK) {
			Logger::Get().Warn(fmt::format("G-SYNC foreground: enable hook failed: {}", MH_StatusToString(status)));
			return false;
		}
		hookEnabled = true;
	}
	return true;
#else
	Logger::Get().Warn("G-SYNC foreground: API hook unavailable on this architecture");
	return false;
#endif
}
bool RefreshModules() noexcept {
	control.nextModules = GetTickCount64() + 5000;
	// Exact candidate modules, never a filename prefix or all foreign callers.
	// Explicit diagnostics report API hits; membership is not driver proof.
	static constexpr std::array names{ L"nvwgf2umx.dll", L"nvwgf2um.dll", L"nvldumdx.dll", L"dxgi.dll", L"d3d11.dll" };
	static_assert(names.size() == pinnedCallers.size());
	for (size_t i = 0; i < names.size(); ++i) {
		if (pinnedCallers[i].size) continue;
		HMODULE module = nullptr;
		// Look up only missing exact candidates; never enumerate every DLL.
		// PIN precedes querying the range, so unload/reuse cannot invalidate it.
		if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, names[i], &module)) continue;
		MODULEINFO info{};
		if (K32GetModuleInformation(GetCurrentProcess(), module, &info, DWORD(sizeof(info))) && info.SizeOfImage)
			pinnedCallers[i] = { uintptr_t(info.lpBaseOfDll), info.SizeOfImage };
	}
	const bool changed = control.snapshot.callers != pinnedCallers;
	control.snapshot.callers = pinnedCallers;
	return changed;
}
void Publish() noexcept {
	control.snapshot.armed = control.snapshot.mode != ForegroundCompatibilityMode::Off && control.standardVRR &&
		!control.retired && !control.suspended && control.deviceReady && control.nvidia &&
		SupportsForegroundQuery(control.path) && control.potential &&
		MatchesPair(control.snapshot.source, control.snapshot.root) &&
		MatchesPair(control.snapshot.output, control.snapshot.outputRoot);
	published.Publish(control.snapshot);
}
void DrainRecords() noexcept {
	for (auto& slot : records) {
		unsigned ready = 2;
		if (!slot.state.compare_exchange_strong(ready, 3, std::memory_order_acquire)) continue;
		const auto record = slot.value;
		slot.state.store(0, std::memory_order_release);
		HMODULE module = nullptr;
		wchar_t path[MAX_PATH]{};
		std::wstring_view name = L"unknown";
		if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(record.caller), &module) && GetModuleFileNameW(module, path, MAX_PATH)) {
			name = path;
			if (const auto slash = name.find_last_of(L"\\/"); slash != name.npos) name.remove_prefix(slash + 1);
		}
		Logger::Get().Info(fmt::format(
			"G-SYNC foreground query: epoch={} phase={} module={} offset=0x{:x} thread={} target={} real=0x{:x} returned=0x{:x} realRole={}",
			record.epoch, uint32_t(record.phase), StrHelper::UTF16ToUTF8(name), record.caller - uintptr_t(module),
			record.thread, record.target, record.real, record.result, record.role));
	}
}
void LogState(const char* event) noexcept {
	const HWND real = NvidiaForegroundCompatibility::RealForegroundWindow();
	const auto targets = targetQueries.load(std::memory_order_relaxed), applied = appliedQueries.load(std::memory_order_relaxed);
	DWORD pid = 0;
	QueryWindowThreadProcessId(real, pid);
	Logger::Get().Info(fmt::format(
		"G-SYNC foreground: event={} epoch={} mode={} requested={} installed={} armed={} appliedQueries={} targetQueries={} "
		"active=unknown reason={} real=0x{:x} realPid={} source=0x{:x} root=0x{:x} output=0x{:x} sourceForeground={} "
		"outputOwner=0x{:x} outputExStyle=0x{:x} outputPrevious=0x{:x} outputRoot=0x{:x} "
		"realRole={} mainUI=0x{:x} parameterHost=0x{:x} presenter={} multiplier={} windowed={} diagnostics={} "
		"adapterLuid={:x}:{:x} overrideEligible={}", event, control.snapshot.epoch, uint32_t(control.snapshot.mode),
		control.requested, hookEnabled, control.snapshot.armed, applied, targets,
		control.suspended ? "parameter-interaction" : control.reason, uintptr_t(real), pid,
		control.snapshot.source.window, control.snapshot.root.window, control.snapshot.output.window,
		SourceForeground(control.snapshot, real), uintptr_t(GetWindow(HWND(control.snapshot.output.window), GW_OWNER)),
		uintptr_t(GetWindowLongPtrW(HWND(control.snapshot.output.window), GWL_EXSTYLE)),
		uintptr_t(GetWindow(HWND(control.snapshot.outputRoot.window), GW_HWNDPREV)), control.snapshot.outputRoot.window,
		ForegroundRole(control.snapshot, real), control.snapshot.mainUI.window, control.snapshot.parameterHost.window,
		PathName(control.path), control.multiplier, control.windowed, control.snapshot.diagnostics, uint32_t(control.luidHigh), control.luidLow,
		control.snapshot.armed && SourceForeground(control.snapshot, real) && OutputUsable(control.snapshot)));
	control.lastForeground = real;
	control.lastArmed = control.snapshot.armed;
	control.lastApplied = applied;
	control.lastTargets = targets;
}
void LogPerformance(uint64_t now, bool final = false) noexcept {
	if (!VRRPerformance::Enabled()) return;
	const uint64_t elapsed = now - control.performanceSampleTick;
	if (!final && elapsed < 5000) return;
	const auto sample = VRRPerformance::Read();
	std::string fields;
	for (size_t i = 0; i < sample.size(); ++i)
		fmt::format_to(std::back_inserter(fields), " {}={}", VRRPerformance::Names[i], sample[i] - control.performanceSample[i]);
	Logger::Get().Info(fmt::format(
		"VRR performance: epoch={} requested={} mode={} diagnostics={} presenter={} multiplier={} windowMs={} final={} "
		"gpuTiming=unmeasured displayFrames=unmeasured{}",
		control.snapshot.epoch, control.standardVRR, uint32_t(control.snapshot.mode), control.snapshot.diagnostics,
		PathName(control.path), control.multiplier, elapsed, final, fields));
	control.performanceSample = sample;
	control.performanceSampleTick = now;
}
void EndReference(bool restore) noexcept {
	const HWND output = HWND(control.snapshot.output.window);
	control.referenceActive = false;
	KillTimer(output, NvidiaForegroundCompatibility::ReferenceTimer);
	if (restore && NvidiaForegroundCompatibility::RealForegroundWindow() == output &&
		SourceForeground(control.snapshot, control.referenceReturn))
		SetForegroundWindow(control.referenceReturn);
	control.referenceRestore = false;
	control.referenceReturn = nullptr;
	control.reason = "reference-ended";
	LogState("reference-end");
}
}

HWND NvidiaForegroundCompatibility::RealForegroundWindow() noexcept {
	++realQueryDepth;
	const HWND result = originalForeground.load(std::memory_order_acquire)();
	--realQueryDepth;
	return result;
}
void NvidiaForegroundCompatibility::Prepare(const SourceWindowIdentity& source, HWND output, uint64_t epoch,
	bool standardVRR, bool windowed, const SessionWindowIdentity& mainUI) noexcept {
	control = {};
	control.retired = false;
	control.requested = standardVRR;
	control.standardVRR = standardVRR;
	control.windowed = windowed;
	control.snapshot.epoch = epoch;
	control.snapshot.source = { uintptr_t(source.capture), source.capturePid, source.captureTid, source.captureClass };
	control.snapshot.root = { uintptr_t(source.root), source.rootPid, source.rootTid, source.rootClass };
	control.snapshot.output = Identity(output);
	control.snapshot.outputRoot = Identity(GetAncestor(output, GA_ROOT));
	if (mainUI.Matches() && mainUI.pid == GetCurrentProcessId())
		control.snapshot.mainUI = { uintptr_t(mainUI.window), mainUI.pid, mainUI.tid, mainUI.atom };
	control.snapshot.mode = standardVRR ? ForegroundCompatibilityMode::Query : ForegroundCompatibilityMode::Off;
	wchar_t diagnostic[32]{};
	const DWORD length = GetEnvironmentVariableW(L"MAGPIE_GSYNC_DIAGNOSTIC", diagnostic, DWORD(std::size(diagnostic)));
	if (length && length < std::size(diagnostic)) {
		if (!_wcsicmp(diagnostic, L"standard")) control.snapshot.mode = ForegroundCompatibilityMode::Off;
		else if (!_wcsicmp(diagnostic, L"trace")) control.snapshot.mode = ForegroundCompatibilityMode::Trace;
		else if (!_wcsicmp(diagnostic, L"reference")) control.snapshot.mode = ForegroundCompatibilityMode::Reference;
		else if (!_wcsicmp(diagnostic, L"profile")) control.snapshot.diagnostics = true;
	}
	control.snapshot.diagnostics |= control.snapshot.mode == ForegroundCompatibilityMode::Trace;
	wchar_t performance[2]{};
	const bool performanceEnabled = GetEnvironmentVariableW(L"MAGPIE_VRR_PERFORMANCE", performance, DWORD(std::size(performance))) == 1 &&
		performance[0] == L'1';
	VRRPerformance::Start(performanceEnabled);
	control.performanceSampleTick = GetTickCount64();
	control.snapshot.diagnostics |= performanceEnabled;
	control.requested = control.snapshot.mode != ForegroundCompatibilityMode::Off;
	control.reason = control.requested ? "initializing-device" : "disabled";
	if (!standardVRR) {
		control.snapshot.mode = ForegroundCompatibilityMode::Off;
		control.reason = "standard-vrr-disabled";
	}
	targetQueries.store(0); appliedQueries.store(0);
	for (auto& mask : observedPhases) mask.store(0);
	for (auto& slot : unknownQueries) { slot.page.store(0); slot.phases.store(0); }
	Phase(ForegroundQueryPhase::Preparing);
	if (!control.requested || control.snapshot.mode == ForegroundCompatibilityMode::Off) {
		published.Publish(control.snapshot);
		if (control.requested) LogState("prepare");
		return;
	}
	if (control.snapshot.mode != ForegroundCompatibilityMode::Reference) RefreshModules();
	published.Publish(control.snapshot);
	if ((control.snapshot.mode == ForegroundCompatibilityMode::Trace || control.snapshot.mode == ForegroundCompatibilityMode::Query) &&
		!InstallHook()) {
		control.snapshot.mode = ForegroundCompatibilityMode::Off;
		control.reason = "hook-failed";
		published.Publish(control.snapshot);
	}
	if (control.requested) LogState("prepare");
}
void NvidiaForegroundCompatibility::DeviceInitialized(uint32_t vendor, uint32_t luidLow, int32_t luidHigh,
	ForegroundPresentationPath path, uint32_t multiplier) noexcept {
	if (control.retired) return;
	control.deviceReady = true;
	control.nvidia = vendor == 0x10de;
	control.path = path;
	control.multiplier = multiplier;
	control.luidLow = luidLow; control.luidHigh = luidHigh;
	if (!control.requested || control.snapshot.mode == ForegroundCompatibilityMode::Off) return;
	control.reason = !control.nvidia ? "non-nvidia-output" : !SupportsForegroundQuery(path) ?
		"presenter-path-unavailable" : "initializing-swap-chain";
	if (!control.nvidia && control.snapshot.mode == ForegroundCompatibilityMode::Query)
		control.snapshot.mode = ForegroundCompatibilityMode::Off;
	if (control.snapshot.mode != ForegroundCompatibilityMode::Reference) RefreshModules();
	Publish();
	LogState("device-initialized");
}
bool NvidiaForegroundCompatibility::BindPresentationWindow(HWND window) noexcept {
	if (control.retired || uintptr_t(window) != control.snapshot.output.window || !Matches(control.snapshot.output)) {
		Reject("swap-chain-window-mismatch");
		return false;
	}
	if (control.requested) LogState("swap-chain-window-bound");
	return true;
}
void NvidiaForegroundCompatibility::FrontendWindows(HWND parameterHost, HWND realForeground) noexcept {
	if (control.retired) return;
	if (uintptr_t(parameterHost) != control.snapshot.parameterHost.window) {
		control.snapshot.parameterHost = parameterHost ? Identity(parameterHost) : ForegroundWindowIdentity{};
		// Only diagnostic role classification uses this identity.
		published.Publish(control.snapshot);
	}
	// Also retain native-focus controls when VRR is off. Only transitions log.
	if (realForeground != control.lastForeground) LogState("foreground-change");
}
void NvidiaForegroundCompatibility::MainUIChanged(const SessionWindowIdentity& identity) noexcept {
	if (control.retired) return;
	ForegroundWindowIdentity mainUI;
	if (identity.Matches() && identity.pid == GetCurrentProcessId())
		mainUI = { uintptr_t(identity.window), identity.pid, identity.tid, identity.atom };
	if (mainUI == control.snapshot.mainUI) return;
	control.snapshot.mainUI = mainUI;
	Publish();
	LogState("main-ui-identity");
}
void NvidiaForegroundCompatibility::PresentationState(bool canCreateTearing, bool canPresentTearing) noexcept {
	if (control.retired || !control.requested || control.snapshot.mode == ForegroundCompatibilityMode::Off) return;
	const bool changed = control.potential != canCreateTearing || control.present != canPresentTearing;
	control.potential = canCreateTearing;
	control.present = canPresentTearing;
	if (control.nvidia && SupportsForegroundQuery(control.path))
		control.reason = canCreateTearing ? "presentation-query-ready" : "vrr-path-unavailable";
	if (changed) {
		if (canCreateTearing && control.snapshot.mode != ForegroundCompatibilityMode::Reference) RefreshModules();
		Publish(); LogState("presentation-state");
	}
}
void NvidiaForegroundCompatibility::Reject(const char* reason) noexcept {
	if (control.retired || !control.requested || control.snapshot.mode == ForegroundCompatibilityMode::Off) return;
	control.snapshot.mode = ForegroundCompatibilityMode::Off;
	control.snapshot.armed = false;
	control.reason = reason;
	published.Publish(control.snapshot);
	LogState("session-rejected");
}
void NvidiaForegroundCompatibility::Phase(ForegroundQueryPhase phase) noexcept {
	if (!control.retired && control.snapshot.diagnostics && control.snapshot.mode != ForegroundCompatibilityMode::Off)
		queryPhase.store(phase, std::memory_order_relaxed);
}
void NvidiaForegroundCompatibility::FrontendSuspended(bool suspended) noexcept {
	if (control.retired || !control.requested || control.suspended == suspended) return;
	control.suspended = suspended;
	Publish();
	LogState(suspended ? "frontend-suspend" : "frontend-resume");
}
void NvidiaForegroundCompatibility::OutputShown() noexcept {
	if (control.retired) return;
	control.snapshot.outputShown = true;
	Publish();
	if (!control.requested) return;
	LogState("first-output-shown");
	if (control.snapshot.mode != ForegroundCompatibilityMode::Reference || !control.snapshot.armed || !control.present) return;
	const HWND real = RealForegroundWindow();
	if (!SourceForeground(control.snapshot, real)) return;
	control.referenceReturn = real;
	control.referenceDeadline = GetTickCount64() + 10000;
	control.referenceActive = true;
	const HWND output = HWND(control.snapshot.output.window);
	if (!SetTimer(output, ReferenceTimer, 100, nullptr) || !SetForegroundWindow(output)) {
		EndReference(true);
		return;
	}
	LogState("reference-begin-10s");
}
bool NvidiaForegroundCompatibility::ReferenceOwnsForeground() noexcept {
	return control.referenceActive && GetTickCount64() < control.referenceDeadline &&
		Matches(control.snapshot.output) && RealForegroundWindow() == HWND(control.snapshot.output.window);
}
void NvidiaForegroundCompatibility::Poll() noexcept {
	if (control.retired || ((!control.requested || control.snapshot.mode == ForegroundCompatibilityMode::Off) &&
		!VRRPerformance::Enabled())) return;
	const uint64_t now = GetTickCount64();
	LogPerformance(now);
	if (!control.requested || control.snapshot.mode == ForegroundCompatibilityMode::Off) return;
	if (control.referenceActive && (!ReferenceOwnsForeground() || !control.snapshot.armed || !OutputUsable(control.snapshot))) EndReference(true);
	if (now < control.nextPoll) return;
	control.nextPoll = now + 1000;
	if (now >= control.nextModules && control.snapshot.mode != ForegroundCompatibilityMode::Reference && RefreshModules()) Publish();
	if (control.snapshot.diagnostics) DrainRecords();
	if (RealForegroundWindow() != control.lastForeground || control.lastArmed != control.snapshot.armed ||
		(control.snapshot.diagnostics && (appliedQueries.load() != control.lastApplied ||
			targetQueries.load() != control.lastTargets))) LogState("sample");
}
void NvidiaForegroundCompatibility::Retire() noexcept {
	if (control.retired) return;
	control.retired = true;
	control.snapshot.mode = ForegroundCompatibilityMode::Off;
	control.snapshot.armed = false;
	control.referenceRestore = control.referenceActive;
	control.referenceActive = false;
	control.reason = "retired";
	published.Publish(control.snapshot);
}
void NvidiaForegroundCompatibility::ReleaseReferenceFocus() noexcept {
	if (control.referenceRestore) EndReference(true);
}
void NvidiaForegroundCompatibility::FinishRetire() noexcept {
	LogPerformance(GetTickCount64(), true);
	VRRPerformance::Stop();
	if (control.requested) {
		if (control.snapshot.diagnostics) DrainRecords();
		LogState("retire-complete");
	}
#if defined(_M_X64) || defined(_M_IX86)
	if (hookEnabled) {
		const MH_STATUS status = MH_DisableHook(hookTarget);
		if (status == MH_OK) hookEnabled = false;
		else Logger::Get().Warn(fmt::format("G-SYNC foreground: disable hook failed, passthrough retained: {}", MH_StatusToString(status)));
	}
#endif
	// Keep one disabled trampoline until process exit. UI queries or late UMD
	// workers may still hold it; removing/freeing it here would be unsafe.
}

}
