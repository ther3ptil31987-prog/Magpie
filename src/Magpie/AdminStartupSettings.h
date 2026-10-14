#pragma once
#include <rapidjson/document.h>
#include <cstdint>

namespace Magpie {
constexpr uint32_t ADMIN_STARTUP_DEFAULTS_VERSION = 1;

// The marker distinguishes the old default false from subsequent user choices.
template<class JsonRoot>
inline bool ReadAdminStartupSettings(const JsonRoot& root, bool& enabled, uint32_t& version) {
	version = 0;
	if (const auto marker = root.FindMember("adminStartupDefaultsVersion"); marker != root.MemberEnd() && marker->value.IsUint())
		version = marker->value.GetUint();
	enabled = true;
	if (version < ADMIN_STARTUP_DEFAULTS_VERSION) {
		version = ADMIN_STARTUP_DEFAULTS_VERSION;
		return true;
	}
	if (const auto value = root.FindMember("alwaysRunAsAdmin"); value != root.MemberEnd() && value->value.IsBool())
		enabled = value->value.GetBool();
	else if (const auto legacy = root.FindMember("alwaysRunAsElevated"); legacy != root.MemberEnd() && legacy->value.IsBool())
		enabled = legacy->value.GetBool();
	return false;
}
}
