"""Extract changed production paths for deterministic native regression tests.

Only the fence clock is substituted. COM/SDK calls are fakes; the pinned SDK
worker flags expression is exercised, not a real swapchain or display.
"""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('output', type=Path)
parser.add_argument('--sdk-directory', required=True, type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]


def block(text, signature):
    start = text.index(signature)
    begin = text.index('{', start)
    depth, end = 1, begin + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


paths = [root / 'src/Magpie.Core/FSR3FGPresenter.cpp',
         root / 'src/Magpie.Core/Renderer.cpp',
         root / 'src/Magpie.Core/FrameGuidanceService.cpp',
         args.sdk_directory / 'Kits/FidelityFX/framegeneration/fsr3/dx12/FrameInterpolationSwapchainDX12.cpp']
presenter, renderer, guidance, sdk = [p.read_text(encoding='utf-8-sig') for p in paths]
wait = block(presenter, 'static bool WaitForFence(').replace('std::chrono::steady_clock', 'TestClock')
callback = block(presenter, 'static ffxReturnCode_t GenerateFrame(')
commit_start = presenter.index('if (FAILED(hr)) return impl.Fail("proxy Present", hr);')
commit_end = presenter.index('++impl.baseSubmissions;', commit_start) + len('++impl.baseSubmissions;')
commit = presenter[commit_start:commit_end]
reset = presenter[presenter.index('impl.callbackResult = FFX_API_RETURN_OK;', presenter.index('bool FSR3FGPresenter::EndFrame(')):]
reset = reset[:reset.index('const auto start')]
finish = block(guidance, 'const auto finish =') + ';'
guard = block(renderer, 'if (!isNewCaptureFrame && (_HasSoftwareFrameGenerator() || _HasProxyFrameGenerator()) &&')
guard = guard.replace('d3dDC->Flush();', '').replace('return;', 'return false;')
flags_start = sdk.index('    const UINT uSyncInterval', sdk.index('void presentToSwapChain('))
flags = sdk[flags_start:sdk.index('    struct AntiLag2Data', flags_start)]

prefix = r'''
#include <windows.h>
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string_view>
#include <vector>
#include <ffx_api.h>
#include <ffx_framegeneration.h>
#include "FSR3FGPolicy.h"
#include "FrameGuidanceTypes.h"
#include "PresentationFrameRate.h"
#include "EffectFrameState.h"
#include "VRRPresentationPolicy.h"
using namespace Magpie;
static unsigned checks = 0;
void Check(bool value, const char* message) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
struct TestClock {
    using duration = std::chrono::steady_clock::duration;
    using time_point = std::chrono::time_point<TestClock>;
    static inline int64_t milliseconds = 0;
    static time_point now() { return time_point(std::chrono::milliseconds(milliseconds)); }
};
struct FakeFence {
    uint64_t completed = 0;
    HRESULT registration = S_OK;
    unsigned registrations = 0;
    uint64_t GetCompletedValue() const { return completed; }
    HRESULT SetEventOnCompletion(uint64_t, HANDLE) { ++registrations; return registration; }
};
struct FakeDevice { HRESULT GetDeviceRemovedReason() { return DXGI_ERROR_DEVICE_REMOVED; } };
struct FakeEvent { HANDLE get() const { return nullptr; } };
struct FakeDispatch {
    ffxReturnCode_t result = FFX_API_RETURN_OK;
    template<class A, class B> ffxReturnCode_t operator()(A*, B*) const { return result; }
};
struct FakeLogger {
    static FakeLogger& Get() { static FakeLogger logger; return logger; }
    void Warn(const char*) {}
};
using Logger = FakeLogger;
namespace Magpie {
struct FSR3FGPresenter {
    struct Impl {
        FakeFence* fence12;
        FakeDevice* device12;
        FakeEvent fenceEvent;
        bool fatal = false, idlePresentation = false, geometryReset = false;
        std::string_view failure;
        FSR3FGHistory history;
        FSR3FGHistory::Decision decision{true, 0, 0, 42};
        std::optional<uint32_t> callbackGeneratedCount;
        uint64_t callbackDispatches = 0, resetPassThroughs = 0, interpolations = 0, baseSubmissions = 0;
        ffxReturnCode_t callbackResult = FFX_API_RETURN_OK;
        FakeDispatch dispatch;
        ffxContext fgContext = nullptr;
        bool Fail(std::string_view text, HRESULT = E_FAIL) noexcept { fatal = true; failure = text; return false; }
        bool Check(ffxReturnCode_t result, std::string_view text) { return result == FFX_API_RETURN_OK || Fail(text); }
    };
};
}
struct Wake { uint64_t completed; DWORD result; int64_t elapsed; };
FakeFence* activeFence = nullptr;
std::vector<Wake> wakes;
size_t wakeIndex = 0;
DWORD TestWaitForSingleObject(HANDLE, DWORD timeout) {
    Check(wakeIndex < wakes.size(), "bounded number of waits");
    const auto wake = wakes[wakeIndex++];
    Check(timeout > 0 && timeout <= 3000, "one total deadline, no renewed timeout");
    activeFence->completed = wake.completed;
    TestClock::milliseconds += wake.elapsed;
    return wake.result;
}
#define WaitForSingleObject TestWaitForSingleObject
'''

