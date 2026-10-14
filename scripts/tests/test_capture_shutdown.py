"""Generate an MSVC CPU harness from the actual capture-error callback and ShowError.

Run with Python and an output directory, then compile the generated C++ with
cl /std:c++20 /EHsc /I src/Magpie.Core/include and run the executable.
The Windows dispatcher/capture objects are fakes; callback bodies are extracted
unchanged from production code so a missing guard fails the regression cases.
"""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
core = root / 'src/Magpie.Core'
renderer = (core / 'Renderer.cpp').read_text(encoding='utf-8-sig')
window = (core / 'ScalingWindow.h').read_text(encoding='utf-8-sig')
teardown = (core / 'ScalingWindow.cpp').read_text(encoding='utf-8-sig')
capture = (core / 'GraphicsCaptureFrameSource.cpp').read_text(encoding='utf-8-sig')

error = renderer.split('case FrameSourceState::Error:', 1)[1]
start = error.index('TryEnqueue(') + len('TryEnqueue(')
end = error.index('\n\t\t\t});', start)
callback = error[start:end] + '\n}'
start = window.index('void ShowError(')
end = window.index('\n\t}', start) + len('\n\t}')
show_error = window[start:end]

# Integration ordering not exercised by the fake dispatcher.
assert teardown.index('++_runId') < teardown.index('ClearOverlayStates')
assert teardown.index('BeginShutdown') < teardown.index('_cursorManager.reset()')
assert teardown.index('void ScalingWindow::_FinishDestroy') < teardown.index('_cursorManager.reset()')
assert 'std::make_shared<ScalingSessionLifetime>(ScalingWindow::RunId())' in renderer
assert 'session = _sessionLifetime' in callback
assert capture.index('if (_captureStopping) return FrameSourceState::Waiting;') < capture.index('if (!_captureSession || !_captureFramePool)')
cursor = capture.split('void GraphicsCaptureFrameSource::OnCursorVisibilityChanged', 1)[1].split('static bool CalcWindow', 1)[0]
assert cursor.index('_captureStopping = true') < cursor.index('_StopCapture()')
backend = renderer.split('void Renderer::_BackendThreadProc()', 1)[1].split('void Renderer::_UpdateFrameRateLimits()', 1)[0]
assert backend.index('IsStopping()', backend.index('traceMessages.End()')) < backend.index('_frameSource->Update()')
assert backend.index('IsStopping()', backend.index('_frameSource->Update()')) < backend.index('switch (frameSourceState)')

