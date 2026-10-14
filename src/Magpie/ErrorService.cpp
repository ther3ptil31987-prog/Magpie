#include "pch.h"
#include "ErrorService.h"
#include "App.h"
#include "CommonSharedConstants.h"
#include "Logger.h"
#include "StrHelper.h"
#include "ToastService.h"
#include "Win32Helper.h"
#include "AdaptersService.h"
#include "AppSettings.h"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace Magpie {

static const wchar_t* MessageKey(ScalingError error) noexcept {
	switch (error) {
	case ScalingError::InvalidScalingMode: return L"Message_InvalidScalingMode";
	case ScalingError::TouchSupport: return L"Message_TouchSupport";
	case ScalingError::Windowed3DGameMode: return L"Message_Windowed3DGameMode";
	case ScalingError::WindowedDesktopDuplication: return L"Message_WindowedDesktopDuplication";
	case ScalingError::InvalidSourceWindow: return L"Message_InvalidSourceWindow";
	case ScalingError::Maximized: return L"Message_Maximized";
	case ScalingError::WindowIntegrityQueryFailed: return L"Message_WindowIntegrityQueryFailed";
	case ScalingError::DlssNrStopped: return L"Message_DlssNrStopped";
	case ScalingError::LowIntegrityLevel: return L"Message_LowIntegrityLevel";
	case ScalingError::InvalidCropping: return L"Message_InvalidCropping";
	case ScalingError::BannedInWindowedMode: return L"Message_BannedInWindowedMode";
	case ScalingError::ScalingFailedGeneral: return L"Message_ScalingFailedGeneral";
	case ScalingError::CaptureFailed: return L"Message_CaptureFailed";
	case ScalingError::HdrComponentExpectedHdr: return L"Message_HdrComponentExpectedHdr";
	case ScalingError::HdrComponentExpectedSdr: return L"Message_HdrComponentExpectedSdr";
	case ScalingError::HdrComponentMissingPair: return L"Message_HdrComponentMissingPair";
	case ScalingError::HdrComponentInvalidParameters: return L"Message_HdrComponentInvalidParameters";
	case ScalingError::HdrCaptureRequired: return L"Message_HdrCaptureRequired";
	case ScalingError::HdrDisplayRequired: return L"Message_HdrDisplayRequired";
	case ScalingError::HdrCaptureMethodRequired: return L"Message_HdrCaptureMethodRequired";
	case ScalingError::RtxHdrUnavailable: return L"Message_RtxHdrUnavailable";
	case ScalingError::CaptureMethodUnavailable: return L"Message_CaptureMethodUnavailable";
	case ScalingError::CreateFenceFailed: return L"Message_CreateFenceFailed";
	case ScalingError::NvidiaVsrPathUnsupported: return L"Message_NvidiaVsrPathUnsupported";
	case ScalingError::OpticalFlowProviderUnavailable: return L"Message_OpticalFlowProviderUnavailable";
	case ScalingError::NvidiaOpticalFlowUnsupported: return L"Message_NvidiaOpticalFlowUnsupported";
	case ScalingError::NvidiaOpticalFlowQualityUnsupported: return L"Message_NvidiaOpticalFlowQualityUnsupported";
	case ScalingError::AmdOpticalFlowUnsupported: return L"Message_AmdOpticalFlowUnsupported";
	case ScalingError::OpticalFlowInteropFailed: return L"Message_OpticalFlowInteropFailed";
	case ScalingError::ConflictingFrameGenerationEffects: return L"Message_ConflictingFrameGenerationEffects";
	case ScalingError::XeSSMfgOpticalFlowUnsupported: return L"Message_XeSSMfgOpticalFlowUnsupported";
	case ScalingError::XeSSMfgCompatibilityUnavailable: return L"Message_XeSSMfgCompatibilityUnavailable";
	case ScalingError::XeSSMfgUnsupported: return L"Message_XeSSMfgUnsupported";
	case ScalingError::XeSSMfgMultiplierUnsupported: return L"Message_XeSSMfgMultiplierUnsupported";
	case ScalingError::ScalingModeNotSelected: return L"Message_ScalingModeNotSelected";
	case ScalingError::ScalingModeEmpty: return L"Message_ScalingModeEmpty";
	case ScalingError::ScalingModeUnknownEffect: return L"Message_ScalingModeUnknownEffect";
	case ScalingError::GraphicsDeviceInitFailed: return L"Message_GraphicsDeviceInitFailed";
	case ScalingError::PresentationInitFailed: return L"Message_PresentationInitFailed";
	case ScalingError::FSR3FGEffectChainUnsupported: return L"Message_FSR3FGEffectChainUnsupported";
	case ScalingError::FSR3FGHdrUnsupported: return L"Message_FSR3FGHdrUnsupported";
	case ScalingError::EffectCompileFailed: return L"Message_EffectCompileFailed";
	case ScalingError::EffectResourceFailed: return L"Message_EffectResourceFailed";
	case ScalingError::NativeEffectInitFailed: return L"Message_NativeEffectInitFailed";
	case ScalingError::FrameGenerationInitFailed: return L"Message_FrameGenerationInitFailed";
	case ScalingError::OverlayInitFailed: return L"Message_OverlayInitFailed";
	case ScalingError::SharedTextureOpenFailed: return L"Message_SharedTextureOpenFailed";
	case ScalingError::DlssNrUnavailable: return L"Message_DlssNrUnavailable";
	case ScalingError::FrameGenerationDisabled: return L"Message_FrameGenerationDisabled";
	case ScalingError::ConfigurationWriteFailed: return L"Message_ConfigurationWriteFailed";
	case ScalingError::EffectParameterConflict: return L"Message_EffectParameterConflict";
	case ScalingError::EffectParameterLiveFailed: return L"Message_EffectParameterLiveFailed";
	case ScalingError::ScreenshotDirectoryFailed: return L"Message_ScreenshotDirectoryFailed";
	case ScalingError::ScreenshotEncodeFailed: return L"Message_ScreenshotEncodeFailed";
	case ScalingError::ScreenshotWriteFailed: return L"Message_ScreenshotWriteFailed";
	case ScalingError::ScreenshotReadbackFailed: return L"Message_ScreenshotReadbackFailed";
	case ScalingError::SourceWindowClosed: return L"Message_SourceWindowClosed";
	case ScalingError::SourceWindowUnresponsive: return L"Message_SourceWindowUnresponsive";
	case ScalingError::SourceWindowTooSmall: return L"Message_SourceWindowTooSmall";
	case ScalingError::SourceWindowOffscreen: return L"Message_SourceWindowOffscreen";
	case ScalingError::SourceWindowUnsupported: return L"Message_SourceWindowUnsupported";
	case ScalingError::SourceWindowGeometryFailed: return L"Message_SourceWindowGeometryFailed";
	case ScalingError::ScalingAlreadyActive: return L"Message_ScalingAlreadyActive";
	case ScalingError::ScalingWindowCreationFailed: return L"Message_ScalingWindowCreationFailed";
	case ScalingError::DisplayLayoutFailed: return L"Message_DisplayLayoutFailed";
	case ScalingError::ImportReadFailed: return L"Message_ImportReadFailed";
	case ScalingError::ImportEmpty: return L"Message_ImportEmpty";
	case ScalingError::ImportInvalidJson: return L"Message_ImportInvalidJson";
	case ScalingError::ImportWrongFileType: return L"Message_ImportWrongFileType";
	case ScalingError::ImportIncompatible: return L"Message_ImportIncompatible";
	case ScalingError::ExportWriteFailed: return L"Message_ExportWriteFailed";
	case ScalingError::FileDialogFailed: return L"Message_FileDialogFailed";
	case ScalingError::PassThroughUnavailable: return L"Message_PassThroughUnavailable";
	case ScalingError::NgxRestartRequired: return L"Message_NgxRestartRequired";
	case ScalingError::ScreenshotIntermediateEncodeFailed: return L"Message_ScreenshotIntermediateEncodeFailed";
	case ScalingError::ConfigurationRecoveredBackup: return L"Message_ConfigurationRecoveredBackup";
	case ScalingError::ConfigurationRecoveredPartial: return L"Message_ConfigurationRecoveredPartial";
	case ScalingError::ConfigurationRepaired: return L"Message_ConfigurationRepaired";
	case ScalingError::ConfigurationResetDefaults: return L"Message_ConfigurationResetDefaults";
	case ScalingError::DuplicateScalingModeNames: return L"Message_DuplicateScalingModeNames";
	default: return L"Message_ScalingFailedGeneral";
	}
}

