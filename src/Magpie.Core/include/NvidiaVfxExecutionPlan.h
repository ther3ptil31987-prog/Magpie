#pragma once
#include "NvidiaVfxParameters.h"
#include "HdrComponents.h"
#include <cstddef>

namespace Magpie {
enum class NvidiaVfxPlanError { None, DuplicateGenerator, InvalidSettings, ExpectedSdr, UnsupportedPostEffect, InvalidColorPlan };
struct NvidiaVfxExecutionPlan {
	bool enabled = false;
	size_t generatorIndex = size_t(-1), postStart = 0, errorIndex = 0;
	NvidiaVfxPlanError error = NvidiaVfxPlanError::None;
	NvidiaVfxFrameGenerationSettings settings{};
};
// Conservative first-release contract. Expand only after per-output colour and
// temporal semantics have been checked; never silently move a user's effects.
inline bool NvidiaVfxPostEffectSupported(std::string_view name) noexcept {
	return name == NVIDIA_VFX_TRUE_HDR_ID || name == NVIDIA_VFX_VFG_ID ||
		name == "Bicubic" || name == "Bilinear" || name == "CAS\\CAS" || name == "FSR\\FSR_RCAS";
}
template<class Effects> NvidiaVfxExecutionPlan PlanNvidiaVfxExecution(const Effects& effects,
	const HdrComponentPlan& colors) noexcept {
	NvidiaVfxExecutionPlan plan; plan.postStart = effects.size();
	for (size_t i = 0; i < effects.size(); ++i) {
		if (effects[i].name == NVIDIA_VFX_TRUE_HDR_ID && plan.postStart == effects.size()) plan.postStart = i;
		if (effects[i].name != NVIDIA_VFX_VFG_ID) continue;
		if (plan.enabled) { plan.error = NvidiaVfxPlanError::DuplicateGenerator; plan.errorIndex = i; return plan; }
		plan.enabled = true; plan.generatorIndex = i;
		const auto read = [&](const char* key, float fallback, int min, int max) {
			const auto it = effects[i].parameters.find(key);
			const float value = it == effects[i].parameters.end() ? fallback : it->second;
			if (!std::isfinite(value) || value < min || value > max || std::floor(value) != value) {
				plan.error = NvidiaVfxPlanError::InvalidSettings; plan.errorIndex = i; return int(fallback);
			}
			return int(value);
		};
		plan.settings.multiplier = uint32_t(read("multiplier", 2, 2, 8));
		plan.settings.model = uint32_t(read("model", 1, 0, 1));
		if (!NvidiaVfxFrameGenerationSettingsValid(plan.settings)) {
			plan.error = NvidiaVfxPlanError::InvalidSettings; plan.errorIndex = i;
		}
		plan.settings.automaticShotDetection = read("automaticShotDetection", 1, 0, 1) != 0;
	}
	if (!plan.enabled || plan.error != NvidiaVfxPlanError::None) return plan;
	if (colors.error != HdrComponentError::None) {
		plan.error = NvidiaVfxPlanError::InvalidColorPlan; plan.errorIndex = colors.errorIndex; return plan;
	}
	const bool inputHdr = colors.enabled && (plan.postStart == effects.size() ? colors.outputHdr :
		plan.postStart < colors.stages.size() && colors.stages[plan.postStart].inputHdr);
	if (inputHdr) { plan.error = NvidiaVfxPlanError::ExpectedSdr; plan.errorIndex = plan.generatorIndex; return plan; }
	for (size_t i = plan.postStart; i < effects.size(); ++i)
		if (!NvidiaVfxPostEffectSupported(effects[i].name)) {
			plan.error = NvidiaVfxPlanError::UnsupportedPostEffect; plan.errorIndex = i; return plan;
		}
	return plan;
}
}
