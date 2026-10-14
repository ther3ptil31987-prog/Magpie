#pragma once
#include <utility>
#include <d3d11.h>
#include <winrt/base.h>

namespace Magpie {

// Copy shared motion while the slot's keyed mutex is held. Only FSR samples
// this stable copy; XeSS copies it into its own texture without creating an SRV.
inline HRESULT EnsureFrontendMotionTexture(ID3D11Device* device,
	ID3D11Texture2D* source, bool shaderReadable,
	winrt::com_ptr<ID3D11Texture2D>& stable) noexcept {
	D3D11_TEXTURE2D_DESC required{};
	source->GetDesc(&required);
	required.Usage = D3D11_USAGE_DEFAULT;
	required.BindFlags = shaderReadable ? D3D11_BIND_SHADER_RESOURCE : 0;
	required.CPUAccessFlags = 0;
	required.MiscFlags = 0;
	if (stable) {
		D3D11_TEXTURE2D_DESC current{};
		stable->GetDesc(&current);
		if (current.Width == required.Width && current.Height == required.Height &&
			current.Format == required.Format && current.MipLevels == required.MipLevels &&
			current.ArraySize == required.ArraySize &&
			current.SampleDesc.Count == required.SampleDesc.Count &&
			current.SampleDesc.Quality == required.SampleDesc.Quality &&
			current.Usage == required.Usage && current.BindFlags == required.BindFlags &&
			current.CPUAccessFlags == 0 && current.MiscFlags == 0) return S_OK;
	}
	// Publish only a fully created replacement. The previous resource may still
	// be retained by a presenter until its next successful view update.
	winrt::com_ptr<ID3D11Texture2D> replacement;
	const HRESULT hr = device->CreateTexture2D(&required, nullptr, replacement.put());
	if (SUCCEEDED(hr)) stable = std::move(replacement);
	return hr;
}

}