static std::filesystem::path IssueRecordPath() {
	return AppSettings::Get().ConfigDir() / L"last-issue.json";
}

void ErrorService::Initialize() noexcept {
	try {
		const auto path = IssueRecordPath();
		std::error_code ec;
		const auto size = std::filesystem::file_size(path, ec);
		if (ec || size > 128 * 1024) return;
		std::string json;
		if (!Win32Helper::ReadTextFile(path.c_str(), json)) return;
		rapidjson::Document doc;
		doc.Parse(json.data(), json.size());
		if (!doc.IsObject() || !doc.HasMember("summary") || !doc["summary"].IsString() ||
			!doc.HasMember("details") || !doc["details"].IsString()) return;
		const auto loader = winrt::ResourceLoader::GetForViewIndependentUse(CommonSharedConstants::APP_RESOURCE_MAP_ID);
		_summary = winrt::hstring(fmt::format(fmt::runtime(std::wstring_view(loader.GetString(L"ErrorDetails_PreviousRun"))),
			StrHelper::UTF8ToUTF16(doc["summary"].GetString())));
		_details = winrt::hstring(std::wstring(loader.GetString(L"ErrorDetails_PreviousRunHint")) + L"\n\n" +
			StrHelper::UTF8ToUTF16(doc["details"].GetString()));
		// Never replay actions for a previous run: unsaved drafts and profiles may have changed.
		_action = IssueAction::None;
		_isInformational = true;
		_isIssueVisible = true;
		++_revision;
	} catch (...) {
		Logger::Get().Warn("Unable to restore last diagnostic record");
	}
}

