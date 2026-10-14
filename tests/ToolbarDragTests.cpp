// Production toolbar with real ImGui and installed Segoe icons; no native windows or GPU.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#undef GetObject
#include <imgui.h>
#include <imgui_internal.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include "ToolbarPlacement.h"
#include "../src/Magpie.Core/PresentationFrameRate.h"
#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Magpie {
using namespace std::chrono;
template<class T> constexpr T FLOAT_EPSILON = T(0.00001);
constexpr float CORNER_ROUNDING = 6;
const char* TOOLBAR_WINDOW_ID = "toolbar";
enum class ParameterPanelState : uint8_t { Closed, Edit, Preview };
enum class ToolbarState { Off, AlwaysShow, AutoHide };
enum class OverlayAction { EffectParameters, Profiler };
enum class ScalingState { Idle, Scaling };
#include "ToolbarSession.inc"
template<class T> using SmallVector = std::vector<T>;
struct EffectPassDesc { std::string desc = "pass"; std::vector<int> outputs{ 0 }; };
struct EffectDesc {
	std::string name = "Effect";
	std::vector<EffectPassDesc> passes{{}};
	struct Texture { std::string name = "Output"; };
	std::vector<Texture> textures{{}};
};
std::string_view GetEffectDisplayName(const EffectDesc& desc) { return desc.name; }
std::string IconLabel(ImWchar icon) {
	const wchar_t value = wchar_t(icon); char bytes[4];
	const int size = WideCharToMultiByte(CP_UTF8, 0, &value, 1, bytes, 4, nullptr, nullptr);
	return std::string(bytes, size);
}
struct OverlayHelper {
#include "ToolbarIcons.inc"
};
struct StrHelper { template<class... T> static std::string Concat(T&&... t) { std::string s; (s.append(t),...); return s; } };
struct Win32Helper { struct Version { bool IsWin11() const { return true; } }; static Version GetOSVersion() { return {}; } };
bool sourceCanMinimize=true;
LONG GetWindowStyle(HWND) { return sourceCanMinimize ? WS_MINIMIZEBOX : 0; }
struct Options {
	bool windowed = false;
	bool IsWindowedMode() const { return windowed; }
	bool IsDeveloperMode() const { return false; }
	std::function<void(bool, ToolbarDock, uint32_t)> saveToolbarDock;
	struct Labels { std::string pin, profiler, parameters, comparison, screenshot, fullscreen, windowed; } toolbarShortcutLabels;
};
struct Cursor {
	POINT position{};
	POINT CursorPos() const { return position; }
	HCURSOR CursorHandle() const { return (HCURSOR)1; }
};
struct RendererMock {
	bool HasFrameGeneration() const { return false; }
	PresentationRateSnapshot PresentationRate() const { return {}; }
	RECT dest{ 100, 200, 2020, 1280 };
	const RECT& DestRect() const { return dest; }
	bool passThrough = false;
	bool IsPassThroughActive() const { return passThrough; }
	bool SetPassThroughActive(bool value) { passThrough = value; return true; }
	std::vector<const EffectDesc*> effects;
	const auto& ActiveEffectDescs() const { return effects; }
	void TakeDisplayedScreenshot() {}
	void TakeScreenshot(uint32_t, uint32_t = 0, uint32_t = 0) {}
	void Render(bool) {}
};
using Renderer = RendererMock;
struct ScalingWindow {
	Options options;
	Cursor cursor;
	RendererMock renderer;
	bool moving = false;
	static ScalingWindow& Get() { static ScalingWindow s; return s; }
	HWND Handle() const { return nullptr; }
	const auto& Options() const { return options; }
	const auto& CursorManager() const { return cursor; }
	auto& Renderer() { return renderer; }
	const auto& Renderer() const { return renderer; }
	struct Source { HWND Handle() const { return nullptr; } };
	Source SrcTracker() const { return {}; }
	bool IsResizingOrMoving() const { return moving; }
	static uint32_t RunId() { return 1; }
	struct Dispatch { template<class T> void TryEnqueue(T&&) {} };
	static Dispatch Dispatcher() { return {}; }
	void ToggleScaling(bool) {}
	void RequestStop(uint32_t) {}
};
struct InputMock {
	bool canceled = false;
	ImVec2 raw{};
	bool FrameInputCanceled() const { return canceled; }
	ImVec2 FrameMousePosition() const { return raw; }
	void Tooltip(const char*, float) {}
	const char* GetHoveredWindowId() const {
		return ImGui::GetCurrentContext()->HoveredWindow ? TOOLBAR_WINDOW_ID : nullptr;
	}
	std::optional<ImVec4> GetWindowRect(const char*) const {
		const auto w = ImGui::FindWindowByName("##toolbar");
		return w ? std::optional<ImVec4>({w->Pos.x,w->Pos.y,w->Pos.x+w->Size.x,w->Pos.y+w->Size.y}) : std::nullopt;
	}
};
struct OverlayDrawer {
	bool _isHdrOverlay = false;
	ToolbarPlacement _toolbarPlacement;
	InputMock _imguiImpl;
	float _dpiScale = 1, _lastToolbarAlpha = 1;
	bool _isToolbarPinned = true, _isToolbarVisible = true, _isProfilerVisible = false;
	bool _isEffectParametersVisible = false, _parameterFocusSwitchingEnabled = false;
	bool _parameterEditPending = false;
	uint32_t _parameterEditRunId = 0;
	bool _isCursorOnCaptionArea = false, _isToolbarItemActive = false, _isToolbarDragHovered = false, _overlayDirty = false;
	ParameterPanelState _parameterPanelState = ParameterPanelState::Closed, _pendingParameterPanelState = ParameterPanelState::Edit;
	std::optional<ImVec4> _stagedToolbarRect, _presentedToolbarRect;
	std::vector<ImVec4> _stagedToolbarButtons, _presentedToolbarButtons;
	ImFont* _fontUI = nullptr;
	ImFont* _fontIcons = nullptr;
	ImFont* _fontMonoNumbers = nullptr;
	bool _DrawToolbar(uint32_t, int&) noexcept;
	void _DrawToolbarDockHints(const ToolbarGeometry&) noexcept;
	bool IsToolbarAt(POINT) const noexcept;
	float _CalcToolbarAlpha() const noexcept;
	OverlaySessionState CaptureSessionState() const noexcept;
	void RestoreSessionState(const OverlaySessionState&) noexcept;
	bool IsEditingParameters() const { return false; }
	bool _HasParameterForeground() const { return false; }
	void _SetParameterPanelState(ParameterPanelState value, bool, const char* = "test") { _parameterPanelState = value; _isEffectParametersVisible = value != ParameterPanelState::Closed; }
	void _ClearStatesIfNoVisibleWindow() {}
	void InvokeAction(OverlayAction action) { if (action == OverlayAction::EffectParameters) _isEffectParametersVisible = !_isEffectParametersVisible; else _isProfilerVisible = !_isProfilerVisible; }
	std::string _GetResourceString(std::wstring_view) const { return "Hint"; }
	std::string frameRateText = "120/240 FPS";
	std::string _FormatFrameRate(uint32_t) const { return frameRateText; }
	void _ShowComparisonStatus(bool) {}
	ToolbarState ToolbarState() const { return ToolbarState::AlwaysShow; }
	void ToolbarState(Magpie::ToolbarState) {}
};
struct Profile {
	ToolbarDockSettings toolbarDocks;
	std::shared_ptr<const uint8_t> runtimeIdentity = std::make_shared<const uint8_t>(0);
	int unrelated = 42;
};
struct AppSettings {
	Profile defaultProfile;
	std::vector<Profile> profiles;
	int saves = 0;
	static AppSettings& Get() { static AppSettings s; return s; }
	Profile& DefaultProfile() { return defaultProfile; }
	auto& Profiles() { return profiles; }
	void SaveAsync() { ++saves; }
};
struct RuntimeMock {
	ScalingState state = ScalingState::Scaling;
	uint32_t id = 1;
	ScalingState State() const { return state; }
	uint32_t RunId() const { return id; }
};
struct ScalingService {
	std::optional<RuntimeMock> _scalingRuntime{ RuntimeMock{} };
	void _SaveToolbarDock(std::weak_ptr<const uint8_t>, bool, ToolbarDock, uint32_t);
};
struct JsonHelper {
	static bool ReadUInt(const rapidjson::GenericObject<true, rapidjson::Value>&,
		const char*, uint32_t&, bool required = false) noexcept;
};
std::vector<ImVec4> toolbarButtons;
bool TrackedToolbarButton(const char* label, const ImVec2& size) {
	const bool clicked = ImGui::Button(label, size);
	const auto actual = ImGui::GetItemRectSize();
	assert(std::abs(actual.x-size.x)<.01f && std::abs(actual.y-size.y)<.01f);
	assert(ImGui::GetStyle().ButtonTextAlign.x==.5f && ImGui::GetStyle().ButtonTextAlign.y==.5f);
	const auto lo=ImGui::GetItemRectMin(), hi=ImGui::GetItemRectMax();
	toolbarButtons.emplace_back(lo.x,lo.y,hi.x,hi.y);
	return clicked;
}
ImVec4 toolbarText;
void TrackedToolbarText(const char* text) {
	ImGui::TextUnformatted(text);
	// Read the rendered item: TextUnformatted can add the preceding line's baseline offset.
	const auto min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
	toolbarText={min.x,min.y,max.x,max.y};
}
#include "ToolbarDragProduction.inc"
}
using namespace Magpie;

