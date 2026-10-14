#include "pch.h"
#include "Win32Helper.h"
#include "OverlayDrawer.h"
#include "ScalingWindow.h"
#include "Renderer.h"
#include "CursorManager.h"
#include "Logger.h"
#include <imgui_internal.h>

namespace Magpie {

OverlayDrawer::~OverlayDrawer() noexcept {
	_parameterInputTransition = true;
	if (_hwndParameterInput) DestroyWindow(_hwndParameterInput);
}

bool OverlayDrawer::_HasParameterForeground() const noexcept {
	if (!_parameterFocusSwitchingEnabled) return false;
	auto& scaling = ScalingWindow::Get();
	const HWND foreground = Win32Helper::GetRealForegroundWindow();
	return scaling.IsParameterForeground(foreground, _parameterInputTransition ||
		std::chrono::steady_clock::now() < _parameterFocusSettlesAt);
}

bool OverlayDrawer::_OwnsParameterInput() const noexcept {
	return _parameterInputOwned && _parameterInputRunId == ScalingWindow::RunId() &&
		_hwndParameterInput && Win32Helper::GetRealForegroundWindow() == _hwndParameterInput &&
		GetFocus() == _hwndParameterInput && _HasParameterForeground();
}

bool OverlayDrawer::_PositionParameterInputHost(const RECT& rect) noexcept {
	const bool positioned = PositionParameterInputHost(_hwndParameterInput, ScalingWindow::Get().Handle(), rect);
	if (!positioned) {
		if (!_parameterLayerFailed) Logger::Get().Warn("Parameter input host hidden: output layer unavailable");
		if (_hwndParameterInput) ShowWindow(_hwndParameterInput, SW_HIDE);
	}
	_parameterLayerFailed = !positioned;
	return positioned;
}

bool OverlayDrawer::_EnsureParameterInputHost() noexcept {
	if (!_parameterFocusSwitchingEnabled) return false;
	if (!_hwndParameterInput) {
		static const ATOM windowClass = [] {
			WNDCLASSEXW wc{ sizeof(wc) };
			wc.lpfnWndProc = _ParameterInputWndProc;
			wc.hInstance = GetModuleHandle(nullptr);
			wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
			wc.lpszClassName = L"Magpie_ParameterInputHost";
			return RegisterClassExW(&wc);
		}();
		if (windowClass) {
			// No redirection surface: fully transparent visuals, rectangular USER32
			// hit testing. Alpha-zero layered windows would pass the first click through.
			_hwndParameterInput = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
				MAKEINTATOM(windowClass), L"Magpie parameters", WS_POPUP,
				0, 0, 1, 1, ScalingWindow::Get().Handle(), nullptr, GetModuleHandle(nullptr), this);
		}
	}
	if (!_hwndParameterInput) {
		Logger::Get().Win32Error("Create parameter input host failed");
		return false;
	}
	return true;
}

