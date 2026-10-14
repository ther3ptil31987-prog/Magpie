#pragma once
#include "ScalingOptions.h"

namespace Magpie {

class DeviceResources;
class NgxD3D12Core;
class NativeEffectBackend;
struct EffectOption;

struct NativeEffectBackendResult {
	bool recognized = false;
	std::unique_ptr<NativeEffectBackend> backend;
	ScalingError error = ScalingError::NoError;
	std::string diagnostic;
	uint32_t systemError = 0;
	DlssNrFailure dlssNrFailure;
	std::string dlssNrEnvironment;
};

NativeEffectBackendResult CreateNativeEffectBackend(
	std::string_view effectName,
	const EffectOption& option,
	DeviceResources& resources,
	NgxD3D12Core& ngxCore,
	ID3D11Texture2D* input,
	ID3D11Texture2D* output,
	std::optional<bool> hdrDomain = std::nullopt
) noexcept;

}