void Near(float a, float b) { assert(std::abs(a-b) < 1.1f); }

void PlacementTests() {
	ToolbarPlacement placement;
	ToolbarGeometry geometry(1920, 1080, 1);
	const auto original = placement.Layout(false, geometry);
	Near(original.x, 732); Near(original.y, -6);
	placement.Begin(false, geometry, original.x+10, 0);
	auto rect = placement.Layout(false, geometry);
	Near(rect.x,original.x); Near(rect.y,original.y);
	placement.Update(geometry, 500, 700);
	assert(!placement.Target());
	assert(!placement.Release(geometry,500,700));
	Near(placement.Layout(false,geometry).x,original.x);
	assert(placement.Dock(false)==ToolbarDock::Top);
	placement.Begin(false,geometry,original.x+10,0);
	assert(placement.Release(geometry,300,1079)==ToolbarDock::Bottom);
	Near(placement.Layout(false,geometry).x,290);
	assert(placement.Dock(false)==ToolbarDock::Bottom && placement.Dock(true)==ToolbarDock::Top);
	const auto bottom=placement.Layout(false,geometry);
	placement.Begin(false,geometry,bottom.x+10,1079);
	assert(!placement.Release(geometry,800,1079));
	Near(placement.Layout(false,geometry).x,790);
	placement.Begin(false,geometry,800,1079);
	placement.Update(geometry,1000,0); placement.Cancel();
	assert(placement.Dock(false)==ToolbarDock::Bottom);
	Near(placement.Layout(false,geometry).x,790);
	placement.Begin(false,geometry,800,1079);
	assert(!placement.Release(geometry,1950,0)); // pointer outside viewport is invalid
	assert(placement.Dock(false)==ToolbarDock::Bottom);
	for (float dpi : {1.0f,1.25f,1.5f,2.0f}) {
		for (auto viewport : {ImVec2{1920,1080},ImVec2{800,600},ImVec2{320,240},ImVec2{80,60}}) {
			ToolbarGeometry g(viewport.x,viewport.y,dpi);
			for (bool windowed : {true,false}) {
				placement.state.horizontal[windowed?1:0]=5000;
				auto r=placement.Layout(windowed,g);
				assert(r.x>=0 && r.x+r.width<=viewport.x+0.01f);
				assert(r.y<viewport.y && r.y+r.height>0);
				placement.Begin(windowed,g,r.x+10*g.scale,std::max(0.0f,r.y+15*g.scale));
				placement.Release(g,viewport.x-1,viewport.y-1);
				r=placement.Layout(windowed,g);
				assert(r.x>=0 && r.x+r.width<=viewport.x+0.01f);
			}
		}
	}
	placement.state.horizontal[0]=std::numeric_limits<float>::quiet_NaN();
	assert(std::isfinite(placement.Layout(false,geometry).x));
	ToolbarPlacement fresh;
	fresh.state.docks = placement.state.docks;
	Near(fresh.Layout(false,geometry).x,732);
}

