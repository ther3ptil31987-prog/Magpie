#pragma once
#include "../HdrFrame.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

namespace Magpie {

inline float ExportHalfToFloat(uint16_t bits) noexcept {
	const unsigned exponent = (bits >> 10) & 31;
	const unsigned mantissa = bits & 1023;
	const float magnitude = exponent == 31
		? (mantissa ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity())
		: exponent == 0 ? std::ldexp(float(mantissa), -24)
		: std::ldexp(float(1024 + mantissa), int(exponent) - 25);
	return bits & 32768 ? -magnitude : magnitude;
}

// PNG is an SDR sRGB image. The color contract, rather than FP16 storage,
// decides whether to apply the display tone map. DDS keeps the original bytes.
inline bool PreparePngPixels(uint32_t width, uint32_t height, DXGI_FORMAT format,
	std::span<const uint8_t> input, uint32_t rowPitch, const ColorDescription* hdrColor,
	std::vector<uint8_t>& output) {
	uint32_t pixelSize;
	switch (format) {
	case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UNORM: pixelSize = 4; break;
	case DXGI_FORMAT_R16G16B16A16_FLOAT: pixelSize = 8; break;
	case DXGI_FORMAT_R32G32B32A32_FLOAT: pixelSize = 16; break;
	default: return false;
	}
	if (!width || !height || uint64_t(width) * pixelSize > rowPitch ||
		uint64_t(rowPitch) * height > input.size() ||
		uint64_t(width) * height * 4 > std::numeric_limits<uint32_t>::max()) return false;
	output.resize(size_t(width) * height * 4);
	auto finite = [](float value) { return std::isfinite(value) ? value : 0.f; };
	auto encode = [](float value) {
		value = std::clamp(value, 0.f, 1.f);
		return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.f / 2.4f) - 0.055f;
	};
	for (uint32_t y = 0; y < height; ++y) for (uint32_t x = 0; x < width; ++x) {
		const auto* source = input.data() + size_t(y) * rowPitch + size_t(x) * pixelSize;
		std::array<float, 4> value;
		if (format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
			std::array<uint16_t, 4> half;
			std::memcpy(half.data(), source, 8);
			for (size_t c = 0; c < 4; ++c) value[c] = ExportHalfToFloat(half[c]);
		} else if (format == DXGI_FORMAT_R32G32B32A32_FLOAT) {
			std::memcpy(value.data(), source, 16);
		} else if (format == DXGI_FORMAT_R10G10B10A2_UNORM) {
			uint32_t packed;
			std::memcpy(&packed, source, 4);
			value = { float(packed & 1023) / 1023, float((packed >> 10) & 1023) / 1023,
				float((packed >> 20) & 1023) / 1023, float(packed >> 30) / 3 };
		} else {
			const bool bgra = format == DXGI_FORMAT_B8G8R8A8_UNORM;
			value = { float(source[bgra ? 2 : 0]) / 255, float(source[1]) / 255,
				float(source[bgra ? 0 : 2]) / 255, float(source[3]) / 255 };
		}
		for (auto& channel : value) channel = finite(channel);
		if (hdrColor) {
			if (hdrColor->transfer == HdrTransferFunction::PQ) {
				for (size_t c = 0; c < 3; ++c) {
					const float p = std::pow(std::clamp(value[c], 0.f, 1.f), 32.f / 2523.f);
					value[c] = std::pow(std::max(p - 3424.f / 4096.f, 0.f) /
						std::max(2413.f / 128.f - 2392.f / 128.f * p, 1e-6f), 16384.f / 2610.f) * 125.f;
				}
			} else if (hdrColor->transfer != HdrTransferFunction::Linear) return false;
			if (hdrColor->primaries == HdrColorPrimaries::Rec2020) {
				const auto rgb = value;
				value[0] = 1.660491f * rgb[0] - 0.5876411f * rgb[1] - 0.0728499f * rgb[2];
				value[1] = -0.1245505f * rgb[0] + 1.1328999f * rgb[1] - 0.0083494f * rgb[2];
				value[2] = -0.0181508f * rgb[0] - 0.1005789f * rgb[1] + 1.1187297f * rgb[2];
			} else if (hdrColor->primaries != HdrColorPrimaries::Rec709) return false;
			const float white = std::isfinite(hdrColor->sdrWhiteNits) && hdrColor->sdrWhiteNits > 0
				? hdrColor->sdrWhiteNits / 80 : 203.f / 80;
			for (size_t c = 0; c < 3; ++c) value[c] = std::max(value[c], 0.f) / white;
			const float luminance = .2126f * value[0] + .7152f * value[1] + .0722f * value[2];
			for (size_t c = 0; c < 3; ++c) value[c] = encode(value[c] / (1 + luminance));
		}
		auto* dest = output.data() + (size_t(y) * width + x) * 4;
		for (size_t c = 0; c < 4; ++c) dest[c] = uint8_t(std::lround(std::clamp(finite(value[c]), 0.f, 1.f) * 255));
	}
	return true;
}
}
