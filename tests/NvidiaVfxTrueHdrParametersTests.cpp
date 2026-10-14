#include <cassert>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include "NvidiaVfxTrueHdrParameters.h"

struct Effect { std::map<std::string, float> parameters; };
int main() {
	using namespace Magpie;
	Effect option;
	const auto original = *ReadNvidiaVfxTrueHdrTunables(option);
	assert((original.values == std::array<uint32_t, 5>{100, 100, 50, 1000, 0}));
	constexpr const char* keys[]{"contrast", "saturation", "middleGray", "peakNits", "debanding"};
	constexpr float lo[]{0, 0, 10, 400, 0}, hi[]{200, 200, 100, 2000, 1};
	for (size_t i = 0; i < 5; ++i) {
		for (float v : {lo[i], hi[i]}) { option.parameters = {{keys[i], v}}; assert(ReadNvidiaVfxTrueHdrTunables(option)); }
		for (float v : {lo[i] - 1, hi[i] + 1, lo[i] + .5f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
			option.parameters = {{keys[i], v}}; assert(!ReadNvidiaVfxTrueHdrTunables(option));
		}
	}
	option.parameters = {{"debanding", 0}};
	assert(ReadNvidiaVfxTrueHdrTunables(option)->values[4] == 1);
	int calls = 0;
	assert(UpdateNvidiaVfxTrueHdrTunables(original, original, [&](size_t, uint32_t) { ++calls; return true; }) == NvidiaVfxTrueHdrUpdate::Unchanged);
	assert(calls == 0);
	NvidiaVfxTrueHdrTunables next{{1, 2, 10, 400, 1}};
	// Inject a failure after the SDK has already mutated the failed setter.
	for (size_t failing = 0; failing < 5; ++failing) {
		auto sdk = original; bool injected = false;
		const auto result = UpdateNvidiaVfxTrueHdrTunables(original, next, [&](size_t i, uint32_t value) {
			sdk.values[i] = value;
			if (!injected && i == failing) { injected = true; return false; }
			return true;
		});
		assert(result == NvidiaVfxTrueHdrUpdate::Rejected && sdk == original);
	}
	for (size_t failing = 0; failing < 5; ++failing) {
		int failures = 0;
		const auto result = UpdateNvidiaVfxTrueHdrTunables(original, next, [&](size_t i, uint32_t) {
			if (i == failing) { ++failures; return false; } return true;
		});
		assert(result == NvidiaVfxTrueHdrUpdate::Invalidated && failures == 2);
	}
	auto committed = original, sdk = original;
	for (int revision = 0; revision < 1000; ++revision) {
		auto target = committed; target.values[1] = uint32_t(revision % 201);
		const auto result = UpdateNvidiaVfxTrueHdrTunables(committed, target, [&](size_t i, uint32_t v) { sdk.values[i] = v; return true; });
		assert(result == NvidiaVfxTrueHdrUpdate::Applied || result == NvidiaVfxTrueHdrUpdate::Unchanged);
		committed = target; assert(sdk == committed);
	}
	std::cout << "PASS HDR tuple validation, partial mutation rollback, invalidation, duplicate suppression and 1000 commits\n";
}
