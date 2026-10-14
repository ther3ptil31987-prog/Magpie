#include "../src/Magpie/ProfileFrameRefresh.h"
#include "../src/Magpie.Core/CursorRefreshPolicy.h"
#include "../src/Magpie.Core/FramePresentationTiming.h"
#include "../src/Magpie/ConfigRecovery.h"
#include <rapidjson/writer.h>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace Magpie;
using namespace std::chrono_literals;
static int checks = 0;
static void Check(bool value, const char* message) {
	++checks;
	if (!value) throw std::runtime_error(message);
}
static FrameRefreshSettings Read(std::string_view text, FrameSyncSettings fallback = {}, float idle = 10,
	bool migrate = true) {
	rapidjson::Document doc;
	doc.Parse(text.data(), text.size());
	Check(!doc.HasParseError(), "Test JSON invalid");
	return ReadProfileFrameRefresh(std::as_const(doc).GetObj(), fallback, idle, migrate);
}
static std::string Write(FrameRefreshSettings s) {
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	writer.StartObject(); WriteProfileFrameRefresh(writer, s); writer.EndObject();
	return buffer.GetString();
}
int main() {
	const FrameRefreshSettings defaults;
	Check(!defaults.vrrEnabled, "New profile unexpectedly enables VRR");
	Check(!Read(R"({"vrr":true})").vrrEnabled, "Dormant global VRR silently became active");
	Check(!Read(R"({"vrr":true,"frameRefresh":{}})").vrrEnabled, "Partial refresh schema imported global VRR");
	for (const char* invalid : {"0", "1", "null", "\"true\"", "{}"}) {
		Check(!Read(std::string("{\"frameRefresh\":{\"vrrEnabled\":") + invalid + "}}").vrrEnabled,
			"Malformed VRR opt-in accepted");
	}
	auto vrr = defaults; vrr.vrrEnabled = true;
	Check(Read(Write(vrr)) == vrr, "Profile VRR opt-in lost on roundtrip");
	auto stale = defaults, edited = defaults, merged = vrr;
	edited.contentRate = 45;
	Check(MergeFrameRefreshSettings(merged, stale, edited) && merged.vrrEnabled && merged.contentRate == 45,
		"Stale runtime panel cleared a newer profile VRR opt-in");
	for (bool standard : {false, true}) for (bool legacyCompatibility : {false, true}) {
		const auto json = std::string("{\"frameRefresh\":{\"vrrEnabled\":") + (standard ? "true" : "false") +
			",\"nvidiaForegroundCompatibilityEnabled\":" + (legacyCompatibility ? "true" : "false") + "}}";
		const auto migrated = Read(json);
		Check(migrated.vrrEnabled == standard, "Legacy second switch changed authoritative VRR intent");
		const auto saved = Write(migrated);
		Check(saved.find("nvidiaForegroundCompatibilityEnabled") == std::string::npos,
			"New serialization retained obsolete second switch");
		Check(Read(saved) == migrated, "Unified VRR migration did not roundtrip");
	}
	Check(!Read(R"({"frameRefresh":{"nvidiaForegroundCompatibilityEnabled":true}})").vrrEnabled,
		"Legacy compatibility-only preference enabled VRR");
	for (const char* invalid : {"0", "1", "null", "\"true\"", "{}"}) {
		Check(Read(std::string("{\"frameRefresh\":{\"vrrEnabled\":true,\"nvidiaForegroundCompatibilityEnabled\":") + invalid + "}}").vrrEnabled,
			"Malformed obsolete field disabled explicit VRR intent");
		Check(!Read(std::string("{\"frameRefresh\":{\"vrrEnabled\":") + invalid + ",\"nvidiaForegroundCompatibilityEnabled\":true}}").vrrEnabled,
			"Malformed VRR field imported legacy compatibility");
	}
	Check(defaults.IsValid() && defaults.contentMode == ContentFrameRateMode::Custom &&
		defaults.contentRate == 60 && defaults.pacing == FrameSyncMode::FrontEdge &&
		defaults.cursorMode == CursorRefreshMode::Supplement && defaults.cursorRate == 60 &&
		defaults.cursorSupplement == CursorSupplementMode::Custom && defaults.idleEnabled && defaults.idleRate == 30,
		"Screenshot defaults changed");
	Check(Read("{}", {}, 10, false) == defaults, "New missing schema didn't use defaults");
	Check(Read(R"({"frameRefresh":{}})", {}, 5) == defaults, "Partial new schema reimported global idle");
	Check(Read(R"({"frameRefresh":null,"frontEdgeSync":false})") == defaults, "Malformed object reimported old fields");
	for (uint32_t content = 0; content < 3; ++content) for (uint32_t pacing = 0; pacing < 3; ++pacing)
	for (uint32_t cursor = 0; cursor < 3; ++cursor) for (uint32_t supplement = 0; supplement < 2; ++supplement)
	for (bool idle : {false, true}) {
		auto s = defaults;
		s.contentMode = ContentFrameRateMode(content); s.pacing = FrameSyncMode(pacing);
		s.cursorMode = CursorRefreshMode(cursor); s.cursorSupplement = CursorSupplementMode(supplement);
		s.contentRate = 23.5f; s.cursorRate = 144; s.idleRate = 17; s.idleEnabled = idle;
		Check(Read(Write(s), {}, 7) == NormalizeContentFrameRate(s), "Mode/numeric roundtrip lost hidden value");
		Check(s.ContentSync().enabled == (content != 0), "Source mode has active fixed pacing");
		Check(s.ContentSync().frameRate == (content == 1 ? 0 : 23.5f), "Automatic target became fixed");
		Check(s.Cursor().preferOriginalFrames == (cursor != 0) &&
			s.Cursor().minimumRefreshEnabled == (cursor == 2), "Cursor mode projection wrong");
	}
	for (bool enabled : {false, true}) for (bool cap : {false, true}) for (float target : {0.0f, 30.0f, 120.0f})
	for (float limit : {20.0f, 60.0f, 144.0f}) for (auto mode : {FrameSyncMode::FrontEdge, FrameSyncMode::Async, FrameSyncMode::Reflex}) {
		const std::string json = std::string("{\"frameRateLimiterEnabled\":") + (cap ? "true" : "false") +
			",\"maxFrameRate\":" + std::to_string(limit) + "}";
		const auto s = Read(json, {enabled, target, mode}, 5);
		Check(s.idleRate == 5 && s.idleEnabled, "Old global minimum lost");
		Check(s.pacing == mode && s.ContentSync().enabled == (enabled && target > 0), "Legacy fixed pacing or removed Auto migration changed");
		Check((enabled && target == 0) || s.ContentSync().frameRate == target, "Disabled-sync filter follow target lost its old Auto/fixed semantics");
		Check(s.ContentLimit().has_value() == (cap && !(enabled && target == 0)), "Old independent limit lost");
		if (s.ContentLimit()) Check(*s.ContentLimit() == limit, "Old cap value lost");
		if (enabled && target > 0) for (double display : {60, 144, 240}) for (unsigned multiplier : {1u, 2u, 3u, 4u}) {
			const double previous = ResolvePresentationFrameRate(target, cap ? limit : 0, display, multiplier);
			const double migrated = ResolvePresentationFrameRate(s.ContentSync().frameRate,
				s.ContentLimit().value_or(0), display, multiplier);
			Check(previous == migrated, "Migration changed auto/cap/multiplier effective target");
		}
		Check(Read(Write(s), {false, 77, FrameSyncMode::Reflex}, 30) == s, "Migration repeated on reload");
	}
	for (bool original : {false, true}) for (bool minimum : {false, true}) {
		const auto s = Read(std::string("{\"cursorPreferOriginalFrames\":") + (original ? "true" : "false") +
			",\"cursorMinimumRefreshEnabled\":" + (minimum ? "true" : "false") + "}");
		Check(s.Cursor().preferOriginalFrames == original && s.Cursor().minimumRefreshEnabled == minimum,
			"Legacy four-way cursor behavior changed");
		Check(Read(Write(s)) == s, "Legacy cursor metadata lost");
	}
	Check(!Read("{}", {}, 0).idleEnabled, "Old zero idle became enabled");
	for (const char* invalid : {"0", "-1", "1001", "0.5", "null", "true", "\"bad\"", "1e100"}) {
		const auto s = Read(std::string("{\"frameRefresh\":{\"contentRate\":") + invalid +
			",\"cursorRate\":" + invalid + ",\"idleRate\":" + invalid + "}}");
		Check(s == defaults, "Invalid rate failed independent default repair");
	}
	Check(Read(R"({"frameRefresh":{"contentMode":99,"pacing":-1,"cursorMode":"2","cursorSupplement":3,"idleEnabled":0}})") == defaults,
		"Invalid enum/bool changed default");
	auto bad = defaults; bad.contentRate = std::numeric_limits<float>::quiet_NaN();
	Check(Read(Write(bad)) == defaults, "Nonfinite runtime value was serialized");
	// Independent profile vs panel edits merge; conflict is atomic.
	auto before = defaults, panel = before, current = before;
	panel.cursorRate = 144; current.contentRate = 30;
	Check(MergeFrameRefreshSettings(current, before, panel) && current.contentRate == 30 && current.cursorRate == 144,
		"Independent stale panel edit overwrote profile");
	panel.contentRate = 40; const auto unchanged = current;
	Check(!MergeFrameRefreshSettings(current, before, panel) && current == unchanged, "Conflicting save wasn't atomic");
	current = before; panel = before; panel.contentRate = 0;
	Check(!MergeFrameRefreshSettings(current, before, panel), "Invalid panel edit accepted");
	auto automatic = defaults; automatic.cursorSupplement = CursorSupplementMode::Auto;
	CursorRefreshPolicy cursor;
	cursor.Configure(automatic.Cursor(), 144);
	using Clock = CursorRefreshPolicy::Clock;
	const auto start = Clock::time_point{};
	const CursorVisualState oldCursor{1, 0, 0, true, false, false};
	auto moved = oldCursor; moved.x = 20;
	cursor.ObserveContent({1, 1, 1}, false); cursor.Prepare(oldCursor, start); cursor.Presented(true, start);
	Check(!cursor.NeedsRedraw(moved, start + 6ms) && cursor.NeedsRedraw(moved, start + 7ms), "Auto cursor isn't display rate");
	cursor.SetDisplayRate(60);
	Check(!cursor.NeedsRedraw(moved, start + 10ms) && cursor.NeedsRedraw(moved, start + 17ms), "Auto cursor failed monitor change");
	cursor.Prepare(moved, start + 17ms); cursor.Presented(false, start + 17ms);
	Check(cursor.NeedsRedraw(moved, start + 18ms), "Failed present postponed cursor");
	cursor.Prepare(moved, start + 18ms); cursor.Presented(true, start + 18ms);
	Check(!cursor.NeedsRedraw(moved, start + 1s), "Stationary cursor kept requesting presents");
	const auto repaired = ConfigRecovery::Prepare(R"({"scalingModes":[],"profiles":[{"frameRefresh":{"contentRate":0,"idleRate":30,"idleEnabled":false,"cursorMode":99}}]})", "", "Recovered");
	Check(repaired.kind == ConfigRecovery::Kind::Repaired, "New fields bypassed recovery");
	const auto fixed = ReadProfileFrameRefresh(std::as_const(repaired.document)["profiles"][0].GetObj());
	Check(fixed.contentRate == 60 && fixed.idleRate == 30 && !fixed.idleEnabled,
		"Recovery changed valid off/idle value");
	const auto malformed = ConfigRecovery::Prepare(R"({"scalingModes":[],"profiles":[{"frameRefresh":false,"frontEdgeSync":false}]})", "", "Recovered");
	Check(ReadProfileFrameRefresh(std::as_const(malformed.document)["profiles"][0].GetObj()) == defaults,
		"Recovery erased new-schema marker");
	// Simple mode projects runtime values without destroying advanced preferences.
	struct Options {
		FrameRefreshSettings frameRefresh;
		bool isVRREnabled = false, isFrontEdgeSyncEnabled = false;
		float frontEdgeSyncFrameRate = 0, minFrameRate = 0;
		FrameSyncMode frameSyncMode{};
		std::optional<float> maxFrameRate;
		CursorRefreshSettings cursorRefresh;
	};
	for (const auto contentMode : {ContentFrameRateMode::Source, ContentFrameRateMode::Custom})
	for (float contentRate : {15.f, 60.f, 90.f, 144.f, 360.f}) {
		auto saved = defaults;
		saved.contentMode = contentMode; saved.contentRate = contentRate;
		saved.pacing = FrameSyncMode::Reflex; saved.cursorMode = CursorRefreshMode::Responsive;
		saved.cursorRate = 237; saved.cursorSupplement = CursorSupplementMode::Auto;
		saved.vrrEnabled = true; saved.legacyContentLimit = 20; saved.legacyResponsiveMinimum = true;
		const auto original = saved;
		Options simple, advanced;
		ApplyFrameRefreshSettings(simple, saved, true);
		ApplyFrameRefreshSettings(advanced, saved, false);
		Check(saved == original && simple.frameRefresh == original, "Simple projection overwrote advanced preferences");
		Check(simple.isVRREnabled && simple.frameSyncMode == FrameSyncMode::FrontEdge && !simple.maxFrameRate,
			"Simple projection lost shared VRR or retained hidden pacing/legacy cap");
		Check(simple.cursorRefresh.preferOriginalFrames && simple.cursorRefresh.minimumRefreshEnabled &&
			!simple.cursorRefresh.automaticRefreshRate && simple.cursorRefresh.minimumRefreshRate ==
			(contentMode == ContentFrameRateMode::Custom ? contentRate : 60), "Simple cursor target did not follow content or source-60 rule");
		Check(simple.isFrontEdgeSyncEnabled == (contentMode == ContentFrameRateMode::Custom), "Source acquired an implicit cap");
		Check(advanced.isVRREnabled && advanced.frameSyncMode == FrameSyncMode::Reflex && advanced.maxFrameRate == 20,
			"Returning to advanced lost settings");
		auto simpleEdit = saved; simpleEdit.contentRate = 75; simpleEdit.idleRate = 15;
		Check(MergeFrameRefreshSettings(saved, original, simpleEdit) && saved.cursorRate == 237 && saved.vrrEnabled,
			"Simple shared edit lost advanced fields");
		Check(Read(Write(saved)) == saved, "Advanced fields did not survive simple save/reload");
	}
	const auto migratedAuto = Read(R"({"frameRefresh":{"contentMode":1,"contentRate":90,"pacing":2,"cursorRate":144,"vrrEnabled":true,"legacyContentLimit":30}})");
	Check(migratedAuto.contentMode == ContentFrameRateMode::Source && !migratedAuto.ContentLimit() &&
		migratedAuto.pacing == FrameSyncMode::Reflex && migratedAuto.cursorRate == 144 && migratedAuto.vrrEnabled,
		"Auto migration lost unrelated advanced values");
	Check(Read(Write(migratedAuto)) == migratedAuto, "Auto returned after reload");
	std::cout << "Unified refresh: " << checks << " checks passed\n";
}
