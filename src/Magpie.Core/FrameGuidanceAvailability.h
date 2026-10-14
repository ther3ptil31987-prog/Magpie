#pragma once
#include <cstdint>
#include <string_view>

namespace Magpie {
// Provenance of a provider result, independent of the vector values. A valid
// stationary image can have entirely zero vectors without being a fallback.
enum class FrameGuidanceAvailability : uint8_t {
	Unavailable, Valid, ResetOnly, ProviderFailed, ProviderStopped, SynchronizationFailed
};
constexpr std::string_view FrameGuidanceAvailabilityName(FrameGuidanceAvailability value) noexcept {
	switch (value) {
	case FrameGuidanceAvailability::Valid: return "valid";
	case FrameGuidanceAvailability::ResetOnly: return "reset-only";
	case FrameGuidanceAvailability::ProviderFailed: return "provider-failed";
	case FrameGuidanceAvailability::ProviderStopped: return "provider-stopped";
	case FrameGuidanceAvailability::SynchronizationFailed: return "synchronization-failed";
	default: return "unavailable";
	}
}
}
