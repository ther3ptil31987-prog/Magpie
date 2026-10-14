#pragma once
#include "WindowIdentity.h"

namespace Magpie {

enum class SourceForegroundKind { None, InvalidSource, Source, ParameterHost, Output, SourcePopup, External, MagpieMainUI };

// Registered on the owning UI thread; copied into a scaling session. Matching
// PID, TID and class protects against a destroyed/reused HWND.
struct SessionWindowIdentity {
	HWND window = nullptr;
	DWORD pid = 0, tid = 0;
	ULONG_PTR atom = 0;
	bool Matches() const noexcept {
		DWORD actualPid = 0;
		return window && IsWindow(window) && tid && pid &&
			QueryWindowThreadProcessId(window, actualPid) == tid && actualPid == pid &&
			GetClassLongPtrW(window, GCW_ATOM) == atom;
	}
};

inline SessionWindowIdentity RegisterSessionWindow(HWND window) noexcept {
	SessionWindowIdentity result;
	result.window = window;
	if (window) {
		result.tid = QueryWindowThreadProcessId(window, result.pid);
		result.atom = GetClassLongPtrW(window, GCW_ATOM);
	}
	return result;
}

inline bool IsSourceSessionForeground(SourceForegroundKind kind, bool outputTransition = false) noexcept {
	return kind == SourceForegroundKind::Source || kind == SourceForegroundKind::ParameterHost ||
		(outputTransition && kind == SourceForegroundKind::Output);
}

inline bool IsWindowInOwnerChain(HWND window, HWND owner) noexcept {
	for (unsigned i = 0; window && i < 64; ++i, window = GetWindow(window, GW_OWNER)) {
		if (window == owner) return true;
	}
	return false;
}

inline HWND EnabledSourcePopup(HWND source, HWND excluded = nullptr) noexcept {
	auto eligible = [&](HWND popup) {
		return popup && popup != source && IsWindowInOwnerChain(popup, source) &&
			IsWindowVisible(popup) && IsWindowEnabled(popup) &&
			!(excluded && IsWindowInOwnerChain(popup, excluded));
	};
	const HWND popup = GetWindow(source, GW_ENABLEDPOPUP);
	if (eligible(popup)) return popup;
	// NativeOwned outputs/input helpers are themselves owned popups. Ignore
	// that branch when finding a real application popup; do not confuse self
	// (the documented no-popup result) with a modal window.
	struct Search { HWND source, excluded, found; } search{ source, excluded, nullptr };
	EnumWindows([](HWND window, LPARAM data) -> BOOL {
		auto& s = *reinterpret_cast<Search*>(data);
		if (GetWindow(window, GW_OWNER) == s.source && IsWindowVisible(window) && IsWindowEnabled(window) &&
			!(s.excluded && IsWindowInOwnerChain(window, s.excluded))) {
			s.found = window;
			return FALSE;
		}
		return TRUE;
	}, reinterpret_cast<LPARAM>(&search));
	return search.found;
}

// Classification has no activation side effects. The caller supplies the
// coordinator's validated identity and this session's actual helper HWNDs.
inline SourceForegroundKind ClassifySourceForeground(HWND foreground, bool sourceValid,
	HWND capture, HWND root, HWND output, HWND parameterHost, const SessionWindowIdentity& mainUI = {}) noexcept {
	if (!sourceValid) return SourceForegroundKind::InvalidSource;
	if (!foreground) return SourceForegroundKind::None;
	if (foreground == capture || foreground == root) return SourceForegroundKind::Source;
	if (foreground == output && IsWindow(output)) return SourceForegroundKind::Output;
	if (foreground == parameterHost && IsWindow(parameterHost) && IsWindow(output) &&
		GetWindow(parameterHost, GW_OWNER) == output &&
		GetWindowThreadProcessId(parameterHost, nullptr) == GetWindowThreadProcessId(output, nullptr))
		return SourceForegroundKind::ParameterHost;
	if (mainUI.Matches() && IsWindowInOwnerChain(foreground, mainUI.window) &&
		!IsWindowInOwnerChain(foreground, output)) return SourceForegroundKind::MagpieMainUI;
	if (IsWindowInOwnerChain(foreground, root) && !IsWindowInOwnerChain(foreground, output) &&
		IsWindowVisible(foreground) && IsWindowEnabled(foreground)) return SourceForegroundKind::SourcePopup;
	return SourceForegroundKind::External;
}

inline bool PositionParameterInputHost(HWND host, HWND output, const RECT& rect) noexcept {
	if (!IsWindow(host) || !IsWindow(output) || !IsWindowVisible(output) ||
		GetWindow(host, GW_OWNER) != output) return false;
	constexpr UINT flags = SWP_NOACTIVATE | SWP_NOOWNERZORDER;
	const auto outputTopmost = GetWindowLongPtrW(output, GWL_EXSTYLE) & WS_EX_TOPMOST;
	RECT current{};
	if (GetWindowRect(host, &current) && EqualRect(&current, &rect) && IsWindowVisible(host) &&
		GetWindow(output, GW_HWNDPREV) == host &&
		(GetWindowLongPtrW(host, GWL_EXSTYLE) & WS_EX_TOPMOST) == outputTopmost) return true;
	if ((GetWindowLongPtrW(host, GWL_EXSTYLE) & WS_EX_TOPMOST) != outputTopmost &&
		!SetWindowPos(host, outputTopmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
			flags | SWP_NOMOVE | SWP_NOSIZE)) return false;
	// Move only the host, immediately ahead of the already ordered output.
	// In particular, do not raise it over source popups or unrelated windows.
	const HWND previous = GetWindow(output, GW_HWNDPREV);
	if (!SetWindowPos(host, previous ? previous : HWND_TOP, rect.left, rect.top,
		rect.right - rect.left, rect.bottom - rect.top,
		flags | SWP_SHOWWINDOW | (previous == host ? SWP_NOZORDER : 0))) return false;
	return IsWindow(output) && IsWindowVisible(output) && IsWindowVisible(host) &&
		GetWindow(host, GW_OWNER) == output && GetWindow(output, GW_HWNDPREV) == host &&
		(GetWindowLongPtrW(host, GWL_EXSTYLE) & WS_EX_TOPMOST) == outputTopmost &&
		(GetWindowLongPtrW(output, GWL_EXSTYLE) & WS_EX_TOPMOST) == outputTopmost;
}

}
