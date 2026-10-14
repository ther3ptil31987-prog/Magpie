#pragma once
#include "HdrComponents.h"
#include "HdrColorTransform.h"

namespace Magpie {

inline HdrTransformParameters HdrComponentTransform(const HdrComponentPlan& plan, size_t index) noexcept {
	const auto& stage = plan.stages[index];
	const auto& settings = stage.pairIndex < plan.stages.size() ? plan.stages[stage.pairIndex] : stage;
	HdrTransformParameters result;
	result.sdrWhiteNits = settings.whiteNits;
	result.hdrPeakNits = settings.peakNits;
	result.exposure = settings.exposure;
	result.shoulder = settings.shoulder;
	return result;
}

inline void ApplyHdrComponentOutputColor(HdrFrameMetadata& metadata,
	const HdrComponentPlan& plan, size_t index) noexcept {
	if (!plan.enabled || index >= plan.stages.size()) return;
	const auto& stage = plan.stages[index];
	if (stage.kind == HdrComponentKind::None) return;
	const auto settings = HdrComponentTransform(plan, index);
	metadata.overlayReferenceWhiteNits = stage.outputHdr ? settings.sdrWhiteNits : 0;
	if (stage.kind == HdrComponentKind::SdrToHdr && stage.pairIndex == size_t(-1)) {
		metadata.overlayReferenceWhiteNits *= settings.exposure;
	}
	auto& color = metadata.color;
	color.primaries = HdrColorPrimaries::Rec709;
	color.transfer = stage.outputHdr ? HdrTransferFunction::Linear : HdrTransferFunction::SRGB;
	color.range = stage.outputHdr ? HdrColorRange::SceneLinear : HdrColorRange::Full;
	color.dxgiColorSpace = stage.outputHdr ? DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
	color.referenceWhiteNits = 80;
	color.sdrWhiteNits = settings.sdrWhiteNits;
	color.displayPeakNits = settings.hdrPeakNits;
	color.isPreExposed = false;
	color.preExposure = 1;
	color.isSceneReferred = false;
	// Source mastering metadata is no longer valid after an explicit transform.
	color.metadata = {};
}

struct HdrExportColor {
	bool hdr = false;
	ColorDescription color{};
};

inline HdrExportColor ResolveHdrExportColor(const ColorDescription& source,
	const HdrComponentPlan& plan, size_t index, bool legacyHdr) noexcept {
	HdrFrameMetadata metadata{};
	metadata.color = source;
	ApplyHdrComponentOutputColor(metadata, plan, index);
	// Renderer may append a Bicubic after the configured stages. It inherits
	// the final domain, and must not index beyond the configured component plan.
	return { plan.enabled ? (index < plan.stages.size() ? plan.stages[index].outputHdr : plan.outputHdr) : legacyHdr,
		metadata.color };
}

}
