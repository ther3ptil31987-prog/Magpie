#pragma once
#include "EffectDesc.h"
#include <string_view>

namespace Magpie {
// Product-specific policy is opt-in for these built-ins. Third-party STEP
// declarations retain their existing normalization and UI behavior.
inline void ApplyHdrParameterPolicy(std::string_view effect, std::vector<EffectParameterDesc>& parameters) {
	if (effect != "Color\\HDR_to_SDR" && effect != "Color\\SDR_to_HDR" && effect != "RTXVideo\\RTXVideo_HDR") return;
	for (auto& parameter : parameters) {
		if (!parameter.choices.empty() || parameter.name == "debanding") continue;
		parameter.inputStep = 1;
		parameter.sliderStep = 5;
		parameter.coarseStep = 50;
		parameter.editMinimum = std::visit([](auto& c) { return float(c.minValue); }, parameter.constant);
		if (parameter.name == "nativeWhiteNits") {
			parameter.segmentedSteps = true;
			parameter.editMinimum = 80;
		} else if (parameter.name == "peakNits") parameter.sliderStep = 10;
		else if (parameter.name == "exposure" || parameter.name == "shoulder") {
			parameter.inputStep = .01f; parameter.sliderStep = .05f; parameter.coarseStep = .25f;
		} else if (parameter.name == "middleGray") {
			parameter.sliderStep = 1; parameter.coarseStep = 5;
		} else if (parameter.name == "contrast" || parameter.name == "saturation") parameter.coarseStep = 10;
	}
}

template<class Map, class Key>
float NativeHdrWhiteValue(const Map& values, const Key& merged, const Key& white, const Key& exposure) {
	if (const auto it = values.find(merged); it != values.end()) return it->second;
	const auto w = values.find(white), e = values.find(exposure);
	return (w == values.end() ? 203.f : w->second) * (e == values.end() ? 1.f : e->second);
}
}
