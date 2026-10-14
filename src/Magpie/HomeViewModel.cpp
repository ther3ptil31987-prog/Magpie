#include "pch.h"
#include "HomeViewModel.h"
#if __has_include("HomeViewModel.g.cpp")
#include "HomeViewModel.g.cpp"
#endif
#include "AppSettings.h"
#include "ProfileService.h"
#include "ScalingService.h"
#include "Win32Helper.h"
#include "StrHelper.h"
#include "UpdateService.h"
#include "CommonSharedConstants.h"
#include "TouchHelper.h"
#include "LocalizationService.h"
#include "App.h"
#include "RootPage.h"
#include "FileDialogHelper.h"
#include "Logger.h"
#include "ErrorService.h"
#include "ContentDialogHelper.h"
#include <winrt/Windows.ApplicationModel.DataTransfer.h>

using namespace Magpie;

namespace winrt::Magpie::implementation {

HomeViewModel::HomeViewModel() {
	_frameRefreshModeChangedRevoker = AppSettings::Get().FrameRefreshModeChanged(auto_revoke, [this] {
		RaisePropertyChanged(L"DefaultFrameRefreshSummary");
	});
	_duplicateFrameDetectionModeChangedRevoker = AppSettings::Get().DuplicateFrameDetectionModeChanged(auto_revoke, [this] {
		RaisePropertyChanged(L"DuplicateFrameDetectionMode");
		RaisePropertyChanged(L"IsDynamicDection");
		RaisePropertyChanged(L"IsStatisticsForDynamicDetectionEnabled");
	});
	_frameRefreshChangedRevoker = ProfileService::Get().FrameRefreshChanged(auto_revoke, [this](const ::Magpie::Profile& profile) {
		if (profile.runtimeIdentity == AppSettings::Get().DefaultProfile().runtimeIdentity)
			RaisePropertyChanged(L"DefaultFrameRefreshSummary");
	});
	_issueChangedRevoker = ErrorService::Get().Changed(auto_revoke, [this] {
		RaisePropertyChanged(L"ShowDlssNrStatus");
		RaisePropertyChanged(L"DlssNrStatusSummary");
		RaisePropertyChanged(L"ShowRecentIssue");
		RaisePropertyChanged(L"RecentIssueSummary");
		RaisePropertyChanged(L"RecentIssueSeverity");
		RaisePropertyChanged(L"ShowIssueHistory");
	});
	ScalingService& ScalingService = ScalingService::Get();

	_isScalingChangedRevoker = ScalingService.IsScalingChanged(
		auto_revoke, std::bind_front(&HomeViewModel::_ScalingService_IsScalingChanged, this));
	_isTimerOnRevoker = ScalingService.IsTimerOnChanged(
		auto_revoke, std::bind_front(&HomeViewModel::_ScalingService_IsTimerOnChanged, this));
	_timerTickRevoker = ScalingService.TimerTick(
		auto_revoke, std::bind_front(&HomeViewModel::_ScalingService_TimerTick, this));

	UpdateService& updateService = UpdateService::Get();
	_showUpdateCard = updateService.IsShowOnHomePage();
	_isShowOnHomePageChangedRevoker = updateService.IsShowOnHomePageChanged(
		auto_revoke,
		[this](bool value) {
			if (value) {
				ShowUpdateCard(true);
			}
		}
	);
}

bool HomeViewModel::ShowDlssNrStatus() const { return !ErrorService::Get().DlssNrSummary().empty(); }
hstring HomeViewModel::DlssNrStatusSummary() const { return hstring(ErrorService::Get().DlssNrSummary()); }
void HomeViewModel::OpenDlssNrEffects() {
	if (const auto root = App::Get().RootPage()) {
		auto context = ErrorService::Get().DlssNrContext();
		if (const auto session = ErrorService::Get().DlssNrSession()) {
			const auto snapshot = session->Read();
			for (uint32_t i = 0; i < snapshot.effects.size(); ++i) if (snapshot.effects[i].Failed()) { context.effectIndex = i; break; }
		}
		root->NavigateToIssueEffects(context);
	}
}

bool HomeViewModel::ShowRecentIssue() const noexcept {
	return ErrorService::Get().IsIssueVisible();
}

void HomeViewModel::ShowRecentIssue(bool value) {
	if (!value) ErrorService::Get().DismissIssue();
}

hstring HomeViewModel::RecentIssueSummary() const noexcept {
	return ErrorService::Get().Summary();
}

MUXC::InfoBarSeverity HomeViewModel::RecentIssueSeverity() const noexcept {
	return ErrorService::Get().IsInformational() ? MUXC::InfoBarSeverity::Informational : MUXC::InfoBarSeverity::Warning;
}

bool HomeViewModel::ShowIssueHistory() const noexcept {
	return ErrorService::Get().HasIssue() && !ErrorService::Get().IsIssueVisible();
}

fire_and_forget HomeViewModel::ShowRecentIssueDetails() noexcept {
	try {
		if (!ErrorService::Get().HasIssue() || ContentDialogHelper::IsAnyDialogOpen()) co_return;
		const auto root = App::Get().RootPage();
		if (!root) co_return;
		const auto loader = ResourceLoader::GetForViewIndependentUse(CommonSharedConstants::APP_RESOURCE_MAP_ID);
		// Capture the issue the user opened, even if another failure arrives.
		const hstring details = ErrorService::Get().Details();
		const IssueAction action = ErrorService::Get().Action();
		const IssueContext issueContext = ErrorService::Get().Context();
		TextBox text;
		text.Text(details);
		text.IsReadOnly(true);
		text.AcceptsReturn(true);
		text.TextWrapping(TextWrapping::Wrap);
		text.MaxHeight(400);
		ScrollViewer::SetVerticalScrollBarVisibility(text, ScrollBarVisibility::Auto);
		ContentDialog dialog;
		dialog.XamlRoot(root->XamlRoot());
		dialog.RequestedTheme(root->ActualTheme());
		dialog.Title(box_value(loader.GetString(L"ErrorDetails_Title")));
		StackPanel content;
		content.Spacing(12);

		TextBlock actionStatus;
		actionStatus.TextWrapping(TextWrapping::Wrap);
		if (action != IssueAction::None) {
			Button remedy;
			const wchar_t* actionKey = action == IssueAction::RetrySave ? L"ErrorDetails_RetrySave" :
				action == IssueAction::Profile ? L"ErrorDetails_OpenProfile" :
				action == IssueAction::Effects ? L"ErrorDetails_OpenEffects" :
				action == IssueAction::ProgramDirectory ? L"DlssNr_OpenDirectory" :
				action == IssueAction::RecoveryGuide ? L"DlssNr_OpenGuide" : L"ErrorDetails_OpenConfigDirectory";
			remedy.Content(box_value(loader.GetString(actionKey)));
			remedy.Click([action, issueContext, weakDialog = make_weak(dialog), weakRoot = root->get_weak(), loader,
				weakStatus = make_weak(actionStatus)](const IInspectable& sender, const RoutedEventArgs&) {
				const auto dialog = weakDialog.get();
				const auto root = weakRoot.get();
				const auto actionStatus = weakStatus.get();
				if (!dialog || !root || !actionStatus) return;
				const auto remedy = sender.as<Button>();
				if (action == IssueAction::RetrySave) {
					const uint64_t issueRevision = ErrorService::Get().Revision();
					remedy.IsEnabled(false);
					actionStatus.Text(loader.GetString(L"ErrorDetails_Saving"));
					ScalingService::Get().RetryConfigurationSave([issueRevision, weakButton = make_weak(remedy), weakStatus, loader](bool succeeded) {
						App::Get().Dispatcher().TryEnqueue([issueRevision, weakButton, weakStatus, loader, succeeded] {
							if (succeeded) ErrorService::Get().SavedSuccessfully(issueRevision);
							if (auto status = weakStatus.get()) status.Text(loader.GetString(succeeded ? L"ErrorDetails_SaveSucceeded" : L"ErrorDetails_SaveStillFailed"));
							if (auto button = weakButton.get()) button.IsEnabled(!succeeded);
						});
					});
				} else if (action == IssueAction::Profile) {
					if (root->NavigateToIssueProfile(issueContext.profileName, issueContext.profilePath, issueContext.profileClass)) dialog.Hide();
					else actionStatus.Text(loader.GetString(L"ErrorDetails_ProfileChanged"));
				} else if (action == IssueAction::Effects) {
					dialog.Hide();
					root->NavigateToIssueEffects(issueContext);
				} else if (action == IssueAction::ProgramDirectory || action == IssueAction::RecoveryGuide) {
					const auto directory = Win32Helper::GetExePath().parent_path();
					const auto path = action == IssueAction::RecoveryGuide ? directory / L"DLSSNR-TROUBLESHOOTING.md" : directory;
					if (!Win32Helper::ShellOpen(path.c_str())) actionStatus.Text(hstring(
						std::wstring(loader.GetString(L"DlssNr_OpenFailed")) + L"\n" + path.native()));
				} else if (!Win32Helper::ShellOpen(AppSettings::Get().ConfigDir().c_str())) {
					actionStatus.Text(hstring(std::wstring(loader.GetString(L"ErrorDetails_OpenConfigFailed")) + L"\n" + AppSettings::Get().ConfigDir().native()));
				}
			});
			content.Children().Append(remedy);
			content.Children().Append(actionStatus);
		}
		if (issueContext.dlssNrStatus) {
			if (action != IssueAction::Profile && issueContext.hasProfile && issueContext.dlssNrStatus->failure.reason == DlssNrReason::UnsupportedAdapter) {
				Button profile;
				profile.Content(box_value(loader.GetString(L"ErrorDetails_OpenProfile")));
				profile.Click([weakRoot = root->get_weak(), weakDialog = make_weak(dialog), weakStatus = make_weak(actionStatus), issueContext, loader](const IInspectable&, const RoutedEventArgs&) {
					if (const auto root = weakRoot.get()) {
						if (root->NavigateToIssueProfile(issueContext.profileName, issueContext.profilePath, issueContext.profileClass)) {
							if (const auto dialog = weakDialog.get()) dialog.Hide();
						} else if (const auto status = weakStatus.get()) status.Text(loader.GetString(L"ErrorDetails_ProfileChanged"));
					}
				});
				content.Children().Append(profile);
			}
			if (action != IssueAction::Effects) {
				Button effects;
				effects.Content(box_value(loader.GetString(L"ErrorDetails_OpenEffects")));
				effects.Click([weakRoot = root->get_weak(), weakDialog = make_weak(dialog), issueContext](const IInspectable&, const RoutedEventArgs&) {
					if (const auto root = weakRoot.get()) {
						if (const auto dialog = weakDialog.get()) dialog.Hide();
						root->NavigateToIssueEffects(issueContext);
					}
				});
				content.Children().Append(effects);
			}
			for (const bool guide : { false, true }) {
				if ((guide && action == IssueAction::RecoveryGuide) || (!guide && action == IssueAction::ProgramDirectory)) continue;
				Button button;
				button.Content(box_value(loader.GetString(guide ? L"DlssNr_OpenGuide" : L"DlssNr_OpenDirectory")));
				button.Click([guide, weakStatus = make_weak(actionStatus), loader](const IInspectable&, const RoutedEventArgs&) {
					const auto directory = Win32Helper::GetExePath().parent_path();
					const auto path = guide ? directory / L"DLSSNR-TROUBLESHOOTING.md" : directory;
					if (!Win32Helper::ShellOpen(path.c_str())) if (auto status = weakStatus.get())
						status.Text(hstring(std::wstring(loader.GetString(L"DlssNr_OpenFailed")) + L"\n" + path.native()));
				});
				content.Children().Append(button);
			}
		}
		content.Children().Append(text);
		ScrollViewer viewport;
		viewport.MaxHeight(480);
		viewport.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
		viewport.Content(content);
		dialog.Content(viewport);
		dialog.PrimaryButtonText(loader.GetString(L"ErrorDetails_Copy"));
		dialog.SecondaryButtonText(loader.GetString(L"ErrorDetails_OpenLogs"));
		dialog.CloseButtonText(loader.GetString(L"ErrorDetails_Close"));
		dialog.DefaultButton(ContentDialogButton::Close);
		dialog.PrimaryButtonClick([details, loader](const ContentDialog& sender, const ContentDialogButtonClickEventArgs& args) {
			args.Cancel(true);
			try {
				Windows::ApplicationModel::DataTransfer::DataPackage data;
				data.SetText(details);
				Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(data);
				sender.PrimaryButtonText(loader.GetString(L"ErrorDetails_Copied"));
			} catch (...) {
				sender.Title(box_value(loader.GetString(L"ErrorDetails_CopyFailed")));
			}
		});
		dialog.SecondaryButtonClick([loader](const ContentDialog& sender, const ContentDialogButtonClickEventArgs& args) {
			args.Cancel(true);
			const auto path = Win32Helper::GetExePath().parent_path() / CommonSharedConstants::LOGS_DIR;
			if (!Win32Helper::ShellOpen(path.c_str())) {
				sender.Title(box_value(loader.GetString(L"ErrorDetails_OpenLogsFailed")));
			}
		});
		co_await ContentDialogHelper::ShowAsync(dialog);
	} catch (...) {
		Logger::Get().Error("Unable to open error details dialog");
	}
}

hstring HomeViewModel::TimerDescription() const noexcept {
	ResourceLoader resourceLoader =
		ResourceLoader::GetForCurrentView(CommonSharedConstants::APP_RESOURCE_MAP_ID);
	hstring fmtStr = resourceLoader.GetString(L"Home_Activation_Timer_Description");
	return hstring(fmt::format(
		fmt::runtime(std::wstring_view(fmtStr)),
		AppSettings::Get().CountdownSeconds()
	));
}

bool HomeViewModel::IsTimerOn() const noexcept {
	return ScalingService::Get().IsTimerOn();
}

double HomeViewModel::TimerProgressRingValue() const noexcept {
	ScalingService& ScalingService = ScalingService::Get();
	return ScalingService.IsTimerOn() ? ScalingService.TimerProgress() : 1.0f;
}

hstring HomeViewModel::TimerLabelText() const noexcept {
	ScalingService& ScalingService = ScalingService::Get();
	return to_hstring((int)std::ceil(ScalingService.SecondsLeft()));
}

bool HomeViewModel::IsNotRunning() const noexcept {
	return !ScalingService::Get().IsScaling();
}

hstring HomeViewModel::TimerButtonText(bool windowedMode) const noexcept {
	ResourceLoader resourceLoader =
		ResourceLoader::GetForCurrentView(CommonSharedConstants::APP_RESOURCE_MAP_ID);
	if (ScalingService::Get().IsTimerOn(windowedMode)) {
		return resourceLoader.GetString(L"Home_Activation_Timer_Cancel");
	} else {
		return resourceLoader.GetString(L"Home_Activation_Timer_Start");
	}
}

void HomeViewModel::ToggleTimerFullscreen() const noexcept {
	_ToggleTimer(false);
}

void HomeViewModel::ToggleTimerWindowed() const noexcept {
	_ToggleTimer(true);
}

uint32_t HomeViewModel::Delay() const noexcept {
	return AppSettings::Get().CountdownSeconds();
}

void HomeViewModel::Delay(uint32_t value) {
	AppSettings::Get().CountdownSeconds(value);
	RaisePropertyChanged(L"Delay");
	RaisePropertyChanged(L"DelayText");
	RaisePropertyChanged(L"TimerDescription");
}

hstring HomeViewModel::DelayText() const noexcept {
	return App::Get().DoubleFormatter().FormatDouble(Delay());
}

void HomeViewModel::ShowUpdateCard(bool value) noexcept {
	_showUpdateCard = value;
	if (!value) {
		UpdateService::Get().IsShowOnHomePage(false);
	}
	
	RaisePropertyChanged(L"ShowUpdateCard");
	RaisePropertyChanged(L"UpdateCardTitle");
}

hstring HomeViewModel::UpdateCardTitle() const noexcept {
	UpdateService& updateService = UpdateService::Get();
	if (updateService.Status() < UpdateStatus::Available) {
		return {};
	}

	ResourceLoader resourceLoader =
		ResourceLoader::GetForCurrentView(CommonSharedConstants::APP_RESOURCE_MAP_ID);
	hstring titleFmt = resourceLoader.GetString(L"About_Version_UpdateCard_Title");
	return hstring(fmt::format(fmt::runtime(std::wstring_view(titleFmt)), updateService.Tag()));
}

bool HomeViewModel::IsAutoCheckForUpdates() const noexcept {
	return AppSettings::Get().IsAutoCheckForUpdates();
}

void HomeViewModel::IsAutoCheckForUpdates(bool value) noexcept {
	AppSettings::Get().IsAutoCheckForUpdates(value);
}

void HomeViewModel::DownloadAndInstall() {
	UpdateService::Get().DownloadAndInstall();
	App::Get().RootPage()->NavigateToAboutPage();
}

void HomeViewModel::ReleaseNotes() {
	Win32Helper::ShellOpen(UpdateService::Get().ReleasePage().c_str());
}

void HomeViewModel::RemindMeLater() {
	ShowUpdateCard(false);
}

hstring HomeViewModel::InitialToolbarStateDescription() const noexcept {
	static constexpr std::array STATE_STRING_IDS = {
		L"Home_Toolbar_InitialState_Off/Content",
		L"Home_Toolbar_InitialState_AlwaysShow/Content",
		L"Home_Toolbar_InitialState_AutoHide/Content"
	};

	const ToolbarState fullscreenInitialState =
		AppSettings::Get().FullscreenInitialToolbarState();
	const ToolbarState windowedInitialState =
		AppSettings::Get().WindowedInitialToolbarState();

	const ResourceLoader resourceLoader =
		ResourceLoader::GetForCurrentView(CommonSharedConstants::APP_RESOURCE_MAP_ID);
	if (fullscreenInitialState == windowedInitialState) {
		return resourceLoader.GetString(STATE_STRING_IDS[(uint32_t)fullscreenInitialState]);
	} else {
		return hstring(StrHelper::Concat(
			resourceLoader.GetString(STATE_STRING_IDS[(uint32_t)fullscreenInitialState]),
			L" | ",
			resourceLoader.GetString(STATE_STRING_IDS[(uint32_t)windowedInitialState]))
		);
	}
}

int HomeViewModel::FullscreenInitialToolbarState() const noexcept {
	return (int)AppSettings::Get().FullscreenInitialToolbarState();
}

void HomeViewModel::FullscreenInitialToolbarState(int value) {
	if (value < 0) {
		return;
	}

	const ToolbarState state = (ToolbarState)value;

	AppSettings& settings = AppSettings::Get();
	if (settings.FullscreenInitialToolbarState() == state) {
		return;
	}

	settings.FullscreenInitialToolbarState(state);
	RaisePropertyChanged(L"FullscreenInitialToolbarState");
	RaisePropertyChanged(L"InitialToolbarStateDescription");
}

int HomeViewModel::WindowedInitialToolbarState() const noexcept {
	return (int)AppSettings::Get().WindowedInitialToolbarState();
}

void HomeViewModel::WindowedInitialToolbarState(int value) {
	if (value < 0) {
		return;
	}

	const ToolbarState state = (ToolbarState)value;

	AppSettings& settings = AppSettings::Get();
	if (settings.WindowedInitialToolbarState() == state) {
		return;
	}

	settings.WindowedInitialToolbarState(state);
	RaisePropertyChanged(L"WindowedInitialToolbarState");
	RaisePropertyChanged(L"InitialToolbarStateDescription");
}

int HomeViewModel::ScreenshotContent() const noexcept {
	return static_cast<int>(AppSettings::Get().ScreenshotContent());
}

void HomeViewModel::ScreenshotContent(int value) {
	// ComboBox temporarily clears selection while rebuilding its items.
	if (value < 0 || value > 2) return;
	auto& settings = AppSettings::Get();
	const auto content = NormalizeScreenshotContent(static_cast<uint32_t>(value));
	if (content == settings.ScreenshotContent()) return;
	settings.ScreenshotContent(content);
	ScalingService::Get().UpdateScreenshotContent(content);
	RaisePropertyChanged(L"ScreenshotContent");
}

hstring HomeViewModel::ScreenshotSaveDirectory() const noexcept {
	return hstring(AppSettings::Get().ScreenshotsDir().native());
}

void HomeViewModel::OpenScreenshotSaveDirectory() const noexcept {
	const std::filesystem::path saveDir = AppSettings::Get().ScreenshotsDir();
	if (Win32Helper::CreateDir(saveDir.native(), true)) {
		Win32Helper::ShellOpen(saveDir.c_str());
	}
}

fire_and_forget HomeViewModel::ChangeScreenshotSaveDirectory() noexcept {
	const ResourceLoader resourceLoader =
		ResourceLoader::GetForCurrentView(CommonSharedConstants::APP_RESOURCE_MAP_ID);
	const hstring titleStr = resourceLoader.GetString(L"Dialog_SetlectScreenshotSaveDirectory_Title");

	const std::filesystem::path oldValue = AppSettings::Get().ScreenshotsDir();

	auto weakThis = get_weak();

	// 在主线程使用 IFileOpenDialog 有些问题，尤其在 Win10 中
	co_await resume_background();

	com_ptr<IFileOpenDialog> pickFolderDialog =
		try_create_instance<IFileOpenDialog>(CLSID_FileOpenDialog);
	if (!pickFolderDialog) {
		Logger::Get().Error("创建 FileSaveDialog 失败");
		co_return;
	}
	
	pickFolderDialog->SetTitle(titleStr.c_str());

	if (!oldValue.empty()) {
		// 选择父目录作为初始目录
		const std::filesystem::path parentDir = oldValue.parent_path();

		com_ptr<IShellItem> shellItem;
		HRESULT hr = SHCreateItemFromParsingName(
			parentDir.empty() ? oldValue.c_str() : parentDir.c_str(),
			nullptr,
			IID_PPV_ARGS(&shellItem)
		);
		if (SUCCEEDED(hr)) {
			pickFolderDialog->SetFolder(shellItem.get());
		} else {
			Logger::Get().ComError("SHCreateItemFromParsingName 失败", hr);
		}
	}

	std::optional<std::filesystem::path> screenshotDir =
		FileDialogHelper::OpenFileDialog(pickFolderDialog.get(), FOS_PICKFOLDERS);
	if (!screenshotDir || screenshotDir->empty() || *screenshotDir == oldValue) {
		co_return;
	}

	co_await App::Get().Dispatcher();

	if (weakThis.get()) {
		AppSettings::Get().ScreenshotsDir(*screenshotDir);
		RaisePropertyChanged(L"ScreenshotSaveDirectory");
	}
}

bool HomeViewModel::IsTouchSupportEnabled() const noexcept {
	// 不检查版本号是否匹配
	return TouchHelper::IsTouchSupportEnabled();
}

fire_and_forget HomeViewModel::IsTouchSupportEnabled(bool value) {
	if (IsTouchSupportEnabled() == value) {
		co_return;
	}

	auto weakThis = get_weak();

	// UAC 可能导致 XAML Islands 崩溃，因此不能在主线程上执行 ShellExecute，
	// 见 https://github.com/microsoft/microsoft-ui-xaml/issues/4952
	co_await resume_background();

	TouchHelper::IsTouchSupportEnabled(value);

	co_await App::Get().Dispatcher();

	if (weakThis.get()) {
		RaisePropertyChanged(L"IsTouchSupportEnabled");
		RaisePropertyChanged(L"IsShowTouchSupportInfoBar");
	}
}

Uri HomeViewModel::TouchSupportLearnMoreUrl() const noexcept {
	if (LocalizationService::Get().Language() == L"zh-hans"sv) {
		return Uri(L"https://github.com/SAOG0721/Magpie/blob/experimental/docs/%E5%85%B3%E4%BA%8E%E8%A7%A6%E6%8E%A7%E6%94%AF%E6%8C%81.md");
	} else {
		return Uri(L"https://github.com/SAOG0721/Magpie/blob/experimental/docs/About%20touch%20support.md");
	}
}

bool HomeViewModel::IsShowTouchSupportInfoBar() const noexcept {
	return !Win32Helper::IsProcessElevated() && IsTouchSupportEnabled();
}

bool HomeViewModel::IsAllowScalingMaximized() const noexcept {
	return AppSettings::Get().IsAllowScalingMaximized();
}

void HomeViewModel::IsAllowScalingMaximized(bool value) {
	AppSettings& settings = AppSettings::Get();

	if (settings.IsAllowScalingMaximized() == value) {
		return;
	}

	settings.IsAllowScalingMaximized(value);
	RaisePropertyChanged(L"IsAllowScalingMaximized");

	if (value) {
		ScalingService::Get().CheckForeground();
	}
}

bool HomeViewModel::IsSimulateExclusiveFullscreen() const noexcept {
	return AppSettings::Get().IsSimulateExclusiveFullscreen();
}

void HomeViewModel::IsSimulateExclusiveFullscreen(bool value) {
	AppSettings& settings = AppSettings::Get();

	if (settings.IsSimulateExclusiveFullscreen() == value) {
		return;
	}

	settings.IsSimulateExclusiveFullscreen(value);
	RaisePropertyChanged(L"IsSimulateExclusiveFullscreen");
}

bool HomeViewModel::IsInlineParams() const noexcept {
	return AppSettings::Get().IsInlineParams();
}

void HomeViewModel::IsInlineParams(bool value) {
	AppSettings& settings = AppSettings::Get();

	if (settings.IsInlineParams() == value) {
		return;
	}

	settings.IsInlineParams(value);
	RaisePropertyChanged(L"IsInlineParams");
}

bool HomeViewModel::IsStopEffectsOnTaskSwitchEnabled() const noexcept {
	return AppSettings::Get().IsStopEffectsOnTaskSwitchEnabled();
}

void HomeViewModel::IsStopEffectsOnTaskSwitchEnabled(bool value) {
	auto& settings = AppSettings::Get();
	if (settings.IsStopEffectsOnTaskSwitchEnabled() == value) return;
	settings.IsStopEffectsOnTaskSwitchEnabled(value);
	RaisePropertyChanged(L"IsStopEffectsOnTaskSwitchEnabled");
}

bool HomeViewModel::IsVRREnabled() const noexcept {
	return AppSettings::Get().IsVRREnabled();
}

void HomeViewModel::IsVRREnabled(bool value) {
	auto& settings = AppSettings::Get();
	if (settings.IsVRREnabled() == value) return;
	settings.IsVRREnabled(value);
	RaisePropertyChanged(L"IsVRREnabled");
}

hstring HomeViewModel::DefaultFrameRefreshSummary() const {
	const auto loader = ResourceLoader::GetForViewIndependentUse(CommonSharedConstants::APP_RESOURCE_MAP_ID);
	const auto s = EffectiveFrameRefreshSettings(AppSettings::Get().DefaultProfile().frameRefresh,
		AppSettings::Get().IsSimpleFrameRefresh());
	const wchar_t* cursorKeys[]{ L"FrameRefresh_Responsive/Content", L"FrameRefresh_OriginalOnly/Content", L"FrameRefresh_Supplement/Content" };
	std::wstring content(loader.GetString(s.contentMode == ContentFrameRateMode::Custom ?
		L"FrameRefresh_Custom/Content" : L"FrameRefresh_Source/Content"));
	if (s.contentMode == ContentFrameRateMode::Custom) content += fmt::format(L" {} FPS", s.contentRate);
	if (s.legacyContentLimit > 0) content += L" (" + fmt::format(
		fmt::runtime(std::wstring_view(loader.GetString(L"FrameRefresh_LegacyCapNotice"))), s.legacyContentLimit) + L")";
	std::wstring cursor(loader.GetString(cursorKeys[uint32_t(s.cursorMode)]));
	if (s.cursorMode == CursorRefreshMode::Supplement) cursor += s.cursorSupplement == CursorSupplementMode::Auto ?
		L" (" + std::wstring(loader.GetString(L"FrameRefresh_Auto/Content")) + L")" : fmt::format(L" {} FPS", s.cursorRate);
	const auto idle = s.idleEnabled ? fmt::format(L"{} FPS", s.idleRate) : std::wstring(loader.GetString(L"FrameRefresh_Off/Content"));
	if (AppSettings::Get().IsSimpleFrameRefresh()) {
		const auto vrr = loader.GetString(s.vrrEnabled ? L"FrameRefresh_VRR_Input/OnContent" : L"FrameRefresh_VRR_Input/OffContent");
		return hstring(fmt::format(fmt::runtime(std::wstring_view(loader.GetString(L"Home_FrameRefresh_SimpleSummary"))), content, idle, std::wstring_view(vrr)));
	}
	return hstring(fmt::format(fmt::runtime(std::wstring_view(loader.GetString(L"Home_FrameRefresh_Summary"))), content, cursor, idle));
}
void HomeViewModel::EditDefaultFrameRefresh() {
	if (const auto root = App::Get().RootPage()) root->NavigateToIssueProfile({}, {}, {});
}

bool HomeViewModel::IsDeveloperMode() const noexcept {
	return AppSettings::Get().IsDeveloperMode();
}

void HomeViewModel::IsDeveloperMode(bool value) {
	AppSettings& settings = AppSettings::Get();

	if (settings.IsDeveloperMode() == value) {
		return;
	}

	settings.IsDeveloperMode(value);
	RaisePropertyChanged(L"IsDeveloperMode");
	if (!value) {
		for (const auto* property : { L"IsDebugMode", L"IsBenchmarkMode", L"IsEffectCacheDisabled",
			L"IsFontCacheDisabled", L"IsSaveEffectSources", L"IsWarningsAreErrors",
			L"IsStatisticsForDynamicDetectionEnabled", L"IsFP16Disabled" }) {
			RaisePropertyChanged(property);
		}
	}
}

void HomeViewModel::LocateMagpieLogs() noexcept {
	Win32Helper::ShellOpen(StrHelper::Concat(L".\\", CommonSharedConstants::LOGS_DIR).c_str());
}

static fire_and_forget LocateTempLogs(const wchar_t* logName) noexcept {
	std::wstring path(MAX_PATH + 1, L'\0');

	const DWORD len = GetTempPath(MAX_PATH + 2, path.data());
	if (len <= 0) {
		co_return;
	}

	path.resize(len);
	if (!path.ends_with(L'\\')) {
		path.push_back(L'\\');
	}

	path.append(logName);
	if (!Win32Helper::FileExists(path.c_str())) {
		co_return;
	}

	co_await resume_background();
	Win32Helper::OpenFolderAndSelectFile(path.c_str());
}

void HomeViewModel::LocateTouchHelperLogs() noexcept {
	LocateTempLogs(CommonSharedConstants::TOUCH_HELPER_LOG_NAME);
}

void HomeViewModel::LocateUpdaterLogs() noexcept {
	LocateTempLogs(CommonSharedConstants::UPDATER_LOG_NAME);
}

bool HomeViewModel::IsDebugMode() const noexcept {
	return AppSettings::Get().IsDebugMode();
}

void HomeViewModel::IsDebugMode(bool value) {
	AppSettings& settings = AppSettings::Get();

	if (settings.IsDebugMode() == value) {
		return;
	}

	settings.IsDebugMode(value);
	RaisePropertyChanged(L"IsDebugMode");
}

bool HomeViewModel::IsBenchmarkMode() const noexcept {
	return AppSettings::Get().IsBenchmarkMode();
}

void HomeViewModel::IsBenchmarkMode(bool value) {
	AppSettings& settings = AppSettings::Get();

	if (settings.IsBenchmarkMode() == value) {
		return;
	}

	settings.IsBenchmarkMode(value);
	RaisePropertyChanged(L"IsBenchmarkMode");
}

bool HomeViewModel::IsTopmostDisabled() const noexcept {
	return AppSettings::Get().IsTopmostDisabled();
}

void HomeViewModel::IsTopmostDisabled(bool value) {
	AppSettings& settings = AppSettings::Get();

	if (settings.IsTopmostDisabled() == value) {
		return;
	}

	settings.IsTopmostDisabled(value);
	RaisePropertyChanged(L"IsTopmostDisabled");
}

bool HomeViewModel::IsEffectCacheDisabled() const noexcept {
	return AppSettings::Get().IsEffectCacheDisabled();
}

void HomeViewModel::IsEffectCacheDisabled(bool value) {
	AppSettings& settings = AppSettings::Get();

	if (settings.IsEffectCacheDisabled() == value) {
		return;
	}

	settings.IsEffectCacheDisabled(value);
	RaisePropertyChanged(L"IsEffectCacheDisabled");
}

bool HomeViewModel::IsFontCacheDisabled() const noexcept {
	return AppSettings::Get().IsFontCacheDisabled();
}

void HomeViewModel::IsFontCacheDisabled(bool value) {
	AppSettings& settings = AppSettings::Get();

	if (settings.IsFontCacheDisabled() == value) {
		return;
	}

	settings.IsFontCacheDisabled(value);
	RaisePropertyChanged(L"IsFontCacheDisabled");
}

bool HomeViewModel::IsSaveEffectSources() const noexcept {
	return AppSettings::Get().IsSaveEffectSources();
}

void HomeViewModel::IsSaveEffectSources(bool value) {
	AppSettings& settings = AppSettings::Get();

	if (settings.IsSaveEffectSources() == value) {
		return;
	}

	settings.IsSaveEffectSources(value);
	RaisePropertyChanged(L"IsSaveEffectSources");
}

bool HomeViewModel::IsWarningsAreErrors() const noexcept {
	return AppSettings::Get().IsWarningsAreErrors();
}

void HomeViewModel::IsWarningsAreErrors(bool value) {
	AppSettings& settings = AppSettings::Get();

	if (settings.IsWarningsAreErrors() == value) {
		return;
	}

	settings.IsWarningsAreErrors(value);
	RaisePropertyChanged(L"IsWarningsAreErrors");
}

bool HomeViewModel::IsFP16Disabled() const noexcept {
	return AppSettings::Get().IsFP16Disabled();
}

void HomeViewModel::IsFP16Disabled(bool value) {
	AppSettings& settings = AppSettings::Get();

	if (settings.IsFP16Disabled() == value) {
		return;
	}

	settings.IsFP16Disabled(value);
	RaisePropertyChanged(L"IsFP16Disabled");
}

int HomeViewModel::DuplicateFrameDetectionMode() const noexcept {
	return (int)AppSettings::Get().DuplicateFrameDetectionMode();
}

void HomeViewModel::DuplicateFrameDetectionMode(int value) {
	if (value < 0) {
		return;
	}

	const auto mode = (::Magpie::DuplicateFrameDetectionMode)value;

	AppSettings& settings = AppSettings::Get();
	if (settings.DuplicateFrameDetectionMode() == mode) {
		return;
	}

	settings.DuplicateFrameDetectionMode(mode);

	RaisePropertyChanged(L"DuplicateFrameDetectionMode");
	RaisePropertyChanged(L"IsDynamicDection");

	if (mode != ::Magpie::DuplicateFrameDetectionMode::Dynamic) {
		settings.IsStatisticsForDynamicDetectionEnabled(false);
		RaisePropertyChanged(L"IsStatisticsForDynamicDetectionEnabled");
	}
}

bool HomeViewModel::IsDynamicDection() const noexcept {
	return AppSettings::Get().DuplicateFrameDetectionMode() == ::Magpie::DuplicateFrameDetectionMode::Dynamic;
}

bool HomeViewModel::IsStatisticsForDynamicDetectionEnabled() const noexcept {
	return AppSettings::Get().IsStatisticsForDynamicDetectionEnabled();
}

void HomeViewModel::IsStatisticsForDynamicDetectionEnabled(bool value) {
	AppSettings& settings = AppSettings::Get();

	if (settings.IsStatisticsForDynamicDetectionEnabled() == value) {
		return;
	}

	settings.IsStatisticsForDynamicDetectionEnabled(value);
	RaisePropertyChanged(L"IsStatisticsForDynamicDetectionEnabled");
}

void HomeViewModel::_ScalingService_IsTimerOnChanged(bool value, bool) {
	if (!value) {
		RaisePropertyChanged(L"TimerProgressRingValue");
	}

	RaisePropertyChanged(L"TimerProgressRingValue");
	RaisePropertyChanged(L"TimerLabelText");
	RaisePropertyChanged(L"TimerButtonText");
	RaisePropertyChanged(L"IsTimerOn");
}

void HomeViewModel::_ScalingService_TimerTick(double) {
	RaisePropertyChanged(L"TimerProgressRingValue");
	RaisePropertyChanged(L"TimerLabelText");
}

void HomeViewModel::_ScalingService_IsScalingChanged(bool) {
	RaisePropertyChanged(L"IsNotRunning");
}

void HomeViewModel::_ToggleTimer(bool windowedMode) const noexcept {
	ScalingService& scalingService = ScalingService::Get();
	if (scalingService.IsTimerOn(windowedMode)) {
		scalingService.StopTimer();
	} else {
		scalingService.StartTimer(windowedMode);
	}
}

}