bool OverlayDrawer::_BeginParameterInput(const char* request) noexcept {
	// An asynchronous restart must never activate over an unrelated application.
	if (!_HasParameterForeground()) {
		Logger::Get().Info("Parameter editing activation skipped: foreground is outside the scaling session");
		return false;
	}
	if (_parameterHostCanceled) _EndParameterInput(false);
	const uint32_t runId = ScalingWindow::RunId();
	_parameterInputTransition = true;
	if (!_EnsureParameterInputHost()) {
		_parameterInputTransition = false;
		return false;
	}
	auto& scaling = ScalingWindow::Get();
	if (runId != ScalingWindow::RunId() || !scaling.SourceIdentityValid()) {
		_parameterInputTransition = false;
		return false;
	}
	_parameterPanelState = ParameterPanelState::Edit;
	_parameterFocusSettlesAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
	_pendingParameterPanelState = ParameterPanelState::Edit;
	_parameterHeldButtons = 0;
	_returnClickPending = _escapePending = _parameterResumeClickPending = false;
	_parameterHeldKeys.fill(false);
	_parameterInheritedKeys.fill(false);
	ClearStates();
	const RECT& rect = scaling.RendererRect();
	bool positioned = _PositionParameterInputHost(rect);
	const HWND currentForeground = Win32Helper::GetRealForegroundWindow();
	if (positioned && runId == ScalingWindow::RunId() && scaling.IsParameterForeground(currentForeground, true))
		SetForegroundWindow(_hwndParameterInput);
	if (Win32Helper::GetRealForegroundWindow() == _hwndParameterInput && runId == ScalingWindow::RunId()) SetFocus(_hwndParameterInput);
	// Activation itself can raise a window within its band. Restore the host's
	// relation to the output before accepting input ownership.
	if (positioned && runId == ScalingWindow::RunId() &&
		scaling.IsParameterForeground(Win32Helper::GetRealForegroundWindow(), true) &&
		Win32Helper::GetRealForegroundWindow() == _hwndParameterInput && GetFocus() == _hwndParameterInput)
		positioned = _PositionParameterInputHost(rect);
	const bool activated = positioned && runId == ScalingWindow::RunId() &&
		scaling.IsParameterForeground(Win32Helper::GetRealForegroundWindow(), true) &&
		Win32Helper::GetRealForegroundWindow() == _hwndParameterInput && GetFocus() == _hwndParameterInput;
	_parameterInputOwned = activated;
	_parameterInputRunId = activated ? runId : 0;
	if (activated) {
		ClipCursor(nullptr);
		_imguiImpl.ParameterEditing(true);
		for (int key : { VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN }) {
			if (GetAsyncKeyState(key) & 0x8000) {
				// The original DOWN belongs to the previously focused window.
				// Its UP may already be queued there when focus changes.
				_parameterInheritedKeys[key] = true;
				_imguiImpl.MessageHandler(WM_KEYDOWN, key, 0);
			}
		}
		scaling.CursorManager().Update();
		SetTimer(_hwndParameterInput, 1, 16, nullptr);
		Logger::Get().Info(fmt::format("Parameter Edit acquired: request={} run={} capture={:#x} root={:#x} host={:#x} foreground={:#x} threadFocus={:#x} settleMs=100",
			request, runId, uintptr_t(scaling.SrcTracker().Handle()), uintptr_t(scaling.SourceRoot()),
			uintptr_t(_hwndParameterInput), uintptr_t(Win32Helper::GetRealForegroundWindow()), uintptr_t(GetFocus())));
	} else {
		if (runId == ScalingWindow::RunId() && Win32Helper::GetRealForegroundWindow() == _hwndParameterInput)
			scaling.SrcTracker().SetFocus(_hwndParameterInput);
		ShowWindow(_hwndParameterInput, SW_HIDE);
		_parameterPanelState = ParameterPanelState::Preview;
		Logger::Get().Warn(fmt::format("Parameter editing activation failed: request={} run={} kind={} capture={:#x} root={:#x} host={:#x} foreground={:#x} threadFocus={:#x}",
			request, runId, int(scaling.ClassifyForeground(Win32Helper::GetRealForegroundWindow())), uintptr_t(scaling.SrcTracker().Handle()),
			uintptr_t(scaling.SourceRoot()), uintptr_t(_hwndParameterInput), uintptr_t(Win32Helper::GetRealForegroundWindow()), uintptr_t(GetFocus())));
	}
	_parameterInputTransition = false;
	return activated;
}

