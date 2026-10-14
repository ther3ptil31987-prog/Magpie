#pragma once
#include <cstdint>
#include <string_view>
#include "FrameGuidanceAvailability.h"

namespace Magpie {

// Pure input contract, shared with the native tests. These IDs describe accepted
// capture frames; captureSequence is a session identity, not a frame counter.
struct FSR3FGSourceSample {
	uint64_t frameId = 0, sequence = 0, generation = 0;
	int64_t timestamp100ns = 0;
	int64_t captureAdmissionTime100ns = 0;
	uint64_t outputHistoryRevision = 0;
};

enum class FSR3FGReset : uint32_t {
	None = 0, FirstFrame = 1, CaptureSession = 2, ResourceGeneration = 4,
	FrameGap = 8, Timestamp = 16, Guidance = 32, Geometry = 64, TimingSource = 128, OutputHistory = 256
};

enum class FSR3FGTimingSource { Bootstrap, CaptureTimestamp, CaptureAdmissionEstimate };
constexpr std::string_view FSR3FGTimingSourceName(FSR3FGTimingSource source) noexcept {
	switch (source) {
	case FSR3FGTimingSource::CaptureTimestamp: return "capture-timestamp";
	case FSR3FGTimingSource::CaptureAdmissionEstimate: return "capture-admission-estimate";
	default: return "bootstrap";
	}
}

class FSR3FGHistory {
public:
	struct Decision {
		bool submit; uint32_t reset; float deltaMs; uint64_t sdkFrameId;
		FSR3FGTimingSource timingSource = FSR3FGTimingSource::Bootstrap;
	};
	Decision Next(FSR3FGSourceSample sample, bool guidanceReset, bool geometryReset) const noexcept {
		// Repeated UI/forced redraws must not feed stale images to the SDK.
		if (!sample.frameId || (sample.frameId == _previous.frameId &&
			sample.sequence == _previous.sequence && sample.generation == _previous.generation))
			return {};
		uint32_t reset = 0;
		auto add = [&](FSR3FGReset reason) { reset |= static_cast<uint32_t>(reason); };
		if (!_previous.frameId) add(FSR3FGReset::FirstFrame);
		else {
			if (sample.sequence != _previous.sequence) add(FSR3FGReset::CaptureSession);
			if (sample.generation != _previous.generation) add(FSR3FGReset::ResourceGeneration);
			if (sample.outputHistoryRevision != _previous.outputHistoryRevision) add(FSR3FGReset::OutputHistory);
			if (sample.frameId != _previous.frameId + 1) add(FSR3FGReset::FrameGap);
		}
		const bool nativeNow = sample.timestamp100ns > 0, nativePrevious = _previous.timestamp100ns > 0;
		if (_previous.frameId && nativeNow != nativePrevious) add(FSR3FGReset::TimingSource);
		// Native capture time wins. Missing native time is not a permanent reset:
		// other capture APIs use a CPU observation attached to the accepted input.
		// Never mix these clock domains or fall back over a broken native clock.
		const int64_t currentTime = nativeNow ? sample.timestamp100ns : sample.captureAdmissionTime100ns;
		const int64_t previousTime = nativePrevious ? _previous.timestamp100ns : _previous.captureAdmissionTime100ns;
		const double delta = (double(currentTime) - double(previousTime)) / 10000.0;
		const bool validTime = nativeNow == nativePrevious && currentTime > 0 && previousTime > 0 &&
			delta >= 0.125 && delta < 500.0;
		if (!validTime) add(FSR3FGReset::Timestamp);
		if (guidanceReset) add(FSR3FGReset::Guidance);
		if (geometryReset) add(FSR3FGReset::Geometry);
		// Bootstrap with a nominal duration only on reset. Never use Present
		// blocking time as a substitute for the capture time of a valid pair.
		return { true, reset, validTime ? float(delta) : 1000.0f / 60.0f, _sdkFrameId,
			!validTime ? FSR3FGTimingSource::Bootstrap : nativeNow ? FSR3FGTimingSource::CaptureTimestamp :
			FSR3FGTimingSource::CaptureAdmissionEstimate };
	}
	void Submitted(FSR3FGSourceSample sample) noexcept { _previous = sample; ++_sdkFrameId; }
	Decision Replay() const noexcept { return { true, 0, 0, _sdkFrameId }; }
	void ReplaySubmitted() noexcept { ++_sdkFrameId; }
	void Reset() noexcept { _previous = {}; /* SDK ID stays monotonic across resize. */ }
private:
	FSR3FGSourceSample _previous;
	uint64_t _sdkFrameId = 1;
};

// Capacity retries of the same reset sample are harmless. A second missing
// sample is not a bootstrap; a mandatory provider must not silently disappear.
class FSR3FGMotionAdmission {
public:
	bool Accept(FrameGuidanceAvailability availability, FSR3FGSourceSample sample) noexcept {
		if (availability == FrameGuidanceAvailability::Valid) { _missing = {}; return true; }
		if (availability != FrameGuidanceAvailability::ResetOnly || !sample.frameId) return false;
		if (_missing.frameId && _missing.sequence == sample.sequence &&
			_missing.generation == sample.generation && _missing.frameId != sample.frameId) return false;
		_missing = sample;
		return true;
	}
private:
	FSR3FGSourceSample _missing{};
};

// The pinned SDK ignores Present flags while interpolating, but honors vsync.
// Use sync=1 when tearing is disallowed, including idle and environment changes.
constexpr uint32_t FSR3FGSyncInterval(bool allowTearing) noexcept { return allowTearing ? 0u : 1u; }

// Provider names are SDK version strings. Reject substring matches (3.1.60,
// ML names, etc.); IDs themselves must come from ffxQuery, never from constants.
constexpr bool IsFSR3FGProvider(std::string_view name, bool swapchain) noexcept {
	return name == (swapchain ? "3.1.7" : "3.1.6");
}

// Until a distortion-field contract exists, external motion is admitted only
// for known effects preserving rectangular image coordinates. Other chains
// must be rejected; internal optical flow alone is not a supported fallback.
constexpr bool SupportsFSR3FGExternalMotion(std::string_view name) noexcept {
	return name == "FSR3FG\\FSR3_FrameGeneration" || name == "DLSS\\DLSS_SR" ||
		name == "FSR2\\FSR2_SR" || name == "FSR3\\FSR3_SR" || name == "FSR4\\FSR4_SR" ||
		name == "XeSS\\XeSS_SR" || name == "DLSSNR\\DLSSNR_AI_Filter" ||
		name == "RTXVideo\\RTXVideo_VSR" || name == "NvidiaVFX\\Upscale" ||
		name == "NIS\\NIS" || name == "NIS\\NVSharpen" ||
		name == "Bilinear" || name == "Bicubic" || name == "Lanczos" ||
		name == "Nearest" || name == "FrameRate_Filter" || name == "Utility\\FrameRate_Filter" ||
		name == "FSR\\FSR_EASU" || name == "FSR\\FSR_RCAS";
}

}
