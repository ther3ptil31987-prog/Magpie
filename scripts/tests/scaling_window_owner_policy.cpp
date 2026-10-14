#include <windows.h>
#include <cassert>
#include <cstdlib>
#include <functional>
#include <chrono>
#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <fmt/format.h>

enum class ApiMode { Native, Reject, AppliedDirtyError, Unexpected, DestroyOutput, DestroySource };
static ApiMode apiMode = ApiMode::Native;
static DWORD apiError = ERROR_INVALID_PARAMETER;
static HWND alternate;
static bool rejectPosition = false;
static BOOL PositionForTest(HWND window, HWND after, int x, int y, int cx, int cy, UINT flags) {
    if (rejectPosition) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    return SetWindowPos(window, after, x, y, cx, cy, flags);
}
static LONG_PTR SetOwnerForTest(HWND window, int index, LONG_PTR value) {
    if (index != GWLP_HWNDPARENT) return SetWindowLongPtrW(window, index, value);
    switch (apiMode) {
    case ApiMode::Reject: SetLastError(apiError); return 0;
    case ApiMode::AppliedDirtyError: {
        auto previous = SetWindowLongPtrW(window, index, value);
        SetLastError(ERROR_INVALID_WINDOW_HANDLE); return previous;
    }
    case ApiMode::Unexpected:
        SetWindowLongPtrW(window, index, reinterpret_cast<LONG_PTR>(alternate));
        SetLastError(ERROR_ACCESS_DENIED); return 0;
    case ApiMode::DestroyOutput:
        assert(DestroyWindow(window)); SetLastError(ERROR_INVALID_WINDOW_HANDLE); return 0;
    case ApiMode::DestroySource:
        assert(DestroyWindow(reinterpret_cast<HWND>(value))); SetLastError(ERROR_INVALID_WINDOW_HANDLE); return 0;
    default: return SetWindowLongPtrW(window, index, value);
    }
}
static HWND ReadOwnerForTest(HWND window, UINT relation) {
    auto value = GetWindow(window, relation);
    SetLastError(ERROR_NOT_ENOUGH_MEMORY); return value;
}
#define SetWindowLongPtrW SetOwnerForTest
#define GetWindow ReadOwnerForTest
static bool rejectPidQuery = false;
static DWORD ThreadForTest(HWND window, DWORD* pid) {
    if (pid && rejectPidQuery) { *pid = 0; SetLastError(0); return 0; }
    return GetWindowThreadProcessId(window, pid);
}
static HWINEVENTHOOK HookForTest(DWORD first, DWORD last, HMODULE module, WINEVENTPROC callback,
    DWORD pid, DWORD tid, DWORD flags) {
    if (rejectPidQuery && first != EVENT_SYSTEM_FOREGROUND) assert(pid == GetCurrentProcessId());
    return SetWinEventHook(first, last, module, callback, pid, tid, flags);
}
#define GetWindowThreadProcessId ThreadForTest
#define SetWinEventHook HookForTest
#define SetWindowPos PositionForTest
#include "SourceWindowCoordinator.h"
#undef SetWindowPos
#undef SetWinEventHook
#undef GetWindowThreadProcessId
#undef GetWindow
#undef SetWindowLongPtrW
#include "CaptureTargetLifetime.h"
using namespace Magpie;
struct NvidiaForegroundCompatibility {
    static void Poll() {}
    static void Retire() {}
};
using namespace std::chrono_literals;
enum class ScalingError { NoError, ScalingWindowCreationFailed, SourceWindowClosed, SourceWindowUnresponsive, LowIntegrityLevel, CaptureFailed };
static SourceWindowCoordinator _sourceCoordinator;
static HWND output;
static bool hung, permission = true, pending, _isFirstFrame = true, showSucceeded = true;
static DWORD sourceIntegrity = 1;
static int detachCalls, reports, toasts, shows;
static std::string reportedContext;
static std::string _startupFailureContext;
static uint32_t _startupFailureSystemError;
static bool _isDestroying, _isSrcRepositioning, _sourceStateCheckDeferred;
static std::chrono::steady_clock::time_point _nextSourceStateCheck;
static int transition, focusUpdates;
static ScalingError startupError;
static bool startupRendererFailed;
static uint32_t reportedError;
static std::vector<std::string> infoMessages;
struct Logger {
    struct DiagnosticCapture {
        std::string Details() const { return "recovered startup detail"; }
        uint32_t SystemError() const { return 999; }
    };
    static Logger& Get() { static Logger l; return l; }
    void Info(std::string_view message) { infoMessages.emplace_back(message); }
    void Warn(std::string_view) {} void Error(std::string_view) {}
    void Win32Warn(std::string_view) {}
};
namespace Win32Helper {
static bool IsWindowHung(HWND) { return hung; }
static bool GetWindowIntegrityLevel(HWND, DWORD& level) { level = sourceIntegrity; return permission; }
static DWORD GetCurrentProcessIntegrityLevel() { return 1; }
static std::wstring GetWindowClassName(HWND h) { wchar_t n[256]{}; GetClassNameW(h,n,256); return n; }
}
namespace StrHelper {
static std::string UTF16ToUTF8(const std::wstring& s) { std::string out; for(wchar_t c:s) out.push_back(static_cast<char>(c)); return out; }
}
struct Options {
    std::function<void(HWND,ScalingError,std::string_view,uint32_t)> reportErrorDetails;
    std::function<void(HWND,ScalingError)> showError;
    bool IsTopmostDisabled() const { return false; }
} _options;
struct SourceTracker { HWND Handle() const { return _sourceCoordinator.Source().capture; } bool IsSessionForeground() const { return false; } } _srcTracker;
struct Renderer {
    void OnSourceFocusChanged() {}
    std::string InitializationContext() const { return "capture initialization failed"; }
    uint32_t InitializationSystemError() const { return 1234; }
    const std::wstring& MotionConfigurationNotice() const { static const std::wstring empty; return empty; }
};
static std::unique_ptr<Renderer> _renderer = std::make_unique<Renderer>();
static HWND Handle() { return output; }
static HWND SourceRoot() { return _sourceCoordinator.Source().root; }
static uint32_t RunId() { return 1; }
static void ShowError(ScalingError) { ++reports; }
static void _DelayedStop(bool=false,bool reposition=false) { pending=true; transition=reposition?1:2; }
static bool HasPendingSourceTransition() { return pending; }
static void CleanAfterSrcRepositioned() { _sourceCoordinator.Reset(); _isSrcRepositioning=false; }
struct Cursor {}; static std::unique_ptr<Cursor> _cursorManager=std::make_unique<Cursor>();
static bool _UpdateSrcState(bool& reposition,bool& focusedChanged) {
    focusedChanged=false;
    reposition=!IsWindowVisible(SourceRoot()) || IsIconic(SourceRoot());
    return !reposition;
}
struct AuxiliaryWindow { HWND value=nullptr; operator bool() const { return value!=nullptr; } HWND get() const { return value; } };
static std::array<AuxiliaryWindow,4> _hwndTouchHoles{}, _hwndResizeHelpers{};
static void _HideOutputForSourceTransition() noexcept;
static void _UpdateFocusStateAsync() { ++focusUpdates; }
static void Stop() { pending=true; }
static ScalingError _StartImpl(HWND,bool& rendererInitializationFailed) {
    rendererInitializationFailed=startupRendererFailed; return startupError;
}
static void ShowToast(std::wstring_view) { ++toasts; }
static std::wstring GetLocalizedString(std::wstring_view) { return L"started"; }
static BOOL DetachForTest(DWORD, DWORD, BOOL attach) { assert(!attach); ++detachCalls; return TRUE; }
static bool _CalcTopmostState() { return _sourceCoordinator.NeedsTopmost(false,false); }
static bool _EstablishWindowCoordination() noexcept;
static bool _Show() { ++shows; return _EstablishWindowCoordination() && showSucceeded; }
#define AttachThreadInput DetachForTest
#include "ScalingWindowOwnerStartup.inc"
#undef AttachThreadInput
#include "ScalingWindowOwnerReport.inc"