void CenterSnapTests() {
    for (float dpi : {1.f,1.25f,1.5f,2.f}) for (float width : {1920.f,1921.f}) {
        ToolbarGeometry g(width,1080,dpi);
        const float center=(g.viewportWidth-g.width)/2;
        for (bool windowed : {false,true}) for (auto dock : {ToolbarDock::Top,ToolbarDock::Bottom}) {
            for (float grab : {2.f,18.f,23.f}) {
                ToolbarPlacement p; p.state.docks.ForMode(windowed)=dock;
                const float y=dock==ToolbarDock::Top ? 1.f : 1079.f;
                p.Begin(windowed,g,center+grab,y);
                for (float delta : {-13.f,-12.f,-11.f,0.f,11.f,12.f,13.f,0.f,-13.f}) {
                    p.Update(g,center+grab+delta,y);
                    assert(p.IsCenterSnapped()==(std::abs(delta)<=12));
                    const auto shown=p.Layout(windowed,g), preview=p.Preview(g);
                    assert(std::abs(shown.x-preview.x)<1e-5f);
                    assert(std::abs(shown.x-(std::abs(delta)<=12 ? center : center+delta))<1e-5f);
                }
                p.Update(g,center+grab,500); assert(!p.IsCenterSnapped() && !p.Target());
                p.Update(g,center+grab,y); assert(p.IsCenterSnapped());
                assert(!p.Release(g,center+grab+12,y));
                assert(!p.IsCenterSnapped() && !p.IsDragging());
                assert(std::abs(p.Layout(windowed,g).x-center)<1e-5f);
                p.Begin(windowed,g,center+grab,y); p.Update(g,center+grab+20,y);
                p.Cancel(); assert(!p.IsCenterSnapped());
                assert(std::abs(p.Layout(windowed,g).x-center)<1e-5f);
            }
        }
    }
}

