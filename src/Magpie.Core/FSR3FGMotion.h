#pragma once
#include <cstdint>
#include <string_view>
#include <utility>
#include <d3d11.h>
#include <winrt/base.h>

namespace Magpie {

// Pixel-space current-to-previous vectors are resized into the content extent
// at texture origin. generationRect supplies the offset into the backbuffer.
// Point sampling avoids averaging opposite motion across disocclusion edges.
inline constexpr std::string_view FSR3FG_MOTION_SHADER = R"(
Texture2D<float2> inputMotion : register(t0);
RWTexture2D<float2> outputMotion : register(u0);
cbuffer Constants : register(b0) { uint2 sourceSize; uint2 contentSize; };
[numthreads(8,8,1)] void Main(uint3 id : SV_DispatchThreadID) {
    if (any(id.xy >= contentSize)) return;
    uint2 p = min(uint2((float2(id.xy) + 0.5) * sourceSize / contentSize), sourceSize - 1);
    float2 mv = inputMotion.Load(int3(p, 0));
    outputMotion[id.xy] = all(isfinite(mv)) ? mv * float2(contentSize) / sourceSize : 0;
})";

class FSR3FGMotionSource {
public:
	// S_FALSE means ordinary missing/stale guidance; an invalid resource for a
	// matching frame is an internal contract failure, not a reset-only frame.
	HRESULT Prepare(ID3D11Device* device, ID3D11Texture2D* motion,
		uint64_t frameId, uint64_t sourceFrameId) noexcept {
		if (!motion || !frameId || frameId != sourceFrameId) return S_FALSE;
		motion->GetDesc(&_desc);
		winrt::com_ptr<ID3D11Device> owner;
		motion->GetDevice(owner.put());
		// Compare COM identities, not addresses of different device interfaces
		// (the presenter normally holds ID3D11Device5).
		winrt::com_ptr<IUnknown> ownerIdentity, expectedIdentity;
		_sameDevice = false;
		HRESULT hr = owner->QueryInterface(IID_PPV_ARGS(ownerIdentity.put()));
		if (SUCCEEDED(hr)) hr = device->QueryInterface(IID_PPV_ARGS(expectedIdentity.put()));
		if (FAILED(hr)) return hr;
		_sameDevice = ownerIdentity.get() == expectedIdentity.get();
		if (_desc.Format != DXGI_FORMAT_R16G16_FLOAT || !_desc.Width || !_desc.Height ||
			_desc.MipLevels != 1 || _desc.ArraySize != 1 ||
			_desc.SampleDesc.Count != 1 || _desc.SampleDesc.Quality != 0 ||
			!(_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) || !_sameDevice) {
			return E_INVALIDARG;
		}
		if (_texture.get() != motion || !_srv) {
			winrt::com_ptr<ID3D11ShaderResourceView> view;
			hr = device->CreateShaderResourceView(motion, nullptr, view.put());
			if (FAILED(hr)) return hr;
			_srv = std::move(view);
			_texture.copy_from(motion);
		}
		return S_OK;
	}

	const D3D11_TEXTURE2D_DESC& Desc() const noexcept { return _desc; }
	ID3D11ShaderResourceView* View() const noexcept { return _srv.get(); }
	bool SameDevice() const noexcept { return _sameDevice; }

private:
	D3D11_TEXTURE2D_DESC _desc{};
	bool _sameDevice = false;
	winrt::com_ptr<ID3D11Texture2D> _texture;
	winrt::com_ptr<ID3D11ShaderResourceView> _srv;
};

// Shared by the presenter and native resource regression; callers dispatch
// only after Prepare returned S_OK and after validating the content rectangle.
inline void RemapFSR3FGMotion(ID3D11DeviceContext* context,
	ID3D11ComputeShader* shader, ID3D11Buffer* constants,
	const FSR3FGMotionSource& source, ID3D11UnorderedAccessView* output,
	uint32_t contentWidth, uint32_t contentHeight) noexcept {
	const uint32_t dimensions[]{ source.Desc().Width, source.Desc().Height, contentWidth, contentHeight };
	context->UpdateSubresource(constants, 0, nullptr, dimensions, 0, 0);
	ID3D11ShaderResourceView* srv = source.View();
	context->CSSetShader(shader, nullptr, 0);
	context->CSSetShaderResources(0, 1, &srv);
	context->CSSetUnorderedAccessViews(0, 1, &output, nullptr);
	context->CSSetConstantBuffers(0, 1, &constants);
	context->Dispatch((contentWidth + 7) / 8, (contentHeight + 7) / 8, 1);
	srv = nullptr; output = nullptr;
	context->CSSetShaderResources(0, 1, &srv);
	context->CSSetUnorderedAccessViews(0, 1, &output, nullptr);
}

}
