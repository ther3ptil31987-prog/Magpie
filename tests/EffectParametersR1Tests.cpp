#include "RTXVideoParameters.h"
#include "DlssnrAutoHdr.h"
#include "EffectParameterPersistence.h"
#include <cassert>
#include <limits>
#include <map>
#include <iostream>
#include <string>

struct StoredEffect {
	std::wstring name;
	std::map<std::wstring, float> parameters;
	std::pair<float, float> scale{1.5f, 2.0f};
	int scalingType = 3;
};

int main() {
	using namespace Magpie;
	for (int family = 0; family < 2; ++family)
		for (int tier = 0; tier < 4; ++tier) {
			StoredEffect effect{std::wstring(RTX_VIDEO_IDS[family][tier]), {{L"other", 0.25f}}};
			assert(MigrateEffectParametersR1(effect));
			assert(effect.name == RTXVideoCanonicalId<wchar_t>(1));
			assert(effect.parameters.at(L"category") == (family == 0 ? 3.f : 0.f));
			assert(effect.parameters.at(L"strength") == tier);
			assert(effect.parameters.at(L"other") == 0.25f);
			assert(effect.scale.first == 1.5f && effect.scale.second == 2.0f && effect.scalingType == 3);
			assert(!MigrateEffectParametersR1(effect));
		}
	StoredEffect fresh{L"RTXVideo\\RTXVideo_VSR"};
	assert(MigrateEffectParametersR1(fresh) && fresh.parameters.at(L"strength") == 1);
	for (float invalid : {-1.0f, 4.0f, 1.5f, std::numeric_limits<float>::infinity(),
						  std::numeric_limits<float>::quiet_NaN()}) {
		fresh.parameters[L"strength"] = invalid;
		assert(MigrateEffectParametersR1(fresh) && fresh.parameters.at(L"strength") == 1);
		assert(!MigrateEffectParametersR1(fresh));
	}
	StoredEffect custom{L"RTXVideo\\RTXVideo_VSR_High_Custom", {{L"strength", 12.0f}}};
	assert(!MigrateEffectParametersR1(custom) && custom.parameters.at(L"strength") == 12);
	StoredEffect nr{L"DLSSNR\\DLSSNR_AI_Filter",
					{{L"experimentalHdrPath", 0.0f},
					 {L"experimentalHdrScale", 4.5f},
					 {L"style", 2.0f},
					 {L"intensity", 0.5f}}};
	assert(MigrateEffectParametersR1(nr));
	assert(nr.parameters.size() == 2 && nr.parameters.at(L"style") == 2 &&
		   nr.parameters.at(L"intensity") == 0.5f);
	assert(!MigrateEffectParametersR1(nr));
	std::map<std::string, float> desired{{"strength", 3.0f}};
	assert(RestoreRejectedEffectParameter(desired, "strength", 3.0f, 1.0f));
	assert(desired.at("strength") == 1.0f);
	desired["strength"] = 2.0f;
	assert(!RestoreRejectedEffectParameter(desired, "strength", 3.0f, 1.0f));
	assert(desired.at("strength") == 2.0f);

	HdrFrameMetadata input{};
	input.width = 1920;
	input.height = 1080;
	input.valid = true;
	input.sourceFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	assert(!UseDlssnrAutoHdr(true, true, input)); // FP16 with unknown semantics.
	input.color.primaries = HdrColorPrimaries::Rec709;
	input.color.transfer = HdrTransferFunction::Linear;
	input.color.range = HdrColorRange::SceneLinear;
	input.stage = HdrFrameStage::RawCapture;
	assert(!UseDlssnrAutoHdr(true, true, input));
	input.stage = HdrFrameStage::CanonicalInput;
	assert(UseDlssnrAutoHdr(true, true, input));
	assert(!UseDlssnrAutoHdr(false, true, input));
	assert(!UseDlssnrAutoHdr(true, false, input));
	// Capture retains the original color description after decoding. Converted
	// SDR/PQ/HLG is canonical too; the boundary uses its current SDR white.
	for (auto transfer : {HdrTransferFunction::SRGB, HdrTransferFunction::PQ, HdrTransferFunction::HLG}) {
		input.color.transfer = transfer;
		input.color.sdrWhiteNits = 240;
		assert(UseDlssnrAutoHdr(true, true, input));
	}
	input.color.sdrWhiteNits = std::numeric_limits<float>::quiet_NaN();
	assert(!UseDlssnrAutoHdr(true, true, input));
	input.color.sdrWhiteNits = 0;
	assert(!UseDlssnrAutoHdr(true, true, input));
	std::cout << "R1 migration: eight aliases, idempotence, preserved settings, defaults and invalid values "
				 "passed. Automatic HDR input/capability cases passed.\n";
}
