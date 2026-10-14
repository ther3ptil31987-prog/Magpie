#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

namespace Magpie {

struct NvidiaVfxTrueHdrTunables {
	// SDK order: contrast, saturation, middle gray, luminance, debanding off.
	std::array<uint32_t, 5> values{100, 100, 50, 1000, 0};
	bool operator==(const NvidiaVfxTrueHdrTunables&) const = default;
};

template<class Effect> std::optional<NvidiaVfxTrueHdrTunables> ReadNvidiaVfxTrueHdrTunables(
	const Effect& option) noexcept {
	constexpr const char* keys[]{"contrast", "saturation", "middleGray", "peakNits", "debanding"};
	constexpr float defaults[]{100, 100, 50, 1000, 1};
	constexpr float minimum[]{0, 0, 10, 400, 0};
	constexpr float maximum[]{200, 200, 100, 2000, 1};
	NvidiaVfxTrueHdrTunables result;
	for (size_t i = 0; i < result.values.size(); ++i) {
		const auto it = option.parameters.find(keys[i]);
		const float value = it == option.parameters.end() ? defaults[i] : it->second;
		if (!std::isfinite(value) || value < minimum[i] || value > maximum[i] || std::floor(value) != value)
			return std::nullopt;
		result.values[i] = i == 4 ? (value == 0 ? 1u : 0u) : uint32_t(value);
	}
	return result;
}

enum class NvidiaVfxTrueHdrUpdate { Unchanged, Applied, Rejected, Invalidated };

// Execute between outputs on the owning backend thread. Restore the complete
// committed tuple after a partial failure, including the setter that failed.
template<class Set> NvidiaVfxTrueHdrUpdate UpdateNvidiaVfxTrueHdrTunables(
	const NvidiaVfxTrueHdrTunables& previous, const NvidiaVfxTrueHdrTunables& candidate, Set&& set) noexcept {
	if (previous == candidate) return NvidiaVfxTrueHdrUpdate::Unchanged;
	for (size_t i = 0; i < candidate.values.size(); ++i) {
		if (previous.values[i] == candidate.values[i]) continue;
		if (set(i, candidate.values[i])) continue;
		bool restored = true;
		for (size_t j = 0; j < previous.values.size(); ++j) {
			if (!set(j, previous.values[j])) restored = false;
		}
		return restored ? NvidiaVfxTrueHdrUpdate::Rejected : NvidiaVfxTrueHdrUpdate::Invalidated;
	}
	return NvidiaVfxTrueHdrUpdate::Applied;
}

} // namespace Magpie