middle = r'''
#undef WaitForSingleObject
bool Commit(FSR3FGPresenter::Impl& impl, HRESULT hr, std::optional<uint32_t>& _lastSubmittedFrameCount) {
    std::optional<uint32_t> _lastPresentedFrameCount = 999;
    std::chrono::steady_clock::time_point _lastSubmissionTime;
''' + commit + r'''
    Check(!_lastPresentedFrameCount, "SDK-presented count remains unknown");
    return true;
}
void BeginSubmission(FSR3FGPresenter::Impl& impl, std::optional<uint32_t>& count) {
    count.reset();
''' + reset + r'''
}
struct ConsumerViews {
    FrameGuidanceView produced;
    FrameGuidanceAvailability motionAvailability = FrameGuidanceAvailability::Unavailable;
};
using FrameGuidanceConsumerViews = ConsumerViews;
struct Provider { bool ready; unsigned consecutiveFailures; };
ConsumerViews Finish(ConsumerViews view, bool ready, unsigned failures) {
    const auto request = MotionVectorRequest::Amd(AmdOpticalFlowMode::Quality);
    std::vector<Provider> _providers{{ready, failures}};
    auto it = _providers.begin();
    const uint64_t frameId = 1;
    const FrameGuidanceExtent targetExtent{7, 5};
''' + finish + r'''
    return finish(view);
}
struct Publication {
    bool proxy = true, software = false;
    uint64_t _backendOutputHistoryRevision = 0, _lastPublishedOutputHistoryRevision = 0;
    bool _HasProxyFrameGenerator() const { return proxy; }
    bool _HasSoftwareFrameGenerator() const { return software; }
    bool Publish(bool isNewCaptureFrame) {
''' + guard + r'''
        _lastPublishedOutputHistoryRevision = _backendOutputHistoryRevision;
        return true;
    }
};
struct MockPresenter { void* swapChain = nullptr; };
struct MockPacing { bool vsync; bool tearingSupported; };
bool isExclusiveFullscreen(void*) { return false; }
UINT SDKFlags(MockPresenter* presenter, MockPacing* pacingEntry) {
''' + flags + r'''
    return uFlags;
}
'''

