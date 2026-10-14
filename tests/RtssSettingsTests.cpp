#include "../src/Magpie/ProfileRtss.h"
#include "../src/Magpie.Core/include/FramePacingOptions.h"
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

using namespace Magpie;
static int checks = 0;
static void Check(bool value, const char* message) {
	++checks;
	if (!value) throw std::runtime_error(message);
}
static RtssSettings Read(std::string_view text) {
	rapidjson::Document document;
	document.Parse(text.data(), text.size());
	Check(!document.HasParseError() && document.IsObject(), "Test JSON invalid");
	return ReadProfileRtss(std::as_const(document).GetObj());
}
static std::string Write(RtssSettings value) {
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	writer.StartObject(); WriteProfileRtss(writer, value); writer.EndObject();
	return buffer.GetString();
}
int main() {
	const RtssSettings defaults;
	Check(defaults.IsValid() && defaults.scope == RtssScope::None && defaults.customRate == 60 &&
		defaults.exitPolicy == RtssExitPolicy::RestoreOriginal, "Defaults allow an unexpected external write");
	for (const auto json : { "{}", R"({"rtss":null})", R"({"rtss":[]})", R"({"rtss":{}})" })
		Check(Read(json) == defaults, "Missing or malformed object did not use defaults");
	for (uint32_t scope = 0; scope < 3; ++scope) for (uint32_t mode = 0; mode < 2; ++mode)
	for (uint32_t policy = 0; policy < 2; ++policy) for (uint32_t rate : { 15u, 60u, 360u }) {
		RtssSettings s{ RtssScope(scope), RtssRateMode(mode), rate, RtssExitPolicy(policy) };
		Check(Read(Write(s)) == s, "Preferences lost on round trip");
		for (bool enabled : {false, true}) {
			s.simpleContentSync = enabled;
			const auto saved = Read(Write(s));
			Check(saved == s, "Simple intent and advanced preferences round trip independently");
			const auto simple = EffectiveRtssSettings(saved, true);
			Check(simple.scope == (enabled ? RtssScope::Target : RtssScope::None) &&
				simple.rateMode == RtssRateMode::ContentSync && simple.exitPolicy == RtssExitPolicy::RestoreOriginal,
				"Simple RTSS projection incorrect");
			Check(EffectiveRtssSettings(saved, false) == s, "Advanced settings overwritten by projection");
			Check(!simple.ResolveRate(false, 60), "Source unexpectedly synchronizes to guessed target");
		}
		if (scope == 0) Check(!s.ResolveRate(true, 60), "Unmanaged settings can resolve an external write");
		else if (mode == 0) Check(s.ResolveRate(false, 0) == rate, "Manual cap requires a fixed content target");
		else Check(!s.ResolveRate(false, 60), "Follow-source synchronization guessed a target");
	}
	for (const auto field : { "scope", "rateMode", "exitPolicy" }) {
		for (const auto bad : { "999", "-1", "null", "1.5", "\"1\"" }) {
			// Avoid duplicate scope fields, which RapidJSON would resolve by the first key.
			const std::string clean = std::string("{\"rtss\":{\"") + field + "\":" + bad +
				(field == std::string_view("scope") ? "}}" : ",\"scope\":2}}");
			Check(Read(clean).scope == RtssScope::None, "Damaged enum grants external control");
		}
	}
	for (const auto bad : { "0", "14", "361", "59.5", "null", "\"60\"" })
		Check(Read(std::string("{\"rtss\":{\"scope\":2,\"customRate\":") + bad + "}}").scope == RtssScope::None,
			"Damaged custom rate grants external control");
	for (const auto bad : { "0", "14", "361", "59.5", "null", "\"60\"" }) {
		const auto restored = Read(std::string("{\"rtss\":{\"scope\":2,\"rateMode\":1,\"exitPolicy\":0,\"customRate\":") + bad + "}}");
		Check(restored.scope == RtssScope::Target && restored.rateMode == RtssRateMode::ContentSync && restored.exitPolicy == RtssExitPolicy::KeepChanges,
			"Unused manual damage preserves scope, sync and exit choice");
		Check(Read(Write(restored)) == restored && restored.ResolveRate(true, 72) == 72, "Unused manual damage survives next save and sync");
	}
	for (uint32_t bad : {0u, 14u, 361u, UINT32_MAX}) {
		RtssSettings s{RtssScope::Global, RtssRateMode::ContentSync, bad, RtssExitPolicy::RestoreOriginal};
		Check(s.IsValid() && s.ResolveRate(true, 72) == 72, "Sync validity ignores unused manual value");
		const auto saved = Read(Write(s));
		Check(saved.scope == s.scope && saved.rateMode == s.rateMode && saved.exitPolicy == s.exitPolicy && saved.customRate == 60,
			"Writer normalizes only damaged unused preference");
		s.rateMode = RtssRateMode::Custom;
		Check(!s.IsValid() && !s.ResolveRate(true, 72), "Active invalid manual value cannot grant external write");
	}
	Check(RtssSettings::EditedRate(15) == 15 && RtssSettings::EditedRate(360) == 360, "Editor endpoints");
	Check(RtssSettings::EditedRate(59.5) == 60, "Editor integer conversion");
	for (double bad : { 0.0, 14.0, 361.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() })
		Check(!RtssSettings::EditedRate(bad), "Invalid editor value accepted");
	auto sync = defaults; sync.scope = RtssScope::Target; sync.rateMode = RtssRateMode::ContentSync;
	for (unsigned multiplier : { 1u, 2u, 3u, 4u }) {
		const auto base = ResolvePresentationFrameRate(0, 0, 144, multiplier);
		Check(sync.ResolveRate(true, base) == uint32_t(std::round(144.0 / multiplier)), "Sync used FG output FPS");
	}
	Check(sync.ResolveRate(true, ResolvePresentationFrameRate(0, 0, 119.88, 2)) == 60, "Fractional automatic base not rounded");
	Check(sync.ResolveRate(true, ResolvePresentationFrameRate(90, 45, 144, 2)) == 45, "Existing base cap ignored");
	for (double bad : { 0.0, -1.0, 1001.0, std::numeric_limits<double>::quiet_NaN() })
		Check(!sync.ResolveRate(true, bad), "Invalid runtime base accepted");
	std::cout << "RTSS settings: " << checks << " checks passed\n";
}