void ErrorService::_Persist() noexcept {
	try {
		rapidjson::StringBuffer json;
		rapidjson::Writer<rapidjson::StringBuffer> writer(json);
		writer.StartObject();
		const std::string summary = StrHelper::UTF16ToUTF8(_summary);
		const std::string details = StrHelper::UTF16ToUTF8(_details);
		writer.Key("summary"); writer.String(summary.c_str(), static_cast<rapidjson::SizeType>(summary.size()));
		writer.Key("details"); writer.String(details.c_str(), static_cast<rapidjson::SizeType>(details.size()));
		writer.EndObject();
		const auto path = IssueRecordPath();
		const auto temporary = std::filesystem::path(path.native() + L".tmp");
		if (Win32Helper::WriteTextFile(temporary.c_str(), { json.GetString(), json.GetLength() })) {
			MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
		}
	} catch (...) {
		// A diagnostic write failure must not replace the user's original problem.
	}
}

void ErrorService::ConfigurationSaved(uint64_t saveRevision, uint64_t submittedIssueRevision) noexcept {
	winrt::Magpie::implementation::App::Get().Dispatcher().TryEnqueue([this, saveRevision, submittedIssueRevision] {
		if (!_saveOutcomes.Record(saveRevision, true)) return;
		// A failure may have reached the UI after this successful save was submitted.
		// Compare save versions as well as the issue snapshot, and preserve unrelated issues.
		if (_saveOutcomes.ResolvesCurrentIssue(saveRevision, submittedIssueRevision, _revision, _action)) {
			SavedSuccessfully(_revision);
		}
	});
}

void ErrorService::SavedSuccessfully(uint64_t issueRevision) noexcept {
	if (_revision != issueRevision || _action != IssueAction::RetrySave) return;
	try {
		const auto loader = winrt::ResourceLoader::GetForViewIndependentUse(CommonSharedConstants::APP_RESOURCE_MAP_ID);
		_summary = loader.GetString(L"ErrorDetails_SaveSucceeded");
		_isInformational = true;
		_details = winrt::hstring(std::wstring(_summary) + L"\n\n" + std::wstring(_details));
		_action = IssueAction::None;
		++_revision;
		_Persist();
		Changed.Invoke();
	} catch (...) {}
}

void ErrorService::DismissIssue() {
	if (!_isIssueVisible) return;
	_isIssueVisible = false;
	Changed.Invoke();
}

void ErrorService::BeginScalingSession(std::shared_ptr<DlssNrSessionState> session, IssueContext context) {
	_dlssNrSession = std::move(session);
	_dlssNrContext = std::move(context);
	Changed.Invoke();
}

