#define NOMINMAX
#include <windows.h>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <optional>
#include <wil/common.h>
#include "CommonDefines.h"
#include "ErrorGuidance.h"
#include <cassert>
#include <iostream>

using namespace Magpie;

int main() {
	assert(GetIssueAction(ScalingError::DlssNrStopped) == IssueAction::Effects);
	assert(GetIssueAction(ScalingError::WindowIntegrityQueryFailed) == IssueAction::None);
	DlssNrStatus nr{ .state = DlssNrState::Skipped };
	nr.failure.reason = DlssNrReason::LibraryMissing;
	assert(GetDlssNrIssueAction(nr, true) == IssueAction::ProgramDirectory);
	nr.restartRequired = true;
	assert(GetDlssNrIssueAction(nr, true) == IssueAction::RecoveryGuide);
	nr.restartRequired = false;
	nr.failure.reason = DlssNrReason::UnsupportedAdapter;
	assert(GetDlssNrIssueAction(nr, true) == IssueAction::Profile);
	assert(GetDlssNrIssueAction(nr, false) == IssueAction::Effects);
	nr.failure.reason = DlssNrReason::Compatibility;
	assert(GetDlssNrIssueAction(nr, true) == IssueAction::Effects);
	assert(GetIssueAction(ScalingError::CaptureMethodUnavailable) == IssueAction::Profile);
	// The next step must exist on this machine and differ from the current value.
	assert(!CanSuggestAnotherAdapter(0));
	assert(!CanSuggestAnotherAdapter(1));
	assert(CanSuggestAnotherAdapter(2));
	assert(!CanSuggestGraphicsCapture(CaptureMethod::GraphicsCapture));
	assert(CanSuggestGraphicsCapture(CaptureMethod::DesktopDuplication));
	assert(CanSuggestGraphicsCapture(CaptureMethod::GDI));
	assert(CanSuggestGraphicsCapture(CaptureMethod::DwmSharedSurface));
	assert(!CanSuggestGraphicsCapture(CaptureMethod::COUNT));
	assert(!CanSuggestGraphicsCapture(static_cast<CaptureMethod>(-1)));
	assert(!CanSuggestGraphicsCapture(static_cast<CaptureMethod>(100)));

	// Access denial while reading/selecting is not evidence of a write failure.
	assert(GetFileOperation(ScalingError::ImportReadFailed) == FileOperation::Read);
	assert(GetFileOperation(ScalingError::FileDialogFailed) == FileOperation::None);
	assert(GetFileOperation(ScalingError::CaptureFailed) == FileOperation::None);
	assert(GetFileOperation(ScalingError::ConfigurationWriteFailed) == FileOperation::Write);
	assert(GetFileOperation(ScalingError::ScreenshotWriteFailed) == FileOperation::Write);

	// Saving can be retried without restarting effects or changing a setting.
	assert(GetIssueAction(ScalingError::ConfigurationWriteFailed) == IssueAction::RetrySave);
	assert(GetIssueAction(ScalingError::CaptureFailed) == IssueAction::Profile);
	assert(GetIssueAction(ScalingError::ScalingModeUnknownEffect) == IssueAction::Effects);
	assert(GetIssueAction(ScalingError::ConflictingFrameGenerationEffects) == IssueAction::Effects);

	// A recovered configuration needs an explanation, not another failure action.
	for (auto error : { ScalingError::ConfigurationRecoveredBackup,
		ScalingError::ConfigurationRecoveredPartial, ScalingError::ConfigurationRepaired,
		ScalingError::ConfigurationResetDefaults }) {
		assert(IsRecoveryNotice(error));
		assert(GetIssueAction(error) == IssueAction::ConfigurationDirectory);
		assert(GetFileOperation(error) == FileOperation::None);
	}
	assert(!IsRecoveryNotice(ScalingError::ConfigurationWriteFailed));
	assert(GetIssueAction(static_cast<ScalingError>(10000)) == IssueAction::None);
	// Both writes start before the failure is visible. The later successful write
	// still resolves that failure even though its submitted issue snapshot is older.
	ConfigurationSaveOutcomes queued;
	assert(queued.Record(1, false));
	assert(queued.Record(2, true));
	assert(queued.ResolvesCurrentIssue(2, 0, 1, IssueAction::RetrySave));
	assert(!queued.ResolvesCurrentIssue(2, 0, 2, IssueAction::Effects));
	assert(!queued.ResolvesCurrentIssue(2, 0, 2, IssueAction::Profile));
	assert(!queued.ResolvesCurrentIssue(2, 0, 2, IssueAction::ConfigurationDirectory));

	// Reverse completion order: an old failure arriving after success must never
	// restore an already resolved warning or replace a different current issue.
	ConfigurationSaveOutcomes reversed;
	assert(reversed.Record(2, true));
	assert(!reversed.Record(1, false));
	assert(reversed.lastFailureRevision == 0);
	assert(!reversed.ResolvesCurrentIssue(2, 0, 1, IssueAction::Profile));

	// A newer failure remains visible when an older success arrives late. A fresh
	// explicit retry can resolve it, while repeated failures remain reportable.
	assert(queued.Record(3, false));
	assert(!queued.Record(2, true));
	assert(!queued.ResolvesCurrentIssue(2, 1, 1, IssueAction::RetrySave));
	assert(queued.Record(4, false));
	assert(queued.Record(5, true));
	assert(queued.ResolvesCurrentIssue(5, 4, 4, IssueAction::RetrySave));
	assert(!queued.Record(3, false));
	assert(queued.latestRevision == 5 && queued.lastFailureRevision == 4);
	assert(!queued.ResolvesCurrentIssue(5, 4, 5, IssueAction::None));
	std::cout << "Error guidance: capture/GPU boundaries, read versus write, actionable recovery "
		"and unknown-error fallback; out-of-order save outcomes passed.\n";
}