tests = r'''
int main() {
    FakeFence fence;
    FakeDevice device;
    FSR3FGPresenter::Impl impl{&fence, &device, {}};
    activeFence = &fence;
    auto resetFence = [&](std::vector<Wake> events) {
        fence = {}; impl.fatal = false; impl.failure = {};
        TestClock::milliseconds = 0; wakes = std::move(events); wakeIndex = 0;
    };
    resetFence({{42, WAIT_OBJECT_0, 1}});
    Check(WaitForFence(impl, 42) && !impl.fatal, "normal target completes");
    resetFence({{41, WAIT_OBJECT_0, 1}, {42, WAIT_OBJECT_0, 100}});
    Check(WaitForFence(impl, 42) && wakeIndex == 2 && fence.registrations == 1, "stale event does not authorize allocator reuse");
    resetFence({{UINT64_MAX, WAIT_OBJECT_0, 1}});
    Check(!WaitForFence(impl, 42) && impl.fatal, "device removal during wait fails");
    resetFence({{40, WAIT_TIMEOUT, 3000}});
    Check(!WaitForFence(impl, 41) && impl.fatal, "timeout never implies completion");
    impl.fatal = false; wakes = {{41, WAIT_OBJECT_0, 1}, {42, WAIT_OBJECT_0, 1}}; wakeIndex = 0;
    Check(WaitForFence(impl, 42) && wakeIndex == 2, "drain after timeout still waits for its own target");
    resetFence({{1, WAIT_OBJECT_0, 2900}, {2, WAIT_TIMEOUT, 100}});
    Check(!WaitForFence(impl, 42) && TestClock::milliseconds == 3000, "spurious wakes cannot extend deadline");
    resetFence({}); fence.registration = E_FAIL;
    Check(!WaitForFence(impl, 42), "event registration failure fails closed");
    resetFence({{0, WAIT_FAILED, 1}});
    Check(!WaitForFence(impl, 42), "OS wait failure fails closed");

    resetFence({});
    std::optional<uint32_t> count;
    auto submit = [&](bool reset, uint64_t id, ffxReturnCode_t result, HRESULT present) {
        BeginSubmission(impl, count);
        ffxDispatchDescFrameGeneration desc{}; desc.frameID = id; desc.numGeneratedFrames = 1; desc.reset = reset;
        impl.dispatch.result = result;
        GenerateFrame(&desc, &impl);
        return Commit(impl, present, count);
    };
    Check(submit(false, 42, FFX_API_RETURN_OK, S_OK) && count == 2u, "successful callback plus present reports 2 submissions");
    Check(submit(true, 42, FFX_API_RETURN_OK, S_OK) && count == 1u, "SDK reset reports real only");
    impl.decision.reset = uint32_t(FSR3FGReset::OutputHistory);
    Check(submit(false, 42, FFX_API_RETURN_OK, S_OK) && count == 1u, "host output revision reset excludes generated copy");
    impl.decision.reset = 0;
    Check(!submit(false, 41, FFX_API_RETURN_OK, S_OK) && !count, "wrong callback ID cannot reuse previous count");
    Check(!submit(false, 42, FFX_API_RETURN_ERROR_PARAMETER, S_OK) && !count, "callback failure reports no successful count");
    Check(!submit(false, 42, FFX_API_RETURN_OK, E_FAIL) && !count, "present failure cannot publish callback count");
    BeginSubmission(impl, count);
    Check(Commit(impl, S_OK, count) && !count, "missing callback remains unknown");
    BeginSubmission(impl, count); impl.idlePresentation = true;
    Check(Commit(impl, S_OK, count) && !count && impl.geometryReset, "idle publishes no content and resets resume history");

    ConsumerViews view{};
    for (auto* resource : {&view.produced.depth, &view.produced.motion, &view.produced.confidence}) {
        resource->texture = reinterpret_cast<ID3D11Texture2D*>(uintptr_t(1));
        resource->metadata.frameId = 1; resource->metadata.valid = true;
        resource->metadata.sourceExtent = {7,5}; resource->metadata.validRegion = {0,0,7,5};
    }
    view.produced.depth.format = DXGI_FORMAT_R32_FLOAT;
    view.produced.motion.format = DXGI_FORMAT_R16G16_FLOAT;
    view.produced.confidence.format = DXGI_FORMAT_R8_UNORM;
    Check(Finish(view, true, 0).motionAvailability == FrameGuidanceAvailability::Valid, "valid motion remains valid even if vectors happen to be zero");
    view.produced.motion.metadata.isZero = true;
    view.produced.motion.metadata.requiresHistoryReset = true;
    view.produced.motion.metadata.resetReason = FrameGuidanceResetReason::Initialize;
    Check(Finish(view, true, 0).motionAvailability == FrameGuidanceAvailability::ResetOnly, "real provider bootstrap is classified separately");
    for (unsigned failures : {1u, 2u, 3u})
        Check(Finish(view, true, failures).motionAvailability == FrameGuidanceAvailability::ProviderFailed, "fallback cannot disguise a runtime failure as bootstrap");
    Check(Finish(view, false, 3).motionAvailability == FrameGuidanceAvailability::ProviderStopped, "stopped provider persists across later frames");
    Check(Finish(view, false, 0).motionAvailability == FrameGuidanceAvailability::ProviderStopped, "resize failure remains explicit without failure counter");
    Check(Finish({}, true, 0).motionAvailability == FrameGuidanceAvailability::Unavailable, "invalid adaptation does not claim valid motion");

    EffectFrameState effect;
    EffectFrameKey key{.frameId=1, .parameterRevision=0, .captureSequence=1, .resourceGeneration=1};
    effect.Commit(key, true);
    Publication publication;
    publication._backendOutputHistoryRevision = effect.OutputHistoryRevision();
    Check(publication.Publish(true), "real capture publishes");
    Check(!publication.Publish(false), "unchanged stale capture remains suppressed");
    effect.ParametersChanged(); key.parameterRevision = effect.ParameterRevision(); effect.Commit(key, true);
    publication._backendOutputHistoryRevision = effect.OutputHistoryRevision();
    Check(publication.Publish(false), "live edit publishes immediately without new capture");
    Check(!publication.Publish(false), "same edited output is not republished indefinitely");
    publication.proxy = false; publication.software = true; ++publication._backendOutputHistoryRevision;
    Check(!publication.Publish(false), "software FG stale input behavior unchanged");

    for (bool requested : {false, true}) for (bool supported : {false, true})
        for (auto environment : {VRRUnavailableReason::None, VRRUnavailableReason::CrossAdapter,
            VRRUnavailableReason::StaleDisplayTopology, VRRUnavailableReason::SpanningOutputs}) {
            VRRPresentationPolicy policy;
            policy.Configure(requested, supported); policy.PathAvailable(true);
            policy.Environment(VRRUnavailableReason::None); policy.AltEnterDisabled(true);
            policy.Created(policy.CreationFlags()); policy.Environment(environment);
            MockPresenter proxy;
            MockPacing pacing{FSR3FGSyncInterval(policy.PresentFlags() != 0) != 0, supported};
            Check(bool(SDKFlags(&proxy, &pacing) & DXGI_PRESENT_ALLOW_TEARING) == bool(policy.PresentFlags()),
                "pinned SDK worker flags honor host eligibility for real and idle submissions");
        }
    std::printf("FSR3FG review regression: %u checks passed; simulated API/queue behavior, no display acceptance.\n", checks);
}
'''
args.output.mkdir(parents=True, exist_ok=True)
(args.output / 'FSR3FGReviewTests.cpp').write_text(prefix + wait + '\n' + callback + middle + tests, encoding='utf-8')
(args.output / 'extraction-manifest.json').write_text(json.dumps([
    {'path': str(p), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths
], indent=2), encoding='utf-8')
print(args.output / 'FSR3FGReviewTests.cpp')
