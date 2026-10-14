// Production resolver with deterministic Win32 endpoints; no game/API hooks.
#include <windows.h>
#include <cassert>
#include <iostream>
#include <memory>
#include <string_view>

namespace {
const HWND testWindow = HWND(uintptr_t(1));
enum class Change { None, Destroy, Thread, Class };
DWORD ownerPid, ownerTid, pidOutput, threadError, ownerError;
ULONG_PTR testAtom;
bool alive, rejectPid, zeroDirectTid, rejectTid, rejectThread, rejectOwner;
Change change;
unsigned opens, closes, pidQueries;

void Reset() {
	ownerPid = 42; ownerTid = 43; testAtom = 44; pidOutput = 42;
	alive = true;
	rejectPid = zeroDirectTid = rejectTid = rejectThread = rejectOwner = false;
	threadError = ERROR_ACCESS_DENIED; ownerError = ERROR_ACCESS_DENIED;
	change = Change::None; opens = closes = pidQueries = 0;
}
DWORD WINAPI ThreadForTest(HWND hwnd, DWORD* pid) {
	if (hwnd != testWindow || !alive) return 0;
	if (pid) {
		++pidQueries;
		*pid = pidOutput;
		SetLastError(ERROR_SUCCESS); // Observed hook failure does not set an error.
		return rejectPid || zeroDirectTid ? 0 : ownerTid;
	}
	return rejectTid ? 0 : ownerTid;
}
BOOL WINAPI ExistsForTest(HWND hwnd) { return hwnd == testWindow && alive; }
ULONG_PTR AtomForTest(HWND hwnd, int index) {
	assert(index == GCW_ATOM);
	return ExistsForTest(hwnd) ? testAtom : 0;
}
HANDLE WINAPI OpenThreadForTest(DWORD access, BOOL inherit, DWORD tid) {
	assert(access == THREAD_QUERY_LIMITED_INFORMATION && !inherit && tid == ownerTid);
	++opens;
	if (rejectThread) { SetLastError(threadError); return nullptr; }
	return HANDLE(uintptr_t(tid));
}
DWORD WINAPI OwnerForTest(HANDLE handle) {
	assert(handle == HANDLE(uintptr_t(ownerTid)));
	if (rejectOwner) { SetLastError(ownerError); return 0; }
	if (change == Change::Destroy) alive = false;
	if (change == Change::Thread) ++ownerTid;
	if (change == Change::Class) ++testAtom;
	return ownerPid;
}
BOOL WINAPI CloseForTest(HANDLE handle) {
	assert(handle); ++closes;
	SetLastError(999); // Verify cleanup cannot replace the actual failure.
	return TRUE;
}
}
#define GetWindowThreadProcessId ThreadForTest
#define IsWindow ExistsForTest
#define GetClassLongPtrW AtomForTest
#define OpenThread OpenThreadForTest
#define GetProcessIdOfThread OwnerForTest
#define CloseHandle CloseForTest
#include "WindowIdentity.h"

int main() {
	using Magpie::QueryWindowThreadProcessId;
	Reset();
	DWORD pid = 999;
	bool recovered = true;
	const char* failureStage = "stale";
	assert(QueryWindowThreadProcessId(testWindow, pid, &recovered, &failureStage) == 43 && pid == 42 && !recovered && !failureStage);
	assert(pidQueries == 1 && opens == 0 && closes == 0);
	// Failures with zero/stale PID and partial success must all recover a fresh pair.
	for (unsigned mode = 0; mode < 4; ++mode) {
		Reset();
		rejectPid = mode < 2; pidOutput = mode == 1 ? 999 : 0;
		zeroDirectTid = mode == 3; if (mode == 3) pidOutput = 999;
		assert(QueryWindowThreadProcessId(testWindow, pid, &recovered) == 43 && pid == 42 && recovered);
		assert(opens == 1 && closes == 1);
	}
	auto failure = [&](DWORD expected) {
		pid = 999; recovered = true;
		assert(!QueryWindowThreadProcessId(testWindow, pid, &recovered, &failureStage));
		assert(!pid && !recovered && failureStage && GetLastError() == expected);
	};
	Reset(); rejectPid = rejectTid = true;
	failure(ERROR_INVALID_WINDOW_HANDLE); assert(!opens);
	Reset(); rejectPid = rejectThread = true;
	failure(ERROR_ACCESS_DENIED); assert(opens == 1 && !closes);
	assert(std::string_view(failureStage) == "OpenThread");
	Reset(); rejectPid = rejectOwner = true;
	failure(ERROR_ACCESS_DENIED); assert(opens == 1 && closes == 1);
	assert(std::string_view(failureStage) == "GetProcessIdOfThread");
	ownerError = ERROR_SUCCESS;
	failure(ERROR_INVALID_DATA); assert(opens == 2 && closes == 2);
	for (auto mutation : {Change::Destroy, Change::Thread, Change::Class}) {
		Reset(); rejectPid = true; change = mutation;
		failure(ERROR_INVALID_WINDOW_HANDLE); assert(opens == 1 && closes == 1);
		assert(std::string_view(failureStage) == "Window revalidation");
	}
	Reset(); rejectPid = true; testAtom = 0;
	failure(ERROR_INVALID_WINDOW_HANDLE); assert(!opens);
	Reset(); alive = false;
	failure(ERROR_INVALID_WINDOW_HANDLE); assert(!opens);
	pid = 999;
	assert(!QueryWindowThreadProcessId(nullptr, pid) && !pid);
	// No cached PID survives a later lookup or destruction/recreation.
	Reset(); rejectPid = true;
	assert(QueryWindowThreadProcessId(testWindow, pid) == 43 && pid == 42);
	ownerTid = 53; ownerPid = 52;
	assert(QueryWindowThreadProcessId(testWindow, pid) == 53 && pid == 52);
	assert(opens == 2 && closes == 2);
	std::cout << "PASS: production window identity fast path, hooked/partial queries, limited access, zero PID, cleanup, races and fresh identity\n";
}