void ErrorService::DlssNrStatusChanged(uint32_t runId, uint32_t index, const DlssNrStatus& status, IssueContext context) {
	context.runId = runId;
	context.effectIndex = index;
	context.dlssNrStatus = status;
	winrt::Magpie::implementation::App::Get().Dispatcher().TryEnqueue([this, context = std::move(context)] {
		if (context.dlssNrSession != _dlssNrSession || !_dlssNrSession) return;
		const auto current = _dlssNrSession->Read();
		if (current.runId != context.runId) return;
		Changed.Invoke();
		if (context.effectIndex >= current.effects.size() || !context.dlssNrStatus->Failed()) return;
		const auto& status = current.effects[context.effectIndex];
		if (!status.Failed() || status.failure != context.dlssNrStatus->failure) return;
		Report(status.state == DlssNrState::Stopped ? ScalingError::DlssNrStopped : ScalingError::DlssNrUnavailable,
			{}, nullptr, 0, context);
	});
}

std::wstring ErrorService::DlssNrSummary() const {
	if (!_dlssNrSession) return {};
	const auto loader = winrt::ResourceLoader::GetForViewIndependentUse(CommonSharedConstants::APP_RESOURCE_MAP_ID);
	const auto snapshot = _dlssNrSession->Read();
	std::wstring text;
	for (size_t i = 0; i < snapshot.effects.size() && i < _dlssNrContext.effectNames.size(); ++i) {
		if (_dlssNrContext.effectNames[i] != L"DLSSNR\\DLSSNR_AI_Filter") continue;
		const auto& status = snapshot.effects[i];
		if (!text.empty()) text += L"\n\n";
		text += fmt::format(L"{} / #{} DLSSNR: {}", _dlssNrContext.scalingModeName, i + 1,
			std::wstring(loader.GetString(DlssNrStateKey(status.state))));
		if (status.Failed()) {
			text += L"\n" + std::wstring(loader.GetString(DlssNrReasonKey(status.failure.reason)));
			text += L"\n" + std::wstring(loader.GetString(DlssNrAdviceKey(status)));
		}
	}
	return text;
}

