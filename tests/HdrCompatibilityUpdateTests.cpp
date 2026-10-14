#include <windows.h>
#include <winrt/base.h>
#include <array>
#include <ranges>
#include <string>
#include <vector>
#include "TextureExportPixels.h"
#include "EffectParameterValue.h"
#include "HdrParameterPolicy.h"
#include "EffectParameterRules.h"
#include <cassert>
#include <iostream>
#include <map>

using namespace Magpie;
int main() {
	std::vector<uint8_t> result;
	const std::array<uint16_t, 4> white{ 0x3c00, 0x3c00, 0x3c00, 0x3800 };
	const auto bytes = std::span(reinterpret_cast<const uint8_t*>(white.data()), sizeof(white));
	ColorDescription color;
	color.primaries = HdrColorPrimaries::Rec709;
	color.transfer = HdrTransferFunction::Linear;
	color.sdrWhiteNits = 80;
	assert(PreparePngPixels(1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, bytes, 8, &color, result));
	assert(result == std::vector<uint8_t>({188, 188, 188, 128}));
	assert(PreparePngPixels(1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, bytes, 8, nullptr, result));
	assert(result == std::vector<uint8_t>({255, 255, 255, 128}));
	const std::array<uint8_t, 8> bgra{ 17, 39, 81, 157, 0, 0, 0, 0 };
	assert(PreparePngPixels(1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, bgra, 8, nullptr, result));
	assert(result == std::vector<uint8_t>({81, 39, 17, 157}));
	assert(!PreparePngPixels(2, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, bytes, 8, &color, result));
	assert(!PreparePngPixels(1, 2, DXGI_FORMAT_R16G16B16A16_FLOAT, bytes, 8, &color, result));
	assert(!PreparePngPixels(1, 1, DXGI_FORMAT_UNKNOWN, bytes, 8, &color, result));
	const std::array<float, 4> bright{ 1500.f/80, 1500.f/80, 1500.f/80, .25f };
	assert(PreparePngPixels(1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT,
		std::span(reinterpret_cast<const uint8_t*>(bright.data()), sizeof(bright)), 16, &color, result));
	assert(result[0] > 188 && result[0] < 255 && result[3] == 64);
	assert(ExportHalfToFloat(0x8000) == 0 && ExportHalfToFloat(0x0001) > 0);
	assert(std::isnan(ExportHalfToFloat(0x7e00)) && std::isinf(ExportHalfToFloat(0x7c00)));
	auto get = [](std::string_view key, float fallback) { return key == "mode" ? 1.f : fallback; };
	assert(!IsEffectParameterVisible("Color\\HDR_to_SDR", "peakNits", get));
	assert(IsEffectParameterVisible("Custom", "peakNits", get));
	std::vector<EffectParameterDesc> parameters{ { .name = "nativeWhiteNits",
		.constant = EffectConstant<float>{203, 8, 4000, 1} } };
	ApplyHdrParameterPolicy("Color\\SDR_to_HDR", parameters);
	const auto& whiteParameter = parameters.front();
	for (const float value : {8.f, 79.999f, 203.f, 1000.f, 1500.f, 4000.f})
		assert(NormalizeEffectParameterValue(whiteParameter, value) == value);
	assert(NormalizeEffectParameterInput(whiteParameter, 8) == 80);
	assert(StepEffectParameter(whiteParameter, 203, true) == 205);
	assert(StepEffectParameter(whiteParameter, 203, false) == 200);
	assert(StepEffectParameter(whiteParameter, 200, true) == 203);
	for (const float value : {400.f, 1000.f, 1500.f}) {
		const float next = StepEffectParameter(whiteParameter, value, true);
		assert(StepEffectParameter(whiteParameter, next, false) == value);
		const float previous = StepEffectParameter(whiteParameter, value, false);
		assert(StepEffectParameter(whiteParameter, previous, true) == value);
	}
	assert(StepEffectParameter(whiteParameter, 1000, true) == 1050);
	assert(StepEffectParameter(whiteParameter, 400, true) == 410);
	assert(StepEffectParameter(whiteParameter, 1499, true, true) == 1500);
	int tick = 0, maximum = 0;
	for (const float value : {203.f, 1000.f, 1500.f, 4000.f}) {
		assert(GetEffectParameterTicks(whiteParameter, value, tick, maximum));
		assert(GetEffectParameterValueFromTick(whiteParameter, tick, maximum) == value);
	}
	EffectParameterDesc legacy{ .constant = EffectConstant<float>{203,80,4000,50} };
	assert(NormalizeEffectParameterValue(legacy, 203) == 180);
	assert(NormalizeEffectParameterValue(legacy, 1500) == 1480);
	const std::map<std::string, float> old{{"whiteNits", 80}, {"exposure", .1f}};
	assert(NativeHdrWhiteValue(old, std::string("nativeWhiteNits"), std::string("whiteNits"), std::string("exposure")) == 8);
	assert(!old.contains("nativeWhiteNits"));
	std::cout << "PASS: actual pixel formats, FP16 HDR display mapping, encoded SDR FP16, BGRA/padding, alpha, bounds and conditional peak visibility\n";
	std::cout << "PASS: storage/input precision, default 203, 1000/1500/4000, segmented bidirectional ticks, coarse absolute grid and legacy value preservation\n";
}
