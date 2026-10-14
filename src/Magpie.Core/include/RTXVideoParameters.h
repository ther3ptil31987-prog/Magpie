#pragma once
#include <cmath>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include "NvidiaVfxParameters.h"

namespace Magpie {

// Saved strength is always 0..3. Vendor QualityLevel is a separate ABI.
inline constexpr int RTX_VIDEO_DEFAULT_STRENGTH = 1;
inline constexpr std::wstring_view RTX_VIDEO_IDS[2][4] = {
	{L"RTXVideo\\RTXVideo_Denoise_Low", L"RTXVideo\\RTXVideo_Denoise_Medium",
	 L"RTXVideo\\RTXVideo_Denoise_High", L"RTXVideo\\RTXVideo_Denoise_Ultra"},
	{L"RTXVideo\\RTXVideo_VSR_Low", L"RTXVideo\\RTXVideo_VSR_Medium", L"RTXVideo\\RTXVideo_VSR_High",
	 L"RTXVideo\\RTXVideo_VSR_Ultra"}};

template <class Char> constexpr std::basic_string_view<Char> RTXVideoCanonicalId(int family) noexcept {
	if constexpr (std::is_same_v<Char, wchar_t>)
		return family == 0 ? L"RTXVideo\\RTXVideo_Denoise" : L"RTXVideo\\RTXVideo_VSR";
	else
		return family == 0 ? "RTXVideo\\RTXVideo_Denoise" : "RTXVideo\\RTXVideo_VSR";
}

template <class Char> constexpr int RTXVideoFamily(std::basic_string_view<Char> id) noexcept {
	for (int family = 0; family < 2; ++family) {
		const auto canonical = RTXVideoCanonicalId<Char>(family);
		if (id == canonical)
			return family;
		for (auto legacy : RTX_VIDEO_IDS[family]) {
			if (legacy.size() != id.size())
				continue;
			bool equal = true;
			for (size_t i = 0; i < id.size(); ++i)
				equal &= legacy[i] == id[i];
			if (equal)
				return family;
		}
	}
	return -1;
}

inline int RTXVideoStrength(std::wstring_view id) noexcept {
	for (int family = 0; family < 2; ++family)
		for (int i = 0; i < 4; ++i)
			if (RTX_VIDEO_IDS[family][i] == id)
				return i;
	return -1;
}

inline int RTXVideoFamily(std::wstring_view id) noexcept {
	return RTXVideoFamily<wchar_t>(id);
}
inline int RTXVideoFamily(std::string_view id) noexcept {
	return RTXVideoFamily<char>(id);
}

inline int NormalizeRTXVideoStrength(float value) noexcept {
	return std::isfinite(value) && value >= 0 && value <= 3 && std::floor(value) == value
			   ? static_cast<int>(value)
			   : RTX_VIDEO_DEFAULT_STRENGTH;
}

inline uint32_t RTXVideoQualityLevel(int family, int strength) noexcept {
	return static_cast<uint32_t>((family == 0 ? 8 : 1) + strength);
}

// Exact legacy aliases only; never rewrite third-party effects by prefix.
template <class Effect> bool MigrateEffectParametersR1(Effect &effect) {
	bool changed = false;
	const int oldStrength = RTXVideoStrength(effect.name);
	const int family = RTXVideoFamily(std::wstring_view(effect.name));
	if (oldStrength >= 0) {
		effect.name = RTXVideoCanonicalId<wchar_t>(family);
		effect.parameters[L"strength"] = static_cast<float>(oldStrength);
		changed = true;
	} else if (family >= 0) {
		auto [it, inserted] = effect.parameters.try_emplace(L"strength", float(RTX_VIDEO_DEFAULT_STRENGTH));
		const float normalized = float(NormalizeRTXVideoStrength(it->second));
		changed = inserted || normalized != it->second;
		it->second = normalized;
	}
	if (family >= 0) {
		if (effect.name != L"RTXVideo\\RTXVideo_VSR") {
			effect.name = L"RTXVideo\\RTXVideo_VSR";
			changed = true;
		}
		// Only the former Denoise family changes category. Legacy quality indices
		// remain strength 0..3; they are not the vendor's continuous Strength.
		auto [category, inserted] = effect.parameters.try_emplace(L"category", family == 0 ? 3.f : 0.f);
		const int selected = family == 0 ? 3 : NvidiaVfxCategory(category->second);
		changed |= inserted || category->second != float(selected);
		category->second = float(selected);
		auto& tier = effect.parameters[L"strength"];
		const float normalized = float(NvidiaVfxTier(selected, tier));
		changed |= normalized != tier;
		tier = normalized;
	}
	if (effect.name == L"DLSSNR\\DLSSNR_AI_Filter") {
		changed |= effect.parameters.erase(L"experimentalHdrPath") != 0;
		changed |= effect.parameters.erase(L"experimentalHdrScale") != 0;
	}
	return changed;
}

} // namespace Magpie
