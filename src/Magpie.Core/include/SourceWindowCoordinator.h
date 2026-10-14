#pragma once
#include "ScalingWindowOwner.h"
#include "SourceWindowRelations.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace Magpie {

enum class WindowCoordinationMode { Preparing, NativeOwned, ManagedUnowned };

struct SourceWindowIdentity {
	HWND capture = nullptr;
	HWND root = nullptr;
	DWORD capturePid = 0, captureTid = 0, rootPid = 0, rootTid = 0;
	ULONG_PTR captureClass = 0, rootClass = 0;
	LONG_PTR captureStyle = 0, rootStyle = 0;

	bool Read(HWND source) noexcept {
		*this = {};
		capture = source;
		root = GetAncestor(source, GA_ROOT);
		captureTid = QueryWindowThreadProcessId(capture, capturePid);
		if (!captureTid) return false;
		if (root == capture) {
			rootTid = captureTid;
			rootPid = capturePid;
		} else {
			rootTid = QueryWindowThreadProcessId(root, rootPid);
			if (!rootTid) return false;
		}
		captureClass = GetClassLongPtrW(capture, GCW_ATOM);
		rootClass = GetClassLongPtrW(root, GCW_ATOM);
		captureStyle = GetWindowLongPtrW(capture, GWL_STYLE);
		rootStyle = GetWindowLongPtrW(root, GWL_STYLE);
		return Matches();
	}
	bool Matches() const noexcept {
		DWORD pid = 0;
		if (!captureTid || !capturePid || !rootTid || !rootPid || !capture || !root ||
			!IsWindow(capture) || !IsWindow(root) || capture == GetDesktopWindow() ||
			root == GetDesktopWindow() || (GetWindowLongPtrW(root, GWL_STYLE) & WS_CHILD) ||
			GetAncestor(capture, GA_ROOT) != root) return false;
		if (QueryWindowThreadProcessId(capture, pid) != captureTid || pid != capturePid ||
			GetClassLongPtrW(capture, GCW_ATOM) != captureClass) return false;
		if (root == capture) return rootTid == captureTid && rootPid == capturePid && rootClass == captureClass;
		return QueryWindowThreadProcessId(root, pid) == rootTid && pid == rootPid &&
			GetClassLongPtrW(root, GCW_ATOM) == rootClass;
	}
};