harness = r'''
#include "ScalingSessionLifetime.h"
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
using namespace Magpie;
enum class ScalingError { CaptureFailed };
using HWND = int;
struct OptionsState {
    std::function<void(HWND, ScalingError)> showError;
    std::function<void(HWND, ScalingError, std::string_view, uint32_t)> reportErrorDetails;
};
struct SourceTracker { HWND Handle() const { return 7; } };
struct ScalingWindow {
    static inline uint32_t runId = 10;
    static ScalingWindow& Get() { static ScalingWindow w; return w; }
    static uint32_t RunId() { return runId; }
    OptionsState _options;
    SourceTracker _srcTracker;
    bool open = true;
    int stops = 0, optionReads = 0;
    explicit operator bool() const { return open; }
    const OptionsState& Options() { ++optionReads; return _options; }
    const SourceTracker& SrcTracker() { return _srcTracker; }
    void Stop() { ++stops; open = false; ++runId; _options = {}; }
    SHOW_ERROR
};
struct FrameSource {
    const char* CaptureErrorContext() { return "test capture error"; }
    int CaptureErrorCode() { return 123; }
};
struct Renderer {
    std::shared_ptr<ScalingSessionLifetime> _sessionLifetime =
        std::make_shared<ScalingSessionLifetime>(ScalingWindow::RunId());
    std::unique_ptr<FrameSource> _frameSource = std::make_unique<FrameSource>();
    std::function<void()> ErrorCallback() { return CALLBACK; }
};
int shown = 0, reported = 0;
void show(HWND h, ScalingError) noexcept { assert(h == 7); ++shown; }
void report(HWND h, ScalingError, std::string_view context, uint32_t code) noexcept {
    assert(h == 7 && context == "test capture error" && code == 123); ++reported;
}
void reset() { ScalingWindow::Get() = {}; shown = reported = 0; }
int main() {
    auto& window = ScalingWindow::Get();
    // Empty fallback is safe independently of the session guard.
    window.ShowError(ScalingError::CaptureFailed);
    assert(shown == 0);
    window._options.showError = show;
    window.ShowError(ScalingError::CaptureFailed);
    assert(shown == 1);
    // A stateful report keeps its snapshot alive if options are cleared in flight.
    {
        reset();
        auto snapshot = std::make_shared<std::string>("original source profile");
        std::weak_ptr<std::string> weakSnapshot = snapshot;
        window._options.showError = [snapshot](HWND h, ScalingError) {
            ScalingWindow::Get()._options = {};
            assert(h == 7 && *snapshot == "original source profile");
            ++shown;
        };
        snapshot.reset();
        window.ShowError(ScalingError::CaptureFailed);
        assert(shown == 1 && weakSnapshot.expired());
    }
    // Current errors still report and stop normally, including fallback.
    reset(); Renderer active; window._options.reportErrorDetails = report;
    active.ErrorCallback()(); assert(reported == 1 && window.stops == 1);
    reset(); Renderer fallback; window._options.showError = show;
    fallback.ErrorCallback()(); assert(shown == 1 && window.stops == 1);
    // A live session with optional callbacks absent may still stop safely.
    reset(); Renderer noHandlers; noHandlers.ErrorCallback()(); assert(window.stops == 1);
    // Stop begins before the global ID changes or options are cleared.
    reset(); Renderer stopping; auto pending = stopping.ErrorCallback();
    stopping._sessionLifetime->RequestStop(); pending();
    assert(window.stops == 0 && window.optionReads == 0);
    // The original renderer and frame source can be gone before dispatch.
    reset(); std::weak_ptr<ScalingSessionLifetime> weak;
    { Renderer old; weak = old._sessionLifetime; pending = old.ErrorCallback(); old._sessionLifetime->RequestStop(); }
    assert(!weak.expired()); window.open = false; ++ScalingWindow::runId;
    pending(); assert(window.stops == 0 && window.optionReads == 0);
    pending = {}; assert(weak.expired());
    // No handle, even if generation still matches: do not read callbacks.
    reset(); Renderer noWindow; window.open = false; noWindow.ErrorCallback()();
    assert(window.stops == 0 && window.optionReads == 0);
    // Error produced after stop must retain its creation-time generation.
    reset(); Renderer lateProducer; ++ScalingWindow::runId;
    lateProducer.ErrorCallback()(); assert(window.stops == 0 && window.optionReads == 0);
    // A late old callback must never stop a successor, including reused IDs.
    for (int i = 0; i < 1000; ++i) {
        reset(); Renderer old; pending = old.ErrorCallback();
        old._sessionLifetime->RequestStop(); ++ScalingWindow::runId;
        Renderer successor; window._options.reportErrorDetails = report;
        pending(); assert(window.open && window.stops == 0 && reported == 0);
        successor.ErrorCallback()(); assert(window.stops == 1 && reported == 1);
    }
    reset(); Renderer reused; pending = reused.ErrorCallback();
    reused._sessionLifetime->RequestStop(); Renderer sameId;
    assert(sameId._sessionLifetime->IsCurrent(ScalingWindow::runId));
    pending(); assert(window.stops == 0);
    // Cancellation publication is observed by the backend thread.
    ScalingSessionLifetime shared(42);
    std::atomic<bool> started = false;
    std::thread backend([&] {
        started.store(true);
        while (!shared.IsStopping()) std::this_thread::yield();
        assert(!shared.IsCurrent(42));
    });
    while (!started.load()) std::this_thread::yield();
    shared.RequestStop(); backend.join();
    std::cout << "Capture shutdown: 12 scenarios + 1000 restart cycles passed; 8 integration ordering checks passed.\n";
}
'''.replace('SHOW_ERROR', show_error).replace('CALLBACK', callback)

output = Path(sys.argv[1]).resolve()
output.mkdir(parents=True, exist_ok=True)
(output / 'capture_shutdown.cpp').write_text(harness, encoding='utf-8')
print(output / 'capture_shutdown.cpp')
