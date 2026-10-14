#pragma once
#include "Event.h"
#include "ScalingOptions.h"
#include "ErrorGuidance.h"

namespace Magpie {

// Snapshot at activation; never resolve the source by the current foreground window.
struct IssueContext {
	bool hasProfile = false;
	std::wstring profileName;
	std::wstring profilePath;
	std::wstring profileClass;
	std::wstring scalingModeName;
	std::wstring windowTitle;
	CaptureMethod captureMethod = CaptureMethod::GraphicsCapture;
	std::vector<std::wstring> effectNames;
	std::weak_ptr<const uint8_t> scalingModeIdentity;
	std::vector<std::weak_ptr<const uint8_t>> effectIdentities;
	uint32_t effectIndex = UINT32_MAX;
	std::shared_ptr<DlssNrSessionState> dlssNrSession;
	uint32_t runId = 0;
	std::optional<DlssNrStatus> dlssNrStatus;
};

// Reports may originate on a renderer or file-writing thread. Only the UI
// dispatcher touches the stored presentation or invokes Changed.
class ErrorService {
public:
	static ErrorService& Get() noexcept { static ErrorService service; return service; }
	void Report(ScalingError error, std::string context = {}, HWND target = nullptr,
		uint32_t systemError = 0, IssueContext issueContext = {}, uint64_t saveRevision = 0) noexcept;
	void Initialize() noexcept;
	void BeginScalingSession(std::shared_ptr<DlssNrSessionState> session, IssueContext context);
	void DlssNrStatusChanged(uint32_t runId, uint32_t index, const DlssNrStatus& status, IssueContext context);
	std::wstring DlssNrSummary() const;
	const IssueContext& DlssNrContext() const noexcept { return _dlssNrContext; }
	std::shared_ptr<DlssNrSessionState> DlssNrSession() const noexcept { return _dlssNrSession; }
	IssueAction Action() const noexcept { return _action; }
	const IssueContext& Context() const noexcept { return _issueContext; }
	uint64_t Revision() const noexcept { return _revision; }
	bool IsInformational() const noexcept { return _isInformational; }
	void SavedSuccessfully(uint64_t issueRevision) noexcept;
	void ConfigurationSaved(uint64_t saveRevision, uint64_t submittedIssueRevision) noexcept;
	bool HasIssue() const noexcept { return !_summary.empty(); }
	bool IsIssueVisible() const noexcept { return _isIssueVisible; }
	void DismissIssue();
	const winrt::hstring& Summary() const noexcept { return _summary; }
	const winrt::hstring& Details() const noexcept { return _details; }
	Event<> Changed;
private:
	std::shared_ptr<DlssNrSessionState> _dlssNrSession;
	IssueContext _dlssNrContext;
	void _Persist() noexcept;
	IssueAction _action = IssueAction::None;
	IssueContext _issueContext;
	std::wstring _startupRecoveryDetails;
	uint64_t _revision = 0;
	ConfigurationSaveOutcomes _saveOutcomes;
	bool _isIssueVisible = false;
	bool _isInformational = false;
	winrt::hstring _summary, _details;
	ScalingError _lastError = ScalingError::NoError;
	std::string _lastContext;
	uint32_t _lastSystemError = 0;
	uint32_t _repeatCount = 0;
	std::chrono::steady_clock::time_point _lastToast{};
};

}