void SettingsTests() {
	Profile p;
	for (const char* text : {"{}", R"({"fullscreenToolbarDock":null,"windowedToolbarDock":false})",
		R"({"fullscreenToolbarDock":-1,"windowedToolbarDock":2})",
		R"({"fullscreenToolbarDock":"Bottom","windowedToolbarDock":1.5})"}) {
		p.toolbarDocks={ToolbarDock::Bottom,ToolbarDock::Bottom}; LoadDocks(p,text);
		assert(p.toolbarDocks.fullscreen==ToolbarDock::Top && p.toolbarDocks.windowed==ToolbarDock::Top);
	}
	for (auto fullscreen : {ToolbarDock::Top,ToolbarDock::Bottom}) {
		for (auto windowed : {ToolbarDock::Top,ToolbarDock::Bottom}) {
			p.toolbarDocks={fullscreen,windowed}; Profile copy; const auto json=SaveDocks(p);
			LoadDocks(copy,json.c_str());
			assert(copy.toolbarDocks.fullscreen==fullscreen && copy.toolbarDocks.windowed==windowed);
			rapidjson::Document d; d.Parse(json.c_str()); assert(d.MemberCount()==2);
		}
	}
	auto& s=AppSettings::Get(); s=AppSettings{}; s.profiles.resize(2);
	ScalingService service;
	const auto app=s.profiles[0].runtimeIdentity, other=s.profiles[1].runtimeIdentity;
	service._SaveToolbarDock(app,false,ToolbarDock::Bottom,1);
	assert(s.profiles[0].toolbarDocks.fullscreen==ToolbarDock::Bottom && s.saves==1);
	assert(s.profiles[0].toolbarDocks.windowed==ToolbarDock::Top && s.defaultProfile.toolbarDocks.fullscreen==ToolbarDock::Top);
	std::swap(s.profiles[0],s.profiles[1]);
	service._SaveToolbarDock(app,true,ToolbarDock::Bottom,1);
	assert(s.profiles[1].toolbarDocks.windowed==ToolbarDock::Bottom && s.profiles[0].toolbarDocks.windowed==ToolbarDock::Top);
	assert(s.profiles[1].unrelated==42 && s.saves==2);
	service._SaveToolbarDock(app,true,ToolbarDock::Bottom,1); assert(s.saves==2);
	s.profiles.erase(s.profiles.begin()+1); s.profiles.emplace_back();
	service._SaveToolbarDock(app,false,ToolbarDock::Bottom,1); assert(s.saves==2);
	service._SaveToolbarDock(other,true,ToolbarDock::Bottom,99); assert(s.saves==2);
	service._scalingRuntime->state=ScalingState::Idle;
	service._SaveToolbarDock(other,true,ToolbarDock::Bottom,1); assert(s.saves==2);
	service._scalingRuntime->state=ScalingState::Scaling;
	service._SaveToolbarDock(s.defaultProfile.runtimeIdentity,true,ToolbarDock::Bottom,1);
	assert(s.defaultProfile.toolbarDocks.windowed==ToolbarDock::Bottom && s.saves==3);
	assert(s.defaultProfile.toolbarDocks.fullscreen==ToolbarDock::Top && s.profiles[0].toolbarDocks.windowed==ToolbarDock::Top);
}

