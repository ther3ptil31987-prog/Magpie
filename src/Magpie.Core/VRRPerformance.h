#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstddef>

namespace Magpie {

// Opt-in instruction/submission counters, available in a normal Release build.
// These are NOT GPU execution times or measured display/scanout frame counts.
class VRRPerformance {
public:
	enum class Event : uint32_t {
		CaptureAccepted, EffectExecute, EffectReuse, RealPublication, GeneratedPublication,
		RealSubmit, GeneratedSubmit, AuxiliarySubmit, IndependentOverlaySubmit, FailedSubmit,
		FullSceneCopy, RegionSceneCopy, TargetClear, StableCopy, OverlayDraw, CursorDraw,
		PresentCall, TearingPresentCall, PresentCpuNs,
		AuxCursorDirty, AuxPanelDirty, AuxInputDirty, AuxBackgroundDirty, AuxUnclassified,
		ReportedFrames, ReportedFrameSamples, Count
	};
	static constexpr std::array Names{
		"capture", "effectExecute", "effectReuse", "realPublication", "generatedPublication",
		"realSubmit", "generatedSubmit", "auxiliarySubmit", "independentOverlaySubmit", "failedSubmit",
		"fullSceneCopy", "regionSceneCopy", "targetClear", "stableCopy", "overlayDraw", "cursorDraw",
		"presentCall", "tearingPresentCall", "presentCpuNs",
		"auxCursorDirty", "auxPanelDirty", "auxInputDirty", "auxBackgroundDirty", "auxUnclassified",
		"reportedFrames", "reportedFrameSamples"
	};
	using Sample = std::array<uint64_t, size_t(Event::Count)>;
	// Frontend, before backend start / after backend join. No in-flight counter
	// writer may overlap resetting a session.
	static void Start(bool enabled) noexcept {
		_enabled.store(false, std::memory_order_relaxed);
		for (auto& counter : _counters) counter.store(0, std::memory_order_relaxed);
		_enabled.store(enabled, std::memory_order_release);
	}
	static void Stop() noexcept { _enabled.store(false, std::memory_order_release); }
	static bool Enabled() noexcept { return _enabled.load(std::memory_order_acquire); }
	static void Record(Event event, uint64_t count = 1) noexcept {
		if (Enabled()) _counters[size_t(event)].fetch_add(count, std::memory_order_relaxed);
	}
	static Sample Read() noexcept {
		Sample result{};
		for (size_t i = 0; i < result.size(); ++i) result[i] = _counters[i].load(std::memory_order_relaxed);
		return result; // Approximate window boundaries across frontend/backend.
	}
	static void AuxiliaryReasons(uint32_t reasons) noexcept {
		if (!reasons) Record(Event::AuxUnclassified);
		if (reasons & 1) Record(Event::AuxCursorDirty);
		if (reasons & 2) Record(Event::AuxPanelDirty);
		if (reasons & 4) Record(Event::AuxInputDirty);
		if (reasons & 8) Record(Event::AuxBackgroundDirty);
	}
	class PresentScope {
	public:
		explicit PresentScope(bool tearing) noexcept : _enabled(Enabled()) {
			if (!_enabled) return;
			_start = std::chrono::steady_clock::now();
			Record(Event::PresentCall);
			if (tearing) Record(Event::TearingPresentCall);
		}
		~PresentScope() noexcept {
			if (_enabled) Record(Event::PresentCpuNs, uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now() - _start).count()));
		}
		PresentScope(const PresentScope&) = delete;
		PresentScope& operator=(const PresentScope&) = delete;
	private:
		bool _enabled;
		std::chrono::steady_clock::time_point _start{};
	};

private:
	static_assert(Names.size() == size_t(Event::Count));
	static inline std::atomic<bool> _enabled = false;
	static inline std::array<std::atomic<uint64_t>, size_t(Event::Count)> _counters{};
};

}
