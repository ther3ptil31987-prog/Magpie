#pragma once
#include "FrameRefreshSettings.h"
#include "ProfileFrameSync.h"
#include <type_traits>

namespace Magpie {

// New fields win as a unit. Missing fields inside that unit receive factory
// defaults, so a partial/damaged new object cannot reapply an old global value.
inline FrameRefreshSettings ReadProfileFrameRefresh(
	const rapidjson::GenericObject<true, rapidjson::Value>& object,
	FrameSyncSettings legacySync = {}, float legacyIdle = 10.0f, bool migrateLegacy = true) noexcept {
	FrameRefreshSettings result;
	const auto member = object.FindMember("frameRefresh");
	if (member != object.MemberEnd()) {
		if (!member->value.IsObject()) return result;
		const auto fields = member->value.GetObj();
		auto enumeration = [&](const char* key, auto& target, uint32_t last) {
			const auto value = fields.FindMember(key);
			if (value != fields.MemberEnd() && value->value.IsUint() && value->value.GetUint() <= last)
				target = static_cast<std::remove_reference_t<decltype(target)>>(value->value.GetUint());
		};
		auto rate = [&](const char* key, float& target) {
			const auto value = fields.FindMember(key);
			if (value != fields.MemberEnd() && value->value.IsNumber())
				target = FrameRefreshSettings::ValidateRate(value->value.GetDouble(), target);
		};
		auto boolean = [&](const char* key, bool& target) {
			const auto value = fields.FindMember(key);
			if (value != fields.MemberEnd() && value->value.IsBool()) target = value->value.GetBool();
		};
		enumeration("contentMode", result.contentMode, 2);
		rate("contentRate", result.contentRate);
		enumeration("pacing", result.pacing, 2);
		enumeration("cursorMode", result.cursorMode, 2);
		enumeration("cursorSupplement", result.cursorSupplement, 1);
		rate("cursorRate", result.cursorRate);
		boolean("idleEnabled", result.idleEnabled);
		boolean("vrrEnabled", result.vrrEnabled);
		// Legacy foreground compatibility is derived from this single VRR intent.
		// Ignore its old field, including malformed values; vrrEnabled is authoritative.
		rate("idleRate", result.idleRate);
		rate("legacyContentLimit", result.legacyContentLimit);
		if (const auto value = fields.FindMember("legacySourceTarget"); value != fields.MemberEnd() && value->value.IsNumber()) {
			const double number = value->value.GetDouble();
			result.legacySourceTarget = number == 0 ? 0 : FrameRefreshSettings::ValidateRate(number, -1);
		}
		boolean("legacyLimiterOnly", result.legacyLimiterOnly);
		boolean("legacyResponsiveMinimum", result.legacyResponsiveMinimum);
		if (result.contentMode != ContentFrameRateMode::Custom) result.legacyLimiterOnly = false;
		return NormalizeContentFrameRate(result);
	}

	if (!migrateLegacy) return result;
	legacySync = ReadProfileFrameSync(object, legacySync);
	bool limiter = false;
	float limit = 60;
	if (const auto value = object.FindMember("frameRateLimiterEnabled"); value != object.MemberEnd() && value->value.IsBool())
		limiter = value->value.GetBool();
	if (const auto value = object.FindMember("maxFrameRate"); value != object.MemberEnd() && value->value.IsNumber()) {
		const double number = value->value.GetDouble();
		limit = number >= 10 ? FrameRefreshSettings::ValidateRate(number) : 60;
	}
	result.pacing = legacySync.mode;
	result.contentRate = legacySync.frameRate > 0 ? legacySync.frameRate : limit;
	result.contentMode = legacySync.enabled ? (legacySync.frameRate == 0 ?
		ContentFrameRateMode::Auto : ContentFrameRateMode::Custom) : ContentFrameRateMode::Source;
	if (!legacySync.enabled) result.legacySourceTarget = legacySync.frameRate;
	if (limiter) {
		if (legacySync.enabled) result.legacyContentLimit = limit;
		else {
			result.contentMode = ContentFrameRateMode::Custom;
			result.contentRate = limit;
			result.legacyLimiterOnly = true;
		}
	}
	// Before this schema the implicit cursor policy was responsive + minimum.
	bool original = false, minimum = true;
	if (const auto value = object.FindMember("cursorPreferOriginalFrames"); value != object.MemberEnd() && value->value.IsBool())
		original = value->value.GetBool();
	if (const auto value = object.FindMember("cursorMinimumRefreshEnabled"); value != object.MemberEnd() && value->value.IsBool())
		minimum = value->value.GetBool();
	if (const auto value = object.FindMember("cursorMinimumRefreshRate"); value != object.MemberEnd() && value->value.IsNumber())
		result.cursorRate = FrameRefreshSettings::ValidateRate(value->value.GetDouble());
	result.cursorMode = !original ? CursorRefreshMode::Responsive :
		minimum ? CursorRefreshMode::Supplement : CursorRefreshMode::OriginalOnly;
	result.legacyResponsiveMinimum = !original && minimum;
	result.idleEnabled = std::isfinite(legacyIdle) && legacyIdle > 0;
	result.idleRate = result.idleEnabled ? FrameRefreshSettings::ValidateRate(legacyIdle, 10) : 30;
	return NormalizeContentFrameRate(result);
}

template<class Writer>
void WriteProfileFrameRefresh(Writer& writer, const FrameRefreshSettings& settings) {
	const auto s = NormalizeContentFrameRate(settings.IsValid() ? settings : FrameRefreshSettings{});
	writer.Key("frameRefresh"); writer.StartObject();
	writer.Key("contentMode"); writer.Uint(uint32_t(s.contentMode));
	writer.Key("contentRate"); writer.Double(s.contentRate);
	writer.Key("pacing"); writer.Uint(uint32_t(s.pacing));
	writer.Key("cursorMode"); writer.Uint(uint32_t(s.cursorMode));
	writer.Key("cursorSupplement"); writer.Uint(uint32_t(s.cursorSupplement));
	writer.Key("cursorRate"); writer.Double(s.cursorRate);
	writer.Key("idleEnabled"); writer.Bool(s.idleEnabled);
	writer.Key("vrrEnabled"); writer.Bool(s.vrrEnabled);
	writer.Key("idleRate"); writer.Double(s.idleRate);
	writer.Key("legacyContentLimit"); writer.Double(s.legacyContentLimit);
	writer.Key("legacySourceTarget"); writer.Double(s.legacySourceTarget);
	writer.Key("legacyLimiterOnly"); writer.Bool(s.legacyLimiterOnly);
	writer.Key("legacyResponsiveMinimum"); writer.Bool(s.legacyResponsiveMinimum);
	writer.EndObject();
}

}
