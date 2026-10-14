#pragma once
#include "HdrFrame.h"
#include <algorithm>
#include <cmath>

namespace Magpie {

// Zero keeps the legacy SDR shader path. A positive scale converts sRGB UI
// colors to linear scRGB, whose fixed reference white is 80 nits.
inline float ResolveOverlayWhiteScale(bool hdrOutput, DXGI_FORMAT targetFormat,
	const HdrFrameMetadata& frame, float monitorSdrWhiteNits) noexcept {
	if (!hdrOutput || targetFormat != DXGI_FORMAT_R16G16B16A16_FLOAT) return 0;
	const auto usable = [](float value) { return std::isfinite(value) && value > 0; };
	float white = frame.overlayReferenceWhiteNits;
	if (!usable(white) && (frame.color.transfer == HdrTransferFunction::Linear ||
		frame.color.transfer == HdrTransferFunction::PQ)) {
		white = frame.color.sdrWhiteNits;
	}
	if (!usable(white)) white = monitorSdrWhiteNits;
	if (!usable(white)) white = 203;
	// UI comfort bounds, independent of the scene's highlight peak. In
	// particular displayPeakNits may describe an effect, not the output monitor.
	return std::clamp(white, 160.0f, 300.0f) / 80.0f;
}

}