void OverlayDrawer::_EndParameterInput(bool returnFocus) noexcept {
	if (!_parameterFocusSwitchingEnabled) return;
	_parameterInputTransition = true;
	const bool ownedFocus = _hwndParameterInput && Win32Helper::GetRealForegroundWindow() == _hwndParameterInput;
	_parameterInputOwned = false;
	_parameterHostCanceled = false;
	_parameterInputRunId = 0;
	_parameterEditPending = false;
	_parameterEditRunId = 0;
	_imguiImpl.ParameterEditing(false);
	ClearStates();
	if (_hwndParameterInput) {
		KillTimer(_hwndParameterInput, 1);
		if (GetCapture() == _hwndParameterInput) ReleaseCapture();
		if (ownedFocus && Win32Helper::GetRealForegroundWindow() == _hwndParameterInput) ClipCursor(nullptr);
		if (ownedFocus && returnFocus && Win32Helper::GetRealForegroundWindow() == _hwndParameterInput &&
			ScalingWindow::Get().SourceIdentityValid()) {
			auto& scaling = ScalingWindow::Get();
			const HWND target = scaling.SourceActivationTarget();
			_parameterFocusFailed = !scaling.SrcTracker().SetFocus(_hwndParameterInput);
			Logger::Get().Info(fmt::format("Parameter input return: run={} target={:#x} foreground={:#x} restored={}",
				ScalingWindow::RunId(), uintptr_t(target), uintptr_t(Win32Helper::GetRealForegroundWindow()), !_parameterFocusFailed));
			if (_parameterFocusFailed) scaling.ShowToast(scaling.GetLocalizedString(L"Overlay_Parameters_FocusFailed"));
		}
		ShowWindow(_hwndParameterInput, SW_HIDE);
	}
	_parameterHeldButtons = 0;
	_parameterHeldKeys.fill(false);
	_parameterInheritedKeys.fill(false);
	_returnClickPending = _escapePending = _parameterResumeClickPending = false;
	_parameterInputTransition = false;
	_parameterFocusSettlesAt = ownedFocus && returnFocus
		? std::chrono::steady_clock::now() + std::chrono::milliseconds(100) : std::chrono::steady_clock::time_point{};
}

void OverlayDrawer::_UpdateParameterPreviewHost() noexcept {
	if (!_parameterFocusSwitchingEnabled) return;
	_imguiImpl.ParameterPreview(_isEffectParametersVisible && _parameterPanelState == ParameterPanelState::Preview);
	if (_parameterInputTransition || IsEditingParameters() || _parameterResumeClickPending) return;
	const auto rect = _imguiImpl.PresentedParameterRect();
	if (_parameterPanelState != ParameterPanelState::Preview || !_isEffectParametersVisible ||
		!_HasParameterForeground() || !rect || !IsWindowVisible(ScalingWindow::Get().Handle())) {
		if (_hwndParameterInput) ShowWindow(_hwndParameterInput, SW_HIDE);
		if (!_HasParameterForeground()) _previewEscapeCanClose = _previewClosePending = false;
		return;
	}
	auto& scaling = ScalingWindow::Get();
	const RECT& dest = scaling.Renderer().DestRect();
	const RECT& render = scaling.RendererRect();
	const RECT target{
		std::max(render.left, dest.left + LONG(std::floor(rect->x))),
		std::max(render.top, dest.top + LONG(std::floor(rect->y))),
		std::min(render.right, dest.left + LONG(std::ceil(rect->z))),
		std::min(render.bottom, dest.top + LONG(std::ceil(rect->w)))
	};
	if (target.left >= target.right || target.top >= target.bottom) {
		if (_hwndParameterInput) ShowWindow(_hwndParameterInput, SW_HIDE);
		return;
	}
	if (!_EnsureParameterInputHost()) return;
	_PositionParameterInputHost(target);
}

bool OverlayDrawer::HandleParameterPreviewEscape(WPARAM message, const KBDLLHOOKSTRUCT& key) noexcept {
	if (!_parameterFocusSwitchingEnabled) return false;
	if (key.vkCode != VK_ESCAPE) return false;
	const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
	const bool up = message == WM_KEYUP || message == WM_SYSKEYUP;
	if (!down && !up) return false;
	if (_previewEscapeOwned) {
		if (up) {
			_previewEscapeOwned = false;
			_previewClosePending = std::exchange(_previewEscapeCanClose, false) &&
				_parameterPanelState == ParameterPanelState::Preview && _HasParameterForeground();
			if (_previewClosePending) {
				_overlayDirty = true;
				PostMessage(ScalingWindow::Get().Handle(), WM_NULL, 0, 0);
			}
		}
		return true; // Own repeats and release only after owning the initial press.
	}
	if (!down || _parameterPanelState != ParameterPanelState::Preview ||
		!_isEffectParametersVisible || !_HasParameterForeground() || (key.flags & LLKHF_ALTDOWN)) return false;
	for (int vk : { VK_ESCAPE, VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN }) {
		if (GetAsyncKeyState(vk) & 0x8000) return false;
	}
	_previewEscapeOwned = _previewEscapeCanClose = true;
	return true;
}

