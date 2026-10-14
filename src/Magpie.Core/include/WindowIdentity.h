#pragma once
#include <windows.h>
#include <memory>

namespace Magpie {

// Returns a complete PID/TID pair, or clears processId and returns zero.
// Some third-party hooks reject the PID-output form while allowing TID-only
// queries. Resolve that thread's owner using documented, read-only APIs.
// No identities/handles are cached across calls: callers retain their existing
// class, root and destruction checks, including on SDK worker threads.
inline DWORD QueryWindowThreadProcessId(HWND window, DWORD& processId,
	bool* usedThreadQuery = nullptr, const char** failureStage = nullptr) noexcept {
	processId = 0;
	if (usedThreadQuery) *usedThreadQuery = false;
	if (failureStage) *failureStage = nullptr;
	const auto fail = [&](const char* stage, DWORD error) {
		if (failureStage) *failureStage = stage;
		SetLastError(error);
		return DWORD(0);
	};
	DWORD pid = 0;
	const DWORD directTid = GetWindowThreadProcessId(window, &pid);
	if (directTid && pid) {
		processId = pid;
		return directTid;
	}

	if (!window || !IsWindow(window)) {
		return fail("IsWindow", ERROR_INVALID_WINDOW_HANDLE);
	}
	const ULONG_PTR atom = GetClassLongPtrW(window, GCW_ATOM);
	const DWORD tid = GetWindowThreadProcessId(window, nullptr);
	if (!tid || !atom || !IsWindow(window)) {
		return fail("TID/class query", ERROR_INVALID_WINDOW_HANDLE);
	}
	std::unique_ptr<void, decltype(&CloseHandle)> thread(
		OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid), &CloseHandle);
	if (!thread) return fail("OpenThread", GetLastError());
	pid = GetProcessIdOfThread(thread.get());
	DWORD error = pid ? ERROR_SUCCESS : GetLastError();
	const char* stage = "GetProcessIdOfThread";
	// A window can disappear/rebind while OpenThread is in progress. Never
	// publish an identity resolved from the previous owner in that case.
	if (pid && (!IsWindow(window) || GetWindowThreadProcessId(window, nullptr) != tid ||
		GetClassLongPtrW(window, GCW_ATOM) != atom)) {
		pid = 0;
		error = ERROR_INVALID_WINDOW_HANDLE;
		stage = "Window revalidation";
	}
	thread.reset();
	if (!pid) {
		return fail(stage, error ? error : ERROR_INVALID_DATA);
	}
	processId = pid;
	if (usedThreadQuery) *usedThreadQuery = true;
	return tid;
}

}
