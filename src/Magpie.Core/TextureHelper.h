#pragma once
#include "EffectDesc.h"
#include "HdrFrame.h"

namespace Magpie {

struct TextureSaveError {
	bool fileWriteFailed = false;
	HRESULT code = S_OK;
};

class TextureHelper {
public:
	// 支持 dds、bmp、jpg、png 和 tiff
	static winrt::com_ptr<ID3D11Texture2D> LoadTexture(const wchar_t* fileName, ID3D11Device* d3dDevice) noexcept;

	// 支持 dds 和 png
	static bool SaveTexture(
		const wchar_t* fileName,
		uint32_t width,
		uint32_t height,
		DXGI_FORMAT format,
		std::span<uint8_t> pixelData,
		uint32_t rowPitch,
		TextureSaveError* error = nullptr,
		const ColorDescription* hdrColor = nullptr
	) noexcept;
	// Encode privately and commit with a no-replace move. Existing files survive
	// another process creating the destination after directory enumeration.
	static bool SaveTextureExclusive(const wchar_t* fileName, uint32_t width,
		uint32_t height, DXGI_FORMAT format, std::span<uint8_t> pixelData,
		uint32_t rowPitch, TextureSaveError* error = nullptr,
		const ColorDescription* hdrColor = nullptr) noexcept;
};

}