void OverlayDrawer::_ToggleParameterPanel() noexcept {
	_SetParameterPanelState(_isEffectParametersVisible ? ParameterPanelState::Closed : ParameterPanelState::Edit);
}

void OverlayDrawer::_SetParameterPanelState(ParameterPanelState state, bool returnFocus, const char* request) noexcept {
	_parameterEditPending = false;
	if (!_parameterFocusSwitchingEnabled) {
		_isEffectParametersVisible = state != ParameterPanelState::Closed;
		_overlayDirty = true;
		_ClearStatesIfNoVisibleWindow();
		return;
	}
	_previewEscapeCanClose = _previewClosePending = false;
	if (state == ParameterPanelState::Edit) {
		_isEffectParametersVisible = true;
		if (!IsEditingParameters()) {
			_parameterFocusFailed = !_BeginParameterInput(request);
			if (_parameterFocusFailed) _parameterPanelState = ParameterPanelState::Preview;
		}
	} else if (IsEditingParameters() && returnFocus) {
		// Keep the host until every physical edge belongs to a complete pair.
		_pendingParameterPanelState = state;
		_FinishParameterInput();
	} else {
		_EndParameterInput(returnFocus);
		_parameterPanelState = state;
		_isEffectParametersVisible = state != ParameterPanelState::Closed;
	}
	_overlayDirty = true;
	_UpdateParameterPreviewHost();
}

void OverlayDrawer::_FinishParameterInput() noexcept {
	_SyncInheritedParameterKeys();
	if (_pendingParameterPanelState == ParameterPanelState::Edit || HasHeldParameterInput()) return;

	// Let ImGui consume a queued release before clearing its active item.
	// Hover moves may keep arriving while the user returns to the game. Only
	// complete control edges need presentation; movement must not postpone focus.
	if (_imguiImpl.HasCriticalInput()) return;
	_EndParameterInput(true);
	_parameterPanelState = _pendingParameterPanelState;
	_isEffectParametersVisible = _parameterPanelState != ParameterPanelState::Closed;
	_UpdateParameterPreviewHost();
	Logger::Get().Info(_parameterPanelState == ParameterPanelState::Preview
		? "Parameter panel returned to Preview" : "Parameter panel closed after input handoff");
}

bool OverlayDrawer::HasHeldParameterInput() const noexcept {
	if (!_parameterFocusSwitchingEnabled) return false;
	if (!_HasParameterForeground()) return false;
	if (_previewEscapeOwned || _parameterResumeClickPending) return true;
	if (!IsEditingParameters() || !_OwnsParameterInput()) return false;
	if (_parameterHeldButtons || std::ranges::any_of(_parameterHeldKeys, [](bool held) { return held; })) return true;
	// Shortcut callbacks can arrive before the invoking modifier messages.
	for (int key : {VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN}) {
		if (GetAsyncKeyState(key) & 0x8000) return true;
	}
	return false;
}

void OverlayDrawer::_SyncInheritedParameterKeys() noexcept {
	if (!IsEditingParameters()) return;
	for (int key : { VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN }) {
		if (_parameterInheritedKeys[key] && !(GetAsyncKeyState(key) & 0x8000)) {
			_parameterInheritedKeys[key] = false;
			_imguiImpl.MessageHandler(WM_KEYUP, key, 0);
			_overlayDirty = true;
		}
	}
}

void OverlayDrawer::ReleaseParameterInput() noexcept {
	if (!_parameterFocusSwitchingEnabled) return;
	_previewEscapeOwned = false;
	_previewEscapeCanClose = _previewClosePending = false;
	const bool editing = IsEditingParameters();
	_EndParameterInput(true);
	_imguiImpl.ParameterPreview(false);
	if (editing) _parameterPanelState = ParameterPanelState::Preview;
}

