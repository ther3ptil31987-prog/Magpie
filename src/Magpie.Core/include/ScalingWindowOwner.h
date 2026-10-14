#pragma once
#include <windows.h>

namespace Magpie {

enum class ScalingWindowOwnerStatus {
	Applied,
	Rejected,
	NotApplied,
	InvalidWindow,
	InvalidOwner,
	ChildWindow,
	InvalidRelationship,
	ApiFailure,
	UnexpectedOwner
};

struct ScalingWindowOwnerResult {
	ScalingWindowOwnerStatus status;
	DWORD systemError;
	HWND actualOwner = nullptr;
	LONG_PTR previousOwner = 0;
	LONG_PTR outputStyle = 0;
	LONG_PTR ownerStyle = 0;

	bool IsApplied() const noexcept { return status == ScalingWindowOwnerStatus::Applied; }
	bool CanContinueUnowned() const noexcept { return status == ScalingWindowOwnerStatus::Rejected; }
	const char* StatusName() const noexcept {
		switch (status) {
		case ScalingWindowOwnerStatus::Applied: return "applied";
		case ScalingWindowOwnerStatus::Rejected: return "rejected";
		case ScalingWindowOwnerStatus::NotApplied: return "owner-not-applied";
		case ScalingWindowOwnerStatus::InvalidWindow: return "invalid-output";
		case ScalingWindowOwnerStatus::InvalidOwner: return "invalid-source";
		case ScalingWindowOwnerStatus::ChildWindow: return "child-output";
		case ScalingWindowOwnerStatus::InvalidRelationship: return "invalid-relationship";
		case ScalingWindowOwnerStatus::ApiFailure: return "api-failure";
		default: return "unexpected-owner";
		}
	}
};

inline ScalingWindowOwnerResult TrySetScalingWindowOwner(HWND window, HWND owner) noexcept {
	if (!IsWindow(window)) {
		return { ScalingWindowOwnerStatus::InvalidWindow, ERROR_INVALID_WINDOW_HANDLE };
	}
	// GWLP_HWNDPARENT on a child changes its parent, not its owner.
	if (GetWindowLongPtrW(window, GWL_STYLE) & WS_CHILD) {
		return { ScalingWindowOwnerStatus::ChildWindow, ERROR_INVALID_PARAMETER };
	}
	if (owner && !IsWindow(owner)) {
		return { ScalingWindowOwnerStatus::InvalidOwner, ERROR_INVALID_WINDOW_HANDLE };
	}
	if (owner) {
		if (window == owner || (GetWindowLongPtrW(owner, GWL_STYLE) & WS_CHILD)) {
			return { ScalingWindowOwnerStatus::InvalidRelationship, ERROR_INVALID_PARAMETER };
		}
		HWND ancestor = owner;
		for (unsigned i = 0; ancestor; ++i, ancestor = GetWindow(ancestor, GW_OWNER)) {
			if (ancestor == window || i == 64) {
				return { ScalingWindowOwnerStatus::InvalidRelationship, ERROR_INVALID_PARAMETER };
			}
		}
	}

	SetLastError(ERROR_SUCCESS);
	const LONG_PTR previousOwner = SetWindowLongPtrW(window, GWLP_HWNDPARENT,
		reinterpret_cast<LONG_PTR>(owner));
	const DWORD systemError = GetLastError();
	const HWND actualOwner = GetWindow(window, GW_OWNER);
	const LONG_PTR outputStyle = GetWindowLongPtrW(window, GWL_STYLE);
	const LONG_PTR ownerStyle = owner ? GetWindowLongPtrW(owner, GWL_STYLE) : 0;
	auto result = [&](ScalingWindowOwnerStatus status, DWORD error) {
		return ScalingWindowOwnerResult{ status, error, actualOwner, previousOwner, outputStyle, ownerStyle };
	};
	// A null readback also occurs for a destroyed HWND. Do not mistake that for
	// successful detachment or a supported unowned session.
	if (!IsWindow(window)) {
		return { ScalingWindowOwnerStatus::InvalidWindow,
			systemError ? systemError : ERROR_INVALID_WINDOW_HANDLE, actualOwner };
	}
	if (owner && !IsWindow(owner)) {
		return { ScalingWindowOwnerStatus::InvalidOwner,
			systemError ? systemError : ERROR_INVALID_WINDOW_HANDLE, actualOwner };
	}
	if (ownerStyle & WS_CHILD) {
		return result(ScalingWindowOwnerStatus::InvalidRelationship, systemError);
	}
	if (GetWindowLongPtrW(window, GWL_STYLE) & WS_CHILD) {
		return { ScalingWindowOwnerStatus::ChildWindow, ERROR_INVALID_PARAMETER, actualOwner };
	}
	if (actualOwner == owner) {
		return result(ScalingWindowOwnerStatus::Applied, systemError);
	}
	if (owner && !actualOwner) {
		// A zero error does not prove that the relationship was established.
		// Keep this distinct from both Applied and the error-87 fallback. Only
		// the coordinator can admit a verified UWP source for this outcome.
		if (systemError == ERROR_SUCCESS) {
			return result(ScalingWindowOwnerStatus::NotApplied, systemError);
		}
		return result(systemError == ERROR_INVALID_PARAMETER && !(ownerStyle & WS_CHILD)
			? ScalingWindowOwnerStatus::Rejected : ScalingWindowOwnerStatus::ApiFailure, systemError);
	}
	return result(ScalingWindowOwnerStatus::UnexpectedOwner, systemError);
}

inline bool SetScalingWindowOwner(HWND window, HWND owner) noexcept {
	const auto result = TrySetScalingWindowOwner(window, owner);
	SetLastError(result.IsApplied() ? ERROR_SUCCESS : result.systemError);
	return result.IsApplied();
}

}
