// Lightweight unit-style validation for the HDR mechanical slice.
//
// This file is intentionally kept independent of the UI/configuration projects
// so it can be compiled as a small console executable when a test runner is
// available. It validates protocol route data and the adapter dispatcher plan.
// Configuration round-trip and source-level invariants are covered by
// scripts\Run-HdrMechanicalValidation.ps1 and by Magpie's normal build.
//
// Suggested compile shape (after MSVC environment is set up):
//   cl /std:c++20 /EHsc /I src\Magpie.Core src\Magpie.Core\HdrFrame.cpp ^
//       src\Magpie.Core\HdrProtocol.cpp src\Magpie.Core\HdrColorTransform.cpp ^
//       src\Magpie.Core\HdrAdapterDispatcher.cpp src\Magpie.Core\HdrEffectBoundary.cpp ^
//       tests\HdrMechanicalTests.cpp

#include "HdrAdapterDispatcher.h"
#include "HdrFrame.h"
#include "HdrProtocol.h"
#include "HdrEffectBoundary.h"
#include "EffectProtocolCatalogC.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

using namespace Magpie;

namespace {

int g_failures = 0;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::printf("FAIL: %s\n", message);
        ++g_failures;
    } else {
        std::printf("PASS: %s\n", message);
    }
}

HdrFormatRoute MakeRoute(HdrAdapterProfile profile) {
    HdrFormatRoute route;
    route.effectId = "TestEffect";
    route.optionId = profile == HdrAdapterProfile::Unknown ? "UnknownRoute" : "Route";
    route.inputFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    route.outputFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    route.inputTransfer = HdrTransferFunction::Linear;
    route.outputTransfer = HdrTransferFunction::Linear;
    route.inputRange = HdrColorRange::SceneLinear;
    route.outputRange = HdrColorRange::SceneLinear;
    route.alphaMode = HdrAlphaMode::Preserve;
    route.evidenceLevel = HdrEvidenceLevel::LocalValidation;
    route.adapterProfile = profile;
    route.hdrNative = profile == HdrAdapterProfile::DirectFP16 ||
        profile == HdrAdapterProfile::BoundedHDR ||
        profile == HdrAdapterProfile::ConditionalFP16;
    route.defaultForHdr = profile == HdrAdapterProfile::DirectFP16;
    route.defaultForSdr = profile == HdrAdapterProfile::SDRCompatible;
    return route;
}

void TestRouteStorageAndCategories() {
    HdrFormatRoute route = MakeRoute(HdrAdapterProfile::DirectFP16);
    Check(route.Id() == "TestEffect/Route", "route id combines effectId and optionId");
    Check(route.IsValid(), "complete route is valid");
    Check(route.IsHdrNative(), "explicit hdrNative route is HDR-native");
    Check(!route.IsHdrAdapter(), "hdrNative route is not an adapter-only route");

    HdrFormatRoute r8Sdr = MakeRoute(HdrAdapterProfile::SDRCompatible);
    r8Sdr.inputFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    r8Sdr.outputFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    r8Sdr.hdrNative = false;
    Check(r8Sdr.IsHdrAdapter(), "R8 SDRCompatible route is an adapter route");
    Check(!r8Sdr.IsHdrNative(), "R8 route is not automatically HDR-native");

    HdrFormatRoute fp16WithoutFlag = MakeRoute(HdrAdapterProfile::DirectFP16);
    fp16WithoutFlag.hdrNative = false;
    Check(!fp16WithoutFlag.IsHdrNative(), "FP16 route without explicit hdrNative is not HDR-native");

    HdrFormatRoute r10 = MakeRoute(HdrAdapterProfile::BoundedHDR);
    r10.inputFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
    r10.outputFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
    r10.inputTransfer = HdrTransferFunction::PQ;
    r10.outputTransfer = HdrTransferFunction::PQ;
    r10.hdrNative = true;
    Check(r10.IsHdrNative(), "R10 route is HDR-native only when PQ/BT.2100 semantics are explicit");

    HdrFormatRoutes routes{ MakeRoute(HdrAdapterProfile::SDRCompatible), MakeRoute(HdrAdapterProfile::DirectFP16) };
    Check(SelectDefaultHdrRoute(routes) != nullptr, "HDR mode selects a default HDR route");
    Check(SelectDefaultHdrRoute(routes)->adapterProfile == HdrAdapterProfile::DirectFP16,
        "HDR mode prefers DirectFP16 when marked defaultForHdr");
    Check(SelectDefaultSdrRoute(routes)->adapterProfile == HdrAdapterProfile::SDRCompatible,
        "SDR mode keeps SDRCompatible default");
    Check(GetAcceptedFormatRoutes(routes).size() == 2, "accepted route list contains valid routes");
    Check(GetHdrNativeFormatRoutes(routes).size() == 1, "hdrNative route list is explicit only");
    Check(GetHdrAdapterFormatRoutes(routes).size() == 1, "hdrAdapter route list contains SDR route");
}