void Frame(OverlayDrawer& drawer, ImVec2 mouse, bool down=false, bool canceled=false, bool right=false,
	ImVec2 viewport={1920,1080}) {
	auto& io=ImGui::GetIO(); io.DisplaySize=viewport; io.DeltaTime=1.0f/60;
	io.AddMousePosEvent(mouse.x,mouse.y); io.AddMouseButtonEvent(0,down); io.AddMouseButtonEvent(1,right);
	drawer._imguiImpl.raw=mouse; drawer._imguiImpl.canceled=canceled;
	ScalingWindow::Get().cursor.position={LONG(mouse.x+100),LONG(mouse.y+200)};
	ImGui::NewFrame(); if(canceled) ImGui::ClearActiveID();
	toolbarButtons.clear();
	int id=0; drawer._DrawToolbar(120,id); ImGui::Render();
	assert(toolbarButtons.size()==(sourceCanMinimize ? 8 : 7));
	for (const auto& rect : toolbarButtons) {
		if (std::abs(rect.y-toolbarButtons.front().y)>=.01f) {
			std::cerr<<"Button alignment: dpi="<<drawer._dpiScale<<" viewport="<<viewport.x<<','<<viewport.y
				<<" firstY="<<toolbarButtons.front().y<<" otherY="<<rect.y<<'\n';
		}
		assert(std::abs(rect.y-toolbarButtons.front().y)<.01f);
		assert(std::abs((rect.z-rect.x)-(toolbarButtons.front().z-toolbarButtons.front().x))<.01f);
	}
	drawer._presentedToolbarRect=drawer._stagedToolbarRect;
	drawer._presentedToolbarButtons=drawer._stagedToolbarButtons;
	const auto bar=drawer._presentedToolbarRect.value();
	// CalcTextSize rounds the scaled text width to whole pixels.
	if (std::abs((toolbarText.x+toolbarText.z)-(bar.x+bar.z))>=1.01f) {
		std::cerr << "FPS horizontal offset: " << (toolbarText.x+toolbarText.z-bar.x-bar.z)/2.f
			<< " px, dpi=" << drawer._dpiScale << " text=" << drawer.frameRateText << '\n';
	}
	assert(std::abs((toolbarText.x+toolbarText.z)-(bar.x+bar.z))<1.01f);
	const auto button = toolbarButtons.front();
	const float verticalOffset = (toolbarText.y+toolbarText.w-button.y-button.w)/2.f;
	if (std::abs(verticalOffset)>=.02f) {
		std::cerr << "FPS vertical offset: " << verticalOffset << " px, dpi=" << drawer._dpiScale << '\n';
	}
	assert(std::abs(verticalOffset)<.02f);
	assert(toolbarText.x >= toolbarButtons[4].z+1.f*ToolbarGeometry(viewport.x,viewport.y,drawer._dpiScale).scale);
	assert(toolbarText.z <= toolbarButtons[5].x-1.f*ToolbarGeometry(viewport.x,viewport.y,drawer._dpiScale).scale);
}

