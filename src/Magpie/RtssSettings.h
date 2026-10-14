#pragma once
#include <cmath>
#include <cstdint>
#include <optional>

namespace Magpie {

enum class RtssScope : uint32_t { None, Global, Target };
enum class RtssRateMode : uint32_t { Custom, ContentSync };
enum class RtssExitPolicy : uint32_t { KeepChanges, RestoreOriginal };

// Preferences only. External snapshots and process identities belong to a session.
struct RtssSettings {
	RtssScope scope = RtssScope::None;
	RtssRateMode rateMode = RtssRateMode::Custom;
	uint32_t customRate = 60;
	RtssExitPolicy exitPolicy = RtssExitPolicy::RestoreOriginal;
	// Independent simple-mode intent; advanced preferences remain untouched.
	bool simpleContentSync = false;
	static constexpr uint32_t MinimumRate = 15;
	static constexpr uint32_t MaximumRate = 360;
	bool operator==(const RtssSettings&) const = default;
	bool IsValid() const noexcept {
		return uint32_t(scope) <= uint32_t(RtssScope::Target) &&
			uint32_t(rateMode) <= uint32_t(RtssRateMode::ContentSync) &&
			uint32_t(exitPolicy) <= uint32_t(RtssExitPolicy::RestoreOriginal) &&
			(rateMode == RtssRateMode::ContentSync || (customRate >= MinimumRate && customRate <= MaximumRate));
	}
	static std::optional<uint32_t> EditedRate(double value) noexcept {
		if (!std::isfinite(value) || value < MinimumRate || value > MaximumRate) return std::nullopt;
		return uint32_t(std::round(value));
	}
	std::optional<uint32_t> ResolveRate(bool fixedContent, double effectiveBaseRate) const noexcept {
		if (!IsValid() || scope == RtssScope::None) return std::nullopt;
		if (rateMode == RtssRateMode::Custom) return customRate;
		if (!fixedContent || !std::isfinite(effectiveBaseRate) || effectiveBaseRate < 1 || effectiveBaseRate > 1000)
			return std::nullopt;
		return uint32_t(std::round(effectiveBaseRate));
	}
};

inline RtssSettings EffectiveRtssSettings(RtssSettings settings, bool simple) noexcept {
	if (simple) {
		settings.scope = settings.simpleContentSync ? RtssScope::Target : RtssScope::None;
		settings.rateMode = RtssRateMode::ContentSync;
		settings.exitPolicy = RtssExitPolicy::RestoreOriginal;
	}
	return settings;
}

}
