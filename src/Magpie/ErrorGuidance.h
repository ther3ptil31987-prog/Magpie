#pragma once
#include "ScalingOptions.h"

namespace Magpie {

enum class IssueAction { None, Profile, Effects, RetrySave, ConfigurationDirectory, ProgramDirectory, RecoveryGuide };
enum class FileOperation { None, Read, Write };

constexpr IssueAction GetDlssNrIssueAction(const DlssNrStatus& status, bool hasProfile) noexcept {
	if (status.restartRequired) return IssueAction::RecoveryGuide;
	switch (status.failure.reason) {
	case DlssNrReason::LibraryMissing:
	case DlssNrReason::LibraryLoadFailed:
	case DlssNrReason::MissingExports:
	case DlssNrReason::BuildDisabled: return IssueAction::ProgramDirectory;
	case DlssNrReason::UnsupportedAdapter: return hasProfile ? IssueAction::Profile : IssueAction::Effects;
	default: return IssueAction::Effects;
	}
}

// UI-thread outcome ordering; background completions can arrive out of order.
struct ConfigurationSaveOutcomes {
	uint64_t latestRevision = 0;
	uint64_t lastFailureRevision = 0;

	constexpr bool Record(uint64_t revision, bool succeeded) noexcept {
		if (revision < latestRevision) return false;
		latestRevision = revision;
		if (!succeeded) lastFailureRevision = revision;
		return true;
	}

	constexpr bool ResolvesCurrentIssue(uint64_t savedRevision, uint64_t submittedIssueRevision,
		uint64_t currentIssueRevision, IssueAction currentAction) const noexcept {
		return savedRevision >= latestRevision && currentAction == IssueAction::RetrySave &&
			(currentIssueRevision == submittedIssueRevision ||
				(lastFailureRevision && lastFailureRevision <= savedRevision));
	}
};

constexpr FileOperation GetFileOperation(ScalingError error) noexcept {
	switch (error) {
	case ScalingError::ImportReadFailed: return FileOperation::Read;
	case ScalingError::ConfigurationWriteFailed:
	case ScalingError::ExportWriteFailed:
	case ScalingError::ScreenshotDirectoryFailed:
	case ScalingError::ScreenshotWriteFailed: return FileOperation::Write;
	default: return FileOperation::None;
	}
}

constexpr IssueAction GetIssueAction(ScalingError error) noexcept {
	switch (error) {
	case ScalingError::ConfigurationWriteFailed: return IssueAction::RetrySave;
	case ScalingError::ConfigurationRecoveredBackup:
	case ScalingError::ConfigurationRepaired:
	case ScalingError::ConfigurationResetDefaults:
	case ScalingError::ConfigurationRecoveredPartial: return IssueAction::ConfigurationDirectory;
	case ScalingError::HdrComponentExpectedHdr:
	case ScalingError::HdrComponentExpectedSdr:
	case ScalingError::HdrComponentMissingPair:
	case ScalingError::HdrComponentInvalidParameters:
	case ScalingError::HdrDisplayRequired:
	case ScalingError::RtxHdrUnavailable:
	case ScalingError::ScalingModeEmpty:
	case ScalingError::DuplicateScalingModeNames:
	case ScalingError::ScalingModeUnknownEffect:
	case ScalingError::EffectCompileFailed:
	case ScalingError::EffectResourceFailed:
	case ScalingError::NativeEffectInitFailed:
	case ScalingError::DlssNrStopped:
	case ScalingError::DlssNrUnavailable:
	case ScalingError::FrameGenerationInitFailed:
	case ScalingError::FrameGenerationDisabled:
	case ScalingError::ConflictingFrameGenerationEffects:
	case ScalingError::OpticalFlowProviderUnavailable:
	case ScalingError::NvidiaOpticalFlowUnsupported:
	case ScalingError::NvidiaOpticalFlowQualityUnsupported:
	case ScalingError::AmdOpticalFlowUnsupported:
	case ScalingError::OpticalFlowInteropFailed:
	case ScalingError::XeSSMfgOpticalFlowUnsupported:
	case ScalingError::XeSSMfgCompatibilityUnavailable:
	case ScalingError::XeSSMfgUnsupported:
	case ScalingError::XeSSMfgMultiplierUnsupported: return IssueAction::Effects;
	case ScalingError::ScalingModeNotSelected:
	case ScalingError::InvalidScalingMode:
	case ScalingError::CaptureFailed:
	case ScalingError::HdrCaptureRequired:
	case ScalingError::HdrCaptureMethodRequired:
	case ScalingError::CaptureMethodUnavailable:
	case ScalingError::CreateFenceFailed:
	case ScalingError::GraphicsDeviceInitFailed:
	case ScalingError::WindowedDesktopDuplication:
	case ScalingError::Windowed3DGameMode:
	case ScalingError::InvalidCropping: return IssueAction::Profile;
	default: return IssueAction::None;
	}
}

constexpr bool CanSuggestGraphicsCapture(CaptureMethod method) noexcept {
	return method > CaptureMethod::GraphicsCapture && method < CaptureMethod::COUNT;
}

constexpr bool CanSuggestAnotherAdapter(size_t adapterCount) noexcept {
	return adapterCount > 1;
}

constexpr bool IsRecoveryNotice(ScalingError error) noexcept {
	return error == ScalingError::ConfigurationRecoveredBackup ||
		error == ScalingError::ConfigurationRecoveredPartial ||
		error == ScalingError::ConfigurationRepaired || error == ScalingError::ConfigurationResetDefaults;
}

}