void TestDispatcherProfiles() {
    HdrAdapterDispatcher dispatcher;
    ColorDescription color{};
    color.referenceWhiteNits = 80.0f;
    color.displayPeakNits = 1000.0f;

    const char* profileNames[] = {
        "DirectFP16", "BoundedHDR", "SDRCompatible",
        "ConditionalFP16", "Unknown", "PresentationTerminal"
    };
    const HdrAdapterProfile profiles[] = {
        HdrAdapterProfile::DirectFP16, HdrAdapterProfile::BoundedHDR,
        HdrAdapterProfile::SDRCompatible, HdrAdapterProfile::ConditionalFP16,
        HdrAdapterProfile::Unknown, HdrAdapterProfile::PresentationTerminal
    };

    for (size_t i = 0; i < std::size(profiles); ++i) {
        HdrFormatRoute route = MakeRoute(profiles[i]);
        const HdrAdapterPlan plan = dispatcher.BuildPlan(route, color);
        Check(plan.profile == profiles[i], profileNames[i]);
    }

    HdrFormatRoute direct = MakeRoute(HdrAdapterProfile::DirectFP16);
    HdrAdapterPlan plan = dispatcher.BuildPlan(direct, color);
    Check(plan.IsNonTerminalCanonical(), "DirectFP16 returns canonical FP16");
    Check(!plan.requiresSdrMapping && !plan.requiresBoundedMapping,
        "DirectFP16 does not introduce SDR mapping");

    HdrFormatRoute sdr = MakeRoute(HdrAdapterProfile::SDRCompatible);
    plan = dispatcher.BuildPlan(sdr, color);
    Check(plan.requiresSdrMapping, "SDRCompatible requires HDR-to-SDR mapping");
    Check(plan.forwardParameters.IsValid() && plan.inverseParameters.IsValid(),
        "SDRCompatible carries paired forward/inverse parameters");

    HdrFormatRoute bounded = MakeRoute(HdrAdapterProfile::BoundedHDR);
    plan = dispatcher.BuildPlan(bounded, color);
    Check(plan.requiresBoundedMapping, "BoundedHDR requires bounded encode/decode");
    Check(plan.forwardParameters.IsValid() && plan.inverseParameters.IsValid(),
        "BoundedHDR carries paired forward/inverse parameters");

    HdrFormatRoute unknown = MakeRoute(HdrAdapterProfile::Unknown);
    unknown.alphaMode = HdrAlphaMode::Unknown;
    plan = dispatcher.BuildPlan(unknown, color);
    Check(plan.usesFallback, "Unknown route selects fallback");
    Check(!plan.fallbackReason.empty(), "Unknown route records fallback reason");
    Check(plan.alphaMode == HdrAlphaMode::ForceOpaque, "Unknown fallback makes alpha explicit");

    HdrFormatRoute terminal = MakeRoute(HdrAdapterProfile::PresentationTerminal);
    plan = dispatcher.BuildPlan(terminal, color);
    Check(plan.isPresentationTerminal, "PresentationTerminal is terminal");
    Check(plan.canonicalOutputFormat == DXGI_FORMAT_UNKNOWN,
        "PresentationTerminal does not promise a normal canonical output");
}

