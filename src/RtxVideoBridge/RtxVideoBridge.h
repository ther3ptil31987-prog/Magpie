#pragma once
#include <d3d11.h>
#include <cstdint>

// The official D3D11 TrueHDR sample accepts both SDR channel layouts. Capture
// starts as BGRA8; preceding image effects usually produce RGBA8.
inline bool MagpieRtxHdrEndpointsSupported(const D3D11_TEXTURE2D_DESC& input,
	const D3D11_TEXTURE2D_DESC& output) noexcept {
	return (input.Format == DXGI_FORMAT_R8G8B8A8_UNORM || input.Format == DXGI_FORMAT_B8G8R8A8_UNORM) &&
		output.Format == DXGI_FORMAT_R16G16B16A16_FLOAT && input.Width == output.Width && input.Height == output.Height &&
		(output.BindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0;
}

// Only plain C-compatible data crosses the private SDK DLL boundary.
// All operations run on the renderer's backend thread.
struct MagpieRtxHdrSettings {
	uint32_t contrast = 100;
	uint32_t saturation = 100;
	uint32_t middleGray = 50;
	uint32_t maxLuminance = 1000;
};
static_assert(sizeof(MagpieRtxHdrSettings) == 16);
using RtxHdrCreateFn = HRESULT(WINAPI*)(ID3D11Device*, const wchar_t*, void**, uint32_t*);
using RtxHdrDrawFn = HRESULT(WINAPI*)(void*, ID3D11Texture2D*, ID3D11Texture2D*, const MagpieRtxHdrSettings*, uint32_t*);
using RtxHdrDestroyFn = HRESULT(WINAPI*)(void*);
enum class RtxHdrCleanupStage : uint32_t { None, ReleaseFeature, DestroyParameters, Shutdown, Faulted, Exception };
inline const char* RtxHdrCleanupStageName(RtxHdrCleanupStage stage) noexcept {
	switch (stage) {
	case RtxHdrCleanupStage::None: return "None";
	case RtxHdrCleanupStage::ReleaseFeature: return "ReleaseFeature";
	case RtxHdrCleanupStage::DestroyParameters: return "DestroyParameters";
	case RtxHdrCleanupStage::Shutdown: return "Shutdown1";
	case RtxHdrCleanupStage::Faulted: return "AlreadyFaulted";
	case RtxHdrCleanupStage::Exception: return "SEHException";
	default: return "Unknown";
	}
}
// Extended diagnostic ABI; keep the original export for older callers.
using RtxHdrDestroyExFn = HRESULT(WINAPI*)(void*, RtxHdrCleanupStage*, uint32_t*);
