"""Extract production first-display and complete retirement paths for native tests."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[2]
window = (root / 'src/Magpie.Core/ScalingWindow.cpp').read_text(encoding='utf-8-sig')
renderer = (root / 'src/Magpie.Core/Renderer.cpp').read_text(encoding='utf-8-sig')
runtime = (root / 'src/Magpie.Core/ScalingRuntime.cpp').read_text(encoding='utf-8-sig')

def block(text, marker):
    start = text.index(marker)
    brace = text.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

startup = block(window, 'ScalingError ScalingWindow::_StartImpl(')
assert 'TrySetScalingWindowOwner(' not in startup
assert startup.count('\n\t\t\tnullptr,\n\t\t\tNULL,') == 2
cancel = block(renderer, 'void Renderer::BeginShutdown(')
assert '_reflex' not in cancel and '.join' not in cancel
assert 'RequestStop' in cancel and 'SetEvent' in cancel
begin = block(window, 'void ScalingWindow::_BeginDestroy(')
assert begin.index('BeginShutdown') < begin.index('TrySetScalingWindowOwner') < begin.index('ReleaseParameterInput')
assert 'result.IsApplied()' in begin
finish = block(window, 'void ScalingWindow::_FinishDestroy(')
toggle = block(window, 'void ScalingWindow::ToggleScaling(')
assert toggle.index('Destroy()') < toggle.index('_options.IsWindowedMode(isWindowedMode)') < toggle.index('RestartAfterSrcRepositioned()')
assert '_renderer.reset()' in finish and 'NgxRuntimeGuard::IsFaulted()' in finish
assert 'hWnd.reset(); // Unowned touch holes' in finish
assert finish.index('_renderer.reset()') < finish.index('parameterSession->Applied()') < finish.index('_options.save')
handler = block(window, 'LRESULT ScalingWindow::_MessageHandler(')
assert '_renderer.reset()' not in handler and '_cursorManager.reset()' not in handler
assert 'CompletePendingDestroy' not in handler and '_FinishDestroy' not in handler
assert '_BeginDestroy(Handle())' in handler and 'NonclientDestroyed()' in handler
complete = block(window, 'bool ScalingWindow::CompletePendingDestroy(')
assert complete.index('_retirement.Ready') < complete.index('_FinishDestroy')
destroy = block(window, 'void ScalingWindow::Destroy(')
assert destroy.index('_BeginDestroy') < destroy.index('DestroyWindow(Handle())') < destroy.rindex('CompletePendingDestroy')
assert runtime.index('PollSourceState()') < runtime.index('ProcessPendingStop()')
show = block(window, 'bool ScalingWindow::_Show(')
assert show.index('_EstablishWindowCoordination()') < show.index('SWP_SHOWWINDOW') < show.index('Broadcast only after')
first = block(window, 'void ScalingWindow::_CompleteFrontendRender(')
assert first.index('if (!_Show()) return') < first.index('ShowToast')
assert '250' in runtime[runtime.index('WaitForFrontendWork(std::chrono::milliseconds'):]

describe = block(window, 'static std::string DescribeScalingWindowOwner(')
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
start_report = block(window, 'void ScalingWindow::Start(')
start_report = start_report[start_report.index('\tLogger::DiagnosticCapture startupDiagnostic;'):]
(out / 'ScalingWindowOwnerReport.inc').write_text('void ReportStartup(HWND hwndSrc) {\n' + start_report, encoding='utf-8')
(out / 'ScalingWindowOwnerStartup.inc').write_text(describe + '\n' +
    block(window, 'bool ScalingWindow::_EstablishWindowCoordination(').replace('ScalingWindow::', '') + '\n' +
    first.replace('ScalingWindow::', '') + '\n' +
    block(window, 'void ScalingWindow::PollSourceState(').replace('ScalingWindow::', '') + '\n' +
    block(window, 'void ScalingWindow::_HideOutputForSourceTransition(').replace('ScalingWindow::', ''), encoding='utf-8')
(out / 'ScalingWindowLifecycle.inc').write_text(describe + '\n' +
    cancel.replace('Renderer::', 'TestRenderer::') + '\n' +
    block(renderer, 'Renderer::~Renderer(').replace('Renderer::~Renderer', 'TestRenderer::~TestRenderer') + '\n' +
    '\n'.join(block(window, marker).replace('ScalingWindow::', 'TestWindow::') for marker in [
    'void ScalingWindow::_BeginDestroy(', 'void ScalingWindow::Destroy(', 'void ScalingWindow::ToggleScaling(',
    'bool ScalingWindow::CompletePendingDestroy(', 'void ScalingWindow::_FinishDestroy(']), encoding='utf-8')
hook = block(renderer, 'LRESULT CALLBACK Renderer::_LowLevelKeyboardHook(')
assert 'session = renderer->_sessionLifetime' in hook
assert hook.index('session->IsCurrent(') < hook.index('Renderer& currentRenderer')
assert 'renderer->IsStopping()' in hook
print('Extracted production first-display, cancellation, destructor, retirement; complete entry checks passed.')
