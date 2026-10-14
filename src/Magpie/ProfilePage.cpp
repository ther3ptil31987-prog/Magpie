#include "pch.h"
#include "ProfilePage.h"
#if __has_include("ProfilePage.g.cpp")
#include "ProfilePage.g.cpp"
#endif
#include "App.h"
#include "CommonSharedConstants.h"
#include "ControlHelper.h"
#include "Profile.h"
#include <winrt/Windows.UI.Xaml.Automation.h>

using namespace ::Magpie;
using namespace winrt;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Input;

namespace winrt::Magpie::implementation {

void ProfilePage::OnNavigatedTo(Navigation::NavigationEventArgs const& args) {
	int profileIdx = args.Parameter().try_as<int>().value();
	_viewModel = make_self<ProfileViewModel>(profileIdx);
	_profileChangedRevoker = ViewModel().as<Data::INotifyPropertyChanged>().PropertyChanged(auto_revoke,
		[weak = get_weak()](IInspectable const&, Data::PropertyChangedEventArgs const& e) {
			if (e.PropertyName() == L"ShowRtssManagement" || e.PropertyName() == L"ShowRtssManagementResult") {
				if (auto page = weak.get()) page->_UpdateRtssManagement();
			}
		});
}

void ProfilePage::ComboBox_DropDownOpened(IInspectable const& sender, IInspectable const&) {
	ControlHelper::ComboBox_DropDownOpened(sender);
}

void ProfilePage::_UpdateProfileCard(winrt::Magpie::SettingsCard const& card) {
	if (!card || card.ActualWidth() <= 0) return;
	auto content = card.Content().try_as<FrameworkElement>();
	if (!content) return;
	const auto padding = card.Padding();
	// A header icon uses the existing 42-DIP text indention. Expanded items
	// already include that indention in their padding; both share the same edge.
	const double left = padding.Left + (card.HeaderIcon() ? 42.0 : 0.0);
	const double available = std::max(0.0, card.ActualWidth() - left - padding.Right - 2.0);
	const double column = Resources().Lookup(box_value(L"ProfileControlColumnWidth")).as<double>();
	if (card.Name() == L"LaunchParametersSettingsCard") {
		content.Width(available);
	} else if (!content.try_as<ToggleSwitch>()) {
		const double preferred = content.try_as<Button>()
			? Resources().Lookup(box_value(L"ProfileActionWidth")).as<double>() : column;
		content.Width(std::min(preferred, available));
		content.HorizontalAlignment(HorizontalAlignment::Right);
	}
}

void ProfilePage::ProfileCard_Loaded(IInspectable const& sender, RoutedEventArgs const&) {
	_UpdateProfileCard(sender.as<winrt::Magpie::SettingsCard>());
	_UpdateRtssManagement();
}

void ProfilePage::ProfileCard_SizeChanged(IInspectable const& sender, SizeChangedEventArgs const&) {
	_UpdateProfileCard(sender.as<winrt::Magpie::SettingsCard>());
}

void ProfilePage::_UpdateProfileExpander(IInspectable const& sender) {
	auto control = sender.as<winrt::Magpie::SettingsExpander>();
	control.ApplyTemplate();
	auto expander = VisualTreeHelper::GetChild(control, 0).as<MUXC::Expander>();
	auto header = expander.Header().as<winrt::Magpie::SettingsCard>();
	if (!header.Tag()) {
		header.Tag(box_value(true));
		// The native header can be arranged after its outer expander on resize.
		header.SizeChanged([weak = get_weak()](IInspectable const& sender, SizeChangedEventArgs const&) {
			if (auto page = weak.get()) page->_UpdateProfileCard(sender.as<winrt::Magpie::SettingsCard>());
		});
	}
	_UpdateProfileCard(header);
}

void ProfilePage::ProfileExpander_Loaded(IInspectable const& sender, RoutedEventArgs const&) {
	_UpdateProfileExpander(sender);
}

void ProfilePage::ProfileExpander_SizeChanged(IInspectable const& sender, SizeChangedEventArgs const&) {
	_UpdateProfileExpander(sender);
}

void ProfilePage::_UpdatePairEditor(Grid const& grid) {
	if (grid.ActualWidth() <= 0) return;
	auto mode = grid.Children().GetAt(0).as<FrameworkElement>();
	auto number = grid.Children().GetAt(1).as<FrameworkElement>();
	const bool visible = number.Visibility() == Visibility::Visible;
	const double column = Resources().Lookup(box_value(L"ProfileControlColumnWidth")).as<double>();
	const double numberWidth = Resources().Lookup(box_value(L"ProfileNumberColumnWidth")).as<double>();
	const double gap = Resources().Lookup(box_value(L"ProfileControlGap")).as<double>();
	const bool stacked = visible && grid.ActualWidth() + 0.5 < column;
	grid.ColumnDefinitions().GetAt(1).Width(GridLengthHelper::FromPixels(visible && !stacked ? gap : 0));
	Grid::SetColumnSpan(mode, stacked ? 3 : 1);
	Grid::SetRow(number, stacked ? 1 : 0);
	Grid::SetColumn(number, stacked ? 0 : 2);
	Grid::SetColumnSpan(number, stacked ? 3 : 1);
	number.Width(stacked ? grid.ActualWidth() : numberWidth);
	number.Margin(Thickness{ 0, stacked ? gap : 0.0, 0, 0 });
}

void ProfilePage::PairEditor_Loaded(IInspectable const& sender, RoutedEventArgs const&) {
	auto grid = sender.as<Grid>();
	if (!grid.Tag()) {
		// Visibility changes must also resize the mode selector (source/off/auto).
		// Weak captures avoid retaining this page through a child callback.
		grid.Tag(box_value(true));
		grid.Children().GetAt(1).RegisterPropertyChangedCallback(UIElement::VisibilityProperty(),
			[weak = get_weak(), weakGrid = make_weak(grid)](DependencyObject const&, DependencyProperty const&) {
				if (auto page = weak.get()) {
					if (auto pair = weakGrid.get()) page->_UpdatePairEditor(pair);
				}
			});
	}
	_UpdatePairEditor(grid);
}

void ProfilePage::PairEditor_SizeChanged(IInspectable const& sender, SizeChangedEventArgs const&) {
	_UpdatePairEditor(sender.as<Grid>());
}

void ProfilePage::RtssManagement_Changed(IInspectable const&, RoutedEventArgs const&) {
	_UpdateRtssManagement();
}

void ProfilePage::_UpdateRtssManagement() {
	if (!RtssManagementToggle() || !RtssResultCard()) return;
	const auto checked = RtssManagementToggle().IsChecked();
	const bool expanded = _viewModel->ShowRtssManagement() && checked && checked.Value();
	const auto visibility = expanded ? Visibility::Visible : Visibility::Collapsed;
	RtssProgramCard().Visibility(visibility);
	RtssSelfCard().Visibility(visibility);
	RtssAfterburnerCard().Visibility(visibility);
	RtssResultCard().Visibility(expanded && _viewModel->ShowRtssManagementResult() ? Visibility::Visible : Visibility::Collapsed);
	RtssManagementChevron().Glyph(expanded ? L"\xE70E" : L"\xE70D");
	Automation::AutomationProperties::SetHelpText(RtssManagementToggle(),
		ResourceLoader::GetForCurrentView(CommonSharedConstants::APP_RESOURCE_MAP_ID)
			.GetString(expanded ? L"Rtss_ManagementExpanded" : L"Rtss_ManagementCollapsed"));
}

void ProfilePage::FrameRefreshHeader_SizeChanged(IInspectable const&, SizeChangedEventArgs const& args) {
	const bool narrow = args.NewSize().Width < 560;
	Grid::SetColumn(FrameRefreshHeaderActions(), narrow ? 0 : 1);
	Grid::SetColumnSpan(FrameRefreshHeaderActions(), narrow ? 2 : 1);
	Grid::SetRow(FrameRefreshHeaderActions(), narrow ? 1 : 0);
	FrameRefreshHeaderActions().Margin(Thickness{ 0, narrow ? 8.0 : 0.0, 0, 0 });
}

void ProfilePage::NumberBox_Loaded(IInspectable const& sender, RoutedEventArgs const&) {
	ControlHelper::NumberBox_Loaded(sender);
	auto input = sender.as<IControlProtected>().GetTemplateChild(L"InputBox").as<TextBox>();
	input.MinWidth(0);
	input.ApplyTemplate();
	// Keep the native editor, selection, context menu, validation and keyboard
	// stepping. Reserve a compact clear slot so focus does not shift the text.
	if (auto clear = input.as<IControlProtected>().GetTemplateChild(L"DeleteButton").try_as<Button>()) {
		clear.MinWidth(24);
		clear.Width(24);
		clear.Padding(Thickness{ 0 });
		if (auto grid = VisualTreeHelper::GetParent(clear).try_as<Grid>()) {
			const auto column = Grid::GetColumn(clear);
			if (uint32_t(column) < grid.ColumnDefinitions().Size()) {
				grid.ColumnDefinitions().GetAt(column).MinWidth(24);
			}
		}
	}
}

void ProfilePage::InitialWindowedScaleFactorComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&) {
	if (InitialWindowedScaleFactorFromSelectedIndex(_viewModel->InitialWindowedScaleFactor()) ==
		InitialWindowedScaleFactor::Custom)
	{
		CustomInitialWindowedScaleFactorEditor().Visibility(Visibility::Visible);
	} else {
		CustomInitialWindowedScaleFactorEditor().Visibility(Visibility::Collapsed);
	}
}

void ProfilePage::CursorScalingComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&) {
	if ((CursorScaling)_viewModel->CursorScaling() == CursorScaling::Custom) {
		CustomCursorScalingEditor().Visibility(Visibility::Visible);
	} else {
		CustomCursorScalingEditor().Visibility(Visibility::Collapsed);
	}
}

void ProfilePage::LaunchParametersTextBox_KeyDown(IInspectable const&, Input::KeyRoutedEventArgs const& args) {
	if (args.Key() == VirtualKey::Enter) {
		Focus(FocusState::Pointer);
	}
}

}
