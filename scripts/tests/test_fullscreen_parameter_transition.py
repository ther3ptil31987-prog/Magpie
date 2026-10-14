from pathlib import Path

import sys
root = Path(__file__).resolve().parents[2]
core = root / 'src/Magpie.Core'
tracker = (core / 'SrcTracker.cpp').read_text(encoding='utf-8-sig')
scaling = (core / 'ScalingWindow.cpp').read_text(encoding='utf-8-sig')

def method(s, signature):
    start = s.index(signature)
    left = s.index('{', start)
    end, depth = left + 1, 1
    while depth:
        depth += (s[end] == '{') - (s[end] == '}')
        end += 1
    return s[start:end]

prefix = r'''
#define UNICODE
#define NOMINMAX
#include <windows.h>
#include "FramePresentationTiming.h"
using Magpie::PresentationJobTiming;
#include <algorithm>
#include <cassert>
#include <functional>
#include <deque>
#include <utility>
#include <iostream>
#include <vector>
static HWND source=(HWND)1, scaling=(HWND)2, host=(HWND)3, foreground=source;
static HWND sourceRoot=source;
static RECT currentRect{100,100,900,700};
static bool visible=true, valid=true, held=false, settling=false;
static UINT showCommand=SW_SHOWNORMAL;
static int queued=0, stateLogs=0;
static std::vector<std::function<void()>> callbacks;
static BOOL FakeIsWindow(HWND) { return valid; }
static BOOL FakeVisible(HWND) { return visible; }
static HWND FakeForeground() { return foreground; }
static BOOL FakeRect(HWND, RECT* r) { *r=currentRect; return TRUE; }
static BOOL FakePlacement(HWND, WINDOWPLACEMENT* p) {
    p->showCmd=showCommand; p->ptMinPosition={-32000,-32000}; p->rcNormalPosition=currentRect; return TRUE;
}
static BOOL FakeMonitor(HMONITOR, MONITORINFO* p) { p->rcMonitor=p->rcWork={0,0,1920,1080}; return TRUE; }
#define IsWindow FakeIsWindow
#define IsWindowVisible FakeVisible
#define IsIconic(...) (showCommand==SW_SHOWMINIMIZED)
#define GetForegroundWindow FakeForeground
#define GetWindowRect FakeRect
#define GetWindowPlacement FakePlacement
#define GetMonitorInfoW FakeMonitor
#define MonitorFromWindow(...) HMONITOR(1)
#define GetSystemMetrics(...) 0
#define GetAsyncKeyState(...) 0
#define GetWindowThreadProcessId(...) 1
#define GetWindow(hwnd, relation) ((relation)==GW_OWNER && (hwnd)==host ? scaling : nullptr)
#define GetGUIThreadInfo(...) FALSE
#define SetWindowPos(...) TRUE
#define PostMessageW(...) TRUE
#include "SourceWindowRelations.h"
bool operator==(const RECT& a,const RECT& b) { return a.left==b.left && a.top==b.top && a.right==b.right && a.bottom==b.bottom; }
bool operator==(const SIZE& a,const SIZE& b) { return a.cx==b.cx && a.cy==b.cy; }
constexpr int SWP_NO_ACTIVATE_MOVE_SIZE=0;
constexpr UINT WM_MAGPIE_SCALINGCHANGED=WM_USER;
static bool IsTopmostWindow(HWND) { return false; }
namespace FrameTrace { enum class Event { FrontendPrepare }; struct Scope { Scope(Event) {} }; }
namespace fmt { template<class... T> const char* format(const char* message,T&&...) { return message; } }
namespace Magpie {
struct Logger {
    static Logger& Get() { static Logger x; return x; }
    void Info(const char*) { ++stateLogs; }
    void Error(const char*) {}
    void Win32Error(const char*) {}
};
struct Win32Helper {
    static HWND GetRealForegroundWindow() { return FakeForeground(); }
    template<class T> static T* LoadSystemFunction(const wchar_t*,const char*) { return nullptr; }
    static bool GetWindowFrameRect(HWND,RECT& r) { r={0,0,1000,1000};return true; }
    static bool IntersectRect(RECT& out,const RECT& a,const RECT& b) { return ::IntersectRect(&out,&a,&b); }
    static bool IsWindowHung(HWND) { return false; }
    static SIZE GetSizeOfRect(const RECT& r) { return {r.right-r.left,r.bottom-r.top}; }
    static void OffsetRect(RECT& r,LONG x,LONG y) { r.left+=x;r.right+=x;r.top+=y;r.bottom+=y; }
};
struct WindowHelper { static bool IsForbiddenSystemWindow(HWND) { return false; } };
struct NvidiaForegroundCompatibility {
    static bool ReferenceOwnsForeground() { return false; }
    static void FrontendSuspended(bool) {}
	static void FrontendWindows(HWND, HWND) {}
};
struct Options { SessionWindowIdentity mainUI; bool windowed=false, game3d=false; bool IsWindowedMode() const { return windowed; } bool Is3DGameMode() const { return game3d; } bool IsTouchSupportEnabled() const { return false; } };
struct Cursor { void Update() {} void OnSrcStartMove() {} void OnSrcEndMove() {} void OnSrcRectChanged() {} };
enum class DLSSFGFrameRenderResult { Presented, Retry, Dropped };
struct Renderer {
    int consumed=0;
    bool IsParameterFocusSettling() const { return settling; }
    bool IsEditingParameters() const { return true; }
    HWND ParameterInputHandle() const { return host; }
    void UpdateParameterInputHost() {} bool AllowAutomaticSourceFocus() const { return false; }
    int focusHistoryResets=0;
    void OnSourceFocusChanged() { ++focusHistoryResets; } int CaptureOverlayState() { return 0; }
    DLSSFGFrameRenderResult RenderDLSSFGFrame(uint32_t,uint32_t,PresentationJobTiming&) { ++consumed;return DLSSFGFrameRenderResult::Presented; }
};
class SrcTracker {
public:
    HWND _hWnd=source;
    RECT _windowRect{100,100,900,700},_windowFrameRect=_windowRect,_srcRect=_windowRect;
    bool _isSessionForeground=true,_isSourceForeground=false,_isMoving=false,_isMaximized=false;
    HWND Handle() const { return _hWnd; }
    bool SetFocus(HWND=nullptr) const { foreground=sourceRoot;return true; }
    bool UpdateForeground(HWND) noexcept;
    const RECT& WindowRect() const { return _windowRect; }
    const RECT& SrcRect() const { return _srcRect; }
    bool IsMoving() const { return _isMoving; }
    bool UpdateState(HWND,bool,bool,bool&,bool&,bool&,bool&,bool&) noexcept;
};
class ScalingWindow {
public:
    static ScalingWindow& Get() { static ScalingWindow x;return x; }
    static uint32_t RunId() { return 1; }
    struct Coordinator { bool Ready() const { return valid; } } _sourceCoordinator;
    struct Dispatcher { template<class F> void TryEnqueue(F&& f) const { ++queued;callbacks.emplace_back(std::forward<F>(f)); } } _dispatcher;
    Options _options;
    SrcTracker _srcTracker;
    Renderer renderer; Renderer* _renderer=&renderer;
    Cursor cursor; Cursor* _cursorManager=&cursor;
    RECT _rendererRect{0,0,1920,1080},_windowRect{0,0,1920,1080};
    bool _isResizingOrMoving=false,_isMovingDueToSrcMoved=false,_isSrcRepositioning=false;
    bool _isFirstFrame=false;
    LONG _lastWindowedRendererWidth=0;
    int _repositionOverlayState=0, destroyed=0;
    mutable uint8_t _pendingSourceTransition=0;
    bool _sourceStateCheckDeferred=false;
    const char* _sourceStateChangeReason="unspecified";
    RECT _sourceRectBeforeCheck{};
    bool HasPendingSourceTransition() const { return _pendingSourceTransition!=0; }
    bool ProcessPendingSourceTransition() noexcept;
    struct DLSSFGFrameJob { uint32_t sharedTextureSlot=0, sharedTextureGeneration=0; PresentationJobTiming timing; };
    std::deque<DLSSFGFrameJob> _dlssFgFrameJobs;
    HWND Handle() const { return scaling; }
    const class SrcTracker& SrcTracker() const { return _srcTracker; }
    HWND SourceRoot() const { return sourceRoot; }
    bool SourceIdentityValid() const { return valid; }
    SourceForegroundKind ClassifyForeground(HWND) const noexcept;
    bool IsSessionForeground(HWND) const noexcept;
    Renderer* TryGetRenderer() { return _renderer; }
    bool IsParameterInputWindow(HWND h) const { return h==host; }
    bool HasHeldParameterInput() const { return held; }
    bool _CheckForegroundFor3DGameMode(HWND) const noexcept;
    void _EnsureCaptionVisibleOnScreen() {}
    void _UpdateTouchProps(const RECT&) {}
    void _UpdateFocusStateAsync() {}
    void Destroy() { ++destroyed;_dlssFgFrameJobs.clear();_pendingSourceTransition=0; }
    void Stop() { if(!held) Destroy(); }
    bool _UpdateSrcState(bool&,bool&) noexcept;
    bool _PrepareFrontendRender() noexcept;
    bool RenderNextDLSSFGFrame() noexcept;
    void _CompleteFrontendRender(bool,bool) {}
    void _DelayedStop(bool=false,bool=false) const noexcept;
};
'''

