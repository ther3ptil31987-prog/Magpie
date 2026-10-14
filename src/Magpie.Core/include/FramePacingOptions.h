#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace Magpie {

inline float SanitizePresentationFrameRate(float value) noexcept {
	return std::isfinite(value) && value >= 1.0f ? std::min(value, 1000.0f) : 0.0f;
}

enum class FrameSyncMode : uint32_t { FrontEdge = 0, Async = 1, Reflex = 2 };
enum class FrameSyncBackend { None, FrontEdge, Async, Reflex, XeLL };

inline bool IsValidFrameSyncMode(FrameSyncMode mode) noexcept {
	return static_cast<uint32_t>(mode) <= static_cast<uint32_t>(FrameSyncMode::Reflex);
}

struct FrameSyncSettings {
	bool enabled = true;
	float frameRate = 60.0f;
	FrameSyncMode mode = FrameSyncMode::FrontEdge;
	bool operator==(const FrameSyncSettings&) const = default;
};

// Merge only fields edited by this view. A stale panel must not overwrite a
// concurrent profile edit, and a conflict must leave both fields untouched.
inline bool MergeFrameSyncSettings(FrameSyncSettings& current,
	const FrameSyncSettings& before, const FrameSyncSettings& after) noexcept {
	if (!std::isfinite(after.frameRate) || after.frameRate != SanitizePresentationFrameRate(after.frameRate) ||
		!IsValidFrameSyncMode(after.mode)) return false;
	auto merged = current;
	if (after.enabled != before.enabled) {
		if (current.enabled != before.enabled && current.enabled != after.enabled) return false;
		merged.enabled = after.enabled;
	}
	if (after.frameRate != before.frameRate) {
		if (current.frameRate != before.frameRate && current.frameRate != after.frameRate) return false;
		merged.frameRate = after.frameRate;
	}
	if (after.mode != before.mode) {
		if (current.mode != before.mode && current.mode != after.mode) return false;
		merged.mode = after.mode;
	}
	current = merged;
	return true;
}

// Requested strategy is immutable during a scaling session. Availability of
// Reflex can temporarily change on resize; the renderer then uses Async.
inline FrameSyncBackend ResolveFrameSyncBackend(FrameSyncSettings settings,
	bool dlssFG, bool xessFG, bool frontEdgeSupported, bool benchmark, bool fsrFG = false) noexcept {
	if (!settings.enabled || benchmark) return FrameSyncBackend::None;
	if (xessFG) return FrameSyncBackend::XeLL;
	// AMD owns output pacing; StepTimer limits accepted base frames. No XeLL,
	// Reflex or frontend deadline/sleep is stacked on the proxy swapchain.
	if (fsrFG) return FrameSyncBackend::Async;
	if (settings.mode == FrameSyncMode::Async) return FrameSyncBackend::Async;
	if (settings.mode == FrameSyncMode::Reflex)
		return FrameSyncBackend::Reflex;
	return dlssFG || frontEdgeSupported ? FrameSyncBackend::FrontEdge : FrameSyncBackend::None;
}

inline uint32_t FrameSyncIntervalUs(double frameRate) noexcept {
	// Direct NVAPI: microseconds per base capture/SIMULATION cycle. Generated
	// presents use OUT_OF_BAND markers; do not multiply the base target here.
	return std::isfinite(frameRate) && frameRate > 0
		? static_cast<uint32_t>(std::ceil(1'000'000.0 / std::clamp(frameRate, 1.0, 1000.0))) : 0;
}

inline double ResolvePresentationFrameRate(double requested, double existingLimit,
	double refreshRate, unsigned multiplier = 1) noexcept {
	requested = SanitizePresentationFrameRate(static_cast<float>(requested));
	if (!(std::isfinite(existingLimit) && existingLimit > 0)) existingLimit = 0;
	if (!(std::isfinite(refreshRate) && refreshRate >= 10 && refreshRate <= 1000)) refreshRate = 60;
	const double target = requested > 0 ? requested : refreshRate / std::clamp(multiplier, 1u, 8u);
	return std::clamp(existingLimit > 0 ? std::min(target, existingLimit) : target, 1.0, 1000.0);
}

inline bool IsFrameRateFilterEffect(std::string_view effect) noexcept {
	return effect == "FrameRate_Filter" || effect == "Utility\\FrameRate_Filter";
}

inline bool UsesFrontEdgeSyncFrameRate(bool frontEdgeSyncEnabled, float mode) noexcept {
	// Missing/invalid modes follow the shared setting. Only 1 selects Custom.
	return frontEdgeSyncEnabled || mode != 1.0f;
}

inline float ResolveFrameRateFilterTarget(bool frontEdgeSyncEnabled, float mode,
	float customTarget, float frontEdgeTarget, double refreshRate, unsigned multiplier) noexcept {
	if (UsesFrontEdgeSyncFrameRate(frontEdgeSyncEnabled, mode)) {
		return static_cast<float>(ResolvePresentationFrameRate(frontEdgeTarget, 0, refreshRate, multiplier));
	}
	return std::isfinite(customTarget) ? std::clamp(customTarget, 1.0f, 240.0f) : 60.0f;
}

}