void TestPairedColorBridge() {
    bool bounded = true, monotonic = true, roundTrip = true, endpoints = true;
    bool whiteHeadroom = true, continuous = true, quantizedOrdered = true, quantizedBound = true;
    float worstQuantizedError = 0.0f;
    for (float white : {80.0f, 203.0f, 360.0f}) {
        for (float peakNits : {80.0f, 1000.0f, 4000.0f}) {
            for (float exposure : {0.25f, 1.0f, 4.0f}) {
                for (float shoulder : {0.25f, 1.0f, 2.0f}) {
                    HdrTransformParameters p;
                    p.sdrWhiteNits = white;
                    p.hdrPeakNits = std::max(peakNits, white);
                    p.exposure = exposure;
                    p.shoulder = shoulder;
                    const float peak = p.hdrPeakNits / 80.0f;
                    endpoints &= HdrColorTransform::MapHdrToSdr(0, p) == 0;
                    endpoints &= HdrColorTransform::MapHdrToSdr(-1, p) == 0;
                    endpoints &= HdrColorTransform::MapSdrToHdr(-1, p) == 0;
                    endpoints &= std::abs(HdrColorTransform::MapHdrToSdr(peak, p) - 1) < 1e-6f;
                    endpoints &= std::abs(HdrColorTransform::MapHdrToSdr(peak * 2, p) - 1) < 1e-6f;
                    endpoints &= std::abs(HdrColorTransform::MapSdrToHdr(1, p) - peak) < peak * 1e-6f;
                    const float w = white / 80.0f;
                    if (peak > w) whiteHeadroom &= HdrColorTransform::MapHdrToSdr(w, p) < 1;
                    continuous &= std::abs(HdrColorTransform::MapHdrToSdr(w * 0.99999f, p) -
                        HdrColorTransform::MapHdrToSdr(w * 1.00001f, p)) < 0.0001f;
                    float previous = -1, previousQuantized = -1;
                    for (int i = 0; i <= 4096; ++i) {
                        const float input = peak * i / 4096.0f;
                        const float mapped = HdrColorTransform::MapHdrToSdr(input, p);
                        const float restored = HdrColorTransform::MapSdrToHdr(mapped, p);
                        bounded &= std::isfinite(mapped) && mapped >= 0 && mapped <= 1;
                        monotonic &= mapped >= previous;
                        roundTrip &= std::abs(restored - input) < peak * 0.0002f;
                        previous = mapped;
                        const float encoded = HdrColorTransform::EncodeTransfer(mapped, HdrTransferFunction::SRGB);
                        const float r8 = std::round(encoded * 255.0f) / 255.0f;
                        const float quantized = HdrColorTransform::MapSdrToHdr(
                            HdrColorTransform::DecodeTransfer(r8, HdrTransferFunction::SRGB), p);
                        quantizedOrdered &= quantized >= previousQuantized;
                        // R8 is lossy. Bound the error over these tested display
                        // and exposure settings instead of claiming exact recovery.
                        quantizedBound &= std::abs(quantized - input) <= peak * 0.035f;
                        previousQuantized = quantized;
                        worstQuantizedError = std::max(worstQuantizedError, std::abs(quantized - input));
                    }
                }
            }
        }
    }
    Check(endpoints, "paired bridge preserves black and declared peak endpoints");
    Check(bounded && monotonic, "dense HDR ramps are finite bounded and monotonic");
    Check(roundTrip, "FP32 paired bridge reconstructs ramps across white/exposure/peak settings");
    Check(whiteHeadroom && continuous, "white is continuous and reserves space for brighter highlights");
    Check(quantizedOrdered, "sRGB R8 quantization preserves brightness ordering");
    Check(quantizedBound, "R8 roundtrip error stays below 3.5 percent of peak in tested settings");
    std::printf("R8 bridge is lossy: worst tested absolute error %.4f scRGB units\n", worstQuantizedError);
}

void TestProductionFrameGenerationRoutes() {
    const auto bicubic = EffectProtocolC::Bicubic();
    const auto* resizeRoute = HdrEffectBoundary::SelectRoute(true, bicubic);
    Check(resizeRoute && resizeRoute->adapterProfile == HdrAdapterProfile::DirectFP16 &&
        resizeRoute->inputFormat == DXGI_FORMAT_R16G16B16A16_FLOAT &&
        resizeRoute->outputFormat == DXGI_FORMAT_R16G16B16A16_FLOAT &&
        resizeRoute->inputTransfer == HdrTransferFunction::Linear,
        "Bicubic initial/resize route retains linear HDR on both surfaces");
    for (const char* group : {"DLSSFG", "XeSSFG", "FSR3FG"}) {
        const auto markers = EffectProtocolC::FrameGenerationMarker(group);
        const auto* marker = HdrEffectBoundary::SelectRoute(true, markers);
        Check(marker && marker->adapterProfile == HdrAdapterProfile::DirectFP16 &&
            marker->inputFormat == DXGI_FORMAT_R16G16B16A16_FLOAT &&
            marker->outputFormat == DXGI_FORMAT_R16G16B16A16_FLOAT &&
            marker->inputTransfer == HdrTransferFunction::Linear &&
            marker->outputTransfer == HdrTransferFunction::Linear,
            "production FG marker has a selectable canonical handoff");
        const auto terminals = EffectProtocolC::GetGroupCHdrRoutes(group);
        Check(terminals.size() == 1 && terminals.front().IsPresentationTerminal(),
            "actual FG endpoint remains a distinct presentation terminal");
        Check(SelectDefaultHdrRoute(terminals) == nullptr,
            "effect chain cannot accidentally consume a presenter terminal");
    }
    const auto xess = EffectProtocolC::XeSS();
	Check(EffectProtocolC::FSR3FG().front().inputFormat == DXGI_FORMAT_R8G8B8A8_UNORM &&
		!EffectProtocolC::FSR3FG().front().defaultForHdr,
		"FSR3 FG endpoint is SDR-only until HDR acceptance");
    const auto* selected = HdrEffectBoundary::SelectRoute(true, xess);
    Check(selected && selected->inputFormat == DXGI_FORMAT_R16G16B16A16_FLOAT,
        "XeSS compile/runtime selection chooses declared default rather than first R10 entry");
    Check(EffectProtocolC::XeSSFG().front().inputFormat == DXGI_FORMAT_R10G10B10A2_UNORM &&
        EffectProtocolC::XeSSFG().front().inputTransfer == HdrTransferFunction::PQ,
        "XeSS FG actual endpoint retains its HDR10 contract");
}

}

int main() {
    TestRouteStorageAndCategories();
    TestDispatcherProfiles();
    TestPairedColorBridge();
    TestProductionFrameGenerationRoutes();

    if (g_failures == 0) {
        std::printf("All HDR mechanical tests passed.\n");
        return 0;
    }

    std::printf("%d HDR mechanical test(s) failed.\n", g_failures);
    return 1;
}
