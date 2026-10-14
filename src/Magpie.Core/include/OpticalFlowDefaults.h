#pragma once
#include <cstdint>
#include <string_view>

namespace Magpie {

inline bool IsOpticalFlowConsumer(std::wstring_view name) noexcept {
	return name == L"DLSS\\DLSS_SR" || name == L"DLSSNR\\DLSSNR_AI_Filter" ||
		name == L"DLSSFG\\DLSS_FrameGeneration" || name == L"FSR2\\FSR2_SR" ||
		name == L"FSR3\\FSR3_SR" || name == L"FSR4\\FSR4_SR" || name == L"XeSS\\XeSS_SR" ||
		name == L"XeSSFG\\XeSS_FrameGeneration_x2_ZeroMV" ||
		name == L"XeSSFG\\XeSS_MultiFrameGeneration_ZeroMV" ||
		name == L"Diagnostics\\FrameGuidance_Motion" || name == L"Diagnostics\\FrameGuidance_Confidence";
}

template<class Modes>
bool ApplyOpticalFlowDefaultsMigration(Modes& modes, uint32_t& version) {
	if (version >= 1) return false;
	for (auto& mode : modes) {
		if constexpr (requires { mode.hdrMigrationInvalid; mode.hdrMigrationBackup; }) {
			if (mode.hdrMigrationInvalid || !mode.hdrMigrationBackup.empty()) continue;
		}
		for (auto& effect : mode.effects)
			if (IsOpticalFlowConsumer(effect.name)) effect.parameters[L"opticalFlowMethod"] = 0.0f;
	}
	version = 1;
	return true;
}

}