void ImGuiToolbarTests() {
	ImGui::CreateContext();
	auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.ConfigInputTrickleEventQueue=false;
	ImFontConfig ui; ui.SizePixels=18; auto font=io.Fonts->AddFontDefault(&ui);
	char windows[MAX_PATH]; assert(GetWindowsDirectoryA(windows,MAX_PATH));
	const std::string iconPath=std::string(windows)+"/Fonts/SegoeIcons.ttf";
	auto iconFont=io.Fonts->AddFontFromFileTTF(iconPath.c_str(),16,nullptr,OverlayHelper::ICON_RANGES);
	assert(iconFont);
	unsigned char* pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
	ImGui::GetStyle().WindowMinSize={1,1};
	OverlayDrawer drawer; drawer._fontUI=font; drawer._fontMonoNumbers=font; drawer._fontIcons=iconFont;
	std::vector<std::pair<bool,ToolbarDock>> saved;
	ScalingWindow::Get().options.saveToolbarDock=[&](bool windowed,ToolbarDock dock,uint32_t) { saved.emplace_back(windowed,dock); };
	for(bool windowed : {false,true}) {
		ScalingWindow::Get().options.windowed=windowed;
		drawer._toolbarPlacement.state={};
		Frame(drawer,{-100,-100}); Frame(drawer,{-100,-100});
		const auto r=drawer._presentedToolbarRect.value();
		// Button clicks never begin a drag; the central FPS is a drag surface.
		const auto pin=drawer._presentedToolbarButtons[0];
		const ImVec2 pinPoint{(pin.x+pin.z)/2,(pin.y+pin.w)/2};
		Frame(drawer,pinPoint); Frame(drawer,pinPoint,true);
		assert(!drawer._toolbarPlacement.IsDragging()); Frame(drawer,pinPoint);
		drawer._isToolbarPinned=true;
		const ImVec2 center{(r.x+r.z)/2,10};
		Frame(drawer,center); Frame(drawer,center,true);
		assert(drawer._toolbarPlacement.IsDragging()); Frame(drawer,center,false,true);
		assert(!drawer._toolbarPlacement.IsDragging());
		ImVec2 press{r.x+2,10};
		Frame(drawer,press); Frame(drawer,press,true);
		assert(drawer._toolbarPlacement.IsDragging() && !drawer._isCursorOnCaptionArea);
		assert(ImGui::GetBackgroundDrawList()->VtxBuffer.Size > 0);
		std::vector<ImVec2> cyan;
		for (const auto& vertex : ImGui::GetBackgroundDrawList()->VtxBuffer)
			if (vertex.col == IM_COL32(0,255,255,255)) cyan.push_back(vertex.pos);
		assert(cyan.size()==4);
		assert(cyan[1].x-cyan[0].x==2 && cyan[2].y-cyan[0].y==1080);
		assert(cyan[0].x==959 && cyan[0].y==0);
		Near(ImGui::FindWindowByName("##toolbar")->Pos.x,732);
		Frame(drawer,{500,700},true); assert(!drawer._toolbarPlacement.Target());
		Frame(drawer,{500,700}); assert(!drawer._toolbarPlacement.IsDragging() && saved.empty());
		assert(ImGui::GetBackgroundDrawList()->VtxBuffer.Size == 0);
		Near(ImGui::FindWindowByName("##toolbar")->Pos.x,732);
		Frame(drawer,press); Frame(drawer,press,true);
		Frame(drawer,{350,1079},true);
		assert(drawer._toolbarPlacement.Target()==ToolbarDock::Bottom);
		Frame(drawer,{350,1079});
		assert(saved.size()==1 && saved.back()==std::make_pair(windowed,ToolbarDock::Bottom));
		assert(drawer._toolbarPlacement.Dock(windowed)==ToolbarDock::Bottom);
		Frame(drawer,{350,1079});
		const auto b=drawer._presentedToolbarRect.value();
		assert(drawer.IsToolbarAt({LONG(b.x+101),1279}));
		assert(!drawer.IsToolbarAt({LONG(b.z+101),1279}));
		press={b.x+2,1070}; Frame(drawer,press); Frame(drawer,press,true);
		Frame(drawer,{800,1079},true); Frame(drawer,{800,1079});
		assert(saved.size()==1); // horizontal-only move never saves
		const auto state=drawer.CaptureSessionState();
		OverlayDrawer restored; restored.RestoreSessionState(state);
		Near(restored._toolbarPlacement.Layout(windowed,ToolbarGeometry(1920,1080,1)).x,
			drawer._toolbarPlacement.Layout(windowed,ToolbarGeometry(1920,1080,1)).x);
		Frame(drawer,{800,1070}); const auto grip=drawer._presentedToolbarRect.value();
		press={grip.x+2,1070}; Frame(drawer,press); Frame(drawer,press,true);
		Frame(drawer,{1000,0},true); Frame(drawer,{1000,0},false,true);
		assert(!drawer._toolbarPlacement.IsDragging() && drawer._toolbarPlacement.Dock(windowed)==ToolbarDock::Bottom && saved.size()==1);
		// Bottom screenshot menu must fit above the toolbar on its first visible frame.
		Frame(drawer,{-100,-100});
		auto toolbar=ImGui::FindWindowByName("##toolbar");
		const auto cameraRect=drawer._presentedToolbarButtons[4];
		ImVec2 camera{(cameraRect.x+cameraRect.z)/2,(cameraRect.y+cameraRect.w)/2};
		Frame(drawer,camera); Frame(drawer,camera,false,false,true); Frame(drawer,camera);
		Frame(drawer,camera);
		bool sawPopup = false;
		for(auto popup:ImGui::GetCurrentContext()->Windows) {
			if(popup->Active && (popup->Flags & ImGuiWindowFlags_Popup)) {
				sawPopup = true;
				assert(popup->Pos.y>=0 && popup->Pos.y+popup->Size.y<=1049.1f);
			}
		}
		assert(sawPopup);
		ImGui::ClosePopupsExceptModals();
		drawer._isToolbarPinned=false; drawer._isToolbarItemActive=false;
		ScalingWindow::Get().cursor.position={LONG(toolbar->Pos.x+110),1279};
		Near(drawer._CalcToolbarAlpha(),1);
		ScalingWindow::Get().cursor.position={1100,600};
		Near(drawer._CalcToolbarAlpha(),0);
		drawer._isToolbarPinned=true; saved.clear();
	}
	for(float dpi : {1.0f,1.25f,1.5f,2.0f}) {
		drawer._dpiScale=dpi; io.FontGlobalScale=dpi;
		for(auto viewport : {ImVec2{1920,1080},ImVec2{800,600},ImVec2{320,240},ImVec2{80,60}}) {
			for(auto dock : {ToolbarDock::Top,ToolbarDock::Bottom}) {
				drawer._toolbarPlacement.state.docks.windowed=dock;
				drawer._toolbarPlacement.state.horizontal[1]=5000;
				Frame(drawer,{-100,-100},false,false,false,viewport);
				auto toolbar=ImGui::FindWindowByName("##toolbar");
				assert(toolbar->Pos.x>=0 && toolbar->Pos.x+toolbar->Size.x<=viewport.x+1);
				const auto grip=drawer._presentedToolbarRect.value();
				assert(grip.x>=0 && grip.z<=viewport.x+1 && grip.y>=0 && grip.w<=viewport.y+1 && grip.y<grip.w);
			}
		}
	}
	for (float dpi : {1.f,1.25f,1.5f,2.f}) for (bool windowed : {false,true})
		for (auto dock : {ToolbarDock::Top,ToolbarDock::Bottom}) for (bool minimize : {true,false})
			for (const char* format : {"60 FPS","120/240 FPS","—/60 FPS","1000/4000 FPS"}) {
		drawer._dpiScale=dpi;
		font->Scale=dpi; iconFont->Scale=dpi;
		ScalingWindow::Get().options.windowed=windowed;
		drawer._toolbarPlacement.state.docks.ForMode(windowed)=dock;
		sourceCanMinimize=minimize; drawer.frameRateText=format;
		Frame(drawer,{-100,-100});
	}
	for (bool hdr : {false,true,false}) {
		drawer._isHdrOverlay=hdr;
		Frame(drawer,{-100,-100});
		const auto* toolbar=ImGui::FindWindowByName("##toolbar");
		unsigned maxBackgroundAlpha=0;
		for (const auto& vertex : toolbar->DrawList->VtxBuffer) {
			if ((vertex.col & ~IM_COL32_A_MASK)==(IM_COL32(15,15,15,0) & ~IM_COL32_A_MASK))
				maxBackgroundAlpha=std::max(maxBackgroundAlpha,(vertex.col >> IM_COL32_A_SHIFT) & 255u);
		}
		assert(maxBackgroundAlpha==(hdr ? 255u : 180u));
	}
	ImGui::DestroyContext();
}

int main() {
	PlacementTests(); CenterSnapTests(); SettingsTests(); ImGuiToolbarTests();
	std::cout << "PASS toolbar drag: real ImGui background/FPS drag, button/drop/cancel/menu, 32 mode/viewport/DPI cases, "
		"128 FPS horizontal/vertical alignment cases, raw grab offset, session recovery/new-run centering, "
		"JSON compatibility, profile isolation/reordering/deletion and expired saves.\n";
}
