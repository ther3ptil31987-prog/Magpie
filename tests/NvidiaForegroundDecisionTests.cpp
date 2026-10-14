// Filled with production decisions by prepare_nvidia_foreground_test.py.
// Native metadata and API endpoints are doubles; no live focus or hook changes.
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include "ForegroundCompatibilityState.h"
#include "VRRPerformance.h"
#include <cassert>
#include <iostream>
#include <string_view>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <utility>
#include <memory>

namespace Magpie {
static const HWND fakeCapture = HWND(uintptr_t(1)), fakeRoot = HWND(uintptr_t(2)), fakeOutput = HWND(uintptr_t(3)), other = HWND(uintptr_t(4));
static const HWND fakeMainUI = HWND(uintptr_t(5)), parameter = HWND(uintptr_t(6)), fakePopup = HWND(uintptr_t(7));
struct NativeWindow { DWORD pid, tid; uintptr_t atom; HWND root; bool visible = true, enabled = true, iconic = false; HWND owner = nullptr; };
static std::unordered_map<HWND, NativeWindow> native{
	{fakeCapture, {10, 11, 101, fakeRoot}}, {fakeRoot, {20, 21, 102, fakeRoot}}, {fakeOutput, {77, 78, 103, fakeOutput}},
	{other, {99, 100, 104, other}}, {fakeMainUI, {77, 79, 105, fakeMainUI}},
	{parameter, {77, 78, 106, parameter, true, true, false, fakeOutput}},
	{fakePopup, {20, 21, 107, fakePopup, true, true, false, fakeRoot}}
};
static std::atomic<HWND> fakeForeground = fakeRoot;
static std::atomic<bool> changeDuringIdentity = false;
static bool retireDuringRecord = false;
static thread_local uintptr_t fakeCaller = 0x1010;
static thread_local unsigned windowApiCalls = 0;
static std::atomic<bool> retireDuringIdentity = false;
static std::atomic<bool> retireDuringFinalForeground = false;
static thread_local unsigned foregroundReads = 0;
static bool rejectSourcePid = false, denySourceThread = false;
static std::atomic<unsigned> threadOpens = 0, threadCloses = 0;
static void RetireWhileReading();
static BOOL FakeIsWindow(HWND window) { ++windowApiCalls; return native.contains(window); }
static DWORD FakeThread(HWND window, DWORD* pid) {
	++windowApiCalls;
	const auto it = native.find(window);
	SetLastError(1234);
	if (it == native.end()) return 0;
	if (pid && rejectSourcePid && (window == fakeCapture || window == fakeRoot)) {
		*pid = 0; SetLastError(0); return 0;
	}
	if (pid) *pid = it->second.pid;
	return it->second.tid;
}
static HANDLE FakeOpenThread(DWORD access, BOOL inherit, DWORD tid) {
	assert(access == THREAD_QUERY_LIMITED_INFORMATION && !inherit);
	++threadOpens;
	if (denySourceThread) { SetLastError(ERROR_ACCESS_DENIED); return nullptr; }
	return HANDLE(uintptr_t(tid));
}
static DWORD FakeThreadOwner(HANDLE handle) {
	for (const auto& [window, value] : native) {
		(void)window;
		if (value.tid == uintptr_t(handle)) return value.pid;
	}
	return 0;
}
static BOOL FakeCloseThread(HANDLE handle) { assert(handle); ++threadCloses; return TRUE; }
static ULONG_PTR FakeAtom(HWND window, int) {
	++windowApiCalls;
	if (window == fakeOutput && changeDuringIdentity.exchange(false)) fakeForeground = other;
	if (window == fakeOutput && retireDuringIdentity.exchange(false)) RetireWhileReading();
	return native.at(window).atom;
}
static HWND FakeAncestor(HWND window, UINT) { ++windowApiCalls; return native.at(window).root; }
static BOOL FakeVisible(HWND window) { ++windowApiCalls; return native.at(window).visible; }
static BOOL FakeEnabled(HWND window) { ++windowApiCalls; return native.at(window).enabled; }
static BOOL FakeIconic(HWND window) { ++windowApiCalls; return native.at(window).iconic; }
static HWND WINAPI FakeForeground() {
	if (++foregroundReads == 2 && retireDuringFinalForeground.exchange(false)) RetireWhileReading();
	SetLastError(73); return fakeForeground.load();
}
static void* FakeCaller() { return reinterpret_cast<void*>(fakeCaller); }
static HWND FakeOwner(HWND window, UINT relation) {
	const auto it = native.find(window);
	return relation == GW_OWNER && it != native.end() ? it->second.owner : nullptr;
}
static std::wstring fakeDiagnostic;
static bool fakePerformance = false;
static DWORD FakeEnvironment(const wchar_t* name, wchar_t* buffer, DWORD size) {
	const std::wstring_view value = std::wstring_view(name) == L"MAGPIE_VRR_PERFORMANCE" ?
		(fakePerformance ? L"1" : L"") : std::wstring_view(fakeDiagnostic);
	if (value.size() >= size) return DWORD(value.size() + 1);
	std::copy(value.begin(), value.end(), buffer);
	buffer[value.size()] = 0;
	return DWORD(value.size());
}
static std::array<bool, 5> loadedModules{ true, true, true, true, true };
static std::array<unsigned, 5> moduleLookups{}, moduleInfoReads{};
static BOOL FakeModuleHandle(DWORD flags, LPCWSTR name, HMODULE* module) {
	assert(flags == GET_MODULE_HANDLE_EX_FLAG_PIN);
	static constexpr std::array names{ L"nvwgf2umx.dll", L"nvwgf2um.dll", L"nvldumdx.dll", L"dxgi.dll", L"d3d11.dll" };
	for (size_t i = 0; i < names.size(); ++i) if (std::wstring_view(name) == names[i]) {
		++moduleLookups[i];
		*module = loadedModules[i] ? HMODULE(uintptr_t((i + 1) * 0x1000)) : nullptr;
		return loadedModules[i];
	}
	assert(false); return FALSE;
}
static BOOL FakeModuleInformation(HANDLE, HMODULE module, MODULEINFO* info, DWORD size) {
	assert(size == sizeof(*info));
	const size_t i = uintptr_t(module) / 0x1000 - 1;
	assert(i < moduleInfoReads.size()); ++moduleInfoReads[i];
	info->lpBaseOfDll = module; info->SizeOfImage = 0x100; return TRUE;
}

#define IsWindow FakeIsWindow
#define GetWindowThreadProcessId FakeThread
#define OpenThread FakeOpenThread
#define GetProcessIdOfThread FakeThreadOwner
#define CloseHandle FakeCloseThread
#define GetClassLongPtrW FakeAtom
#define GetAncestor FakeAncestor
#define IsWindowVisible FakeVisible
#define IsWindowEnabled FakeEnabled
#define IsIconic FakeIconic
#define _ReturnAddress FakeCaller
#define GetWindow FakeOwner
#define GetCurrentProcessId() 77
#define GetEnvironmentVariableW FakeEnvironment
#define GetModuleHandleExW FakeModuleHandle
#define K32GetModuleInformation FakeModuleInformation
}
#include "include/SourceWindowRelations.h"
namespace Magpie {
struct SourceWindowIdentity { HWND capture, root; DWORD capturePid, captureTid; uintptr_t captureClass;
	DWORD rootPid, rootTid; uintptr_t rootClass; };
using ForegroundFunction = HWND(WINAPI*)();
static std::atomic<ForegroundFunction> originalForeground = &FakeForeground;
static thread_local unsigned realQueryDepth = 0;
static PublishedForegroundState published;
static void RetireWhileReading() { published.Publish({}); }
static std::atomic<uint64_t> targetQueries = 0, appliedQueries = 0;
static std::atomic<ForegroundQueryPhase> queryPhase = ForegroundQueryPhase::Preparing;
static std::array<std::atomic<uint32_t>, size_t(ForegroundQueryPhase::Count)> observedPhases{};
/* PRODUCTION_QUERY_RECORDS */
QuerySlot* RecordCandidate(const ForegroundQuerySnapshot&, uintptr_t, unsigned, HWND) noexcept;
static QuerySlot* Record(const ForegroundQuerySnapshot& value, uintptr_t caller, unsigned index, HWND real) {
	if (retireDuringRecord) { published.Publish({}); retireDuringRecord = false; }
	return RecordCandidate(value, caller, index, real);
}
struct Logger { static Logger& Get() { static Logger value; return value; } void Warn(std::string_view) {} };
namespace fmt { template<class... T> const char* format(const char* value, T...) { return value; } }
enum MH_STATUS { MH_OK, MH_ERROR_ALREADY_INITIALIZED, MH_ERROR_ALREADY_CREATED, MH_ERROR_MEMORY_PROTECT };
static MH_STATUS initializeResult = MH_OK, createResult = MH_OK, enableResult = MH_OK, disableResult = MH_OK;
static unsigned initializeCalls = 0, createCalls = 0, enableCalls = 0, disableCalls = 0;
static void* hookTarget = nullptr;
static bool hookCreated = false, hookEnabled = false, hookInitialized = false;
static MH_STATUS MH_Initialize() { ++initializeCalls; return initializeResult; }
static MH_STATUS MH_CreateHookApiEx(const wchar_t*, const char*, void*, void** original, void** target) {
	++createCalls; *original = reinterpret_cast<void*>(&FakeForeground); *target = reinterpret_cast<void*>(uintptr_t(0x100));
	return createResult;
}
static MH_STATUS MH_EnableHook(void* target) { assert(target == hookTarget && target); ++enableCalls; return enableResult; }
static MH_STATUS MH_DisableHook(void* target) { assert(target == hookTarget && target); ++disableCalls; return disableResult; }
static const char* MH_StatusToString(MH_STATUS) { return "fake-status"; }
/* PRODUCTION_FOREGROUND_CONTROL */
static void DrainRecords() {}
static void LogState(const char*) {}
static void LogPerformance(uint64_t, bool) {}
struct NvidiaForegroundCompatibility {
	static HWND RealForegroundWindow() noexcept;
	static void Prepare(const SourceWindowIdentity&, HWND, uint64_t, bool, bool, const SessionWindowIdentity&) noexcept;
	static void DeviceInitialized(uint32_t, uint32_t, int32_t, ForegroundPresentationPath, uint32_t) noexcept;
	static bool BindPresentationWindow(HWND) noexcept;
	static void PresentationState(bool, bool) noexcept;
	static void Reject(const char*) noexcept;
	static void FrontendSuspended(bool) noexcept;
	static void MainUIChanged(const SessionWindowIdentity&) noexcept;
	static void Phase(ForegroundQueryPhase) noexcept;
	static void Retire() noexcept;
	static void FinishRetire() noexcept;
};
/* PRODUCTION_FOREGROUND_DECISIONS */

static ForegroundCompatibilitySnapshot Session() {
	ForegroundCompatibilitySnapshot value;
	value.epoch = 1;
	value.mode = ForegroundCompatibilityMode::Query;
	value.armed = true;
	value.diagnostics = true;
	value.source = {uintptr_t(fakeCapture), 10, 11, 101};
	value.root = {uintptr_t(fakeRoot), 20, 21, 102};
	value.output = {uintptr_t(fakeOutput), 77, 78, 103};
	value.outputRoot = value.output;
	value.mainUI = {uintptr_t(fakeMainUI), 77, 79, 105};
	value.parameterHost = {uintptr_t(parameter), 77, 78, 106};
	value.callers[0] = {0x1000, 0x100};
	return value;
}
}
int main() {
	using namespace Magpie;
	initializeResult = MH_ERROR_MEMORY_PROTECT;
	assert(!InstallHook() && !hookInitialized && !createCalls && !enableCalls);
	initializeResult = MH_ERROR_ALREADY_INITIALIZED; createResult = MH_ERROR_ALREADY_CREATED;
	assert(!InstallHook() && hookInitialized && !hookCreated && !enableCalls);
	createResult = MH_OK; enableResult = MH_ERROR_MEMORY_PROTECT;
	assert(!InstallHook() && hookCreated && !hookEnabled);
	const auto previousCreateCalls = createCalls;
	enableResult = MH_OK;
	assert(InstallHook() && hookEnabled && createCalls == previousCreateCalls);
	const auto previousEnableCalls = enableCalls;
	assert(InstallHook() && enableCalls == previousEnableCalls);
	disableResult = MH_ERROR_MEMORY_PROTECT;
	NvidiaForegroundCompatibility::FinishRetire(); assert(hookEnabled);
	disableResult = MH_OK;
	NvidiaForegroundCompatibility::FinishRetire(); assert(!hookEnabled && hookCreated);
	assert(NvidiaForegroundCompatibility::RealForegroundWindow() == fakeRoot);
	// Cache only pinned exact candidates; retry a missing late-loaded DLL.
	loadedModules[2] = false;
	assert(RefreshModules());
	const auto firstLookups = moduleLookups;
	assert(!RefreshModules());
	for (size_t i = 0; i < firstLookups.size(); ++i)
		assert(moduleLookups[i] == firstLookups[i] + (i == 2 ? 1u : 0u));
	loadedModules[2] = true;
	assert(RefreshModules() && control.snapshot.callers[2].size);
	const auto cachedLookups = moduleLookups;
	assert(!RefreshModules() && moduleLookups == cachedLookups);
	for (auto reads : moduleInfoReads) assert(reads == 1);
	const SourceWindowIdentity source{fakeCapture, fakeRoot, 10, 11, 101, 20, 21, 102};
	const auto ui = RegisterSessionWindow(fakeMainUI);
	uint64_t firstSessionEpoch = 10;
	auto start = [&](ForegroundPresentationPath path, bool windowed = false, uint32_t vendor = 0x10de) {
		fakeDiagnostic.clear();
		NvidiaForegroundCompatibility::Prepare(source, fakeOutput, ++firstSessionEpoch, true, windowed, ui);
		assert(!control.snapshot.armed);
		NvidiaForegroundCompatibility::DeviceInitialized(vendor, 123, -4, path,
			path == ForegroundPresentationPath::DLSSFrontendDXGI ? 4u : path == ForegroundPresentationPath::XeSSProxy ? 2u : 1u);
		NvidiaForegroundCompatibility::PresentationState(true, false);
	};
	for (auto path : {ForegroundPresentationPath::OrdinaryDXGI, ForegroundPresentationPath::DLSSFrontendDXGI,
		ForegroundPresentationPath::XeSSProxy}) for (bool windowed : {false, true}) {
		start(path, windowed);
		assert(control.snapshot.armed && control.path == path && control.luidLow == 123 && control.luidHigh == -4);
		assert(!control.snapshot.diagnostics && !VRRPerformance::Enabled());
		assert(DetourForegroundWindow() == fakeOutput);
		assert(NvidiaForegroundCompatibility::BindPresentationWindow(fakeOutput));
		NvidiaForegroundCompatibility::PresentationState(true, true);
		assert(control.snapshot.armed && control.present);
		NvidiaForegroundCompatibility::FrontendSuspended(true);
		assert(!control.snapshot.armed && DetourForegroundWindow() == fakeRoot);
		NvidiaForegroundCompatibility::FrontendSuspended(false);
		assert(control.snapshot.armed && DetourForegroundWindow() == fakeOutput);
		for (HWND foreground : {fakeMainUI, parameter, fakePopup, other, HWND(nullptr)}) {
			fakeForeground = foreground;
			assert(DetourForegroundWindow() == foreground);
		}
		fakeForeground = fakeRoot;
		assert(DetourForegroundWindow() == fakeOutput);
		NvidiaForegroundCompatibility::PresentationState(false, false);
		assert(!control.snapshot.armed && DetourForegroundWindow() == fakeRoot);
		NvidiaForegroundCompatibility::PresentationState(true, true);
		assert(control.snapshot.armed && DetourForegroundWindow() == fakeOutput);
		NvidiaForegroundCompatibility::Retire();
		assert(DetourForegroundWindow() == fakeRoot);
	}
	start(ForegroundPresentationPath::XeSSProxy, false, 0x8086);
	assert(!control.snapshot.armed && DetourForegroundWindow() == fakeRoot);
	for (auto path : {ForegroundPresentationPath::Unknown, ForegroundPresentationPath::Composition}) {
		start(path); assert(!control.snapshot.armed && DetourForegroundWindow() == fakeRoot);
	}
	start(ForegroundPresentationPath::XeSSProxy);
	NvidiaForegroundCompatibility::MainUIChanged({});
	assert(!control.snapshot.mainUI.window && control.snapshot.armed);
	NvidiaForegroundCompatibility::MainUIChanged(ui);
	assert(control.snapshot.mainUI.window == uintptr_t(fakeMainUI) && control.snapshot.armed);
	auto staleUI = ui; ++staleUI.atom;
	NvidiaForegroundCompatibility::MainUIChanged(staleUI);
	assert(!control.snapshot.mainUI.window);
	NvidiaForegroundCompatibility::MainUIChanged(RegisterSessionWindow(other));
	assert(!control.snapshot.mainUI.window);
	NvidiaForegroundCompatibility::MainUIChanged(ui);
	assert(!NvidiaForegroundCompatibility::BindPresentationWindow(fakeMainUI));
	assert(control.snapshot.mode == ForegroundCompatibilityMode::Off && DetourForegroundWindow() == fakeRoot);
	for (const wchar_t* diagnostic : {L"standard", L"trace", L"reference"}) {
		fakeDiagnostic = diagnostic;
		NvidiaForegroundCompatibility::Prepare(source, fakeOutput, ++firstSessionEpoch, true, false, ui);
		NvidiaForegroundCompatibility::DeviceInitialized(0x10de, 0, 0, ForegroundPresentationPath::XeSSProxy, 2);
		NvidiaForegroundCompatibility::PresentationState(true, true);
		assert(DetourForegroundWindow() == fakeRoot);
	}
	fakeDiagnostic = L"profile";
	NvidiaForegroundCompatibility::Prepare(source, fakeOutput, ++firstSessionEpoch, true, false, ui);
	NvidiaForegroundCompatibility::DeviceInitialized(0x10de, 0, 0, ForegroundPresentationPath::XeSSProxy, 2);
	NvidiaForegroundCompatibility::PresentationState(true, true);
	assert(control.snapshot.diagnostics && DetourForegroundWindow() == fakeOutput);
	assert(!VRRPerformance::Enabled());
	fakeDiagnostic = L"standard"; fakePerformance = true;
	NvidiaForegroundCompatibility::Prepare(source, fakeOutput, ++firstSessionEpoch, true, false, ui);
	assert(control.snapshot.mode == ForegroundCompatibilityMode::Off && VRRPerformance::Enabled());
	assert(DetourForegroundWindow() == fakeRoot);
	NvidiaForegroundCompatibility::FinishRetire(); assert(!VRRPerformance::Enabled());
	fakeDiagnostic.clear();
	NvidiaForegroundCompatibility::Prepare(source, fakeOutput, ++firstSessionEpoch, false, false, ui);
	assert(control.snapshot.mode == ForegroundCompatibilityMode::Off && VRRPerformance::Enabled());
	NvidiaForegroundCompatibility::FinishRetire(); fakePerformance = false;
	fakeDiagnostic.clear();
	NvidiaForegroundCompatibility::Prepare(source, fakeOutput, ++firstSessionEpoch, false, false, ui);
	NvidiaForegroundCompatibility::DeviceInitialized(0x10de, 0, 0, ForegroundPresentationPath::DLSSFrontendDXGI, 4);
	assert(control.path == ForegroundPresentationPath::DLSSFrontendDXGI && control.multiplier == 4);
	assert(!control.snapshot.armed && DetourForegroundWindow() == fakeRoot);
	NvidiaForegroundCompatibility::Retire();
	auto value = Session(); published.Publish(value);
	for (auto& slot : records) slot.state = 0;
	for (auto& slot : unknownQueries) { slot.page = 0; slot.phases = 0; }
	queryPhase = ForegroundQueryPhase::Present;
	const auto unknownIndex = unsigned(value.callers.size());
	auto firstUnknown = RecordCandidate(value, 0x9000, unknownIndex, fakeRoot);
	assert(firstUnknown && !firstUnknown->value.target && std::string_view(firstUnknown->value.role) == "source");
	firstUnknown->state = 2;
	assert(!RecordCandidate(value, 0x9008, unknownIndex, fakeRoot));
	auto secondUnknown = RecordCandidate(value, 0xA000, unknownIndex, fakeRoot);
	assert(secondUnknown && !secondUnknown->value.target); secondUnknown->state = 2;
	queryPhase = ForegroundQueryPhase::FirstPresent;
	auto nextPhase = RecordCandidate(value, 0x9000, unknownIndex, fakeRoot);
	assert(nextPhase); nextPhase->state = 2;
	for (auto [foreground, role] : {std::pair{fakeRoot, "source"}, {fakeOutput, "output"}, {fakeMainUI, "magpie-main-ui"},
		{parameter, "parameter-host"}, {fakePopup, "source-popup"}, {other, "external"}, {HWND(nullptr), "none"}})
		assert(std::string_view(ForegroundRole(value, foreground)) == role);
	assert(DetourForegroundWindow() == fakeOutput && GetLastError() == 73);
	assert(NvidiaForegroundCompatibility::RealForegroundWindow() == fakeRoot && GetLastError() == 73);
	fakeCaller = 0x9999;
	assert(DetourForegroundWindow() == fakeRoot);
	fakeCaller = 0x1010;
	for (HWND window : {fakeCapture, fakeRoot}) { fakeForeground = window; assert(DetourForegroundWindow() == fakeOutput); }
	for (HWND window : {other, HWND(nullptr), fakeOutput}) { fakeForeground = window; assert(DetourForegroundWindow() == window); }
	fakeForeground = fakeRoot;
	for (auto mode : {ForegroundCompatibilityMode::Off, ForegroundCompatibilityMode::Trace, ForegroundCompatibilityMode::Reference}) {
		value.mode = mode; published.Publish(value); assert(DetourForegroundWindow() == fakeRoot);
	}
	value = Session(); value.armed = false; published.Publish(value);
	assert(DetourForegroundWindow() == fakeRoot);
	value = Session(); published.Publish(value);
	// Startup may be hidden, but shown outputs must pause during hiding/minimization.
	native[fakeOutput].visible = false;
	assert(DetourForegroundWindow() == fakeOutput);
	value.outputShown = true; published.Publish(value);
	assert(DetourForegroundWindow() == fakeRoot);
	native[fakeOutput].visible = true; native[fakeOutput].iconic = true;
	assert(DetourForegroundWindow() == fakeRoot);
	native[fakeOutput].iconic = false;
	assert(DetourForegroundWindow() == fakeOutput);
	value = Session(); published.Publish(value);
	const auto saved = native[fakeOutput];
	native[fakeOutput].pid = 88; assert(DetourForegroundWindow() == fakeRoot); native[fakeOutput] = saved;
	native[fakeOutput].tid = 89; assert(DetourForegroundWindow() == fakeRoot); native[fakeOutput] = saved;
	native[fakeOutput].atom = 90; assert(DetourForegroundWindow() == fakeRoot); native[fakeOutput] = saved;
	native[fakeRoot].iconic = true; assert(DetourForegroundWindow() == fakeRoot); native[fakeRoot].iconic = false;
	native[fakeRoot].enabled = false; assert(DetourForegroundWindow() == fakeRoot); native[fakeRoot].enabled = true;
	native[fakeCapture].root = other; assert(DetourForegroundWindow() == fakeRoot); native[fakeCapture].root = fakeRoot;
	changeDuringIdentity = true;
	assert(DetourForegroundWindow() == fakeRoot && fakeForeground == other);
	fakeForeground = fakeRoot; retireDuringRecord = true;
	assert(DetourForegroundWindow() == fakeRoot);
	published.Publish(Session());
	appliedQueries = 0;
	std::vector<std::thread> threads;
	for (int i = 0; i < 4; ++i) threads.emplace_back([] {
		for (int j = 0; j < 20000; ++j) assert(DetourForegroundWindow() == fakeOutput);
	});
	for (auto& thread : threads) thread.join();
	assert(appliedQueries == 80000);
	// Normal mode has no record/counter RMWs. Same native identities are checked
	// once, while UWP source/fakeRoot identities remain independently validated.
	value = Session(); value.diagnostics = false; value.outputShown = true;
	value.source = value.root;
	published.Publish(value); targetQueries = 0; appliedQueries = 0;
	for (auto& mask : observedPhases) mask = 0;
	windowApiCalls = 0;
	assert(DetourForegroundWindow() == fakeOutput && windowApiCalls == 13 && GetLastError() == 73);
	assert(threadOpens == 0); // Ordinary windows keep the original query cost.
	rejectSourcePid = true;
	assert(Identity(fakeRoot).pid == 20 && RegisterSessionWindow(fakeRoot).Matches());
	threadOpens = 0; threadCloses = 0;
	assert(DetourForegroundWindow() == fakeOutput && GetLastError() == 73);
	assert(threadOpens == 1 && threadCloses == 1); // Same source/root resolved once.
	denySourceThread = true;
	assert(DetourForegroundWindow() == fakeRoot && GetLastError() == 73);
	denySourceThread = false;
	const auto savedRoot = native[fakeRoot];
	++native[fakeRoot].pid;
	assert(DetourForegroundWindow() == fakeRoot);
	native[fakeRoot] = savedRoot;
	++native[fakeRoot].atom;
	assert(DetourForegroundWindow() == fakeRoot);
	native[fakeRoot] = savedRoot;
	// Worker-thread calls use the resolver without a shared HWND cache.
	threads.clear();
	for (int i = 0; i < 4; ++i) threads.emplace_back([] {
		for (int j = 0; j < 1000; ++j) assert(DetourForegroundWindow() == fakeOutput && GetLastError() == 73);
	});
	for (auto& thread : threads) thread.join();
	rejectSourcePid = false;
	assert(targetQueries == 0 && appliedQueries == 0);
	for (auto& mask : observedPhases) assert(mask == 0);
	ForegroundCompatibilitySnapshot unchanged; uint64_t unchangedVersion;
	assert(published.Read(unchanged, unchangedVersion));
	published.Publish(value);
	assert(published.Current(unchangedVersion));
	ForegroundQuerySnapshot query; uint64_t queryVersion; unsigned callerIndex;
	assert(published.ReadQuery(0x1000, query, queryVersion, callerIndex) && callerIndex == 0);
	assert(!published.ReadQuery(0x1100, query, queryVersion, callerIndex));
	fakeCaller = 0x9999; windowApiCalls = 0;
	assert(DetourForegroundWindow() == fakeRoot && windowApiCalls == 0 && GetLastError() == 73);
	fakeCaller = 0x1010;
	retireDuringIdentity = true;
	assert(DetourForegroundWindow() == fakeRoot);
	windowApiCalls = 0;
	assert(DetourForegroundWindow() == fakeRoot && windowApiCalls == 0 && GetLastError() == 73);
	value = Session(); value.diagnostics = false; value.outputShown = true; published.Publish(value);
	windowApiCalls = 0;
	assert(DetourForegroundWindow() == fakeOutput && windowApiCalls == 16);
	foregroundReads = 0; retireDuringFinalForeground = true;
	assert(DetourForegroundWindow() == fakeRoot && GetLastError() == 73);
	static_assert(sizeof(ForegroundQuerySnapshot) <= 192);

	// Exercise the actual publication implementation with concurrent readers.
	PublishedForegroundState concurrent;
	std::atomic<bool> finished = false;
	std::atomic<unsigned> accepted = 0;
	threads.clear();
	for (int i = 0; i < 4; ++i) threads.emplace_back([&] {
		while (!finished.load()) {
			ForegroundCompatibilitySnapshot current; uint64_t version;
			if (!concurrent.Read(current, version) || !current.epoch) continue;
			assert(current.source.window == current.epoch * 3 && current.output.window == current.epoch * 7);
			assert(current.armed == bool(current.epoch & 1));
			ForegroundQuerySnapshot hot; uint64_t hotVersion; unsigned index;
			if (concurrent.ReadQuery(0x1010, hot, hotVersion, index)) {
				assert(index == 0 && hot.source.window == hot.epoch * 3 && hot.output.window == hot.epoch * 7);
				assert(hot.armed == bool(hot.epoch & 1));
			}
			++accepted;
		}
	});
	for (uint64_t sessionEpoch = 1; sessionEpoch < 10000; ++sessionEpoch) {
		ForegroundCompatibilitySnapshot current;
		current.epoch = sessionEpoch; current.source.window = uintptr_t(sessionEpoch * 3); current.output.window = uintptr_t(sessionEpoch * 7);
		current.armed = bool(sessionEpoch & 1); concurrent.Publish(current);
		current.mode = ForegroundCompatibilityMode::Query;
		current.callers[0] = {0x1000, 0x100}; concurrent.Publish(current);
	}
	finished = true;
	for (auto& thread : threads) thread.join();
	ForegroundCompatibilitySnapshot final; uint64_t version;
	assert(concurrent.Read(final, version) && final.epoch == 9999);
	concurrent.Publish({}); assert(!concurrent.Current(version));
	std::cout << "Production foreground decisions: ordinary/DLSS/XeSS qualification, windowed mode, focus roles, proxy binding, diagnostics, retirement, HWND reuse, LastError, concurrent readers passed\n";
}