production = '\n'.join([
    method(tracker, 'static bool IsWindowMoving('),
    method(tracker, 'static bool IsPrimaryMouseButtonDown('),
    method(tracker, 'bool SrcTracker::UpdateForeground('),
    method(tracker, 'bool SrcTracker::UpdateState('),
    method(scaling, 'SourceForegroundKind ScalingWindow::ClassifyForeground('),
    method(scaling, 'bool ScalingWindow::IsSessionForeground('),
    method(scaling, 'bool ScalingWindow::_CheckForegroundFor3DGameMode('),
    method(scaling, 'bool ScalingWindow::_UpdateSrcState('),
    method(scaling, 'bool ScalingWindow::_PrepareFrontendRender('),
    method(scaling, 'void ScalingWindow::_DelayedStop('),
    method(scaling, 'bool ScalingWindow::ProcessPendingSourceTransition('),
    method(scaling, 'bool ScalingWindow::RenderNextDLSSFGFrame('),
])

tests = r'''
}
using namespace Magpie;
static void reset() {
    auto& w=ScalingWindow::Get(); w._srcTracker=SrcTracker{};w._options.windowed=w._options.game3d=false;
    w._isSrcRepositioning=false;w.destroyed=0;
    foreground=host;currentRect={100,100,900,700};visible=valid=true;held=false;showCommand=SW_SHOWNORMAL;
    callbacks.clear();queued=stateLogs=0;settling=false;w._pendingSourceTransition=0;w._sourceStateCheckDeferred=false;
    w._dlssFgFrameJobs.clear();w.renderer.consumed=w.renderer.focusHistoryResets=0;sourceRoot=source;
}
static bool check(bool& reposition) { bool focus=false; return ScalingWindow::Get()._UpdateSrcState(reposition,focus); }
int main() {
    bool reposition=false;
    reset(); auto& focusWindow=ScalingWindow::Get();focusWindow._options.game3d=true;
    foreground=scaling;settling=true;assert(check(reposition));
    foreground=(HWND)4;assert(!check(reposition));
    foreground=scaling;settling=false;assert(!check(reposition));
    foreground=host;assert(check(reposition));
    sourceRoot=(HWND)5;foreground=sourceRoot;assert(check(reposition));
    std::cout<<"PASS: 3D focus handoff accepts only the settling scaling window; external foreground still stops.\n";
    reset(); assert(check(reposition) && !reposition);
    foreground=source; assert(ScalingWindow::Get()._PrepareFrontendRender());
    foreground=host; assert(ScalingWindow::Get()._PrepareFrontendRender());
    assert(ScalingWindow::Get().renderer.focusHistoryResets==0);
    std::cout<<"PASS: actual source/root to parameter-host transfers keep logical session focus and do not reset content history.\n";
    std::cout<<"PASS: internal parameter focus with unchanged source geometry keeps fullscreen active.\n";
    reset(); ++currentRect.left;++currentRect.right;reposition=false;
    assert(!check(reposition) && reposition);
    std::cout<<"PASS: moving the source by one pixel requests fullscreen teardown/reposition.\n";
    reset();ScalingWindow::Get()._options.windowed=true;++currentRect.left;++currentRect.right;reposition=false;
    assert(check(reposition) && !reposition);
    std::cout<<"PASS: the same source movement is accepted in windowed scaling.\n";
    reset();showCommand=SW_SHOWMINIMIZED;reposition=false;
    assert(!check(reposition) && reposition);
    std::cout<<"PASS: source minimization requests teardown even with internal parameter focus.\n";
    reset();++currentRect.right;held=true;
    auto& w=ScalingWindow::Get();
    for(int i=0;i<1000;++i) {
        assert(!w._PrepareFrontendRender());
        assert(!w.ProcessPendingSourceTransition());
    }
    assert(queued==0 && stateLogs==1 && w.destroyed==0 && w.HasPendingSourceTransition());
    held=false;assert(w.ProcessPendingSourceTransition() && w.destroyed==1);
    std::cout<<"PASS: 1000 held-input retries retain one request and one log; release commits reposition.\n";
    reset();held=true;++currentRect.left;++currentRect.right;
    w._dlssFgFrameJobs.push_back({0,1});
    assert(!w.RenderNextDLSSFGFrame() && w._dlssFgFrameJobs.size()==1 && w.renderer.consumed==0);
    assert(!w.ProcessPendingSourceTransition());
    held=false;reposition=false;assert(check(reposition) && !reposition);
    assert(w.HasPendingSourceTransition() && w._dlssFgFrameJobs.size()==1);
    assert(w.ProcessPendingSourceTransition() && w.destroyed==1 && w._dlssFgFrameJobs.empty());
    std::cout<<"PASS: temporary source movement cannot lose the pending rebuild or orphan FG jobs while input is held.\n";
    reset();settling=true;++currentRect.right;w._dlssFgFrameJobs.push_back({0,1});
    assert(!w.RenderNextDLSSFGFrame() && w._sourceStateCheckDeferred && !w.HasPendingSourceTransition());
    assert(w._srcTracker.WindowRect().right==900 && w._dlssFgFrameJobs.size()==1 && stateLogs==0);
    currentRect.right=900;settling=false;
    assert(w.RenderNextDLSSFGFrame() && w.renderer.consumed==1 && !w.HasPendingSourceTransition());
    std::cout<<"PASS: a temporary focus-transition size change leaves the source baseline and FG queue intact.\n";
    reset();settling=true;++currentRect.right;assert(!w._PrepareFrontendRender());
    settling=false;assert(!w._PrepareFrontendRender() && w.HasPendingSourceTransition());
    assert(w.ProcessPendingSourceTransition() && w.destroyed==1);
    reset();settling=true;valid=false;assert(!w._PrepareFrontendRender() && w._pendingSourceTransition==2);
    assert(w.ProcessPendingSourceTransition() && !w._isSrcRepositioning);
    reset();w._DelayedStop(false,true);w._DelayedStop(false,false);w._DelayedStop(false,true);
    assert(w._pendingSourceTransition==2 && w.ProcessPendingSourceTransition() && !w._isSrcRepositioning);
    std::cout<<"PASS: persistent geometry changes still rebuild; source destruction and explicit stop take precedence. No native UI operations.\n";

}
'''
out=Path(sys.argv[1]).resolve() / 'source_state_paths.cpp'
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(prefix+production+tests,encoding='utf-8')
print(out)
