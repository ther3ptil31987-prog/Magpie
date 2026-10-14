#include "FSR3FGPolicy.h"
#include "FramePacingOptions.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

using namespace Magpie;
static unsigned checks = 0;
static void Check(bool condition, const char* message) {
	++checks;
	if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static bool Has(uint32_t mask, FSR3FGReset reason) { return (mask & uint32_t(reason)) != 0; }

int main() {
	FSR3FGHistory history;
	FSR3FGSourceSample a{1, 7, 3, 10000000};
	auto first = history.Next(a, false, false);
	Check(first.submit && first.sdkFrameId == 1 && Has(first.reset, FSR3FGReset::FirstFrame), "bootstrap resets");
	Check(history.Next(a, false, false).sdkFrameId == first.sdkFrameId, "capacity retry does not consume SDK ID");
	history.Submitted(a);
	Check(!history.Next(a, true, true).submit, "UI/forced redraw cannot advance a duplicate frame");
	FSR3FGSourceSample b{2, 7, 3, 10166667};
	auto second = history.Next(b, false, false);
	Check(second.submit && second.reset == 0 && second.sdkFrameId == 2, "consecutive capture accepted");
	Check(std::abs(second.deltaMs - 16.6667f) < 0.001f, "duration is capture delta, not present time");
	Check(Has(history.Next({4,7,3,10500000}, false, false).reset, FSR3FGReset::FrameGap), "skipped source ID resets despite contiguous SDK ID");
	Check(Has(history.Next({2,8,3,10166667}, false, false).reset, FSR3FGReset::CaptureSession), "new capture session resets");
	Check(Has(history.Next({2,7,4,10166667}, false, false).reset, FSR3FGReset::ResourceGeneration), "resource replacement resets");
	Check(history.Next({1,8,3,10166667}, false, false).submit, "same numeric ID in a new session is not a duplicate");
	Check(history.Next({1,7,4,10166667}, false, false).submit, "same numeric ID after resource recreation is not a duplicate");
	Check(Has(history.Next({2,7,3,10000000}, false, false).reset, FSR3FGReset::Timestamp), "non-increasing timestamp resets");
	Check(Has(history.Next({2,7,3,15000000}, false, false).reset, FSR3FGReset::Timestamp), "500ms gap resets");
	Check(Has(history.Next({2,7,3,0}, false, false).reset, FSR3FGReset::Timestamp), "missing capture time cannot masquerade as valid temporal input");
	Check(Has(history.Next(b, true, false).reset, FSR3FGReset::Guidance), "failed/changed motion resets");
	Check(Has(history.Next(b, false, true).reset, FSR3FGReset::Geometry), "letterbox or geometry change resets");
	history.Submitted(b);
	history.Reset();
	Check(history.Next(b, false, false).sdkFrameId == 3, "resize clears source history while preserving SDK ID monotonicity");
	Check(Has(history.Next(b, false, false).reset, FSR3FGReset::FirstFrame), "resize bootstraps new history");
	Check(!history.Next({}, false, false).submit, "missing frame identity is rejected");
	FSR3FGHistory estimatedHistory;
	FSR3FGSourceSample e1{1, 1, 1, 0, 10000000};
	Check(estimatedHistory.Next(e1, false, false).submit, "missing native timestamp still accepts first input");
	estimatedHistory.Submitted(e1);
	FSR3FGSourceSample e2{2, 1, 1, 0, 10166667};
	auto estimated = estimatedHistory.Next(e2, false, false);
	Check(estimated.reset == 0 && estimated.timingSource == FSR3FGTimingSource::CaptureAdmissionEstimate,
		"non-WGC frames do not reset forever when capture acceptance times are valid");
	Check(std::abs(estimated.deltaMs - 16.6667f) < 0.001f, "capture acceptance estimate retains its own duration");
	Check(!estimatedHistory.Next({1,1,1,0,10500000}, false, false).submit,
		"UI retry with a later clock cannot advance the same source frame");
	Check(Has(estimatedHistory.Next({2,1,1,0,10000000}, false, false).reset, FSR3FGReset::Timestamp),
		"non-increasing fallback clock resets");
	Check(Has(estimatedHistory.Next({2,1,1,0,15000000}, false, false).reset, FSR3FGReset::Timestamp),
		"stalled fallback clock resets");
	estimatedHistory.Submitted(e2);
	Check(Has(estimatedHistory.Next({3,1,1,12000000,10333334}, false, false).reset, FSR3FGReset::TimingSource),
		"switching from estimate to native time resets once instead of mixing epochs");
	FSR3FGHistory nativeHistory;
	nativeHistory.Submitted({1,1,1,10000000,100000000});
	auto native = nativeHistory.Next({2,1,1,10166667,102000000}, false, false);
	Check(native.reset == 0 && native.timingSource == FSR3FGTimingSource::CaptureTimestamp &&
		std::abs(native.deltaMs - 16.6667f) < 0.001f, "native capture clock wins over a delayed CPU observation");
	Check(Has(nativeHistory.Next({2,1,1,10000000,100166667}, false, false).reset, FSR3FGReset::Timestamp),
		"valid fallback cannot hide a broken native clock");
	Check(IsFSR3FGProvider("3.1.6", false) && IsFSR3FGProvider("3.1.7", true), "exact matched providers");
	for (auto name : {"4.0.1", "3.1.60", "FSR 4 3.1.6", "", "3.1.5"})
		Check(!IsFSR3FGProvider(name, false), "no automatic ML/substring/older provider substitution");
	Check(!IsFSR3FGProvider("3.1.6", true), "FG and swapchain versions differ");
	for (auto mode : {FrameSyncMode::FrontEdge, FrameSyncMode::Async, FrameSyncMode::Reflex}) {
		Check(ResolveFrameSyncBackend({true,60,mode},false,false,true,false,true) == FrameSyncBackend::Async,
			"FSR uses producer base limiter for every requested strategy");
		Check(ResolveFrameSyncBackend({false,60,mode},false,false,true,false,true) == FrameSyncBackend::None, "disabled limiter stays disabled");
		Check(ResolveFrameSyncBackend({true,60,mode},false,false,true,true,true) == FrameSyncBackend::None, "benchmark does not gain an implicit limiter");
	}
	Check(ResolvePresentationFrameRate(0,0,144,2) == 72, "auto base target accounts for fixed 2x");
	Check(SupportsFSR3FGExternalMotion("FSR3\\FSR3_SR"), "FSR SR is an optional rectangular input");
	Check(SupportsFSR3FGExternalMotion("Bicubic"), "non-FSR scaling supported");
	Check(!SupportsFSR3FGExternalMotion("CRT\\CRT_Geom"), "distortion cannot consume untransformed guidance");
	Check(!SupportsFSR3FGExternalMotion("Custom\\Unknown"), "unverified external-motion mapping rejected");
	Check(SupportsFSR3FGExternalMotion("NIS\\NIS"), "NIS rectangular scaling admitted");
	Check(SupportsFSR3FGExternalMotion("NIS\\NVSharpen"), "NIS sharpening admitted");
	Check(!SupportsFSR3FGExternalMotion("NIS\\Unknown"), "NIS namespace is not a blanket allowlist");
	FSR3FGHistory revisions;
	FSR3FGSourceSample before{10, 1, 1, 10000000, 0, 7};
	revisions.Submitted(before);
	auto changed = before; changed.frameId++; changed.timestamp100ns += 166667; changed.outputHistoryRevision++;
	Check(revisions.Next(changed, false, false).reset == uint32_t(FSR3FGReset::OutputHistory), "parameter edit resets continuous input once");
	revisions.Submitted(changed); changed.frameId++; changed.timestamp100ns += 166667;
	Check(revisions.Next(changed, false, false).reset == 0, "ordinary frames preserve effect history epoch");
	changed.frameId--; changed.outputHistoryRevision++;
	Check(!revisions.Next(changed, false, false).submit, "same-capture parameter edit must use non-FG redraw");
	FSR3FGMotionAdmission motion;
	Check(motion.Accept(FrameGuidanceAvailability::ResetOnly, {1,1,1}), "provider bootstrap accepted");
	Check(motion.Accept(FrameGuidanceAvailability::ResetOnly, {1,1,1}), "capacity retry is not another missing sample");
	Check(!motion.Accept(FrameGuidanceAvailability::ResetOnly, {2,1,1}), "consecutive reset-only samples fail closed");
	Check(motion.Accept(FrameGuidanceAvailability::ResetOnly, {1,2,1}), "new capture session bootstraps");
	Check(motion.Accept(FrameGuidanceAvailability::Valid, {2,2,1}), "valid stationary vectors accepted regardless of values");
	Check(motion.Accept(FrameGuidanceAvailability::ResetOnly, {3,2,1}), "legitimate reset after valid input accepted");
	Check(motion.Accept(FrameGuidanceAvailability::ResetOnly, {4,2,2}), "resource recreation permits bootstrap");
	for (auto state : {FrameGuidanceAvailability::Unavailable, FrameGuidanceAvailability::ProviderFailed,
		FrameGuidanceAvailability::ProviderStopped, FrameGuidanceAvailability::SynchronizationFailed})
		Check(!motion.Accept(state, {1,3,1}), "failure is fatal even on a first/reset input");
	Check(FSR3FGSyncInterval(false) == 1 && FSR3FGSyncInterval(true) == 0, "SDK sync interval enforces tearing eligibility");
	std::printf("FSR3FG policy: %u checks passed (no GPU/runtime acceptance).\n", checks);
}