void ErrorService::Report(ScalingError error, std::string context, HWND target,
	uint32_t systemError, IssueContext issueContext, uint64_t saveRevision) noexcept {
	if (error == ScalingError::NoError) return;
	try {
		const auto diagnostic = fmt::format("Diagnostic MP-{:03}: system={} context={}",
			static_cast<int>(error), systemError, context);
		if (IsRecoveryNotice(error) || error == ScalingError::DuplicateScalingModeNames) Logger::Get().Info(diagnostic);
		else Logger::Get().Error(diagnostic);
		Logger::Get().Flush();
		winrt::Magpie::implementation::App::Get().Dispatcher().TryEnqueue([this, error, context = std::move(context), target, systemError,
			issueContext = std::move(issueContext), saveRevision]() mutable {
			try {
				// A late renderer completion must not replace a newer session's issue.
				if (issueContext.dlssNrSession) {
					if (issueContext.dlssNrSession != _dlssNrSession) return;
					const auto snapshot = issueContext.dlssNrSession->Read();
					if (issueContext.runId && issueContext.runId != snapshot.runId) return;
					if (error == ScalingError::DlssNrUnavailable || error == ScalingError::DlssNrStopped || error == ScalingError::NgxRestartRequired) {
						if (issueContext.effectIndex == UINT32_MAX) {
							for (uint32_t i = 0; i < snapshot.effects.size(); ++i) if (snapshot.effects[i].Failed()) { issueContext.effectIndex = i; break; }
						}
						if (issueContext.effectIndex < snapshot.effects.size()) {
							const auto& status = snapshot.effects[issueContext.effectIndex];
							if (!status.Failed() || (issueContext.dlssNrStatus && status.failure != issueContext.dlssNrStatus->failure)) return;
							issueContext.dlssNrStatus = status;
						}
					}
				}
				if (error == ScalingError::ConfigurationWriteFailed && saveRevision) {
					if (!_saveOutcomes.Record(saveRevision, false)) return;
				}
				const auto loader = winrt::ResourceLoader::GetForViewIndependentUse(
					CommonSharedConstants::APP_RESOURCE_MAP_ID);
				if (IsRecoveryNotice(error)) {
					_startupRecoveryDetails = std::wstring(loader.GetString(MessageKey(error))) + L"\n" + StrHelper::UTF8ToUTF16(context);
					if (_action == IssueAction::RetrySave) {
						_details = winrt::hstring(std::wstring(_details) + L"\n\n" + _startupRecoveryDetails);
						_Persist();
						Changed.Invoke();
						return;
					}
				}
				const auto now = std::chrono::steady_clock::now();
				const bool repeated = _lastError == error && _lastContext == context &&
					_lastSystemError == systemError && _issueContext.profileName == issueContext.profileName &&
					_issueContext.scalingModeName == issueContext.scalingModeName &&
					_issueContext.dlssNrSession == issueContext.dlssNrSession &&
					_issueContext.effectIndex == issueContext.effectIndex && _issueContext.dlssNrStatus == issueContext.dlssNrStatus;
				_repeatCount = repeated ? _repeatCount + 1 : 1;
				_lastError = error;
				_lastContext = context;
				_lastSystemError = systemError;
				_issueContext = issueContext;
				_isInformational = IsRecoveryNotice(error) || error == ScalingError::DuplicateScalingModeNames;
				_action = GetIssueAction(error);
				if (_action == IssueAction::Profile && !issueContext.hasProfile) _action = IssueAction::None;
				++_revision;
				const wchar_t* key = MessageKey(error);
				if (error == ScalingError::CaptureFailed && issueContext.hasProfile) {
					if (issueContext.captureMethod == CaptureMethod::GraphicsCapture)
						key = L"Message_CaptureFailed_AlreadyGraphicsCapture";
					else if (CanSuggestGraphicsCapture(issueContext.captureMethod))
						key = L"Message_CaptureFailed_TryGraphicsCapture";
				}
				if (error == ScalingError::CreateFenceFailed &&
					!CanSuggestAnotherAdapter(AdaptersService::Get().AdapterInfos().size()))
					key = L"Message_CreateFenceFailed_SingleAdapter";
				_summary = loader.GetString(key);
				if (issueContext.dlssNrStatus) {
					const auto& nr = *issueContext.dlssNrStatus;
					_summary = winrt::hstring(std::wstring(loader.GetString(DlssNrStateKey(nr.state))) + L"\n" +
						std::wstring(loader.GetString(DlssNrReasonKey(nr.failure.reason))) + L"\n" +
						std::wstring(loader.GetString(DlssNrAdviceKey(nr))));
					_action = GetDlssNrIssueAction(nr, issueContext.hasProfile);
				}
				if (error == ScalingError::FSR3FGEffectChainUnsupported)
					_summary = winrt::hstring(fmt::format(fmt::runtime(std::wstring(_summary)), StrHelper::UTF8ToUTF16(context)));
				// Keep one complete, problem-specific sentence in the transient toast.
				std::wstring toastSummary(_summary);
				const auto chineseEnd = toastSummary.find(L'。');
				const auto englishEnd = toastSummary.find(L". ");
				const auto sentenceEnd = (std::min)(chineseEnd, englishEnd);
				if (sentenceEnd != std::wstring::npos) toastSummary.resize(sentenceEnd + 1);
				if (issueContext.hasProfile) {
					const std::wstring profile = issueContext.profileName.empty() ?
						std::wstring(loader.GetString(L"Root_Defaults/Content")) : issueContext.profileName;
					_summary = winrt::hstring(std::wstring(_summary) + L"\n" +
						std::wstring(loader.GetString(L"ErrorDetails_Profile")) + L": " + profile +
						(issueContext.scalingModeName.empty() ? L"" : L"; " +
							std::wstring(loader.GetString(L"ErrorDetails_Group")) + L": " + issueContext.scalingModeName));
				}
				std::wstring advice;
				const uint32_t win32Error = HRESULT_FACILITY(systemError) == FACILITY_WIN32
					? HRESULT_CODE(systemError) : systemError;
				const auto operation = GetFileOperation(error);
				if (win32Error == ERROR_ACCESS_DENIED || win32Error == ERROR_WRITE_PROTECT) {
					advice = loader.GetString(operation == FileOperation::Write ? L"ErrorAdvice_AccessDenied" :
						operation == FileOperation::Read ? L"ErrorAdvice_ReadAccessDenied" : L"ErrorAdvice_OperationAccessDenied");
				} else if (win32Error == ERROR_SHARING_VIOLATION || win32Error == ERROR_LOCK_VIOLATION) {
					advice = loader.GetString(L"ErrorAdvice_FileBusy");
				} else if (operation == FileOperation::Write && (win32Error == ERROR_DISK_FULL || win32Error == ERROR_HANDLE_DISK_FULL)) {
					advice = loader.GetString(L"ErrorAdvice_DiskFull");
				} else if (win32Error == ERROR_PATH_NOT_FOUND || win32Error == ERROR_FILE_NOT_FOUND) {
					advice = loader.GetString(L"ErrorAdvice_PathMissing");
				}
				SYSTEMTIME time{};
				GetLocalTime(&time);
				std::wstring details(_summary);
				if (error == ScalingError::ConfigurationWriteFailed && !_startupRecoveryDetails.empty())
					details += L"\n\n" + _startupRecoveryDetails;
				if (!advice.empty()) details += L"\n\n" + advice;
				auto field = [&](std::wstring_view key, std::wstring_view value) {
					details += L"\n" + std::wstring(loader.GetString(key)) + L": " + std::wstring(value);
				};
				details += L"\n";
				field(L"ErrorDetails_Time", fmt::format(L"{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
					time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond));
				field(L"ErrorDetails_Code", fmt::format(L"MP-{:03}", static_cast<int>(error)));
#ifdef MP_VERSION_STRING
				field(L"ErrorDetails_Version", StrHelper::UTF8ToUTF16(STRINGIFY(MP_VERSION_STRING)));
#endif
				if (!context.empty()) field(L"ErrorDetails_Context", StrHelper::UTF8ToUTF16(context));
				if (issueContext.dlssNrStatus) {
					const auto& nr = *issueContext.dlssNrStatus;
					constexpr const char* stages[]{ "initialize", "load-library", "initialize-library", "create-feature", "evaluate", "resize" };
					constexpr const char* kinds[]{ "none", "Win32", "HRESULT", "NGX", "SEH" };
					details += StrHelper::UTF8ToUTF16(fmt::format("\nRun={} / effect=#{} / stage={} / operation={} / {}=0x{:08X}\n{}",
						issueContext.runId, issueContext.effectIndex + 1, stages[static_cast<size_t>(nr.failure.stage)],
						nr.failure.operation, kinds[static_cast<size_t>(nr.failure.codeKind)], nr.failure.code, nr.environment));
				}
				if (!issueContext.windowTitle.empty()) field(L"ErrorDetails_Window", issueContext.windowTitle);
				if (issueContext.hasProfile) {
					constexpr const wchar_t* methods[] = { L"Graphics Capture", L"Desktop Duplication", L"GDI", L"DwmSharedSurface" };
					const int method = static_cast<int>(issueContext.captureMethod);
					if (method >= 0 && method < 4) field(L"ErrorDetails_Capture", methods[method]);
				}
				if (systemError) field(L"ErrorDetails_SystemCode", fmt::format(L"{} (0x{:08X})", systemError, systemError));
				field(L"ErrorDetails_Count", std::to_wstring(_repeatCount));
				field(L"ErrorDetails_Logs", (Win32Helper::GetExePath().parent_path() / L"logs").native());
				_details = winrt::hstring(details);
				_Persist();
				_isIssueVisible = true;
				Changed.Invoke();
				// Do not flood a continuous save/live-update failure with popups.
				if (!repeated || now - _lastToast >= std::chrono::seconds(15)) {
					_lastToast = now;
					const std::wstring message = (IsRecoveryNotice(error) ?
						std::wstring(loader.GetString(L"ErrorDetails_RecoveryToast")) : error == ScalingError::ConfigurationWriteFailed ?
						std::wstring(loader.GetString(L"ErrorDetails_SaveFailedToast")) : toastSummary) + L"\n" +
						std::wstring(loader.GetString(L"ErrorDetails_HomeHint"));
					if (target && IsWindow(target)) {
						ToastService::Get().ShowMessageOnWindow({}, message, target, std::chrono::seconds(7));
					} else {
						ToastService::Get().ShowMessageInApp({}, message, std::chrono::seconds(7));
					}
				}
			} catch (...) {
				Logger::Get().Error("Unable to present diagnostic; original error was logged");
			}
		});
	} catch (...) {
		Logger::Get().Error("Unable to enqueue diagnostic");
	}
}

}
