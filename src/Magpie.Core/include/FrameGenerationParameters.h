#pragma once
#include <cmath>
#include <string>
#include <string_view>

namespace Magpie {

// The same policy is used by the wide-string App model and narrow-string Core
// sessions. Only built-in FG IDs own these parameters; custom effects do not.
template<class Effect>
bool NormalizeFrameGenerationParameters(Effect& effect) {
	using Char = typename decltype(effect.name)::value_type;
	const auto key = [](std::string_view text) {
		return std::basic_string<Char>(text.begin(), text.end());
	};
	const bool fsr = effect.name == key("FSR3FG\\FSR3_FrameGeneration");
	const bool fg = fsr || effect.name == key("DLSSFG\\DLSS_FrameGeneration") ||
		effect.name == key("XeSSFG\\XeSS_FrameGeneration") ||
		effect.name == key("XeSSFG\\XeSS_FrameGeneration_x2_ZeroMV") ||
		effect.name == key("XeSSFG\\XeSS_MultiFrameGeneration_ZeroMV") ||
		effect.name == key("NvidiaVFX\\VFG");
	if (!fg) return false;
	bool changed = effect.parameters.erase(key("duplicateFrameFiltering")) != 0;
	if (!fsr) return changed;
	const auto valid = [&](std::string_view name, float lo, float hi) {
		const auto it = effect.parameters.find(key(name));
		return it != effect.parameters.end() && std::isfinite(it->second) &&
			it->second >= lo && it->second <= hi && std::floor(it->second) == it->second;
	};
	const auto store = [&](std::string_view name, float value) {
		auto [it, inserted] = effect.parameters.try_emplace(key(name), value);
		if (inserted || it->second != value) {
			it->second = value;
			changed = true;
		}
	};
	// Retired None/missing/invalid method selects the whole default pair. A
	// hidden old AMD Performance choice must not leak into the new default.
	if (!valid("opticalFlowMethod", 1, 2)) {
		store("opticalFlowMethod", 1);
		store("amdOpticalFlowMode", 1);
	}
	if (!valid("amdOpticalFlowMode", 0, 1)) store("amdOpticalFlowMode", 1);
	if (!valid("nvidiaOpticalFlowQuality", 1, 5)) store("nvidiaOpticalFlowQuality", 2);
	return changed;
}

}
