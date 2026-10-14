#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace Magpie {

inline constexpr std::string_view NVIDIA_VFX_UPSCALE_ID = "NvidiaVFX\\Upscale";
inline constexpr std::string_view NVIDIA_VFX_VFG_ID = "NvidiaVFX\\VFG";
// Retain the deployed identities and strength key for import/export compatibility.
inline constexpr std::string_view NVIDIA_VFX_VSR_ID = "RTXVideo\\RTXVideo_VSR";
inline constexpr std::string_view NVIDIA_VFX_TRUE_HDR_ID = "RTXVideo\\RTXVideo_HDR";

enum class NvidiaVfxVsrCategory : uint8_t { Standard, HighBitrate, Streaming, Denoise, Deblur };
inline constexpr int NVIDIA_VFX_DEFAULT_TIER = 1;

inline int NvidiaVfxCategory(float value) noexcept {
	return std::isfinite(value) && value >= 0 && value <= 4 && std::floor(value) == value
		? static_cast<int>(value) : 0;
}
inline bool NvidiaVfxSameSize(int category) noexcept { return category == 3 || category == 4; }
inline bool NvidiaVfxTierValid(int category, float value) noexcept {
	return category >= 0 && category <= 4 && std::isfinite(value) && value >= 0 && value <= 3 && std::floor(value) == value &&
		(category != 2 || value == 1 || value == 3);
}
inline int NvidiaVfxTier(int category, float value) noexcept {
	return NvidiaVfxTierValid(category, value) ? static_cast<int>(value) : NVIDIA_VFX_DEFAULT_TIER;
}
inline uint32_t NvidiaVfxQualityLevel(int category, int tier) noexcept {
	constexpr uint32_t bases[] = { 1, 16, 20, 8, 12 };
	return bases[category >= 0 && category <= 4 ? category : 0] +
		static_cast<uint32_t>(NvidiaVfxTier(category, static_cast<float>(tier)));
}
inline bool NvidiaVfxQualityValid(uint32_t value) noexcept {
	return (value >= 1 && value <= 4) || (value >= 8 && value <= 19) || value == 21 || value == 23;
}

struct NvidiaVfxRatio { uint32_t numerator, denominator; };
inline constexpr std::array<NvidiaVfxRatio, 5> NVIDIA_VFX_UPSCALE_RATIOS{{ {4,3}, {3,2}, {2,1}, {3,1}, {4,1} }};
struct NvidiaVfxExtent { uint32_t width = 0, height = 0; };
enum class NvidiaVfxAdaptation { None, Downsample, Upsample };
struct NvidiaVfxUpscalePlan {
	bool valid = false;
	uint32_t ratioIndex = 0;
	NvidiaVfxExtent native{}, target{};
	NvidiaVfxAdaptation adaptation = NvidiaVfxAdaptation::None;
};
// mode 0 = automatic; 1..5 select the exact fractions above. Target is the
// already-resolved proportional content rectangle, before crop/letterboxing.
inline NvidiaVfxUpscalePlan PlanNvidiaVfxUpscale(NvidiaVfxExtent input,
	NvidiaVfxExtent target, int mode, uint32_t textureLimit = 16384) noexcept {
	NvidiaVfxUpscalePlan result;
	if (!input.width || !input.height || !target.width || !target.height || mode < 0 || mode > 5 ||
		target.width > textureLimit || target.height > textureLimit) return result;
	for (uint32_t index = 0; index < NVIDIA_VFX_UPSCALE_RATIOS.size(); ++index) {
		if (mode && index != static_cast<uint32_t>(mode - 1)) continue;
		const auto ratio = NVIDIA_VFX_UPSCALE_RATIOS[index];
		if (input.width % ratio.denominator || input.height % ratio.denominator) continue;
		const uint64_t width = uint64_t(input.width) / ratio.denominator * ratio.numerator;
		const uint64_t height = uint64_t(input.height) / ratio.denominator * ratio.numerator;
		if (width > textureLimit || height > textureLimit) continue;
		result = { true, index, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) }, target };
		if (mode || (width >= target.width && height >= target.height)) break;
	}
	// Do not silently select a lower tier when the required 4x exceeds device limits.
	if (!mode && result.valid && (result.native.width < target.width || result.native.height < target.height) &&
		result.ratioIndex != 4) return {};
	if (result.valid && (result.native.width != target.width || result.native.height != target.height))
		result.adaptation = result.native.width >= target.width && result.native.height >= target.height
			? NvidiaVfxAdaptation::Downsample : NvidiaVfxAdaptation::Upsample;
	return result;
}

struct NvidiaVfxFrameGenerationSettings {
	uint32_t multiplier = 2;
	uint32_t model = 1;
	bool automaticShotDetection = true;
};
inline bool NvidiaVfxVfgModelValid(float value) noexcept {
	return std::isfinite(value) && (value == 0.f || value == 1.f);
}
// Only the retired, formerly valid High value migrates. Unknown/fractional
// values remain invalid and are handled by the normal parameter policy.
template<class Effect> bool MigrateNvidiaVfxVfgModel(Effect& effect) {
	using Key = typename std::remove_reference_t<decltype(effect.parameters)>::key_type;
	using Char = typename Key::value_type;
	if constexpr (std::is_same_v<Char, wchar_t>) {
		if (effect.name != L"NvidiaVFX\\VFG") return false;
	} else {
		if (effect.name != NVIDIA_VFX_VFG_ID) return false;
	}
	const Key key{ Char('m'), Char('o'), Char('d'), Char('e'), Char('l') };
	const auto it = effect.parameters.find(key);
	if (it == effect.parameters.end() || it->second != 2.f) return false;
	it->second = 1.f;
	return true;
}
inline bool NvidiaVfxFrameGenerationSettingsValid(const NvidiaVfxFrameGenerationSettings& value) noexcept {
	return value.multiplier >= 2 && value.multiplier <= 8 && NvidiaVfxVfgModelValid(float(value.model));
}

} // namespace Magpie
