#pragma once
#include "RtssSettings.h"
#include <rapidjson/document.h>
#include <type_traits>

namespace Magpie {

inline RtssSettings ReadProfileRtss(const rapidjson::GenericObject<true, rapidjson::Value>& object) noexcept {
	RtssSettings result;
	const auto member = object.FindMember("rtss");
	if (member == object.MemberEnd() || !member->value.IsObject()) return result;
	const auto fields = member->value.GetObj();
	bool valid = true;
	if (const auto value = fields.FindMember("simpleContentSync"); value != fields.MemberEnd() && value->value.IsBool())
		result.simpleContentSync = value->value.GetBool();
	auto enumeration = [&](const char* key, auto& target, uint32_t last) {
		const auto value = fields.FindMember(key);
		if (value == fields.MemberEnd()) return;
		if (!value->value.IsUint() || value->value.GetUint() > last) { valid = false; return; }
		target = static_cast<std::remove_reference_t<decltype(target)>>(value->value.GetUint());
	};
	enumeration("scope", result.scope, 2);
	enumeration("rateMode", result.rateMode, 1);
	enumeration("exitPolicy", result.exitPolicy, 1);
	if (const auto value = fields.FindMember("customRate"); value != fields.MemberEnd()) {
		if (value->value.IsUint() && value->value.GetUint() >= RtssSettings::MinimumRate &&
			value->value.GetUint() <= RtssSettings::MaximumRate) result.customRate = value->value.GetUint();
		else if (result.rateMode == RtssRateMode::Custom) valid = false;
	}
	// Do not interpret damaged/forward-version preferences as permission to write externally.
	if (!valid) result.scope = RtssScope::None;
	return result;
}

template<class Writer>
void WriteProfileRtss(Writer& writer, const RtssSettings& settings) {
	auto s = settings.IsValid() ? settings : RtssSettings{};
	if (s.customRate < RtssSettings::MinimumRate || s.customRate > RtssSettings::MaximumRate) s.customRate = 60;
	writer.Key("rtss"); writer.StartObject();
	writer.Key("simpleContentSync"); writer.Bool(s.simpleContentSync);
	writer.Key("scope"); writer.Uint(uint32_t(s.scope));
	writer.Key("rateMode"); writer.Uint(uint32_t(s.rateMode));
	writer.Key("customRate"); writer.Uint(s.customRate);
	writer.Key("exitPolicy"); writer.Uint(uint32_t(s.exitPolicy));
	writer.EndObject();
}

}