// Registered and polled on the scaling frontend thread. Event callbacks only
// latch state and wake its message loop, never touch a Renderer or move a
// window. The source watch survives a reposition/rebuild pause so a destroyed
// source cannot be silently replaced by a recycled HWND.
class SourceWindowCoordinator {
public:
	~SourceWindowCoordinator() noexcept { if (!_hooks.empty()) Reset(); }
	SourceWindowCoordinator() = default;
	SourceWindowCoordinator(const SourceWindowCoordinator&) = delete;
	SourceWindowCoordinator& operator=(const SourceWindowCoordinator&) = delete;
	bool Prepare(HWND source) noexcept {
		Reset();
		_thread = GetCurrentThreadId();
		_epoch = ++_nextEpoch;
		_started = GetTickCount();
		if (!_source.Read(source)) return false;
		const std::array<std::pair<DWORD, DWORD>, 4> ranges{{
			{EVENT_OBJECT_DESTROY, EVENT_OBJECT_HIDE}, {EVENT_OBJECT_REORDER, EVENT_OBJECT_REORDER},
			{EVENT_OBJECT_STATECHANGE, EVENT_OBJECT_LOCATIONCHANGE},
			{EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND}
		}};
		for (const auto& range : ranges) {
			if (!_Hook(range.first, range.second, _source.capturePid)) { _ResetAfterFailure(); return false; }
			if (_source.rootPid != _source.capturePid && !_Hook(range.first, range.second, _source.rootPid)) {
				_ResetAfterFailure(); return false;
			}
		}
		if (!_Hook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, 0)) { _ResetAfterFailure(); return false; }
		_ready = _source.Matches();
		_dirty = true;
		return _ready;
	}
	void Reset() noexcept {
		_ready = false;
		std::erase_if(_registrations, [&](const auto& r) { return r.instance == this; });
		for (auto hook : _hooks) UnhookWinEvent(hook);
		_hooks.clear();
		_source = {};
		_output = nullptr;
		_destroyed = false;
		_dirty = false;
		_mode = WindowCoordinationMode::Preparing;
	}
	void RetireOutput() noexcept { _output = nullptr; _mode = WindowCoordinationMode::Preparing; }
	bool Ready() const noexcept { return _ready && !_destroyed.load(std::memory_order_acquire) && _source.Matches(); }
	const SourceWindowIdentity& Source() const noexcept { return _source; }
	uint64_t Epoch() const noexcept { return _epoch; }
	WindowCoordinationMode Mode() const noexcept { return _mode; }
	const char* ModeName() const noexcept {
		return _mode == WindowCoordinationMode::NativeOwned ? "NativeOwned" :
			_mode == WindowCoordinationMode::ManagedUnowned ? "ManagedUnowned" : "Preparing";
	}
	bool Accept(HWND output, const ScalingWindowOwnerResult& result, bool permissionChecked) noexcept {
		if (!Ready() || !IsWindow(output) || GetWindowThreadProcessId(output, nullptr) != _thread || output == _source.root ||
			(GetWindowLongPtrW(output, GWL_STYLE) & WS_CHILD)) return false;
		if (result.IsApplied() && result.actualOwner == _source.root && GetWindow(output, GW_OWNER) == _source.root) {
			_mode = WindowCoordinationMode::NativeOwned;
		} else if (permissionChecked && !result.actualOwner && !GetWindow(output, GW_OWNER) &&
			(result.CanContinueUnowned() || _CanManageUnappliedUwpOwner(output, result))) {
			// Class/relationship queries must not let a retired source or an
			// output whose owner changed pass the compatibility admission gate.
			if (!Ready() || !IsWindow(output) || GetWindow(output, GW_OWNER)) return false;
			_mode = WindowCoordinationMode::ManagedUnowned;
		} else return false;
		_output = output;
		_dirty = true;
		return true;
	}
	// Public for controlled notification injection against the production
	// controller. Both native and compatibility paths use this exact latch.
	void Notify(uint64_t epoch, DWORD event, HWND window, LONG object = OBJID_WINDOW, LONG child = 0) noexcept {
		if (!_ready || epoch != _epoch) return;
		if (event == EVENT_SYSTEM_FOREGROUND) { _dirty = true; }
		else {
			if (object != OBJID_WINDOW || child != 0 ||
				(window != _source.capture && window != _source.root &&
				 !IsWindowInOwnerChain(window, _source.root))) return;
			if (event == EVENT_OBJECT_DESTROY && (window == _source.capture || window == _source.root)) _destroyed = true;
			_dirty = true;
		}
		PostThreadMessageW(_thread, WM_NULL, 0, 0);
	}
	bool TakeDirty() noexcept { const bool dirty = _dirty; _dirty = false; return dirty; }
	bool NeedsTopmost(bool focused, bool disableAutomatic) const noexcept {
		return (GetWindowLongPtrW(_source.root, GWL_EXSTYLE) & WS_EX_TOPMOST) ||
			(focused && !disableAutomatic && IsWindowEnabled(_source.root) && !EnabledSourcePopup(_source.root, _output));
	}
	bool UpdateManagedOrder(bool topmost, bool focused, bool disableAutomatic) const noexcept {
		if (!Ready() || !IsWindow(_output)) { SetLastError(ERROR_INVALID_WINDOW_HANDLE); return false; }
		if (_mode != WindowCoordinationMode::ManagedUnowned || GetWindow(_output, GW_OWNER)) {
			SetLastError(ERROR_INVALID_STATE);
			return false;
		}
		constexpr UINT flags = SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER;
		const bool oldTopmost = (GetWindowLongPtrW(_output, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
		if (oldTopmost != topmost) {
			for (unsigned attempt = 0; attempt < 10; ++attempt) {
				if (!SetWindowPos(_output, topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, flags)) return false;
				if (((GetWindowLongPtrW(_output, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0) == topmost) break;
			}
			if (((GetWindowLongPtrW(_output, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0) != topmost) {
				SetLastError(ERROR_INVALID_STATE);
				return false;
			}
		}
		// Automatic foreground topmost is allowed by the existing policy. All
		// other cases place only our output immediately before the actual source,
		// below its popup and below unrelated foreground windows already ahead.
		const bool automaticTop = focused && !disableAutomatic && IsWindowEnabled(_source.root) && !EnabledSourcePopup(_source.root, _output);
		HWND previous = GetWindow(_source.root, GW_HWNDPREV);
		if (!automaticTop && previous == _output) return true;
		if (automaticTop) {
			const HWND previousOutput = GetWindow(_output, GW_HWNDPREV);
			if (!previousOutput || IsWindowInOwnerChain(previousOutput, _output)) return true;
		}
		return SetWindowPos(_output, automaticTop ? HWND_TOP : (previous ? previous : HWND_TOP), 0, 0, 0, 0, flags) != FALSE;
	}

private:
	bool _CanManageUnappliedUwpOwner(HWND output, const ScalingWindowOwnerResult& result) const noexcept {
		if (result.status != ScalingWindowOwnerStatus::NotApplied || result.systemError != ERROR_SUCCESS ||
			result.previousOwner != 0 || _source.capture != _source.root) return false;
		// Observed on fullscreen ApplicationFrameWindow sources. Do not extend
		// a zero-error fallback to arbitrary Win32/WinUI windows or capture children.
		wchar_t rootClass[64]{};
		if (!GetClassNameW(_source.root, rootClass, ARRAYSIZE(rootClass)) ||
			std::wstring_view(rootClass) != L"ApplicationFrameWindow") return false;
		// The source owner chain can change after TrySetScalingWindowOwner.
		HWND ancestor = _source.root;
		for (unsigned i = 0; ancestor; ++i, ancestor = GetWindow(ancestor, GW_OWNER)) {
			if (ancestor == output || i == 64) return false;
		}
		return true;
	}
	void _ResetAfterFailure() noexcept { const DWORD error = GetLastError(); Reset(); SetLastError(error); }
	struct Registration { HWINEVENTHOOK hook; SourceWindowCoordinator* instance; uint64_t epoch; DWORD started; };
	static inline thread_local std::vector<Registration> _registrations;
	static inline thread_local uint64_t _nextEpoch = 0;
	static void CALLBACK _OnEvent(HWINEVENTHOOK hook, DWORD event, HWND window, LONG object, LONG child, DWORD, DWORD time) noexcept {
		const auto it = std::find_if(_registrations.begin(), _registrations.end(), [&](const auto& r) { return r.hook == hook; });
		if (it == _registrations.end() || static_cast<int32_t>(time - it->started) < 0) return;
		const Registration registration = *it;
		registration.instance->Notify(registration.epoch, event, window, object, child);
	}
	bool _Hook(DWORD first, DWORD last, DWORD pid) noexcept {
		const auto hook = SetWinEventHook(first, last, nullptr, _OnEvent, pid, 0, WINEVENT_OUTOFCONTEXT);
		if (!hook) return false;
		_hooks.push_back(hook);
		_registrations.push_back({ hook, this, _epoch, _started });
		return true;
	}
	SourceWindowIdentity _source;
	std::vector<HWINEVENTHOOK> _hooks;
	HWND _output = nullptr;
	DWORD _thread = 0, _started = 0;
	uint64_t _epoch = 0;
	// Capture teardown may query source validity on the backend while the
	// frontend retires cursor/input resources. Binding fields stay immutable
	// until that backend joins; only the death latch can change concurrently.
	std::atomic<bool> _destroyed = false;
	bool _ready = false, _dirty = false;
	WindowCoordinationMode _mode = WindowCoordinationMode::Preparing;
};

}
