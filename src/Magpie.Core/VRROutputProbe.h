#pragma once
#include <array>
#include <cstdint>

namespace Magpie {

struct VRROutputProbeKey {
	std::array<int32_t, 4> windowRect{}, monitorRect{};
	uintptr_t monitor = 0;
	bool remote = false, topologyCurrent = false, geometryValid = false;
	bool operator==(const VRROutputProbeKey&) const = default;
};

// Cheap geometry/topology samples continue every 500 ms. Expensive adapter
// output enumeration runs on changes/events plus a 5-second recovery probe.
class VRROutputProbe {
public:
	bool NeedsEnumeration(const VRROutputProbeKey& key, uint64_t nowMs, bool force) noexcept {
		// Window movement/size still qualifies against live geometry, but does
		// not change adapter membership while it remains on the same monitor.
		if (!force && _known && key.monitor == _key.monitor && key.monitorRect == _key.monitorRect &&
			key.topologyCurrent == _key.topologyCurrent && nowMs < _nextRecovery) return false;
		_key = key;
		_known = true;
		_nextRecovery = nowMs + 5000;
		return true;
	}
	void Invalidate() noexcept { _known = false; }
private:
	VRROutputProbeKey _key{};
	uint64_t _nextRecovery = 0;
	bool _known = false;
};

}
