#pragma once
#include "NativeEffectBackend.h"
#include "../RtxVideoBridge/RtxVideoBridge.h"

namespace Magpie {
class RTXVideoHdr final : public NativeEffectBackend {
public:
	~RTXVideoHdr();
	bool Initialize(DeviceResources& resources, ID3D11Texture2D* input,
		ID3D11Texture2D* output, const EffectOption& option) noexcept;
	bool Resize(DeviceResources&, ID3D11Texture2D*, ID3D11Texture2D*) noexcept override;
	bool Draw(const NativeEffectDrawContext& context) noexcept override;
	EffectParameterApplyMode GetParameterApplyMode(std::string_view name) const noexcept override {
		return name == "contrast" || name == "saturation" || name == "middleGray" || name == "peakNits"
			? EffectParameterApplyMode::Live : EffectParameterApplyMode::Restricted;
	}
	bool ApplyLiveParameters(const EffectOption&, std::span<const std::string>) noexcept override;
private:
	wil::unique_hmodule _module;
	void* _instance = nullptr;
	RtxHdrDrawFn _draw = nullptr;
	RtxHdrDestroyFn _destroy = nullptr;
	RtxHdrDestroyExFn _destroyEx = nullptr;
	ID3D11DeviceContext* _context = nullptr;
	MagpieRtxHdrSettings _settings;
};
}
