#pragma once

#include "HdrColorTransform.h"

namespace Magpie {

class BackendDescriptorStore;
class DeviceResources;

class HdrSurfaceAdapter {
public:
	bool MapSdrWhiteToHdr(ID3D11Texture2D* input, ID3D11Texture2D* output,
		const HdrTransformParameters& parameters) const noexcept;
	bool ToneMapHdrForSdrDisplay(ID3D11Texture2D* input, ID3D11Texture2D* output,
		const HdrTransformParameters& parameters) const noexcept;
	bool Initialize(DeviceResources& deviceResources, BackendDescriptorStore& descriptorStore) noexcept;

	bool ConvertHdrToSdr(
		ID3D11Texture2D* input,
		ID3D11Texture2D* output,
		const HdrTransformParameters& parameters,
		HdrTransferFunction outputTransfer = HdrTransferFunction::SRGB
	) const noexcept;

	bool ConvertSdrToHdr(
		ID3D11Texture2D* input,
		ID3D11Texture2D* output,
		const HdrTransformParameters& parameters,
		HdrTransferFunction inputTransfer = HdrTransferFunction::SRGB
	) const noexcept;

	bool ConvertHdrToBounded(
		ID3D11Texture2D* input,
		ID3D11Texture2D* output,
		const HdrTransformParameters& parameters,
		float normalizationScale
	) const noexcept;

	bool ConvertBoundedToHdr(
		ID3D11Texture2D* input,
		ID3D11Texture2D* output,
		const HdrTransformParameters& parameters,
		float normalizationScale
	) const noexcept;

	// Canonical HDR uses absolute nits internally; the FP16 presentation
	// surface uses scRGB where 1.0 represents the 80-nit reference white.
	bool ConvertHdrToScRgb(
		ID3D11Texture2D* input,
		ID3D11Texture2D* output,
		const HdrTransformParameters& parameters
	) const noexcept;

	// Encode canonical linear scRGB into an HDR10/PQ R10 surface for terminal
	// backends such as XeSS-FG. The canonical FP16 surface remains unchanged.
	bool ConvertCanonicalToHdr10(
		ID3D11Texture2D* input,
		ID3D11Texture2D* output,
		const HdrTransformParameters& parameters
	) const noexcept;

	bool ConvertHdr10ToCanonical(
		ID3D11Texture2D* input,
		ID3D11Texture2D* output,
		const HdrTransformParameters& parameters
	) const noexcept;
	// Raw R10G10B10A2 words stored as four RGBA8 bytes by CUDA interop.
	bool ConvertPackedHdr10ToCanonical(ID3D11Texture2D* input, ID3D11Texture2D* output,
		const HdrTransformParameters& parameters) const noexcept;

private:
	bool _Convert(
		ID3D11Texture2D* input,
		ID3D11Texture2D* output,
		const HdrTransformParameters& parameters,
		HdrTransferFunction transfer,
		bool hdrToSdr,
		uint32_t mode = 0,
		float normalizationScale = 1.0f
	) const noexcept;

	DeviceResources* _deviceResources = nullptr;
	BackendDescriptorStore* _descriptorStore = nullptr;
	winrt::com_ptr<ID3D11ComputeShader> _shader;
	winrt::com_ptr<ID3D11Buffer> _constants;
};

}