void OverlayDrawer::SuspendParameterInput() noexcept {
	if (_parameterInputTransition || (!IsEditingParameters() && !_parameterResumeClickPending)) return;
	_SetParameterPanelState(ParameterPanelState::Preview, false);
}

void OverlayDrawer::UpdateParameterInputHost() noexcept {
	if (!_parameterFocusSwitchingEnabled) return;
	if (_parameterHostCanceled) {
		// Retirement owns cleanup after the owner cut. A standalone host death
		// can clear ImGui/cursor input here, after its destruction callback returns.
		if (const auto renderer = ScalingWindow::Get().TryGetRenderer(); renderer && renderer->IsStopping()) return;
		_EndParameterInput(false);
	}
	if (_parameterEditPending) {
		if (_parameterEditRunId != ScalingWindow::RunId() || !_HasParameterForeground()) _parameterEditPending = false;
		else if (IsWindowVisible(ScalingWindow::Get().Handle())) _SetParameterPanelState(ParameterPanelState::Edit, false, "restore after first display");
	}
	if (_parameterResumeClickPending && !_HasParameterForeground()) SuspendParameterInput();
	if (std::exchange(_previewClosePending, false) &&
		_parameterPanelState == ParameterPanelState::Preview && _HasParameterForeground())
		_SetParameterPanelState(ParameterPanelState::Closed, false);
	_UpdateParameterPreviewHost();
	if (!IsEditingParameters() || _parameterInputTransition) return;
	if (!_OwnsParameterInput() || !IsWindowVisible(ScalingWindow::Get().Handle())) {
		Logger::Get().Info(fmt::format("Parameter input canceled: run={} currentRun={} foreground={:#x} threadFocus={:#x}",
			_parameterInputRunId, ScalingWindow::RunId(), uintptr_t(Win32Helper::GetRealForegroundWindow()), uintptr_t(GetFocus())));
		SuspendParameterInput();
		return;
	}
	_SyncInheritedParameterKeys();
	const RECT& rect = ScalingWindow::Get().RendererRect();
	if (!_PositionParameterInputHost(rect)) { SuspendParameterInput(); return; }
	_FinishParameterInput();
}

