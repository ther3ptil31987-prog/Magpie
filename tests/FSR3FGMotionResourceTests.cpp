#include "../src/Magpie.Core/FrontendMotionTexture.h"
#include "../src/Magpie.Core/FSR3FGMotion.h"
#include "../src/Magpie.Core/PresenterFailure.h"
#include <d3d11_4.h>
#include <d3d11sdklayers.h>
#include <d3dcompiler.h>
#include <DirectXPackedVector.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

using namespace Magpie;
using winrt::com_ptr;
using namespace DirectX::PackedVector;
using Pixel = std::array<uint16_t, 2>;
static int checks = 0;
static void Check(bool condition, const char* message) {
	++checks;
	if (!condition) throw std::runtime_error(message);
}
static void Hr(HRESULT hr) {
	if (FAILED(hr)) {
		std::fprintf(stderr, "Unexpected HRESULT: 0x%08X\n", static_cast<unsigned>(hr));
		throw std::runtime_error("D3D call failed");
	}
}
static D3D11_TEXTURE2D_DESC Desc(UINT width, UINT height, UINT bind) {
	D3D11_TEXTURE2D_DESC d{};
	d.Width = width; d.Height = height;
	d.MipLevels = d.ArraySize = d.SampleDesc.Count = 1;
	d.Format = DXGI_FORMAT_R16G16_FLOAT;
	d.Usage = D3D11_USAGE_DEFAULT;
	d.BindFlags = bind;
	return d;
}
static com_ptr<ID3D11Texture2D> Texture(ID3D11Device* device,
	const D3D11_TEXTURE2D_DESC& d, const std::vector<Pixel>* pixels = nullptr) {
	D3D11_SUBRESOURCE_DATA initial{};
	if (pixels) {
		initial.pSysMem = pixels->data();
		initial.SysMemPitch = d.Width * UINT(sizeof(Pixel));
	}
	com_ptr<ID3D11Texture2D> texture;
	Hr(device->CreateTexture2D(&d, pixels ? &initial : nullptr, texture.put()));
	return texture;
}
static std::vector<Pixel> Read(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* source) {
	D3D11_TEXTURE2D_DESC d{}; source->GetDesc(&d);
	d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0;
	d.CPUAccessFlags = D3D11_CPU_ACCESS_READ; d.MiscFlags = 0;
	auto staging = Texture(device, d);
	context->CopyResource(staging.get(), source);
	D3D11_MAPPED_SUBRESOURCE mapped{};
	Hr(context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped));
	std::vector<Pixel> pixels(size_t(d.Width) * d.Height);
	for (UINT y = 0; y < d.Height; ++y) {
		std::memcpy(pixels.data() + size_t(y) * d.Width,
			static_cast<const char*>(mapped.pData) + size_t(y) * mapped.RowPitch, size_t(d.Width) * sizeof(Pixel));
	}
	context->Unmap(staging.get(), 0);
	return pixels;
}

