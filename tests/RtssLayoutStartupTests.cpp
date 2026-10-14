#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

unsigned checks = 0;
void Require(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }

namespace Layout {
enum class Orientation { Horizontal, Vertical };
enum class Visibility { Visible, Collapsed };
enum class StretchChild { None, Last };
struct Size { float Width = 0, Height = 0; };
struct Rect { float X, Y, Width, Height; };
struct Thickness { double Left = 0, Top = 0, Right = 0, Bottom = 0; };
template<class T, unsigned = 0> using SmallVector = std::vector<T>;
#include "RtssWrapLayout.g.h"
struct UIElement {
	Size desired;
	enum Visibility visibility = Visibility::Visible;
	Size DesiredSize() const { return desired; }
	enum Visibility Visibility() const { return visibility; }
};
struct UIElementCollection {
	std::vector<UIElement> elements;
	uint32_t Size() const { return uint32_t(elements.size()); }
	const UIElement& GetAt(uint32_t index) const { return elements.at(index); }
};
struct WrapPanel {
	UIElementCollection _children;
	std::vector<Row> _rows;
	enum Orientation Orientation() const { return Orientation::Horizontal; }
	Thickness Padding() const { return {}; }
	UIElementCollection Children() const { return _children; }
	double HorizontalSpacing() const { return 0; }
	double VerticalSpacing() const { return 0; }
	enum StretchChild StretchChild() const { return StretchChild::None; }
	Size _UpdateRows(Size size);
};
#include "RtssWrapRows.g.h"
void Tests() {
	WrapPanel panel;
	panel._children.elements = { {{70,16}}, {{120,16}}, {{160,16}} };
	Require(panel._UpdateRows({400,100}).Height == 16 && panel._rows.size() == 1, "Segments fit on one row");
	Require(panel._UpdateRows({300,100}).Height == 32 && panel._rows.size() == 2, "Last segment moves whole to next row");
	Require(panel._rows[1].childrenRects[0].size.u == 160 && panel._rows[1].childrenRects[0].position.u == 0, "No clipping or splitting during row packing");
	Require(panel._UpdateRows({150,100}).Height == 48 && panel._rows.size() == 3, "Narrow width recalculates all rows");
	// XAML measures an overlong TextBlock against the full row width; row packing
	// receives its internally wrapped size, not a truncated single-line size.
	panel._children.elements = { {{70,16}}, {{150,48}}, {{80,16}} };
	Require(panel._UpdateRows({150,200}).Height == 80 && panel._rows.size() == 3, "Internally wrapped long segment retains its full height");
	panel._children.elements[1].visibility = Visibility::Collapsed;
	Require(panel._UpdateRows({150,200}).Height == 16, "Omitted segment takes no spacing");
	Require(panel._UpdateRows({400,100}).Height == 16 && panel._rows.size() == 1, "Growing width restores single row");
}
}

