#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace Magpie {

enum class HdrComponentKind { None, HdrToSdr, SdrToHdr, RtxVideoHdr };

inline HdrComponentKind ClassifyHdrComponent(std::string_view name) noexcept {
	if (name == "Color\\HDR_to_SDR") return HdrComponentKind::HdrToSdr;
	if (name == "Color\\SDR_to_HDR") return HdrComponentKind::SdrToHdr;
	if (name == "RTXVideo\\RTXVideo_HDR") return HdrComponentKind::RtxVideoHdr;
	return HdrComponentKind::None;
}

enum class HdrComponentError { None, ExpectedHdr, ExpectedSdr, MissingPair, InvalidParameters };

struct HdrComponentStage {
	HdrComponentKind kind = HdrComponentKind::None;
	bool inputHdr = false;
	bool outputHdr = false;
	// Index of the upstream compatibility mapping. The same immutable settings
	// drive both ends; a display tone map does not create a restorable pair.
	size_t pairIndex = static_cast<size_t>(-1);
	int mode = 0;
	float whiteNits = 203.0f;
	float peakNits = 1000.0f;
	float exposure = 1.0f;
	float shoulder = 1.0f;
};

struct HdrComponentPlan {
	bool enabled = false;
	bool captureHdr = false;
	bool outputHdr = false;
	HdrComponentError error = HdrComponentError::None;
	size_t errorIndex = 0;
	std::vector<HdrComponentStage> stages;
};

// Pure preflight: no SDK calls, windows, graphics devices or file access.
// The first explicit converter determines the requested capture domain.
// An internal intent may preserve a migrated trial chain's capture contract;
// it never inserts, hides, reorders or synchronizes conversion nodes.
template<class Effects>
HdrComponentPlan BuildHdrComponentPlan(const Effects& effects, std::optional<bool> captureIntent = std::nullopt) {
	HdrComponentPlan plan;
	if (captureIntent) {
		plan.enabled = true;
		plan.captureHdr = *captureIntent;
	}
	for (const auto& effect : effects) {
		if (captureIntent) break;
		const auto kind = ClassifyHdrComponent(effect.name);
		if (kind == HdrComponentKind::None) continue;
		plan.enabled = true;
		plan.captureHdr = kind == HdrComponentKind::HdrToSdr;
		break;
	}
	if (!plan.enabled) return plan;
	bool hdr = plan.captureHdr;
	size_t pair = static_cast<size_t>(-1);
	for (size_t i = 0; i < effects.size(); ++i) {
		const auto& effect = effects[i];
		HdrComponentStage stage{ .kind = ClassifyHdrComponent(effect.name), .inputHdr = hdr, .outputHdr = hdr };
		auto fail = [&](HdrComponentError error) {
			plan.error = error;
			plan.errorIndex = i;
		};
		auto read = [&](const char* key, float fallback, float min, float max, bool integral = false) {
			const auto it = effect.parameters.find(key);
			const float value = it == effect.parameters.end() ? fallback : it->second;
			if (!std::isfinite(value) || value < min || value > max || (integral && value != std::floor(value)))
				fail(HdrComponentError::InvalidParameters);
			return value;
		};
		if (stage.kind != HdrComponentKind::None) {
			stage.whiteNits = read("whiteNits", 203, 80, 400);
			stage.peakNits = read("peakNits", 1000, 400, 4000);
			stage.exposure = read("exposure", 1, 0.1f, 4);
			stage.shoulder = read("shoulder", 1, 0.1f, 4);
			if (plan.error != HdrComponentError::None) return plan;
		}
		if (stage.kind == HdrComponentKind::HdrToSdr) {
			if (!hdr) { fail(HdrComponentError::ExpectedHdr); return plan; }
			const float mode = read("mode", 0, 0, 1, true);
			if (plan.error != HdrComponentError::None) return plan;
			stage.mode = static_cast<int>(mode);
			pair = stage.mode == 0 ? i : static_cast<size_t>(-1);
			hdr = false;
		} else if (stage.kind == HdrComponentKind::SdrToHdr) {
			if (hdr) { fail(HdrComponentError::ExpectedSdr); return plan; }
			const float mode = read("mode", 0, 0, 2, true);
			if (plan.error != HdrComponentError::None) return plan;
			stage.mode = static_cast<int>(mode);
			if (stage.mode == 2 && pair == static_cast<size_t>(-1)) {
				fail(HdrComponentError::MissingPair); return plan;
			}
			if (stage.mode != 1) stage.pairIndex = pair;
			// Absence of the merged field deliberately keeps both the pixel and
			// white-point metadata contracts of legacy exposure configurations.
			// Low legacy values remain representable without an implicit clamp.
			if (stage.pairIndex == static_cast<size_t>(-1) && effect.parameters.contains("nativeWhiteNits")) {
				stage.whiteNits = read("nativeWhiteNits", 203, 8, 4000);
				stage.exposure = 1;
				// SDR [0,1] white is also the predicted maximum of this proportional
				// mapping. Keep FG/presentation metadata aligned with the new signal,
				// rather than carrying the unrelated legacy 1000-nit pair peak.
				stage.peakNits = std::max(80.f, stage.whiteNits);
				if (plan.error != HdrComponentError::None) return plan;
			}
			pair = static_cast<size_t>(-1);
			hdr = true;
		} else if (stage.kind == HdrComponentKind::RtxVideoHdr) {
			if (hdr) { fail(HdrComponentError::ExpectedSdr); return plan; }
			stage.peakNits = read("peakNits", 1000, 400, 2000, true);
			read("contrast", 100, 0, 200, true);
			read("saturation", 100, 0, 200, true);
			read("middleGray", 50, 10, 100, true);
			read("debanding", 1, 0, 1, true);
			if (plan.error != HdrComponentError::None) return plan;
			pair = static_cast<size_t>(-1);
			hdr = true;
		}
		stage.outputHdr = hdr;
		plan.stages.push_back(stage);
	}
	plan.outputHdr = hdr;
	return plan;
}

}
