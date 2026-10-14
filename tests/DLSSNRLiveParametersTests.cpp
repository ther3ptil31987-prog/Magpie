#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <vector>
#include "DLSSNRParameters.h"

namespace Magpie {
struct MotionVectorRequest { int method = 0; bool operator==(const MotionVectorRequest&) const = default; };
struct DlssnrExperimentProtocol { bool enabled = false; float scale = 1; };
struct EffectOption { std::map<std::string, float> parameters; };
inline MotionVectorRequest ParseDlssOpticalFlowRequest(const EffectOption& option) {
	const auto it = option.parameters.find("opticalFlowMethod");
	return {it == option.parameters.end() ? 0 : static_cast<int>(it->second)};
}
#include "DLSSNRSettingsUnderTest.h"
enum class EffectParameterApplyMode { Live, RestartRequired, Unavailable, Restricted };
struct DLSSNRFilter {
	struct Impl {
		std::vector<std::unique_ptr<Impl>> laterPasses;
		uint64_t evaluateParameterRevision = 0;
		bool resetHistory = false, residualParametersDirty = false, disabled = false;
	};
	std::unique_ptr<Impl> _impl = std::make_unique<Impl>();
	std::vector<DLSSNRSettings> _passSettings;
	DLSSNRSettings _settings;
	EffectParameterApplyMode GetParameterApplyMode(std::string_view) const noexcept;
	bool ApplyLiveParameters(const EffectOption&, std::span<const std::string>) noexcept;
};
#include "DLSSNRLiveUnderTest.h"
}

int main() {
	using namespace Magpie;
	DLSSNRFilter filter;
	EffectOption option{{{"enableInputResolutionScaling", 1.f}, {"multiPass", 3.f}, {"pass2_intensity", .7f}}};
	for (int i = 1; i <= 3; ++i) filter._passSettings.push_back(ParseDLSSNRSettings(DLSSNRPassOption(option, i)));
	filter._settings = filter._passSettings.front();
	for (int i = 0; i < 2; ++i) filter._impl->laterPasses.push_back(std::make_unique<DLSSNRFilter::Impl>());
	auto& first = *filter._impl;
	auto& second = *first.laterPasses[0];
	auto& third = *first.laterPasses[1];
	std::vector<std::string> names{"residualSaturation", "residualLightness"};
	option.parameters["residualSaturation"] = .3f;
	option.parameters["residualLightness"] = 1.2f;
	assert(filter.ApplyLiveParameters(option, names));
	assert(first.residualParametersDirty && !first.resetHistory && !second.resetHistory && !third.resetHistory);
	assert(first.evaluateParameterRevision == 0 && second.evaluateParameterRevision == 0 && third.evaluateParameterRevision == 0);
	assert(filter._settings.residualSaturation == .3f && filter._settings.residualLightness == 1.2f);
	for (const auto name : DLSSNR_RESIDUAL_PARAMETERS) {
		names = {std::string(name)};
		option.parameters[std::string(name)] = 1;
		assert(filter.ApplyLiveParameters(option,names));
		assert(first.evaluateParameterRevision == 0 && second.evaluateParameterRevision == 0 && third.evaluateParameterRevision == 0);
	}
	names = {"pass3_intensity"}; option.parameters["pass3_intensity"] = .4f;
	assert(filter.ApplyLiveParameters(option, names));
	assert(first.evaluateParameterRevision == 0 && second.evaluateParameterRevision == 0 && third.evaluateParameterRevision == 1);
	assert(!first.resetHistory && !second.resetHistory && third.resetHistory);
	names = {"pass2_intensity"}; option.parameters["pass2_intensity"] = .6f;
	assert(filter.ApplyLiveParameters(option, names));
	assert(!first.resetHistory && second.resetHistory && third.resetHistory);
	assert(filter._passSettings[0].intensity == 1 && filter._passSettings[1].intensity == .6f && filter._passSettings[2].intensity == .4f);
	// Reject the complete transaction before touching revisions, flags or values.
	names = {"intensity", "inputResolutionPercent"}; option.parameters["intensity"] = .2f;
	const auto revision = first.evaluateParameterRevision;
	assert(!filter.ApplyLiveParameters(option, names));
	assert(first.evaluateParameterRevision == revision && filter._passSettings[0].intensity == 1);
	names = {"intensity"}; option.parameters["inputResolutionPercent"] = 50;
	assert(!filter.ApplyLiveParameters(option, names));
	assert(first.evaluateParameterRevision == revision);
	option.parameters["inputResolutionPercent"] = 100;
	assert(filter.ApplyLiveParameters(option, names));
	assert(first.resetHistory && first.evaluateParameterRevision == revision + 1);
	assert(filter.ApplyLiveParameters(option, names));
	assert(first.evaluateParameterRevision == revision + 1); // no effective change
	// Hidden settings are saved without dirtying active passes.
	filter._passSettings.resize(1); first.laterPasses.clear();
	names = {"pass3_intensity"};
	const auto hiddenRevision = first.evaluateParameterRevision;
	assert(filter.ApplyLiveParameters(option, names));
	assert(first.evaluateParameterRevision == hiddenRevision);
	filter._settings.experimentalHdr.enabled = true;
	assert(filter.GetParameterApplyMode("residualSaturation") == EffectParameterApplyMode::Restricted);
	first.disabled = true;
	assert(filter.GetParameterApplyMode("intensity") == EffectParameterApplyMode::Unavailable);
	std::cout << "Production live transaction: NR isolation, dependent history resets, post-only reuse, rejection and hidden settings passed.\n";
}
