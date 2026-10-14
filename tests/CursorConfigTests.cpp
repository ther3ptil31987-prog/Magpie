#include "../src/Magpie/ProfileFrameRefresh.h"
#include "../src/Magpie/ProfileRtss.h"
#include <string>
#include "../src/Magpie/JsonHelper.h"
#include <rapidjson/prettywriter.h>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#define DEFINE_FLAG_ACCESSOR(name, mask, member) \
	bool name() const noexcept { return (member & mask) != 0; } \
	void name(bool value) noexcept { if (value) member |= mask; else member &= ~mask; }
// Existing Profile runtimeIdentity initializes uint8_t with literal 0. Keep
// that unrelated production declaration verbatim in the extracted fixture.
#pragma warning(push)
#pragma warning(disable: 4244)
#include "CursorProfileProduction.inc"
#pragma warning(pop)
#undef DEFINE_FLAG_ACCESSOR

namespace Magpie {
template<class T> constexpr T FLOAT_EPSILON = std::numeric_limits<T>::epsilon();
// Encoding and OS version are unrelated to the cursor fields under test.
struct StrHelper {
	static std::string UTF16ToUTF8(std::wstring_view value) {
		std::string result;
		for (wchar_t c : value) result.push_back(static_cast<char>(c));
		return result;
	}
	static std::wstring UTF8ToUTF16(std::string_view value) { return {value.begin(), value.end()}; }
	static void Trim(std::wstring_view& value) {
		while (!value.empty() && value.front() == L' ') value.remove_prefix(1);
		while (!value.empty() && value.back() == L' ') value.remove_suffix(1);
	}
};
struct Win32Helper {
	struct Version { bool Is20H1OrNewer() const { return true; } };
	static Version GetOSVersion() { return {}; }
};
struct AppSettings {
	std::vector<int> _scalingModes{0};
	bool _isDeveloperMode = true, _isDebugMode = true, _isBenchmarkMode = true;
	bool _isEffectCacheDisabled = true, _isFontCacheDisabled = true;
	bool _isSaveEffectSources = true, _isWarningsAreErrors = true;
	bool _isStatisticsForDynamicDetectionEnabled = true, _isFP16Disabled = true;
	DuplicateFrameDetectionMode _duplicateFrameDetectionMode = DuplicateFrameDetectionMode::Never;
	unsigned saves = 0;
	void SaveAsync() { ++saves; }
	void IsDeveloperMode(bool value) noexcept;
	bool _LoadProfile(const rapidjson::GenericObject<true, rapidjson::Value>&,
		Profile&, bool, bool, const FrameSyncSettings& = {}, float = 10.0f, bool = true) const noexcept;
};
#include "CursorConfigProduction.inc"
}

