#include "ScreenshotContent.h"
#include "ScreenshotNaming.h"
#include "ConfigRecovery.h"
#include <cassert>
#include <iostream>

using namespace Magpie;

int main() {
	static_assert(static_cast<uint32_t>(ScreenshotContent::InputAndOutput) == 0);
	static_assert(static_cast<uint32_t>(ScreenshotContent::OutputOnly) == 1);
	static_assert(static_cast<uint32_t>(ScreenshotContent::InputOnly) == 2);
	for (uint32_t i = 0; i < 3; ++i) assert(static_cast<uint32_t>(NormalizeScreenshotContent(i)) == i);
	for (uint32_t i : { 3u, 99u, UINT32_MAX }) assert(NormalizeScreenshotContent(i) == ScreenshotContent::OutputOnly);
	const ScreenshotFrameIdentity frame{ 12, 0, 3 };
	assert(frame.Matches({ 12, 0, 3 }, false)); // GDI/DXGI use sequence zero.
	assert(!frame.Matches({ 12, 0, 3 }, true));
	assert(!frame.Matches({ 13, 0, 3 }, false));
	assert(!frame.Matches({ 12, 1, 3 }, false));
	assert(!frame.Matches({ 12, 0, 4 }, false));
	assert(!ScreenshotFrameIdentity{}.Matches({}, false));
	for (auto name : { L"Magpie_001.png", L"Magpie_001.dds", L"Magpie_001_input.png",
		L"Magpie_001_output.png", L"magpie_001_OUTPUT.PNG" }) assert(ParseScreenshotNumber(name) == 1);
	assert(ParseScreenshotNumber(L"Magpie_4294967295_input.png") == UINT32_MAX);
	for (auto name : { L"Magpie_0.png", L"Magpie_4294967296.png", L"Magpie_001_input_output.png",
		L"Magpie_001_other.png", L"Magpie_.png", L"Magpie_+1.png", L"Magpie_-1.png",
		L"Magpie_1.jpg", L"Other_001.png", L"Magpie_001.png.bak", L"Magpie_001input.png" })
		assert(ParseScreenshotNumber(name) == 0);
	assert(NextScreenshotNumber({}) == 1);
	assert(NextScreenshotNumber({ 1, 1, 2, 5 }) == 6);
	assert(NextScreenshotNumber({ UINT32_MAX }) == 1);
	assert(NextScreenshotNumber({ 1, 1, 2, 2, UINT32_MAX }) == 3);
	assert(NextScreenshotNumber({ 1, 1, 3, UINT32_MAX }) == 2);
	assert(NextScreenshotNumber({ UINT32_MAX, 4, 1, 3, 2, 2 }) == 5);
	const std::string prefix = R"({"experimentalDlssnrSettingsVersion":2,"experimentalDlssSrSettingsVersion":1,"experimentalDepthRemovalVersion":1,"scalingModes":[],"overlay":{"screenshotsDir":"keep","screenshotContent":)";
	for (auto value : { "0", "1", "2" }) {
		auto plan = ConfigRecovery::Prepare(prefix + value + "}}", {}, "Recovered");
		assert(plan.kind == ConfigRecovery::Kind::None);
		assert(plan.document["overlay"]["screenshotContent"].GetUint() == std::stoul(value));
		assert(ConfigRecovery::Prepare(ConfigRecovery::Serialize(plan.document), {}, "Recovered").kind == ConfigRecovery::Kind::None);
	}
	for (auto value : { "-1", "3", "1.5", "4294967296", "null", "true", "\"input\"", "{}", "[]" }) {
		auto plan = ConfigRecovery::Prepare(prefix + value + "}}", {}, "Recovered");
		assert(plan.kind == ConfigRecovery::Kind::Repaired);
		assert(plan.fields == std::vector<std::string>{"/overlay/screenshotContent"});
		assert(!plan.document["overlay"].HasMember("screenshotContent"));
		assert(std::string_view(plan.document["overlay"]["screenshotsDir"].GetString()) == "keep");
	}
	std::cout << "PASS: screenshot modes, frame identity, legacy/pair naming, overflow and isolated configuration recovery\n";
}
