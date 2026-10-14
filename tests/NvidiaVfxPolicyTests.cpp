#include <d3d11.h>
#include <winrt/base.h>
#include <array>
#include <string>
#include <cassert>
#include <limits>
#include <map>
#include <vector>
#include "NvidiaVfxParameterPolicy.h"
#include "NvidiaVfxExecutionPlan.h"
#include "RTXVideoParameters.h"
#include "EffectParameterPersistence.h"

struct StoredEffect {
	std::string name;
	std::map<std::string,float> parameters;
	int scalingType = 1;
	std::pair<float,float> scale{1.f,1.f};
};
int main() {
	using namespace Magpie;
	// Category-restored dimensions are saved with the parameters, while
	// independent editor changes and conflicting transactions remain protected.
	StoredEffect before{std::string(NVIDIA_VFX_VSR_ID),{{"category",0.f}},1,{1.f,1.f}};
	StoredEffect after = before; after.scalingType = 2; after.scale = {1920.f,1080.f};
	StoredEffect current = before;
	assert(MergeEffectScalingChanges(current,before,after) && current.scale == after.scale);
	assert(MergeEffectScalingChanges(current,before,after));
	current.scale = {1280.f,720.f};
	assert(!MergeEffectScalingChanges(current,before,after));
	assert(MergeEffectScalingChanges(current,before,before) && current.scale.first == 1280.f);
	for (float category : {-1.f,0.f,1.f,2.f,3.f,4.f,5.f,.5f}) {
		struct LegacyEffect { std::wstring name; std::map<std::wstring,float> parameters; };
		LegacyEffect old{L"RTXVideo\\RTXVideo_Denoise_Medium",{{L"category",category}}};
		assert(MigrateEffectParametersR1(old) && old.parameters.at(L"category") == 3.f && old.parameters.at(L"strength") == 1.f);
		assert(!MigrateEffectParametersR1(old));
	}
	const int expected[5][4]{{1,2,3,4},{16,17,18,19},{-1,21,-1,23},{8,9,10,11},{12,13,14,15}};
	for (int family=0; family<5; ++family) for(int tier=0; tier<4; ++tier) {
		assert(NvidiaVfxTierValid(family,float(tier)) == (expected[family][tier] != -1));
		if(expected[family][tier] != -1) assert(NvidiaVfxQualityLevel(family,tier) == uint32_t(expected[family][tier]));
	}
	for (uint32_t reserved : {0u,5u,6u,7u,20u,22u,24u}) assert(!NvidiaVfxQualityValid(reserved));
	StoredEffect vsr{std::string(NVIDIA_VFX_VSR_ID),{{"strength",2.f}},1,{.9f,.9f}};
	ChangeNvidiaVfxVsrCategory(vsr,2); assert(vsr.parameters.at("strength") == 1.f);
	vsr.parameters["strength"]=3.f;
	ChangeNvidiaVfxVsrCategory(vsr,3); assert(vsr.scale.first == 1.f && vsr.scalingType == 0);
	ChangeNvidiaVfxVsrCategory(vsr,0); assert(vsr.parameters.at("strength") == 2.f && vsr.scale.first == .9f);
	ChangeNvidiaVfxVsrCategory(vsr,2); assert(vsr.parameters.at("strength") == 3.f);
	const auto exact=PlanNvidiaVfxUpscale({1280,720},{1920,1080},0);
	assert(exact.valid && exact.ratioIndex==1 && exact.adaptation==NvidiaVfxAdaptation::None);
	const auto down=PlanNvidiaVfxUpscale({1280,720},{2304,1296},0);
	assert(down.valid && down.ratioIndex==2 && down.native.width==2560 && down.adaptation==NvidiaVfxAdaptation::Downsample);
	const auto beyond=PlanNvidiaVfxUpscale({640,360},{3840,2160},0);
	assert(beyond.valid && beyond.ratioIndex==4 && beyond.native.width==2560 && beyond.adaptation==NvidiaVfxAdaptation::Upsample);
	assert(!PlanNvidiaVfxUpscale({1282,720},{1920,1080},1).valid);
	assert(!PlanNvidiaVfxUpscale({4200,2400},{16000,9000},0).valid);
	assert(!PlanNvidiaVfxUpscale({0,720},{1920,1080},0).valid);
	for (uint32_t m=2; m<=8; ++m) {
		std::vector<StoredEffect> effects{{std::string(NVIDIA_VFX_VFG_ID),{{"multiplier",float(m)}}},{std::string(NVIDIA_VFX_TRUE_HDR_ID)},{"Bicubic"}};
		const auto plan=PlanNvidiaVfxExecution(effects,BuildHdrComponentPlan(effects));
		assert(plan.enabled && plan.error==NvidiaVfxPlanError::None && plan.settings.multiplier==m && plan.postStart==1);
		// Moving only the selector does not reorder the image effects or turn
		// VFG into a processor of already-HDR colours.
		std::swap(effects[0],effects[1]);
		assert(PlanNvidiaVfxExecution(effects,BuildHdrComponentPlan(effects)).postStart==0);
	}
	std::vector<StoredEffect> unsafe{{std::string(NVIDIA_VFX_VFG_ID)},{std::string(NVIDIA_VFX_TRUE_HDR_ID)},{"DLSSNR\\DLSSNR_AI_Filter"}};
	assert(PlanNvidiaVfxExecution(unsafe,BuildHdrComponentPlan(unsafe)).error==NvidiaVfxPlanError::UnsupportedPostEffect);
	// RCAS uses the installed FSR ID. Exercise the TrueHDR boundary so this
	// verifies post-processing admission, not the unrestricted SDR pre-chain.
	for (const auto id : {"FSR\\FSR_RCAS", "CAS\\CAS", "Bicubic", "Bilinear"}) {
		unsafe.back().name = id;
		assert(PlanNvidiaVfxExecution(unsafe,BuildHdrComponentPlan(unsafe)).error==NvidiaVfxPlanError::None);
	}
	for (const auto id : {"RCAS\\RCAS", "CAS\\CAS_Scaling", "FSR\\FSR_EASU", "CRT\\CRT_Geom", "custom\\FSR_RCAS"}) {
		unsafe.back().name = id;
		assert(PlanNvidiaVfxExecution(unsafe,BuildHdrComponentPlan(unsafe)).error==NvidiaVfxPlanError::UnsupportedPostEffect);
	}
	std::vector<StoredEffect> hdr{{"Color\\HDR_to_SDR"},{std::string(NVIDIA_VFX_VFG_ID)}};
	assert(PlanNvidiaVfxExecution(hdr,BuildHdrComponentPlan(hdr)).error==NvidiaVfxPlanError::None);
	hdr={{"Color\\SDR_to_HDR"},{std::string(NVIDIA_VFX_VFG_ID)}};
	assert(PlanNvidiaVfxExecution(hdr,BuildHdrComponentPlan(hdr)).error==NvidiaVfxPlanError::ExpectedSdr);
	unsafe[0].parameters["multiplier"]=std::numeric_limits<float>::quiet_NaN();
	assert(PlanNvidiaVfxExecution(unsafe,BuildHdrComponentPlan(unsafe)).error==NvidiaVfxPlanError::InvalidSettings);

	// Retired High migrates at persistence boundaries, not inside the execution
	// plan. Reject direct legacy/invalid runtime requests; preserve other effects.
	for (float value : { 0.f, 1.f, 2.f, -1.f, 3.f, .5f, 1.5f,
		std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() }) {
		StoredEffect effect{std::string(NVIDIA_VFX_VFG_ID),{{"model",value},{"multiplier",4.f},{"duplicateFrameFiltering",0.f}}};
		assert(NvidiaVfxVfgModelValid(value) == (value == 0.f || value == 1.f));
		std::vector<StoredEffect> chain{effect};
		assert((PlanNvidiaVfxExecution(chain,BuildHdrComponentPlan(chain)).error == NvidiaVfxPlanError::None) == NvidiaVfxVfgModelValid(value));
		const bool migrated = MigrateNvidiaVfxVfgModel(effect);
		assert(migrated == (value == 2.f));
		assert(!MigrateNvidiaVfxVfgModel(effect));
		assert(effect.parameters.at("multiplier") == 4.f && effect.parameters.at("duplicateFrameFiltering") == 0.f);
		if (migrated) {
			assert(effect.parameters.at("model") == 1.f);
			chain[0] = effect;
			assert(PlanNvidiaVfxExecution(chain,BuildHdrComponentPlan(chain)).settings.model == 1);
		}
	}
	StoredEffect missing{std::string(NVIDIA_VFX_VFG_ID)};
	assert(!MigrateNvidiaVfxVfgModel(missing) && missing.parameters.empty());
	std::vector<StoredEffect> defaultChain{missing};
	assert(PlanNvidiaVfxExecution(defaultChain,BuildHdrComponentPlan(defaultChain)).settings.model == 1);
	StoredEffect other{"ThirdParty\\VFG",{{"model",2.f}}};
	assert(!MigrateNvidiaVfxVfgModel(other) && other.parameters.at("model") == 2.f);
	struct WideEffect { std::wstring name; std::map<std::wstring,float> parameters; };
	WideEffect stored{L"NvidiaVFX\\VFG",{{L"model",2.f},{L"multiplier",8.f}}};
	assert(MigrateNvidiaVfxVfgModel(stored) && stored.parameters.at(L"model") == 1.f);
	assert(!MigrateNvidiaVfxVfgModel(stored) && stored.parameters.at(L"multiplier") == 8.f);
	for (uint32_t model : { 0u, 1u, 2u, 3u, UINT32_MAX }) {
		NvidiaVfxFrameGenerationSettings settings; settings.model = model;
		assert(NvidiaVfxFrameGenerationSettingsValid(settings) == (model <= 1));
	}
}
