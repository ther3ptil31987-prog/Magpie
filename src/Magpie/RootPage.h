#pragma once
#include "RootPage.g.h"
#include "ErrorService.h"
#include "Event.h"
#include "NewProfileViewModel.h"
#include "ProfileViewModel.h"

namespace Magpie {
struct Profile;
}

namespace winrt::Magpie::implementation {

struct TitleBarControl;

struct RootPage : RootPageT<RootPage> {
	RootPage();
	~RootPage();

	void InitializeComponent();

	void RootPage_Loaded(IInspectable const&, RoutedEventArgs const&);

	void NavigationView_SelectionChanged(MUXC::NavigationView const&, MUXC::NavigationViewSelectionChangedEventArgs const& args);

	void NavigationView_PaneOpening(MUXC::NavigationView const&, IInspectable const&);

	void NavigationView_PaneClosing(MUXC::NavigationView const&, MUXC::NavigationViewPaneClosingEventArgs const&);

	void NavigationView_DisplayModeChanged(MUXC::NavigationView const& nv, MUXC::NavigationViewDisplayModeChangedEventArgs const&);

	void NavigationView_ItemInvoked(MUXC::NavigationView const&, MUXC::NavigationViewItemInvokedEventArgs const& args);

	winrt::Magpie::NewProfileViewModel NewProfileViewModel() const noexcept {
		return *_newProfileViewModel;
	}

	void ComboBox_DropDownOpened(IInspectable const&, IInspectable const&) const;

	void NewProfileConfirmButton_Click(IInspectable const&, RoutedEventArgs const&);

	void NewProfileNameContextFlyout_Opening(IInspectable const&, IInspectable const&);

	void NewProfileNameTextBox_KeyDown(IInspectable const&, Input::KeyRoutedEventArgs const& args);

	void ProfileMoreOptionsButton_Click(IInspectable const& sender, RoutedEventArgs const&);

	void ProfileRenameMenuItem_Click(IInspectable const& sender, RoutedEventArgs const&);

	void ProfileDeleteMenuItem_Click(IInspectable const& sender, RoutedEventArgs const&);

	void ProfileRenameTextBox_Loaded(IInspectable const& sender, RoutedEventArgs const&);

	void ProfileReorderHandle_PointerPressed(
		IInspectable const& sender,
		Input::PointerRoutedEventArgs const& args);

	void ProfileReorderHandle_PointerMoved(
		IInspectable const& sender,
		Input::PointerRoutedEventArgs const& args);

	void ProfileReorderHandle_PointerReleased(
		IInspectable const& sender,
		Input::PointerRoutedEventArgs const& args);

	void ProfileReorderHandle_PointerCanceled(
		IInspectable const& sender,
		Input::PointerRoutedEventArgs const& args);

	void ProfileReorderHandle_PointerCaptureLost(
		IInspectable const& sender,
		Input::PointerRoutedEventArgs const& args);

	void NavigateToAboutPage();
	bool NavigateToIssueProfile(std::wstring_view name, std::wstring_view pathRule, std::wstring_view classRule);
	void NavigateToScalingModes();
	void NavigateToHome();
	void NavigateToIssueEffects(const ::Magpie::IssueContext& context);

	TitleBarControl& TitleBar();

private:
	struct ProfileReorderPreviewItem {
		uint32_t index = 0;
		FrameworkElement container{ nullptr };
		Transform originalRenderTransform{ nullptr };
		CompositeTransform previewTransform{ nullptr };
		double targetOffset = 0;
	};

	void _UpdateTheme(bool updateIcons);

	fire_and_forget _LoadIcon(MUXC::NavigationViewItem const& item, const ::Magpie::Profile& profile);

	MUXC::NavigationViewItem _CreateProfileNavigationViewItem(
		uint32_t index,
		const ::Magpie::Profile& profile);

	void _RebindProfileNavigationViewModels() noexcept;

	void _UpdateIcons(bool skipDesktop);

	void _ProfileService_ProfileAdded(::Magpie::Profile& profile);

	void _ProfileService_ProfileRenamed(uint32_t idx);

	void _ProfileService_ProfileRemoved(uint32_t idx);

	void _ProfileService_ProfileMoved(uint32_t fromIndex, uint32_t toIndex);

	MUXC::NavigationViewItem _FindParentProfileNavigationViewItem(
		DependencyObject const& element) const noexcept;

	uint32_t _GetProfileReorderTargetIndex(double pointerY) const noexcept;

	void _PrepareProfileReorderPreview() noexcept;

	void _UpdateProfileReorderPreview(uint32_t targetIndex) noexcept;

	void _ClearProfileReorderPreview() noexcept;

	void _QueueFinishProfileReorder(bool commit) noexcept;

	void _FinishProfileReorder(bool commit) noexcept;

	void _UpdateNewProfileNameTextBox(bool fillWithTitle);

	::Magpie::MultithreadEvent<bool>::EventRevoker _appThemeChangedRevoker;
	::Magpie::Event<uint32_t>::EventRevoker _dpiChangedRevoker;

	com_ptr<implementation::NewProfileViewModel> _newProfileViewModel = make_self<implementation::NewProfileViewModel>();
	std::vector<com_ptr<implementation::ProfileViewModel>> _profileNavigationViewModels;
	::Magpie::Event<::Magpie::Profile&>::EventRevoker _profileAddedRevoker;
	::Magpie::Event<uint32_t>::EventRevoker _profileRenamedRevoker;
	::Magpie::Event<uint32_t>::EventRevoker _profileRemovedRevoker;
	::Magpie::Event<uint32_t, uint32_t>::EventRevoker _profileMovedRevoker;
	// 当前未被任何代码读取（Opening 事件钩子未接线）；ClangCL -Werror、-Wunused-private-field 会报错
	[[maybe_unused]] Primitives::FlyoutBase::Opening_revoker _contextFlyoutOpeningRevoker;

	Button _profileMoreOptionsButton{ nullptr };
	FrameworkElement _profileReorderHandle{ nullptr };
	FrameworkElement _profileReorderContainer{ nullptr };
	MUXC::NavigationViewItem _profileReorderItem{ nullptr };
	Transform _profileOriginalRenderTransform{ nullptr };
	CompositeTransform _profileDragTransform{ nullptr };
	std::vector<ProfileReorderPreviewItem> _profileReorderPreviewItems;
	std::vector<double> _profileReorderItemCenters;
	double _profileOriginalOpacity = 1;
	double _profilePointerStartY = 0;
	double _profileReorderSlotExtent = 0;
	int32_t _profileOriginalZIndex = 0;
	uint32_t _profileReorderOriginalIndex = 0;
	uint32_t _profileReorderTargetIndex = 0;
	uint32_t _profileReorderPointerId = 0;
	bool _isProfileReorderDragging = false;
	bool _isProfileReorderFinishQueued = false;
	bool _queuedProfileReorderCommit = false;
};

}
