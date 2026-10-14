#pragma once
#include <chrono>
#include <cmath>
#include <algorithm>
#include <optional>

namespace Magpie {

// Frontend-owned. The backend never manufactures capture input to satisfy this
// deadline. Failed attempts are paced too; messages/new content can interrupt.
class IdlePresentationClock {
public:
	using Clock = std::chrono::steady_clock;
	void SetRate(double rate) noexcept {
		_interval = std::isfinite(rate) && rate > 0 ?
			std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / rate)) :
			Clock::duration::zero();
	}
	void Presented(Clock::time_point now, bool content) noexcept {
		_last = now; _started = true;
		if (content) { _lastContent = now; _idle = false; }
	}
	void Attempted(Clock::time_point now) noexcept { _last = now; _idle = true; }
	std::optional<Clock::duration> Remaining(Clock::time_point now) const noexcept {
		if (!_started || _interval <= Clock::duration::zero()) return std::nullopt;
		// A 24/30 FPS stream has ordinary gaps longer than a 60 Hz redraw
		// interval. Do not disable/re-enable proxy FG between every input.
		const auto due = _idle ? _last + _interval :
			std::max(_last + _interval, _lastContent + std::chrono::milliseconds(500));
		return std::max(Clock::duration::zero(), due - now);
	}
	void Reset() noexcept { _started = _idle = false; }
private:
	Clock::duration _interval{};
	Clock::time_point _last{};
	Clock::time_point _lastContent{};
	bool _started = false, _idle = false;
};

}
