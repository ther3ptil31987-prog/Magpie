#pragma once
#include <windows.h>
#include <cstdint>

namespace Magpie {

// Frontend-thread state. Destruction messages are notifications, not a place
// to join a backend or release a driver/SDK. The HWND and its userdata remain
// recorded after WindowBaseT clears Handle() during WM_DESTROY.
class ScalingWindowRetirement {
public:
	bool Begin(HWND window, LONG_PTR identity) noexcept {
		if (_window || !window) return false;
		_window = window;
		_identity = identity;
		return true;
	}
	void NonclientDestroyed() noexcept { _nonclientDestroyed = true; }
	bool Pending() const noexcept { return _window != nullptr; }
	HWND Window() const noexcept { return _window; }
	bool Ready(unsigned callbackDepth) const noexcept {
		if (!_window || !_nonclientDestroyed || callbackDepth) return false;
		// A recycled HWND belonging to a different object cannot retain the old
		// owner relationship. No new ScalingWindow can start while Pending().
		return !IsWindow(_window) || GetWindowLongPtrW(_window, GWLP_USERDATA) != _identity;
	}
	void Complete() noexcept { _window = nullptr; _identity = 0; _nonclientDestroyed = false; }

private:
	HWND _window = nullptr;
	LONG_PTR _identity = 0;
	bool _nonclientDestroyed = false;
};

}
