#include <windows.h>
#include <windowsx.h>
#include <wil/resource.h>
#include <wil/token_helpers.h>
#include <fmt/format.h>
#include <cassert>
#include <atomic>
#include <iostream>
#include <memory>
#include <string>

static bool rejectPidOutput, denyThread, denyProcess, allowSelfTarget;
static unsigned legacyCalls;
static const DWORD hostPid = GetCurrentProcessId();
static DWORD ThreadForTest(HWND hwnd, DWORD* pid) {
	if (pid && rejectPidOutput) { *pid = 0; SetLastError(0); return 0; }
	return GetWindowThreadProcessId(hwnd, pid);
}
static HANDLE OpenThreadForTest(DWORD access, BOOL inherit, DWORD tid) {
	assert(access == THREAD_QUERY_LIMITED_INFORMATION && !inherit);
	if (denyThread) { SetLastError(ERROR_ACCESS_DENIED); return nullptr; }
	return OpenThread(access, inherit, tid);
}
static HANDLE OpenProcessForTest(DWORD access, BOOL inherit, DWORD pid) {
	assert(access == PROCESS_QUERY_LIMITED_INFORMATION && !inherit && pid == hostPid);
	if (denyProcess) { SetLastError(ERROR_ACCESS_DENIED); return nullptr; }
	return OpenProcess(access, inherit, pid);
}
static HANDLE WINAPI LegacyForTest(HWND) { ++legacyCalls; SetLastError(ERROR_ACCESS_DENIED); return nullptr; }
static DWORD CurrentPidForTest() { return allowSelfTarget ? 0 : hostPid; }
#define GetWindowThreadProcessId ThreadForTest
#define OpenThread OpenThreadForTest
#define OpenProcess OpenProcessForTest
#define GetCurrentProcessId CurrentPidForTest
#include "SourceWindowRelations.h"

namespace Magpie {
struct Logger {
	static Logger& Get() { static Logger result; return result; }
	void Info(std::string_view) {} void Error(std::string_view) {}
	void Win32Error(std::string_view) {} void ComError(std::string_view, HRESULT) {}
};
struct Win32Helper {
	static wil::unique_process_handle GetWindowProcessHandle(HWND) noexcept;
	static bool GetWindowIntegrityLevel(HWND, DWORD&) noexcept;
	static bool GetProcessIntegrityLevel(HANDLE, DWORD&) noexcept;
	static DWORD GetCurrentProcessIntegrityLevel() noexcept;
	template<class T> static T* LoadSystemFunction(const wchar_t*, const char*) { return &LegacyForTest; }
	static std::wstring GetWindowExeName(HWND hwnd) {
		return GetWindowProcessHandle(hwnd) ? L"fixture.exe" : L"";
	}
	static std::wstring GetWindowClassName(HWND hwnd) {
		wchar_t name[256]{}; GetClassNameW(hwnd, name, 256); return name;
	}
};
enum class ScalingError { NoError, LowIntegrityLevel, WindowIntegrityQueryFailed };
struct CommonSharedConstants { static constexpr auto SCALING_WINDOW_CLASS_NAME = L"MagpieIdentityTestNoOutput"; };
static std::string ProfileName(const std::wstring& name) { return name == L"fixture.exe" ? "fixture.exe" : ""; }
#include "WindowIdentityIntegration.inc"
}

int main() {
	using namespace Magpie;
	wil::unique_hwnd source(CreateWindowExW(WS_EX_NOACTIVATE, L"STATIC", L"identity test",
		WS_POPUP, -32000, -32000, 32, 32, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr));
	wil::unique_hwnd popup(CreateWindowExW(WS_EX_NOACTIVATE, L"STATIC", L"popup test",
		WS_POPUP, -32000, -32000, 32, 32, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr));
	assert(source && popup);
	DWORD baselineIL = 0, recoveredIL = 0;
	assert(Win32Helper::GetWindowIntegrityLevel(source.get(), baselineIL));
	rejectPidOutput = true;
	assert(Win32Helper::GetWindowIntegrityLevel(source.get(), recoveredIL) && recoveredIL == baselineIL);
	assert(CheckSourceIntegrity(source.get()) == ScalingError::NoError && legacyCalls == 0);
	EnableWindow(source.get(), FALSE);
	assert(IsPopupWindow(popup.get(), source.get()));
	denyThread = true;
	assert(!IsPopupWindow(popup.get(), source.get())); // Two zero PIDs cannot match.
	assert(CheckSourceIntegrity(source.get()) == ScalingError::WindowIntegrityQueryFailed);
	assert(legacyCalls == 1);
	denyThread = false; denyProcess = true;
	assert(!Win32Helper::GetWindowProcessHandle(source.get()) && legacyCalls == 2);
	denyProcess = false;
	// Resolve against our hidden window while modelling an external caller PID.
	assert(!ResolveTarget(source.get())); // Existing self-target exclusion.
	allowSelfTarget = true;
	auto target = ResolveTarget(source.get());
	assert(target && target->processId == hostPid && target->Valid());
	rejectPidOutput = false; assert(target->Valid());
	rejectPidOutput = true; assert(target->Valid());
	denyThread = true; assert(!target->Valid() && !ResolveTarget(source.get()));
	denyThread = false;
	++target->processId; assert(!target->Valid()); --target->processId;
	++target->creation.dwLowDateTime; assert(!target->Valid()); --target->creation.dwLowDateTime;
	source.reset(); assert(!target->Valid());
	std::cout << "PASS: production thread recovery -> process/token -> source admission; legacy failure fallback, popup and RTSS identity/lifetime\n";
}