using namespace Magpie;
static int checks = 0;
static void Check(bool value, const char* message) {
	++checks;
	if (!value) throw std::runtime_error(message);
}
static std::string Serialize(const Profile& profile) {
	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	WriteProfile(writer, profile);
	return buffer.GetString();
}
static void Load(std::string json, Profile& profile, bool isDefault = true) {
	rapidjson::Document document;
	document.Parse(json.c_str());
	Check(!document.HasParseError(), "JSON parsing failed");
	const rapidjson::Document& constant = document;
	Check(AppSettings{}._LoadProfile(constant.GetObj(), profile, isDefault, false), "Production profile load failed");
}
int main() {
	const Profile defaults;
	Check(defaults.frameRefresh.Cursor().preferOriginalFrames && defaults.frameRefresh.Cursor().minimumRefreshEnabled &&
		defaults.frameRefresh.Cursor().minimumRefreshRate == 60, "Screenshot defaults changed");
	Profile tuned;
	tuned.frameRefresh.contentMode = ContentFrameRateMode::Auto;
	tuned.frameRefresh.contentRate = 37; tuned.frameRefresh.pacing = FrameSyncMode::Reflex;
	tuned.frameRefresh.legacyContentLimit = 20; tuned.frameRefresh.cursorSupplement = CursorSupplementMode::Auto;
	tuned.frameRefresh.idleEnabled = false; tuned.frameRefresh.idleRate = 17;
	Profile tunedCopy; tunedCopy.Copy(tuned);
	Check(tunedCopy.frameRefresh == tuned.frameRefresh, "Copy lost unified settings");
	Profile tunedLoad; Load(Serialize(tunedCopy), tunedLoad);
	Check(tunedLoad.frameRefresh == NormalizeContentFrameRate(tuned.frameRefresh), "Production full model migration/roundtrip failed");
	tunedCopy.frameRefresh.contentRate = 38;
	Check(tuned.frameRefresh.contentRate == 37, "Copied content edit leaked to source");
	for (bool original : {false, true}) for (bool minimum : {false, true}) for (float rate : {1.0f, 60.0f, 144.0f, 1000.0f}) {
		Profile profile;
		profile.frameRefresh.cursorMode = !original ? CursorRefreshMode::Responsive : minimum ? CursorRefreshMode::Supplement : CursorRefreshMode::OriginalOnly;
		profile.frameRefresh.legacyResponsiveMinimum = !original && minimum;
		profile.frameRefresh.cursorRate = rate;
		profile.name = L"app"; profile.pathRule = L"C:/game.exe"; profile.classNameRule = L"GameWindow";
		Profile copy;
		copy.Copy(profile);
		Check(copy.frameRefresh.Cursor().preferOriginalFrames == original && copy.frameRefresh.Cursor().minimumRefreshEnabled == minimum &&
			copy.frameRefresh.Cursor().minimumRefreshRate == rate, "Production profile Copy lost cursor fields");
		Check(copy.runtimeIdentity != profile.runtimeIdentity, "Copy shared session identity");
		Profile loaded;
		Load(Serialize(profile), loaded, false);
		Check(loaded.frameRefresh.Cursor().preferOriginalFrames == original && loaded.frameRefresh.Cursor().minimumRefreshEnabled == minimum &&
			loaded.frameRefresh.Cursor().minimumRefreshRate == rate, "Application profile roundtrip failed");
		Profile loadedDefault;
		profile.name.clear();
		Load(Serialize(profile), loadedDefault);
		Check(loadedDefault.frameRefresh.Cursor().preferOriginalFrames == original && loadedDefault.frameRefresh.Cursor().minimumRefreshEnabled == minimum &&
			loadedDefault.frameRefresh.Cursor().minimumRefreshRate == rate, "Default profile roundtrip failed");
		copy.frameRefresh.cursorRate = 99;
		Check(profile.frameRefresh.Cursor().minimumRefreshRate == rate, "Copied profile edits leaked to source");
	}
	Profile legacy;
	legacy.frameRefresh.cursorMode = CursorRefreshMode::OriginalOnly; legacy.frameRefresh.cursorRate = 240;
	Load("{}", legacy);
	Check(!legacy.frameRefresh.Cursor().preferOriginalFrames && legacy.frameRefresh.Cursor().minimumRefreshEnabled &&
		legacy.frameRefresh.Cursor().minimumRefreshRate == 60, "Missing fields retained stale data");
	for (const char* value : {"0", "-3", "1001", "1e100", "null", "true", "\"bad\""}) {
		Profile bad;
		Load(std::string("{\"cursorMinimumRefreshRate\":") + value + "}", bad);
		Check(bad.frameRefresh.Cursor().minimumRefreshRate == 60, "Malformed/range FPS did not default");
	}
	Load("{\"cursorPreferOriginalFrames\":\"true\",\"cursorMinimumRefreshEnabled\":0}", legacy);
	Check(!legacy.frameRefresh.Cursor().preferOriginalFrames && legacy.frameRefresh.Cursor().minimumRefreshEnabled, "Wrong bool types broke defaults");
	for (float value : {0.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
		Profile invalid;
		invalid.frameRefresh.cursorRate = value;
		Profile repaired;
		Load(Serialize(invalid), repaired);
		Check(repaired.frameRefresh.Cursor().minimumRefreshRate == 60, "Writer serialized invalid FPS");
	}
	for (auto mode : {DuplicateFrameDetectionMode::Always, DuplicateFrameDetectionMode::Dynamic, DuplicateFrameDetectionMode::Never}) {
		AppSettings settings;
		settings._duplicateFrameDetectionMode = mode;
		settings.IsDeveloperMode(false);
		Check(settings._duplicateFrameDetectionMode == mode, "Developer OFF reset permanent duplicate setting");
		Check(!settings._isDebugMode && !settings._isStatisticsForDynamicDetectionEnabled &&
			!settings._isFP16Disabled && settings.saves == 1, "Other developer reset/save behavior changed");
		settings.IsDeveloperMode(true);
		Check(settings._duplicateFrameDetectionMode == mode && settings.saves == 2, "Re-enable changed duplicate setting");
	}
	std::cout << "Production profile copy/config/developer mode: " << checks << " checks passed\n";
}
