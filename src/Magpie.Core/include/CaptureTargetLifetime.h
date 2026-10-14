#pragma once
#include <windows.h>
#include <atomic>
#include <cstdint>

namespace Magpie {

// One shared signal per GraphicsCaptureItem, retained by Closed callbacks.
// Replacing/retiring an item disables its wakeup. It never holds a Renderer,
// window pointer, D3D resource or a callback into a subsequent session.
class CaptureTargetLifetime {
public:
	explicit CaptureTargetLifetime(DWORD backendThread) noexcept : _thread(backendThread) {}
	void Close() noexcept {
		_closed.store(true, std::memory_order_release);
		if (!_retired.load(std::memory_order_acquire)) PostThreadMessageW(_thread, WM_NULL, 0, 0);
	}
	bool IsClosed() const noexcept { return _closed.load(std::memory_order_acquire); }
	void Retire() noexcept { _retired.store(true, std::memory_order_release); }

private:
	const DWORD _thread;
	std::atomic<bool> _closed = false;
	std::atomic<bool> _retired = false;
};

}
