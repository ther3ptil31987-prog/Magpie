#pragma once
#include "ProfilePage.g.h"
#include "ProfileViewModel.h"

namespace winrt::Magpie::implementation {

struct ProfilePage : ProfilePageT<ProfilePage> {
	void OnNavigatedTo(Navigation::NavigationEventArgs const& args);

	winrt::Magpie::ProfileViewModel ViewModel() const noexcept {
		return *_viewModel;
	}

	void ComboBox_DropDownOpened(IInspectable const& sender, IInspectable const&);

	void ProfileCard_Loaded(IInspectable const& sender, RoutedEventArgs const&);
	void ProfileCard_SizeChanged(IInspectable const& sender, SizeChangedEventArgs const&);
	void ProfileExpander_Loaded(IInspectable const& sender, RoutedEventArgs const&);
	void ProfileExpander_SizeChanged(IInspectable const& sender, SizeChangedEventArgs const&);
	void PairEditor_Loaded(IInspectable const& sender, RoutedEventArgs const&);
	void PairEditor_SizeChanged(IInspectable const& sender, SizeChangedEventArgs const&);
	void RtssManagement_Changed(IInspectable const&, RoutedEventArgs const&);
	void FrameRefreshHeader_SizeChanged(IInspectable const& sender, SizeChangedEventArgs const& args);

	void NumberBox_Loaded(IInspectable const& sender, RoutedEventArgs const&);

	void InitialWindowedScaleFactorComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&);

	void CursorScalingComboBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&);

	void LaunchParametersTextBox_KeyDown(IInspectable const&, Input::KeyRoutedEventArgs const& args);

private:
	void _UpdateProfileCard(winrt::Magpie::SettingsCard const& card);
	void _UpdateProfileExpander(IInspectable const& sender);
	void _UpdatePairEditor(Grid const& grid);
	void _UpdateRtssManagement();
	Data::INotifyPropertyChanged::PropertyChanged_revoker _profileChangedRevoker;
	com_ptr<ProfileViewModel> _viewModel;
};

}

BASIC_FACTORY(ProfilePage)