int main() try {
	com_ptr<ID3D11Device> device;
	com_ptr<ID3D11DeviceContext> context;
	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_DEBUG,
		nullptr, 0, D3D11_SDK_VERSION, device.put(), nullptr, context.put());
	if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING) {
		device = nullptr; context = nullptr;
		Hr(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
			nullptr, 0, D3D11_SDK_VERSION, device.put(), nullptr, context.put()));
		std::puts("D3D11 debug layer unavailable; debug-message validation skipped.");
	} else Hr(hr);
	auto info = device.try_as<ID3D11InfoQueue>();

	// Reproduce the exact old contract, then repair using Renderer’s helper.
	auto shared = Texture(device.get(), Desc(17, 9, D3D11_BIND_SHADER_RESOURCE));
	com_ptr<ID3D11Texture2D> stable;
	Hr(EnsureFrontendMotionTexture(device.get(), shared.get(), false, stable));
	com_ptr<ID3D11ShaderResourceView> rejected;
	Check(device->CreateShaderResourceView(stable.get(), nullptr, rejected.put()) == E_INVALIDARG,
		"copy-only texture must reproduce the original SRV failure");
	if (info) info->ClearStoredMessages(); // Intentional negative control above.
	FSR3FGMotionSource motion;
	Check(motion.Prepare(device.get(), nullptr, 1, 1) == S_FALSE, "first frame without motion resets");
	Check(motion.Prepare(device.get(), stable.get(), 2, 2) == E_INVALIDARG, "old bind=0 is a contract error");
	Check(!motion.View(), "failed input must not publish a cached SRV");
	auto oldStable = stable;
	Hr(EnsureFrontendMotionTexture(device.get(), shared.get(), true, stable));
	Check(stable.get() != oldStable.get(), "same-size texture must rebuild when bind requirement changes");
	D3D11_TEXTURE2D_DESC stableDesc{}; stable->GetDesc(&stableDesc);
	Check(stableDesc.BindFlags == D3D11_BIND_SHADER_RESOURCE && !stableDesc.MiscFlags &&
		!stableDesc.CPUAccessFlags && stableDesc.Usage == D3D11_USAGE_DEFAULT, "stable copy desc");
	context->CopyResource(stable.get(), shared.get());
	Check(motion.Prepare(device.get(), stable.get(), 2, 2) == S_OK, "second valid frame accepts the repaired copy");
	const auto* cached = motion.View();
	Check(motion.Prepare(device.get(), stable.get(), 3, 3) == S_OK && motion.View() == cached, "reuse cached SRV");
	Check(motion.Prepare(device.get(), stable.get(), 3, 4) == S_FALSE, "stale motion is not a resource failure");
	Check(motion.Prepare(device.get(), stable.get(), 0, 4) == S_FALSE, "zero identity is unavailable motion");
	Check(motion.Prepare(device.get(), oldStable.get(), 4, 4) == E_INVALIDARG && motion.View() == cached,
		"failed replacement retains the previously successful view");
	Check(motion.Prepare(device.get(), stable.get(), 5, 5) == S_OK, "valid guidance can recover after absence");
	auto presenterDevice = device.as<ID3D11Device5>();
	Check(motion.Prepare(presenterDevice.get(), stable.get(), 5, 5) == S_OK,
		"accept the presenter's device interface through COM identity");

	com_ptr<ID3D11Device> other;
	Hr(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
		nullptr, 0, D3D11_SDK_VERSION, other.put(), nullptr, nullptr));
	Check(motion.Prepare(other.get(), stable.get(), 5, 5) == E_INVALIDARG, "reject foreign device even on cached input");
	for (int mode = 0; mode < 4; ++mode) {
		auto d = Desc(8, 8, D3D11_BIND_SHADER_RESOURCE);
		if (mode == 0) d.Format = DXGI_FORMAT_R32G32_FLOAT;
		if (mode == 1) d.MipLevels = 2;
		if (mode == 2) d.ArraySize = 2;
		if (mode == 3) d.SampleDesc.Count = 2;
		auto invalid = Texture(device.get(), d);
		Check(motion.Prepare(device.get(), invalid.get(), 6, 6) == E_INVALIDARG, "reject unsupported motion layout");
	}

	com_ptr<ID3DBlob> bytecode, errors;
	hr = D3DCompile(FSR3FG_MOTION_SHADER.data(), FSR3FG_MOTION_SHADER.size(), "FSR3FGMotion",
		nullptr, nullptr, "Main", "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS,
		0, bytecode.put(), errors.put());
	if (errors) std::fprintf(stderr, "%s\n", static_cast<const char*>(errors->GetBufferPointer()));
	Hr(hr);
	com_ptr<ID3D11ComputeShader> shader;
	Hr(device->CreateComputeShader(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, shader.put()));
	D3D11_BUFFER_DESC constantsDesc{};
	constantsDesc.ByteWidth = 16; constantsDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	com_ptr<ID3D11Buffer> constants;
	Hr(device->CreateBuffer(&constantsDesc, nullptr, constants.put()));

	// Identity, upscale, downscale and non-multiple-of-eight dispatch extents.
	for (const auto& size : std::array<std::array<UINT, 4>, 5>{ {{17, 9, 17, 9}, {3, 2, 9, 6}, {9, 6, 3, 2}, {1, 1, 7, 5}, {7, 5, 11, 8}} }) {
		const auto [sw, sh, cw, ch] = size;
		std::vector<Pixel> input(size_t(sw) * sh);
		for (UINT y = 0; y < sh; ++y) for (UINT x = 0; x < sw; ++x)
			input[size_t(y) * sw + x] = { XMConvertFloatToHalf(float(x) - 2.0f), XMConvertFloatToHalf(1.0f - float(y)) };
		if (input.size() > 1) { input[0][0] = 0x7e00; input[1][1] = 0x7c00; } // NaN/Inf -> zero vector.
		shared = Texture(device.get(), Desc(sw, sh, D3D11_BIND_SHADER_RESOURCE), &input);
		Hr(EnsureFrontendMotionTexture(device.get(), shared.get(), true, stable));
		context->CopyResource(stable.get(), shared.get());
		Check(motion.Prepare(device.get(), stable.get(), 7, 7) == S_OK, "accept resized production copy");
		const auto sameStable = stable;
		Hr(EnsureFrontendMotionTexture(device.get(), shared.get(), true, stable));
		Check(stable.get() == sameStable.get(), "unchanged stable desc reuses texture");
		auto output = Texture(device.get(), Desc(cw + 3, ch + 2, D3D11_BIND_UNORDERED_ACCESS));
		com_ptr<ID3D11UnorderedAccessView> uav;
		Hr(device->CreateUnorderedAccessView(output.get(), nullptr, uav.put()));
		constexpr float sentinel[4]{ 13, 13, 13, 13 };
		context->ClearUnorderedAccessViewFloat(uav.get(), sentinel);
		RemapFSR3FGMotion(context.get(), shader.get(), constants.get(), motion, uav.get(), cw, ch);
		const auto actual = Read(device.get(), context.get(), output.get());
		for (UINT y = 0; y < ch + 2; ++y) for (UINT x = 0; x < cw + 3; ++x) {
			float expected[2]{ 13, 13 };
			if (x < cw && y < ch) {
				const UINT sx = std::min(UINT((float(x) + 0.5f) * float(sw) / float(cw)), sw - 1);
				const UINT sy = std::min(UINT((float(y) + 0.5f) * float(sh) / float(ch)), sh - 1);
				const auto pixel = input[size_t(sy) * sw + sx];
				const float mx = XMConvertHalfToFloat(pixel[0]), my = XMConvertHalfToFloat(pixel[1]);
				const bool finite = std::isfinite(mx) && std::isfinite(my);
				expected[0] = finite ? mx * float(cw) / float(sw) : 0;
				expected[1] = finite ? my * float(ch) / float(sh) : 0;
			}
			for (size_t channel = 0; channel < 2; ++channel) {
				const float value = XMConvertHalfToFloat(actual[size_t(y) * (cw + 3) + x][channel]);
				Check(std::isfinite(value) && std::abs(value - expected[channel]) < 0.02f,
					"remap must preserve vector sign, scale, finite handling and content bounds");
			}
		}
	}

	// XeSS retains copy-only semantics, including after a shader-readable copy.
	Hr(EnsureFrontendMotionTexture(device.get(), shared.get(), false, stable));
	stable->GetDesc(&stableDesc);
	Check(stableDesc.BindFlags == 0, "XeSS stable copy remains copy-only");
	context->CopyResource(stable.get(), shared.get());
	Check(Read(device.get(), context.get(), stable.get()) == Read(device.get(), context.get(), shared.get()),
		"copy-only round trip preserves source bits");
	PresenterFailure failure;
	failure.Record("FSR3FG source motion SRV", uint32_t(E_INVALIDARG));
	failure.Record("FSR3FG drain SDK presents: AMD result=5", uint32_t(E_FAIL));
	Check(failure.code == uint32_t(E_INVALIDARG) && failure.context == "FSR3FG source motion SRV",
		"teardown cannot overwrite first failure");
	if (info) {
		for (UINT64 i = 0; i < info->GetNumStoredMessagesAllowedByRetrievalFilter(); ++i) {
			SIZE_T length = 0; Hr(info->GetMessage(i, nullptr, &length));
			std::vector<char> buffer(length);
			auto* message = reinterpret_cast<D3D11_MESSAGE*>(buffer.data());
			Hr(info->GetMessage(i, message, &length));
			if (message->Severity <= D3D11_MESSAGE_SEVERITY_ERROR) std::fprintf(stderr, "%s\n", message->pDescription);
			Check(message->Severity > D3D11_MESSAGE_SEVERITY_ERROR, "unexpected D3D11 debug-layer error");
		}
		std::puts("D3D11 debug layer checked.");
	}
	std::printf("FSR3FG motion resource regression: %d checks passed (WARP; no AMD SDK/display acceptance).\n", checks);
	return 0;
} catch (const std::exception& e) {
	std::fprintf(stderr, "%s\n", e.what());
	return 1;
}
