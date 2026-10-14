#include "HdrComponents.h"
#include "HdrComponentRuntime.h"
#include "OverlayColor.h"
#include "../src/RtxVideoBridge/RtxVideoBridge.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <map>

using namespace Magpie;
struct Effect { std::string name; std::map<std::string, float> parameters; };
using Effects = std::vector<Effect>;
int main() {
	D3D11_TEXTURE2D_DESC captured{}, output{};
	captured.Width = output.Width = 1920;
	captured.Height = output.Height = 1080;
	output.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	output.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	for (const auto format : {DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM}) {
		captured.Format = format;
		assert(MagpieRtxHdrEndpointsSupported(captured, output));
	}
	captured.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	assert(!MagpieRtxHdrEndpointsSupported(captured, output));
	captured.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	output.Width = 1280;
	assert(!MagpieRtxHdrEndpointsSupported(captured, output));
	output.Width = 1920;
	output.BindFlags = 0;
	assert(!MagpieRtxHdrEndpointsSupported(captured, output));
	assert(!BuildHdrComponentPlan(Effects{{"Bicubic"},{"DLSSNR\\DLSSNR_AI_Filter"}}).enabled);
	assert(!BuildHdrComponentPlan(Effects{{"Custom\\HDR_to_SDR"}}).enabled);
	const Effects pair{{"Color\\HDR_to_SDR", {{"whiteNits",360},{"peakNits",2000},{"exposure",0.5f}}},
		{"RTXVideo\\RTXVideo_VSR"},{"DLSSNR\\DLSSNR_AI_Filter"},{"Color\\SDR_to_HDR"},{"DLSSFG\\DLSS_FrameGeneration"}};
	const auto plan = BuildHdrComponentPlan(pair);
	assert(plan.enabled && plan.captureHdr && plan.outputHdr && plan.error == HdrComponentError::None);
	assert(plan.stages.size() == pair.size());
	assert(plan.stages[0].inputHdr && !plan.stages[0].outputHdr);
	assert(!plan.stages[1].inputHdr && !plan.stages[2].inputHdr);
	assert(plan.stages[3].pairIndex == 0 && plan.stages[3].outputHdr && plan.stages[4].inputHdr);
	const auto mapped = HdrComponentTransform(plan, 3);
	assert(mapped.sdrWhiteNits == 360 && mapped.hdrPeakNits == 2000 && mapped.exposure == 0.5f);
	HdrFrameMetadata metadata;
	metadata.frameId = 77; metadata.captureSequence = 9; metadata.resourceGeneration = 12;
	metadata.timestamp100ns = 123456; metadata.generated = true;
	ApplyHdrComponentOutputColor(metadata, plan, 0);
	assert(metadata.color.transfer == HdrTransferFunction::SRGB);
	ApplyHdrComponentOutputColor(metadata, plan, 3);
	assert(metadata.color.transfer == HdrTransferFunction::Linear && metadata.color.sdrWhiteNits == 360);
	assert(metadata.overlayReferenceWhiteNits == 360); // Paired exposure is undone, not multiplied.
	assert(metadata.frameId == 77 && metadata.captureSequence == 9 && metadata.resourceGeneration == 12 &&
		metadata.timestamp100ns == 123456 && metadata.generated);
	for (const auto& id : {"Color\\SDR_to_HDR", "RTXVideo\\RTXVideo_HDR"}) {
		const auto p = BuildHdrComponentPlan(Effects{{"Bicubic"},{id}});
		assert(!p.captureHdr && p.outputHdr && !p.stages[0].inputHdr && p.error == HdrComponentError::None);
	}
	const auto display = BuildHdrComponentPlan(Effects{{"Color\\HDR_to_SDR",{{"mode",1}}}});
	assert(display.captureHdr && !display.outputHdr && display.error == HdrComponentError::None);
	assert(BuildHdrComponentPlan(Effects{{"Color\\SDR_to_HDR",{{"mode",2}}}}).error == HdrComponentError::MissingPair);
	assert(BuildHdrComponentPlan(Effects{{"Color\\HDR_to_SDR",{{"mode",1}}},{"Color\\SDR_to_HDR",{{"mode",2}}}}).error == HdrComponentError::MissingPair);
	assert(BuildHdrComponentPlan(Effects{{"Color\\SDR_to_HDR"},{"RTXVideo\\RTXVideo_HDR"}}).error == HdrComponentError::ExpectedSdr);
	assert(BuildHdrComponentPlan(Effects{{"Color\\HDR_to_SDR"},{"Color\\HDR_to_SDR"}}).error == HdrComponentError::ExpectedHdr);
	const auto independent = BuildHdrComponentPlan(Effects{{"Color\\HDR_to_SDR"},{"Color\\SDR_to_HDR",{{"mode",1}}},
		{"Color\\HDR_to_SDR",{{"whiteNits",80}}},{"Color\\SDR_to_HDR"}});
	assert(independent.error == HdrComponentError::None && independent.stages[1].pairIndex == size_t(-1));
	assert(independent.stages[3].pairIndex == 2 && HdrComponentTransform(independent,3).sdrWhiteNits == 80);
	for (float value : {-1.0f, 0.5f, 3.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
		assert(BuildHdrComponentPlan(Effects{{"Color\\SDR_to_HDR",{{"mode",value}}}}).error == HdrComponentError::InvalidParameters);
	for (float value : {399.0f, 2001.0f, 1000.5f})
		assert(BuildHdrComponentPlan(Effects{{"RTXVideo\\RTXVideo_HDR",{{"peakNits",value}}}}).error == HdrComponentError::InvalidParameters);
	for (float white : {8.f, 203.f, 1000.f, 1500.f, 4000.f}) {
		const auto p = BuildHdrComponentPlan(Effects{{"Color\\SDR_to_HDR",{{"nativeWhiteNits",white}}}});
		assert(p.error == HdrComponentError::None && p.stages[0].whiteNits == white && p.stages[0].exposure == 1);
		HdrFrameMetadata frame;
		ApplyHdrComponentOutputColor(frame, p, 0);
		assert(frame.color.sdrWhiteNits == white && frame.color.referenceWhiteNits == 80 && frame.color.displayPeakNits == std::max(80.f, white));
		assert(frame.overlayReferenceWhiteNits == white);
		assert(ResolveOverlayWhiteScale(true, output.Format, frame, 250) == std::clamp(white, 160.f, 300.f) / 80);
	}
	for (const auto& values : {std::map<std::string,float>{{"whiteNits",80},{"exposure",.1f}},
		std::map<std::string,float>{{"whiteNits",400},{"exposure",4}}}) {
		const auto p = BuildHdrComponentPlan(Effects{{"Color\\SDR_to_HDR",values}});
		assert(p.error == HdrComponentError::None && p.stages[0].whiteNits == values.at("whiteNits") && p.stages[0].exposure == values.at("exposure"));
		HdrFrameMetadata frame;
		ApplyHdrComponentOutputColor(frame, p, 0);
		assert(frame.overlayReferenceWhiteNits == values.at("whiteNits") * values.at("exposure"));
	}
	const auto paired = BuildHdrComponentPlan(Effects{{"Color\\HDR_to_SDR"},{"Color\\SDR_to_HDR",{{"nativeWhiteNits",1500}}}});
	assert(paired.stages[1].pairIndex == 0 && HdrComponentTransform(paired,1).sdrWhiteNits == 203);
	const auto convertedExport = ResolveHdrExportColor({}, paired, 1, false);
	assert(convertedExport.hdr && convertedExport.color.sdrWhiteNits == 203);
	const auto appendedExport = ResolveHdrExportColor(convertedExport.color, paired, paired.stages.size(), false);
	assert(appendedExport.hdr && appendedExport.color == convertedExport.color);
	assert(!ResolveHdrExportColor({}, display, display.stages.size(), true).hdr);
	assert(!ResolveHdrExportColor({}, {}, 0, false).hdr);
	HdrFrameMetadata frame;
	const auto finalPair = BuildHdrComponentPlan(Effects{{"Color\\HDR_to_SDR",{{"whiteNits",400}}},
		{"Color\\SDR_to_HDR"},{"Color\\HDR_to_SDR",{{"whiteNits",240},{"exposure",2}}},
		{"Color\\SDR_to_HDR"},{"Bicubic"}});
	for (size_t i = 0; i < finalPair.stages.size(); ++i) ApplyHdrComponentOutputColor(frame, finalPair, i);
	assert(frame.overlayReferenceWhiteNits == 240); // Last pair, then an ordinary effect.
	assert(ResolveOverlayWhiteScale(true, output.Format, frame, 80) == 3);
	assert(ResolveOverlayWhiteScale(false, output.Format, frame, 80) == 0);
	assert(ResolveOverlayWhiteScale(true, DXGI_FORMAT_R8G8B8A8_UNORM, frame, 80) == 0);
	assert(ResolveOverlayWhiteScale(true, DXGI_FORMAT_R10G10B10A2_UNORM, frame, 80) == 0);
	frame.color.transfer = HdrTransferFunction::PQ; // XeSS scene is PQ; UI target remains scRGB.
	assert(ResolveOverlayWhiteScale(true, output.Format, frame, 80) == 3);
	const auto trueHdr = BuildHdrComponentPlan(Effects{{"RTXVideo\\RTXVideo_HDR",{{"peakNits",2000},{"middleGray",100}}}});
	ApplyHdrComponentOutputColor(frame, trueHdr, 0);
	assert(frame.overlayReferenceWhiteNits == 203);
	ApplyHdrComponentOutputColor(frame, display, 0);
	assert(frame.overlayReferenceWhiteNits == 0);
	for (float invalid : {0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
		frame = {};
		frame.overlayReferenceWhiteNits = invalid;
		assert(ResolveOverlayWhiteScale(true, output.Format, frame, 240) == 3);
		assert(ResolveOverlayWhiteScale(true, output.Format, frame, invalid) == 203.f / 80);
		frame.color.transfer = HdrTransferFunction::Linear;
		frame.color.sdrWhiteNits = 280;
		assert(ResolveOverlayWhiteScale(true, output.Format, frame, 240) == 3.5f);
		frame.color.sdrWhiteNits = invalid;
		assert(ResolveOverlayWhiteScale(true, output.Format, frame, 240) == 3);
	}
	std::cout << "PASS: HDR component domains, pairing, independent mapping, frame identity, invalid values, RTX SDK parameter bounds and direct BGRA/RGBA capture endpoints\n";
}
