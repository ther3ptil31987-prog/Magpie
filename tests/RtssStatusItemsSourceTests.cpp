#define RTSS_ITEMS_SOURCE_TEST
#include "RtssResourceTestFixture.h"
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>

static void Require(bool value, const char* message) {
	if (!value) throw std::runtime_error(message);
}

int main() {
	try {
		using Windows::Foundation::IInspectable;
		using Windows::UI::Xaml::Controls::ItemsControl;
		init_apartment(apartment_type::single_threaded);
		auto manager = Windows::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();

		// The original collection deterministically fails at the actual XAML setter.
		const auto broken = single_threaded_vector<hstring>({L"RTSS status"});
		ItemsControl negativeControl;
		bool rejected = false;
		try { negativeControl.ItemsSource(broken); }
		catch (const hresult_error& error) { rejected = uint32_t(error.code().value) == 0x80070057; }
		Require(rejected, "Original string vector did not reproduce E_INVALIDARG");

		ItemsControl control;
		unsigned bindings = 0;
		for (int repeat = 0; repeat < 3; ++repeat) {
			ProfileViewModel vm;
			// First loading (scope None) and subsequent one-way binding refreshes.
			for (auto state : States) for (auto scope : {RtssScope::None, RtssScope::Global, RtssScope::Target}) {
				vm.status.state = state;
				vm.data.rtss.scope = scope;
				if (repeat > 0) {
					vm.status.rtss.executable = L"RTSS.exe";
					vm.status.rtss.version = L"7.3.5";
					vm.status.rtss.running = true;
					vm.status.profile = L"Global";
					vm.status.rateKnown = true;
					vm.status.currentRate = 60;
					vm.status.failureStage = L"Rtss_Failure_Read";
					vm.status.profileSaved = vm.status.reloaded = repeat == 2;
				}
				const auto segments = vm.RtssStatusSegments(); // Extracted production body, unchanged IDL type.
				Require(bool(segments.try_as<IIterable<IInspectable>>()), "Status lacks XAML object collection interface");
				control.ItemsSource(segments);
				Require(control.Items().Size() == segments.Size(), "ItemsSource lost status segments");
				std::wstring joined;
				for (uint32_t i = 0; i < segments.Size(); ++i) {
					Require(unbox_value<hstring>(control.Items().GetAt(i)) == segments.GetAt(i), "XAML changed status order or text");
					joined += segments.GetAt(i);
				}
				Require(joined == vm.RtssStatusText(), "Plain status text differs from bound segments");
				++bindings;
			}
		}
		std::cout << "RTSS native XAML ItemsSource: " << bindings
			<< " production bindings passed; original vector rejected with 0x80070057.\n";
		manager.Close();
		return 0;
	} catch (const hresult_error& error) {
		std::cerr << "HRESULT: 0x" << std::hex << uint32_t(error.code().value) << " " << to_string(error.message()) << '\n';
	} catch (const std::exception& error) { std::cerr << error.what() << '\n'; }
	return 1;
}