std::optional<ImGuiInputResult> OverlayDrawer::_HandleParameterInputMessage(
	HWND sourceWindow, UINT msg, WPARAM wParam, LPARAM lParam) noexcept {
	// The host can be destroyed by USER32 before its output's WM_DESTROY.
	// Cancellation has already invalidated the old Renderer on explicit stops;
	// never reactivate or render it from a nested input callback during teardown.
	if (const auto renderer = ScalingWindow::Get().TryGetRenderer(); renderer && renderer->IsStopping()) return std::nullopt;
	if (!_parameterFocusSwitchingEnabled) return std::nullopt;
	if (_parameterInputTransition) return std::nullopt;
	const bool fromHost = sourceWindow && sourceWindow == _hwndParameterInput;
	if (IsEditingParameters() && !_OwnsParameterInput()) {
		SuspendParameterInput();
		return ImGuiInputResult::Redraw;
	}
	if (IsEditingParameters() && !fromHost && (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN ||
		msg == WM_KEYUP || msg == WM_SYSKEYUP || msg == WM_CHAR)) return ImGuiInputResult::Redraw;
	if (msg == WM_CANCELMODE && fromHost) {
		_SetParameterPanelState(ParameterPanelState::Preview, false);
		return ImGuiInputResult::Urgent;
	}
	auto queue = [&](std::optional<POINT> point = std::nullopt) {
		const auto result = _imguiImpl.MessageHandler(msg, wParam, lParam, point);
		if (result != ImGuiInputResult::None) _overlayDirty = true;
		return result;
	};
	int button = -1;
	bool down = false;
	switch (msg) {
	case WM_LBUTTONDOWN: case WM_NCLBUTTONDOWN: down = true; [[fallthrough]];
	case WM_LBUTTONUP: case WM_NCLBUTTONUP: button = 0; break;
	case WM_RBUTTONDOWN: case WM_NCRBUTTONDOWN: down = true; [[fallthrough]];
	case WM_RBUTTONUP: case WM_NCRBUTTONUP: button = 1; break;
	case WM_MBUTTONDOWN: case WM_NCMBUTTONDOWN: down = true; [[fallthrough]];
	case WM_MBUTTONUP: case WM_NCMBUTTONUP: button = 2; break;
	case WM_XBUTTONDOWN: case WM_NCXBUTTONDOWN: down = true; [[fallthrough]];
	case WM_XBUTTONUP: case WM_NCXBUTTONUP: button = GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? 3 : 4; break;
	}
	if (button >= 0) {
		// The event position is immutable even if the cursor or host moved
		// while messages waited for rendering. Both HWND routes use screen coordinates.
		const DWORD position = GetMessagePos();
		const POINT point{ GET_X_LPARAM(position), GET_Y_LPARAM(position) };
		if (down && _parameterPanelState == ParameterPanelState::Preview &&
			!_parameterResumeClickPending && _HasParameterForeground() &&
			_imguiImpl.IsParameterPreviewAt(point)) {
			_SetParameterPanelState(ParameterPanelState::Edit, true, "preview click");
			_parameterResumeClickPending = !IsEditingParameters();
			_parameterHeldButtons = 1u << button;
			SetCapture(_hwndParameterInput);
			if (IsEditingParameters()) queue(point);
			return ImGuiInputResult::Urgent;
		}
		if (_parameterResumeClickPending) {
			if (down) _parameterHeldButtons |= 1u << button;
			else _parameterHeldButtons &= ~(1u << button);
			if (!_parameterHeldButtons) {
				_parameterResumeClickPending = false;
				if (GetCapture() == _hwndParameterInput) ReleaseCapture();
				_FinishParameterInput();
				_UpdateParameterPreviewHost();
			}
			return ImGuiInputResult::Urgent;
		}
		if (!IsEditingParameters()) return std::nullopt;
		if (down) {
			if (!_parameterHeldButtons && !_imguiImpl.OwnsPointerAt(point)) {
				_returnClickPending = true;
				_pendingParameterPanelState = ParameterPanelState::Preview;
			}
			_parameterHeldButtons |= 1u << button;
			SetCapture(_hwndParameterInput);
		} else _parameterHeldButtons &= ~(1u << button);
		if (!_returnClickPending) queue(point);
		if (!_parameterHeldButtons && GetCapture() == _hwndParameterInput) ReleaseCapture();
		_FinishParameterInput();
		return ImGuiInputResult::Urgent;
	}
	if (msg == WM_CAPTURECHANGED && reinterpret_cast<HWND>(lParam) != _hwndParameterInput &&
		reinterpret_cast<HWND>(lParam) != ScalingWindow::Get().Handle()) {
		const bool lostPress = _parameterHeldButtons != 0 || _parameterResumeClickPending;
		_parameterHeldButtons = 0;
		_parameterResumeClickPending = false;
		if (fromHost && lostPress) {
			_SetParameterPanelState(ParameterPanelState::Preview, false);
			return ImGuiInputResult::Urgent;
		}
	}
	if (!IsEditingParameters()) return std::nullopt;
	if (msg == WM_KILLFOCUS || (msg == WM_ACTIVATEAPP && !wParam)) {
		// The scaling window loses focus when its parameter host takes it.
		// A delayed loss message must not undo that successful handoff.
		if (fromHost && Win32Helper::GetRealForegroundWindow() != _hwndParameterInput) SuspendParameterInput();
		return ImGuiInputResult::Redraw;
	}
	if (msg == WM_TIMER && fromHost) {
		UpdateParameterInputHost();
		return ImGuiInputResult::Redraw;
	}
	if (msg == WM_CLOSE && fromHost) {
		_SetParameterPanelState(ParameterPanelState::Closed);
		return ImGuiInputResult::Urgent;
	}
	if (_parameterResumeClickPending &&
		(msg == WM_MOUSEMOVE || msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL)) return ImGuiInputResult::Redraw;
	if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYUP) {
		const bool keyDown = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
		UINT key = UINT(wParam);
		if (key == VK_LCONTROL || key == VK_RCONTROL) key = VK_CONTROL;
		else if (key == VK_LSHIFT || key == VK_RSHIFT) key = VK_SHIFT;
		else if (key == VK_LMENU || key == VK_RMENU) key = VK_MENU;
		if (key < 256) {
			_parameterInheritedKeys[key] = false;
			_parameterHeldKeys[key] = keyDown;
		}
		if (wParam == VK_ESCAPE) {
			if (keyDown && !(lParam & (1LL << 30))) _escapePending = true;
			if (!keyDown && std::exchange(_escapePending, false)) {
				if (!_imguiImpl.DismissParameterPopup()) _SetParameterPanelState(ParameterPanelState::Preview);
				_overlayDirty = true;
			}
			return ImGuiInputResult::Urgent;
		}
		return queue();
	}
	if (msg == WM_CHAR || msg == WM_MOUSEMOVE || msg == WM_NCMOUSEMOVE ||
		msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL || msg == WM_CANCELMODE || msg == WM_CAPTURECHANGED) {
		// An Alt-based hotkey may leave a scaling-window cancellation queued
		// before the host takes focus. Actual host cancellation still clears input.
		if (msg == WM_CANCELMODE && !fromHost && Win32Helper::GetRealForegroundWindow() == _hwndParameterInput)
			return ImGuiInputResult::Redraw;
		return queue();
	}
	return std::nullopt;
}