namespace Startup {
#undef ShellExecuteEx
#define ShellExecuteEx MockShellExecuteEx
bool elevated = false, launchSuccess = true, alwaysAdmin = true;
unsigned launches = 0;
std::vector<std::string> events;
std::wstring verb, parameters;
namespace Win32Helper {
	bool IsProcessElevated() { return elevated; }
	const std::filesystem::path& GetExePath() { static const std::filesystem::path path = L"C:\\isolated\\Magpie.exe"; return path; }
}
struct ShortcutService {
	static ShortcutService& Get() { static ShortcutService instance; return instance; }
	void Uninitialize() { events.push_back("hotkeys"); }
};
namespace fmt {
	template<class... Args> std::string format(const char*, Args...) { return "logged"; }
}
struct Logger {
	static Logger& Get() { static Logger instance; return instance; }
	void Warn(const std::string&) { events.push_back("failure"); }
	void Flush() { events.push_back("flush"); }
};
BOOL ShellExecuteEx(SHELLEXECUTEINFO* info) {
	++launches; events.push_back("launch"); verb = info->lpVerb;
	parameters = info->lpParameters ? info->lpParameters : L"";
	Require(info->fMask == SEE_MASK_NOASYNC, "Restart waits for shell dispatch");
	if (!launchSuccess) SetLastError(ERROR_CANCELLED);
	return launchSuccess;
}
struct Settings { bool IsAlwaysRunAsAdmin() const { return alwaysAdmin; } } settings;
struct App {
	void Quit() { events.push_back("quit"); }
	void _ReleaseMutexes() { events.push_back("mutex"); }
	void _Uninitialize() { events.push_back("shutdown"); }
	void Restart(bool elevated, const wchar_t* arguments) noexcept;
	bool StartupGate(const wchar_t* arguments);
};
#include "RtssRestart.g.h"
#include "RtssStartupGate.g.h"
constexpr uint32_t ADMIN_STARTUP_DEFAULTS_VERSION = 1;
struct AutoStartHelper {
	static inline bool enabled = false, success = true, lastElevated = false;
	static inline unsigned updates = 0;
	static bool IsAutoStartEnabled() { return enabled; }
	static bool EnableAutoStart(bool value) { ++updates; lastElevated = value; return success; }
};
struct AppSettings {
	uint32_t _adminAutoStartDefaultsVersion = 0;
	bool _isAlwaysRunAsAdmin = true;
	unsigned saves = 0;
	void SaveAsync() { ++saves; }
	void UpdateAdminAutoStart() noexcept;
};
#include "RtssAdminAutoStart.g.h"
void Tests() {
	App app;
	for (bool authorized : { false, true }) {
		elevated = false; launchSuccess = authorized; launches = 0; events.clear();
		Require(!app.StartupGate(L"-t -m") && launches == 1 && verb == L"runas" && parameters == L"-t -m", "Ordinary startup requests elevation exactly once and retains CLI");
		Require(events[0] == "hotkeys" && events[1] == "quit" && events[2] == "mutex" && events[3] == "launch" && events.back() == "shutdown", "Old instance releases hotkeys and mutex before replacement");
		if (!authorized) Require(std::find(events.begin(), events.end(), "failure") != events.end(), "UAC cancellation is logged before exiting");
	}
	elevated = true; launches = 0;
	Require(app.StartupGate(L"-t") && !launches, "Elevated startup does not request UAC again");
	elevated = false; alwaysAdmin = false;
	Require(app.StartupGate(L"-t") && !launches, "Later user opt-out remains effective");
	AppSettings data;
	elevated = true; AutoStartHelper::enabled = false;
	data.UpdateAdminAutoStart();
	Require(data._adminAutoStartDefaultsVersion == 1 && data.saves == 1 && AutoStartHelper::updates == 0, "First settings do not enable disabled auto-start");
	data = {}; elevated = false; AutoStartHelper::enabled = true;
	data.UpdateAdminAutoStart();
	Require(data._adminAutoStartDefaultsVersion == 0 && data.saves == 0 && AutoStartHelper::updates == 0, "Old task migration waits for elevated instance");
	elevated = true; AutoStartHelper::success = false;
	data.UpdateAdminAutoStart();
	Require(data._adminAutoStartDefaultsVersion == 0 && data.saves == 0 && AutoStartHelper::updates == 1, "Failed task registration does not claim migration");
	AutoStartHelper::success = true; data.UpdateAdminAutoStart();
	Require(data._adminAutoStartDefaultsVersion == 1 && data.saves == 1 && AutoStartHelper::lastElevated, "Existing task migrated to administrator startup");
	data.UpdateAdminAutoStart();
	Require(data.saves == 1 && AutoStartHelper::updates == 2, "Migration marker prevents repeated task updates");
	data = {}; data._isAlwaysRunAsAdmin = false; elevated = false;
	data.UpdateAdminAutoStart();
	Require(data._adminAutoStartDefaultsVersion == 1 && data.saves == 1 && !AutoStartHelper::lastElevated, "Auto-start respects later opt-out without elevation");
}
}

int main() {
	try { Layout::Tests(); Startup::Tests(); std::cout << "RTSS production layout/startup: " << checks << " checks passed. No XAML rendering or UAC.\n"; return 0; }
	catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
