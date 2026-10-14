#pragma once
#include <cstdint>

namespace Magpie {

// Persisted values and Home's SelectedIndex share this stable order.
enum class ScreenshotContent : uint32_t {
	InputAndOutput = 0,
	OutputOnly = 1,
	InputOnly = 2
};

constexpr ScreenshotContent NormalizeScreenshotContent(uint32_t value) noexcept {
	return value <= 2 ? static_cast<ScreenshotContent>(value) : ScreenshotContent::OutputOnly;
}

struct ScreenshotFrameIdentity {
	uint64_t frameId = 0;
	uint64_t captureSequence = 0;
	uint64_t resourceGeneration = 0;

	constexpr bool Matches(const ScreenshotFrameIdentity& other, bool generated) const noexcept {
		// Sequence zero is valid for capture methods that never interrupt.
		return !generated && frameId != 0 && resourceGeneration != 0 &&
			frameId == other.frameId && captureSequence == other.captureSequence &&
			resourceGeneration == other.resourceGeneration;
	}
};

}
