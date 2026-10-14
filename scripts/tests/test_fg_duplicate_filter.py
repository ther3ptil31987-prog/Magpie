"""Generate a CPU regression from the production capture duplicate-filter branch.

Usage: python scripts/tests/test_fg_duplicate_filter.py OUTPUT
Compile OUTPUT/fg_duplicate.cpp with cl /std:c++20 /EHsc, then run it.
GPU comparisons are faked; filter selection and sequence handling are production code.
"""
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
source = (root / 'src/Magpie.Core/FrameSourceBase.cpp').read_text(encoding='utf-8-sig')
start = source.index('FrameSourceState FrameSourceBase::_FilterDuplicateFrame(')
branch = source[source.index('{', start) + 1:source.index('\nColorDescription FrameSourceBase::')]
branch = branch[:branch.rindex('\n}')]
header = (root / 'src/Magpie.Core/FrameSourceBase.h').read_text(encoding='utf-8-sig')
reasons = re.search(r'enum class CaptureFrameReason.*?\n};', header, re.S)[0]

for family, effect in [('DLSSFG', 'DLSS_FrameGeneration'), ('XeSSFG', 'XeSS_FrameGeneration'),
                       ('FSR3FG', 'FSR3_FrameGeneration'), ('NvidiaVFX', 'VFG')]:
    shader = (root / f'src/Effects/{family}/{effect}.hlsl').read_text(encoding='utf-8-sig')
    assert 'duplicateFrameFiltering' not in shader
    for language in ['en-US', 'zh-Hans', 'zh-Hant']:
        resource = ET.parse(root / f'src/Magpie/Resources.language-{language}.resw')
        key = f'EffectParam_{family}_{effect}_duplicateFrameFiltering_Label'
        assert not resource.findall(f'./data[@name="{key}"]')
assert '_duplicateFrameDetectionOverride' not in source + header