static HWND Window(HWND parent=nullptr, HWND owner=nullptr) {
    HWND h=CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"Magpie window coordination test",
        parent ? WS_CHILD : WS_POPUP, -32000,-32000,32,32,parent?parent:owner,nullptr,GetModuleHandleW(nullptr),nullptr);
    assert(h); return h;
}
static void Close(HWND h) { if (IsWindow(h)) assert(DestroyWindow(h)); }
static void Pump() { MSG m; while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)) DispatchMessageW(&m); }
static void Prepare(HWND source) {
    _sourceCoordinator.Reset(); assert(_sourceCoordinator.Prepare(source));
    Close(output); output=Window(); apiMode=ApiMode::Native; apiError=ERROR_INVALID_PARAMETER;
    detachCalls=reports=toasts=shows=0; hung=pending=false; permission=showSucceeded=_isFirstFrame=true;
    reportedContext.clear(); reportedError=0; _startupFailureContext.clear(); _startupFailureSystemError=0;
    _isDestroying=_isSrcRepositioning=_sourceStateCheckDeferred=false;
    rejectPosition=false; infoMessages.clear();
    _nextSourceStateCheck={}; transition=focusUpdates=0; startupError=ScalingError::NoError; startupRendererFailed=false;
    _options.reportErrorDetails=[](HWND,ScalingError,std::string_view context,uint32_t error) {
        ++reports; reportedContext=context; reportedError=error;
    };
}
static HWND NamedWindow(const wchar_t* name) {
    WNDCLASSW cls{};
    cls.lpfnWndProc=DefWindowProcW; cls.hInstance=GetModuleHandleW(nullptr); cls.lpszClassName=name;
    assert(RegisterClassW(&cls));
    HWND window=CreateWindowExW(WS_EX_NOACTIVATE,name,L"Hidden owner admission fixture",WS_POPUP,
        -32000,-32000,32,32,nullptr,nullptr,cls.hInstance,nullptr);
    assert(window); return window;
}
static bool Logged(std::string_view text) {
    return std::ranges::any_of(infoMessages,[&](const auto& message) { return message.find(text)!=std::string::npos; });
}
static void CheckUwpOwnerNotApplied() {
    // These are hidden Win32 fixtures with controlled class/API results, not
    // real UWP capture or fullscreen acceptance tests.
    HWND source=NamedWindow(L"ApplicationFrameWindow");
    for (bool queued : {false,true}) {
        Prepare(source); apiMode=ApiMode::Reject; apiError=ERROR_SUCCESS;
        auto result=TrySetScalingWindowOwner(output,source);
        assert(!result.IsApplied() && !result.CanContinueUnowned() && !result.actualOwner);
        _CompleteFrontendRender(true,queued);
        assert(_sourceCoordinator.Mode()==WindowCoordinationMode::ManagedUnowned);
        assert(!pending && reports==0 && detachCalls==0 && toasts==1 && !_isFirstFrame);
        assert(Logged("reason=uwp-owner-not-applied") && Logged("Scaling output shown:"));

        // A size-change rebuild keeps the identity watch, but retires the old
        // output and must pass admission again with the current API result.
        const auto epoch=_sourceCoordinator.Epoch();
        _sourceCoordinator.RetireOutput(); Close(output); output=Window();
        assert(_sourceCoordinator.Mode()==WindowCoordinationMode::Preparing);
        _isFirstFrame=true; apiError=ERROR_INVALID_PARAMETER;
        _CompleteFrontendRender(true,queued);
        assert(_sourceCoordinator.Epoch()==epoch && _sourceCoordinator.Mode()==WindowCoordinationMode::ManagedUnowned);
        assert(reports==0 && toasts==2);
        _sourceCoordinator.RetireOutput(); Close(output); output=Window();
        _isFirstFrame=true; apiError=ERROR_SUCCESS;
        _CompleteFrontendRender(true,queued);
        assert(_sourceCoordinator.Epoch()==epoch && _sourceCoordinator.Mode()==WindowCoordinationMode::ManagedUnowned);
        assert(reports==0 && toasts==3);
        _sourceCoordinator.RetireOutput(); Close(output); output=Window();
        _isFirstFrame=true; apiError=ERROR_ACCESS_DENIED;
        _CompleteFrontendRender(true,queued);
        assert(pending && reports==1 && toasts==3 && _isFirstFrame);
    }
    Prepare(source); apiMode=ApiMode::Reject; apiError=ERROR_SUCCESS;
    auto result=TrySetScalingWindowOwner(output,source);
    assert(!_sourceCoordinator.Accept(output,result,false));
    permission=false; _CompleteFrontendRender(true,false);
    assert(reports==1 && reportedError==ERROR_ACCESS_DENIED && toasts==0);
    Prepare(source); apiMode=ApiMode::Reject; apiError=ERROR_SUCCESS; sourceIntegrity=2;
    _CompleteFrontendRender(true,false);
    assert(reports==1 && reportedError==ERROR_ACCESS_DENIED && toasts==0); sourceIntegrity=1;
    for (DWORD code : {DWORD(5),DWORD(1400),DWORD(12345)}) {
        Prepare(source); apiMode=ApiMode::Reject; apiError=code;
        _CompleteFrontendRender(true,false);
        assert(reports==1 && reportedError==code && toasts==0 && _isFirstFrame);
    }
    Prepare(source); apiMode=ApiMode::Reject; apiError=ERROR_SUCCESS;
    result=TrySetScalingWindowOwner(output,source);
    assert(SetWindowLongPtrW(output,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(source))==0);
    assert(!_sourceCoordinator.Accept(output,result,true));
    Prepare(source); apiMode=ApiMode::Reject; apiError=ERROR_SUCCESS;
    result=TrySetScalingWindowOwner(output,source);
    // A relationship changing after the API call must not form an owner cycle.
    SetWindowLongPtrW(source,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(output));
    assert(GetWindow(source,GW_OWNER)==output);
    assert(!_sourceCoordinator.Accept(output,result,true));
    SetWindowLongPtrW(source,GWLP_HWNDPARENT,0);
    Prepare(source); apiMode=ApiMode::Reject; apiError=ERROR_SUCCESS;
    result=TrySetScalingWindowOwner(output,source);
    _sourceCoordinator.Notify(_sourceCoordinator.Epoch(),EVENT_OBJECT_DESTROY,source);
    assert(!_sourceCoordinator.Accept(output,result,true));
    Prepare(source); apiMode=ApiMode::Reject; apiError=ERROR_SUCCESS;
    result=TrySetScalingWindowOwner(output,source); Close(output);
    assert(!_sourceCoordinator.Accept(output,result,true));
    Prepare(source); apiMode=ApiMode::Reject; apiError=ERROR_SUCCESS;
    result=TrySetScalingWindowOwner(output,source); result.previousOwner=1;
    assert(!_sourceCoordinator.Accept(output,result,true));
    Prepare(source); apiMode=ApiMode::Reject; apiError=ERROR_SUCCESS; rejectPosition=true;
    // Force a real ordering operation; an already-correct order needs no API.
    assert(SetWindowPos(output,HWND_TOPMOST,0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE));
    _CompleteFrontendRender(true,false);
    assert(reports==1 && reportedError==ERROR_ACCESS_DENIED && toasts==0 && _isFirstFrame);
    assert(!Logged("Scaling output shown:"));
    // A capture child beneath an ApplicationFrameWindow is outside the
    // observed top-level case, even though its root has the recognized class.
    HWND child=Window(source); Prepare(child); apiMode=ApiMode::Reject; apiError=ERROR_SUCCESS;
    _CompleteFrontendRender(true,false); assert(reports==1 && toasts==0); Close(child);
    Prepare(source); apiMode=ApiMode::Reject; apiError=ERROR_SUCCESS;
    result=TrySetScalingWindowOwner(output,source); Close(source);
    assert(!_sourceCoordinator.Accept(output,result,true));
    for (const auto* name : {L"Windows.UI.Core.CoreWindow",L"WinUIDesktopWin32WindowClass"}) {
        HWND other=NamedWindow(name); Prepare(other); apiMode=ApiMode::Reject; apiError=ERROR_SUCCESS;
        _CompleteFrontendRender(true,false); assert(reports==1 && toasts==0); Close(other);
    }
    _sourceCoordinator.Reset(); Close(output); apiMode=ApiMode::Native;
    std::cout << "PASS: UWP owner-not-applied admission, rebuild, restrictions and first-display failures" << std::endl;
}
static void CheckParameterLayers() {
    HWND root=Window(), child=Window(root), view=Window(), host=Window(nullptr,view), other=Window();
    SourceWindowIdentity identity; assert(identity.Read(child) && identity.root==root && child!=root);
	const auto mainUI = RegisterSessionWindow(other);
	assert(ClassifySourceForeground(other,true,child,root,view,host,mainUI)==SourceForegroundKind::MagpieMainUI);
	assert(!IsSourceSessionForeground(SourceForegroundKind::MagpieMainUI));
	assert(ClassifySourceForeground(view,true,child,root,view,host,mainUI)==SourceForegroundKind::Output);
	for (auto changed : {0,1,2}) {
		auto stale = mainUI;
		if (changed == 0) ++stale.pid;
		if (changed == 1) ++stale.tid;
		if (changed == 2) ++stale.atom;
		assert(ClassifySourceForeground(other,true,child,root,view,host,stale)==SourceForegroundKind::External);
	}
    constexpr UINT flags=SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW|SWP_NOOWNERZORDER;
    for (bool nativeOwner : {false,true}) {
        assert(SetScalingWindowOwner(view,nativeOwner?root:nullptr));
        for (HWND h : {root,child,view,host,other}) assert(SetWindowPos(h,HWND_TOP,0,0,0,0,flags));
        assert(ClassifySourceForeground(root,identity.Matches(),child,root,view,host)==SourceForegroundKind::Source);
        assert(ClassifySourceForeground(host,identity.Matches(),child,root,view,host)==SourceForegroundKind::ParameterHost);
        assert(ClassifySourceForeground(other,identity.Matches(),child,root,view,host)==SourceForegroundKind::External);
        const LONG_PTR rootStyle=GetWindowLongPtrW(root,GWL_EXSTYLE);
        const RECT rect{-32000,-32000,-31968,-31968};
        for (bool topmost : {false,true,false}) {
            assert(SetWindowPos(view,topmost?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,flags));
            assert(SetWindowPos(other,HWND_TOPMOST,0,0,0,0,flags));
            assert(PositionParameterInputHost(host,view,rect));
            assert(GetWindow(view,GW_HWNDPREV)==host);
            bool externalAhead=false;
            for (HWND h=GetWindow(host,GW_HWNDPREV); h; h=GetWindow(h,GW_HWNDPREV))
                if (h==other) { externalAhead=true; break; }
            assert(externalAhead);
            assert(bool(GetWindowLongPtrW(host,GWL_EXSTYLE)&WS_EX_TOPMOST)==topmost);
            assert(bool(GetWindowLongPtrW(view,GWL_EXSTYLE)&WS_EX_TOPMOST)==topmost);
            assert(GetWindowLongPtrW(root,GWL_EXSTYLE)==rootStyle);
            assert(GetWindow(view,GW_OWNER)==(nativeOwner?root:nullptr));
            assert(!EnabledSourcePopup(root,view));
        }
        ShowWindow(view,SW_HIDE); assert(!PositionParameterInputHost(host,view,rect));
    }
    HWND popup=Window(nullptr,root); ShowWindow(popup,SW_SHOWNOACTIVATE);
    EnableWindow(root,FALSE); assert(EnabledSourcePopup(root,view)==popup);
    assert(ClassifySourceForeground(popup,true,child,root,view,host)==SourceForegroundKind::SourcePopup);
    assert(ClassifySourceForeground(host,false,child,root,view,host)==SourceForegroundKind::InvalidSource);
    assert(SetScalingWindowOwner(host,nullptr));
    assert(ClassifySourceForeground(host,true,child,root,view,host)==SourceForegroundKind::External);
    for (HWND h : {popup,host,view,other,child,root}) Close(h);
    std::cout << "PASS: native source/root and host identity, both output owner modes, host layers follow actual output, preserve root styles and remain below external windows\n";
}
int main() {
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    CheckParameterLayers();
    // Existing identity/coordinator code must also work when the PID-output
    // query fails. Native thread/process queries resolve our offscreen windows.
    rejectPidQuery = true;
    HWND hookedRoot = Window(), hookedChild = Window(hookedRoot);
    SourceWindowIdentity hookedIdentity;
    assert(hookedIdentity.Read(hookedChild) && hookedIdentity.capturePid == GetCurrentProcessId());
    assert(hookedIdentity.rootPid == GetCurrentProcessId() && hookedIdentity.Matches());
    assert(RegisterSessionWindow(hookedRoot).Matches());
    SourceWindowCoordinator hookedCoordinator;
    assert(hookedCoordinator.Prepare(hookedRoot) && hookedCoordinator.Ready());
    rejectPidQuery = false; assert(hookedCoordinator.Ready());
    rejectPidQuery = true; assert(hookedCoordinator.Ready());
    hookedCoordinator.Notify(hookedCoordinator.Epoch(), EVENT_OBJECT_DESTROY, hookedRoot);
    assert(!hookedCoordinator.Ready()); hookedCoordinator.Reset();
    Close(hookedChild); Close(hookedRoot);
    assert(!hookedIdentity.Matches());
    rejectPidQuery = false;
    HWND source=Window(); alternate=Window(); Prepare(source);
    auto result=TrySetScalingWindowOwner(output,source);
    assert(result.IsApplied() && result.actualOwner==source);
    assert(TrySetScalingWindowOwner(output,source).IsApplied());
    assert(TrySetScalingWindowOwner(output,nullptr).IsApplied());
    assert(TrySetScalingWindowOwner(output,nullptr).IsApplied());
    assert(TrySetScalingWindowOwner(nullptr,nullptr).status==ScalingWindowOwnerStatus::InvalidWindow);
    assert(TrySetScalingWindowOwner(output,output).status==ScalingWindowOwnerStatus::InvalidRelationship);
    HWND child=Window(output);
    assert(TrySetScalingWindowOwner(child,source).status==ScalingWindowOwnerStatus::ChildWindow);
    assert(TrySetScalingWindowOwner(output,child).status==ScalingWindowOwnerStatus::InvalidRelationship);
    Close(child);
    assert(SetScalingWindowOwner(alternate,output));
    assert(TrySetScalingWindowOwner(output,alternate).status==ScalingWindowOwnerStatus::InvalidRelationship);
    assert(SetScalingWindowOwner(alternate,nullptr));
    for (DWORD code : {DWORD(87),DWORD(5),DWORD(1400),DWORD(0),DWORD(12345)}) {
        Prepare(source); apiMode=ApiMode::Reject; apiError=code;
        result=TrySetScalingWindowOwner(output,source);
        assert(result.systemError==code && !result.actualOwner && result.previousOwner==0);
        if(code==0) assert(result.status==ScalingWindowOwnerStatus::NotApplied);
        assert(result.CanContinueUnowned()==(code==87));
        assert(_EstablishWindowCoordination()==(code==87));
        assert(detachCalls==0 && GetWindow(output,GW_OWNER)==nullptr);
        if(code==87) assert(_sourceCoordinator.Mode()==WindowCoordinationMode::ManagedUnowned);
        else assert(reports==1 && pending && reportedError==code);
    }
    Prepare(source); apiMode=ApiMode::AppliedDirtyError;
    assert(_EstablishWindowCoordination());
    assert(_sourceCoordinator.Mode()==WindowCoordinationMode::NativeOwned && detachCalls==1 && reports==0);
    apiMode=ApiMode::Native; assert(TrySetScalingWindowOwner(output,nullptr).IsApplied());
    Prepare(source); permission=false; apiMode=ApiMode::Reject;
    assert(!_EstablishWindowCoordination() && reports==1 && reportedError==ERROR_ACCESS_DENIED);
    Prepare(source); sourceIntegrity=2; rejectPidQuery=true;
    assert(!_EstablishWindowCoordination() && reports==1 && reportedError==ERROR_ACCESS_DENIED);
    sourceIntegrity=1; rejectPidQuery=false;
    Prepare(source); apiMode=ApiMode::Unexpected;
    assert(!_EstablishWindowCoordination() && reports==1 && reportedError==ERROR_ACCESS_DENIED);
    apiMode=ApiMode::Native; assert(TrySetScalingWindowOwner(output,nullptr).IsApplied());
    Prepare(source); hung=true;
    assert(!_EstablishWindowCoordination() && reports==1 && detachCalls==0);
    Prepare(source); apiMode=ApiMode::Reject;
    result=TrySetScalingWindowOwner(output,source);
    assert(!_sourceCoordinator.Accept(output,result,false));
    _sourceCoordinator.Reset(); assert(!_sourceCoordinator.Accept(output,result,true));
    Prepare(source); apiMode=ApiMode::Reject;
    _CompleteFrontendRender(false,false); assert(shows==0 && toasts==0);
    _CompleteFrontendRender(true,false); assert(shows==1 && toasts==1 && !_isFirstFrame);
    _CompleteFrontendRender(true,false); assert(shows==1 && toasts==1);
    Prepare(source); apiMode=ApiMode::Reject;
    _CompleteFrontendRender(true,true); assert(shows==1 && toasts==1 && !_isFirstFrame);
    Prepare(source); apiMode=ApiMode::Reject; apiError=5;
    _CompleteFrontendRender(true,true); assert(shows==1 && toasts==0 && _isFirstFrame && reports==1);
    Prepare(source); showSucceeded=false;
    _CompleteFrontendRender(true,false); assert(toasts==0 && _isFirstFrame);
    apiMode=ApiMode::Native; assert(TrySetScalingWindowOwner(output,nullptr).IsApplied());

    // Production startup reporting must not reuse a recovered initialization
    // warning's system code for a later source/output failure.
    Prepare(source); startupError=ScalingError::SourceWindowClosed;
    _startupFailureContext="source retired after initialization"; _startupFailureSystemError=1400;
    ReportStartup(source); assert(reports==1 && reportedError==1400 && reportedContext==_startupFailureContext);
    Prepare(source); startupError=ScalingError::CaptureFailed; startupRendererFailed=true;
    ReportStartup(source); assert(reports==1 && reportedError==1234 && reportedContext=="capture initialization failed");
    Prepare(source); startupError=ScalingError::ScalingWindowCreationFailed;
    _startupFailureContext="Create renderer child failed"; _startupFailureSystemError=8;
    ReportStartup(source); assert(reports==1 && reportedError==8 && reportedContext==_startupFailureContext);

    // Real controller identity/root mapping; capture HWND is never substituted.
    Prepare(source); child=Window(source);
    SourceWindowCoordinator mapped; assert(mapped.Prepare(child));
    assert(mapped.Source().capture==child && mapped.Source().root==source);
    auto epoch=mapped.Epoch();
    mapped.Notify(epoch-1,EVENT_OBJECT_DESTROY,child); assert(mapped.Ready());
    mapped.Notify(epoch,EVENT_OBJECT_DESTROY,child); assert(!mapped.Ready());
    // Even if the numeric HWND still exists, a latched death cannot rebind it.
    assert(IsWindow(child)); mapped.RetireOutput(); assert(!mapped.Ready());
    mapped.Reset(); mapped.Notify(epoch,EVENT_OBJECT_SHOW,child); assert(!mapped.Ready());
    Close(child);

    // Controlled lifecycle events require no capture frame or renderer call.
    Prepare(source); const auto sourceEpoch=_sourceCoordinator.Epoch();
    _sourceCoordinator.TakeDirty();
    for(DWORD event : {EVENT_OBJECT_HIDE,EVENT_OBJECT_SHOW,EVENT_SYSTEM_MINIMIZESTART,EVENT_SYSTEM_MINIMIZEEND,
                      EVENT_OBJECT_LOCATIONCHANGE,EVENT_SYSTEM_FOREGROUND}) {
        _sourceCoordinator.Notify(sourceEpoch,event,source);
        assert(_sourceCoordinator.TakeDirty()); assert(!_sourceCoordinator.TakeDirty());
    }
    _sourceCoordinator.Notify(sourceEpoch-1,EVENT_OBJECT_HIDE,source); assert(!_sourceCoordinator.TakeDirty());
    _sourceCoordinator.Notify(sourceEpoch,EVENT_OBJECT_DESTROY,source,OBJID_CLIENT); assert(_sourceCoordinator.Ready());
    _sourceCoordinator.Notify(sourceEpoch,EVENT_OBJECT_DESTROY,source); assert(!_sourceCoordinator.Ready());
    Prepare(source); assert(SetWindowPos(source,nullptr,0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_SHOWWINDOW));
    HWND popup=Window(nullptr,source);
    assert(!EnabledSourcePopup(source));
    assert(SetWindowPos(popup,nullptr,0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_SHOWWINDOW));
    assert(EnabledSourcePopup(source)==popup && !_sourceCoordinator.NeedsTopmost(true,false));
    EnableWindow(popup,FALSE); assert(!EnabledSourcePopup(source));
    EnableWindow(source,FALSE); assert(!_sourceCoordinator.NeedsTopmost(true,false));
    EnableWindow(source,TRUE); assert(_sourceCoordinator.NeedsTopmost(true,false));
    assert(!_sourceCoordinator.NeedsTopmost(true,true));
    HWND topmostSource=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOPMOST,L"STATIC",L"Magpie topmost source",
        WS_POPUP,-32000,-32000,32,32,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    assert(topmostSource && (GetWindowLongPtrW(topmostSource,GWL_EXSTYLE)&WS_EX_TOPMOST));
    SourceWindowCoordinator topmostCoordinator; assert(topmostCoordinator.Prepare(topmostSource));
    assert(topmostCoordinator.NeedsTopmost(false,true)); topmostCoordinator.Reset(); Close(topmostSource);
    Close(popup);
    apiMode=ApiMode::Reject; assert(_EstablishWindowCoordination());
    HWND other=Window();
    assert(SetWindowPos(other,HWND_TOP,0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE));
    assert(_sourceCoordinator.UpdateManagedOrder(false,false,true));
    assert(GetWindow(source,GW_HWNDPREV)==output && GetWindow(output,GW_OWNER)==nullptr);
    // Same topmost flag, changed relative ordering must still be repaired.
    assert(SetWindowPos(other,output,0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE));
    assert(GetWindow(source,GW_HWNDPREV)!=output);
    assert(_sourceCoordinator.UpdateManagedOrder(false,false,true));
    assert(GetWindow(source,GW_HWNDPREV)==output && GetWindow(output,GW_OWNER)==nullptr);
    assert(GetWindow(output,GW_HWNDPREV)==other); // unrelated window remains ahead
    Close(other); apiMode=ApiMode::Native;
    assert(SetScalingWindowOwner(output,source));
    assert(SetWindowPos(output,nullptr,0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_SHOWWINDOW));
    assert(!EnabledSourcePopup(source,output));
    assert(SetScalingWindowOwner(output,nullptr));
    Prepare(source); apiMode=ApiMode::DestroyOutput;
    result=TrySetScalingWindowOwner(output,source); assert(result.status==ScalingWindowOwnerStatus::InvalidWindow);
    Prepare(source); HWND dying=Window(); apiMode=ApiMode::DestroySource;
    result=TrySetScalingWindowOwner(output,dying); assert(result.status==ScalingWindowOwnerStatus::InvalidOwner);
    // Run the actual PollSourceState body with no Render/new-frame call. A
    // source hidden in ManagedUnowned does not let USER32 hide the output;
    // the production poll must do it and request the existing rebuild path.
    Prepare(source); apiMode=ApiMode::Reject; assert(_EstablishWindowCoordination());
    assert(SetWindowPos(output,nullptr,0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_SHOWWINDOW));
    HWND independentHole=Window(); _hwndTouchHoles[0].value=independentHole;
    assert(SetWindowPos(independentHole,nullptr,0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_SHOWWINDOW));
    assert(!GetWindow(independentHole,GW_OWNER));
    ShowWindow(source,SW_HIDE); assert(IsWindowVisible(output) && IsWindowVisible(independentHole));
    _isFirstFrame=false; PollSourceState(); assert(!IsWindowVisible(output) && pending && transition==1);
    assert(!IsWindowVisible(independentHole)); Close(independentHole); _hwndTouchHoles[0].value=nullptr;
    ShowWindow(source,SW_SHOWMINNOACTIVE); Prepare(source); apiMode=ApiMode::Reject; assert(_EstablishWindowCoordination());
    assert(SetWindowPos(output,nullptr,0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_SHOWWINDOW));
    PollSourceState(); assert(!IsWindowVisible(output) && pending && transition==1);
    ShowWindow(source,SW_SHOWNOACTIVATE); Prepare(source); apiMode=ApiMode::Reject; assert(_EstablishWindowCoordination());
    assert(SetWindowPos(output,nullptr,0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_SHOWWINDOW));
    Close(source); PollSourceState(); assert(!IsWindowVisible(output) && pending && transition==2);
    Pump(); assert(!_sourceCoordinator.Ready());
    _sourceCoordinator.Reset(); Close(output); Close(alternate);
    // Per-item Closed callbacks cannot mutate a replacement target's lifetime.
    CaptureTargetLifetime oldTarget(GetCurrentThreadId()), newTarget(GetCurrentThreadId());
    oldTarget.Retire(); std::thread callback([&]{ oldTarget.Close(); }); callback.join();
    assert(oldTarget.IsClosed() && !newTarget.IsClosed());
    newTarget.Close(); assert(newTarget.IsClosed());
    CheckUwpOwnerNotApplied();
    std::cout << "PASS: owner capabilities, production admission/first-frame gate, identities, lifecycle signals, popup/topmost/Z-order and stale Closed" << std::endl;
}