LRESULT CALLBACK OverlayDrawer::_ParameterInputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept {
	if (msg == WM_NCCREATE) {
		auto self = static_cast<OverlayDrawer*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
	}
	auto self = reinterpret_cast<OverlayDrawer*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	if (!self) return DefWindowProc(hwnd, msg, wParam, lParam);
	if (msg == WM_NCDESTROY) {
		self->_hwndParameterInput = nullptr;
		self->_parameterHostCanceled = true;
		self->_parameterInputOwned = false;
		self->_parameterInputRunId = 0;
		self->_parameterEditPending = false;
		self->_previewEscapeOwned = false;
		if (self->_parameterPanelState == ParameterPanelState::Edit || self->_parameterResumeClickPending) {
			self->_parameterPanelState = self->_pendingParameterPanelState == ParameterPanelState::Closed
				? ParameterPanelState::Closed : ParameterPanelState::Preview;
			self->_isEffectParametersVisible = self->_parameterPanelState != ParameterPanelState::Closed;
		}
		self->_parameterHeldButtons = 0;
		self->_parameterHeldKeys.fill(false);
		self->_parameterInheritedKeys.fill(false);
		self->_returnClickPending = self->_escapePending = self->_parameterResumeClickPending = false;
		self->_overlayDirty = true;
		SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	if (msg == WM_NCHITTEST || msg == WM_MOUSEACTIVATE) {
		const bool available = self->_HasParameterForeground() && IsWindowVisible(ScalingWindow::Get().Handle());
		return msg == WM_NCHITTEST ? (available ? HTCLIENT : HTTRANSPARENT) :
			(available ? MA_ACTIVATE : MA_NOACTIVATEANDEAT);
	}
	if (msg == WM_ERASEBKGND) return 1;
	if (msg == WM_PAINT) { PAINTSTRUCT ps{}; BeginPaint(hwnd, &ps); EndPaint(hwnd, &ps); return 0; }
	if (msg == WM_SETCURSOR) {
		SetCursor(LoadCursor(nullptr, self->IsToolbarMoveCursor() ? IDC_SIZEALL : IDC_ARROW));
		return TRUE;
	}
	if (self->_HandleParameterInputMessage(hwnd, msg, wParam, lParam)) {
		if ((msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) && wParam != VK_ESCAPE)
			return DefWindowProc(hwnd, msg, wParam, lParam);
		return msg == WM_XBUTTONDOWN || msg == WM_XBUTTONUP ? TRUE : 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

}