harness = r'''
#include <optional>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <cstdlib>
enum class FrameSourceState { NewFrame, Waiting, Error };
enum class DuplicateFrameDetectionMode { Always, Dynamic, Never };
REASONS
struct ScalingOptions {
    DuplicateFrameDetectionMode duplicateFrameDetectionMode = DuplicateFrameDetectionMode::Dynamic;
    bool game = false, statistics = false;
    bool Is3DGameMode() const { return game; }
    bool IsStatisticsForDynamicDetectionEnabled() const { return statistics; }
};
struct ScalingWindow {
    ScalingOptions options;
    static ScalingWindow& Get() { static ScalingWindow window; return window; }
    const ScalingOptions& Options() { return options; }
};
struct Resource {
    bool valid = false;
    void* get() { return nullptr; }
    explicit operator bool() const { return valid; }
    void operator=(std::nullptr_t) { valid = false; }
};
struct ID3D11DeviceContext4 {
    void CopyResource(void*, void*) { ++copies; }
    int copies = 0;
};
struct Device {
    ID3D11DeviceContext4 context;
    ID3D11DeviceContext4* GetD3DDC() { return &context; }
};
struct Logger {
    static Logger& Get() { static Logger logger; return logger; }
    void Error(const char*) {}
};
constexpr uint16_t INITIAL_CHECK_COUNT=16, INITIAL_SKIP_COUNT=1, MAX_SKIP_COUNT=16;
struct Capture {
    CaptureFrameReason _lastUpdateReason = CaptureFrameReason::Accepted;
    bool _duplicateComparisonFailed = false, initFailed = false, mapFailed = false;
    uint64_t _duplicateCaptureSequence=0, _captureSequence=1;
    bool _isCheckingForDuplicateFrame=true;
    uint32_t _framesLeft=16, _nextSkipCount=1;
    Resource _prevFrame, _prevFrameSrv, _output;
    Device device;
    Device* _deviceResources=&device;
    std::atomic<std::pair<uint32_t,uint32_t>> _statistics{{0,0}};
    int checks=0, initializations=0;
    bool duplicate=true;
    void _ResetDuplicateDetection() { _prevFrame=nullptr; _prevFrameSrv=nullptr; _isCheckingForDuplicateFrame=true; _framesLeft=16; _nextSkipCount=1; }
    bool _InitCheckingForDuplicateFrame() { ++initializations; _prevFrame.valid=true; return !initFailed; }
    bool _IsDuplicateFrame() { ++checks; _duplicateComparisonFailed=mapFailed; return !mapFailed && duplicate; }
    FrameSourceState Feed(FrameSourceState state=FrameSourceState::NewFrame, bool forceAccept=false) {
BRANCH
    }

};
static void Check(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    using State = FrameSourceState;
    auto& options=ScalingWindow::Get().options;
    for (auto mode : {DuplicateFrameDetectionMode::Always, DuplicateFrameDetectionMode::Dynamic, DuplicateFrameDetectionMode::Never}) {
        for (bool game : {false,true}) {
            options.duplicateFrameDetectionMode=mode; options.game=game;
            Capture normal;
            Check(normal.Feed()==State::NewFrame,"common first input was lost");
            const bool globalFilters=!game && mode!=DuplicateFrameDetectionMode::Never;
            Check(normal.Feed()==(globalFilters ? State::Waiting : State::NewFrame),"common global policy changed");
            if (globalFilters) {
                normal.duplicate=false;
                Check(normal.Feed()==State::NewFrame,"common policy lost changed pixels");
                normal.duplicate=true; ++normal._captureSequence;
                Check(normal.Feed()==State::NewFrame,"capture restart lost the first input");
                Check(normal.Feed()==State::Waiting,"common filtering did not resume after restart");
            }
            Check(normal.Feed(State::Waiting)==State::Waiting && normal.Feed(State::Error)==State::Error,
                "capture waiting/error state changed");
        }
    }
    options.game=false;
    options.duplicateFrameDetectionMode=DuplicateFrameDetectionMode::Always;
    Capture hdr;
    Check(hdr.Feed(State::NewFrame,true)==State::NewFrame && hdr.initializations==1,"HDR first frame failed to establish baseline");
    Check(hdr.Feed()==State::Waiting,"HDR duplicate after first conversion was accepted");
    const int before=hdr.checks;
    Check(hdr.Feed(State::NewFrame,true)==State::NewFrame && hdr.checks==before,"metadata/resource change was filtered");
    Check(hdr.Feed()==State::Waiting,"metadata update lost duplicate baseline");
    hdr.mapFailed=true;
    Check(hdr.Feed()==State::NewFrame && hdr._lastUpdateReason==CaptureFrameReason::ReadbackFailed,"readback failure reused a stale duplicate flag");
    hdr.mapFailed=false;
    Check(hdr.Feed()==State::Waiting,"comparison did not recover after readback failure");
    Capture unavailable; unavailable.initFailed=true;
    Check(unavailable.Feed()==State::NewFrame && !unavailable._prevFrame,"partial initialization poisoned future comparisons");
    unavailable.initFailed=false;
    Check(unavailable.Feed()==State::NewFrame && unavailable.Feed()==State::Waiting,"initialization retry lost first-frame semantics");
    options.duplicateFrameDetectionMode=DuplicateFrameDetectionMode::Dynamic;
    for (bool stats : {false,true}) {
        options.statistics=stats;
        Capture dynamic; dynamic.duplicate=false;
        Check(dynamic.Feed()==State::NewFrame,"Dynamic lost first frame");
        int run=0, maximum=0;
        for (int n=0;n<2000;++n) {
            Check(dynamic.Feed()==State::NewFrame,"Dynamic rejected changed input");
            if (dynamic._lastUpdateReason==CaptureFrameReason::DynamicSkip) { ++run; if (run>maximum) maximum=run; }
            else run=0;
        }
        Check(maximum==16,"Dynamic skip bound changed");
        dynamic.duplicate=true;
        bool filtered=false;
        for (int n=0;n<17;++n) if (dynamic.Feed()==State::Waiting) { filtered=true; break; }
        Check(filtered && dynamic._isCheckingForDuplicateFrame && dynamic._nextSkipCount==1,"Dynamic duplicate failed to restore checking");
        Check(dynamic.Feed()==State::Waiting,"Dynamic repeated duplicate advanced processing");
        ++dynamic._captureSequence;
        Check(dynamic.Feed()==State::NewFrame,"Dynamic recovery first frame was filtered");
    }
    std::cout << "Common FG/non-FG duplicate policy: global modes and game mode, changed pixels, metadata and capture restart passed.\n";
}
'''.replace('BRANCH', branch).replace('REASONS', reasons)
output = Path(sys.argv[1]).resolve()
output.mkdir(parents=True, exist_ok=True)
(output / 'fg_duplicate.cpp').write_text(harness, encoding='utf-8')
print(output / 'fg_duplicate.cpp')
