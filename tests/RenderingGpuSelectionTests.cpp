// Real WinRT box/vector identity; resources, persistence and notifications are doubles.
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <wil/resource.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;

struct ResourceLoader {
    static ResourceLoader GetForCurrentView(const wchar_t*) { return {}; }
    hstring GetString(const wchar_t* key) const { return hstring(key); }
};
struct CommonSharedConstants { static constexpr const wchar_t* APP_RESOURCE_MAP_ID = L"test"; };

namespace Magpie {
#include "GraphicsCardIdProduction.inc"
struct Profile { GraphicsCardId graphicsCardId; };
struct AdapterInfo { uint32_t idx, vendorId, deviceId; std::wstring description; };
struct AdaptersService {
    std::vector<AdapterInfo> adapters;
    static AdaptersService& Get() { static AdaptersService value; return value; }
    const std::vector<AdapterInfo>& AdapterInfos() const { return adapters; }
};
struct AppSettings {
    unsigned saves = 0;
    static AppSettings& Get() { static AppSettings value; return value; }
    void SaveAsync() { ++saves; }
};
struct ProfileViewModel {
    Profile* _data;
    IVector<IInspectable> _graphicsCards{nullptr};
    std::vector<GraphicsCardId> _graphicsCardOptions;
    bool _isHandlingAdapterChanged = false;
    std::function<void(std::wstring_view)> notify;
    void RaisePropertyChanged(const wchar_t* name) { if (notify) notify(name); }
#include "RenderingGpuViewModelDeclarations.inc"
};
#include "RenderingGpuViewModelProduction.inc"
}

static unsigned checks = 0;
static void Check(bool value, const char* message) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static bool Same(Magpie::GraphicsCardId a, Magpie::GraphicsCardId b) {
    return a.idx == b.idx && a.vendorId == b.vendorId && a.deviceId == b.deviceId;
}
static void Selected(Magpie::ProfileViewModel& vm, uint32_t index, const char* message) {
    Check(vm.GraphicsCard() == vm.GraphicsCards().GetAt(index), message);
}

int main() {
    init_apartment(apartment_type::single_threaded);
    using namespace Magpie;
    Profile defaults, app;
    ProfileViewModel vm{&defaults, nullptr, {}, false, {}};
    auto& adapters = AdaptersService::Get().adapters;
    auto& settings = AppSettings::Get();
    const AdapterInfo nvidia{4, 0x10de, 0x1234, L"Same GPU name"};
    const AdapterInfo amd{0, 0x1002, 0x5678, L"AMD GPU"};
    const AdapterInfo sameModel{9, 0x10de, 0x1234, L"Same GPU name"};
    const GraphicsCardId pinned{4, nvidia.vendorId, nvidia.deviceId};

    vm._RefreshGraphicsCardSettings();
    Check(vm.GraphicsCards().Size() == 1 && vm.IsNoGraphicsCard(), "zero-card placeholder remains visible");
    Check(!vm.IsGraphicsCardSelectionEnabled(), "zero-card selector is disabled");
    Selected(vm, 0, "zero-card placeholder is selected");
    vm.GraphicsCard(vm.GraphicsCard());
    Check(settings.saves == 0, "zero-card binding writes do not save");

    adapters = {nvidia};
    vm._RefreshGraphicsCardSettings();
    Check(vm.GraphicsCards().Size() == 2 && vm.IsGraphicsCardSelectionEnabled(), "single card offers Auto and explicit GPU");
    Selected(vm, 0, "single-card automatic policy selected");
    vm.GraphicsCard(vm.GraphicsCards().GetAt(1));
    Check(Same(defaults.graphicsCardId, pinned) && settings.saves == 1, "single card can be pinned");
    Selected(vm, 1, "pinned single card stays selected after list refresh");
    vm.GraphicsCard(vm.GraphicsCard());
    Check(settings.saves == 1 && Same(app.graphicsCardId, {}), "binding echo does not save and app profile is isolated");

    auto oldItems = vm.GraphicsCards();
    auto oldAuto = oldItems.GetAt(0);
    auto oldPinned = oldItems.GetAt(1);
    auto sameText = box_value(unbox_value<hstring>(oldAuto));
    uint32_t index = 0;
    Check(!oldItems.IndexOf(sameText, index), "real IVector distinguishes equal-label boxed identities");
    adapters = {amd, nvidia, sameModel};
    vm.notify = [&](std::wstring_view) { vm.GraphicsCard(vm.GraphicsCards().GetAt(0)); };
    vm._RefreshGraphicsCardSettings();
    vm.notify = {};
    Check(Same(defaults.graphicsCardId, pinned) && settings.saves == 1, "synchronous binding writes during refresh are ignored");
    Selected(vm, 2, "selection follows GPU identity after list positions shift");
    vm.GraphicsCard(oldAuto);
    vm.GraphicsCard(oldPinned);
    vm.GraphicsCard(sameText);
    vm.GraphicsCard(nullptr);
    Check(Same(defaults.graphicsCardId, pinned) && settings.saves == 1, "deferred old Auto/GPU and null writes are ignored");

    vm.GraphicsCard(vm.GraphicsCards().GetAt(3));
    Check(defaults.graphicsCardId.idx == 9, "same-model and same-name GPUs remain distinguishable by DXGI index");
    vm.GraphicsCard(vm.GraphicsCards().GetAt(2));
    Check(Same(defaults.graphicsCardId, pinned), "explicit selection maps original DXGI index rather than vector position");

    // Adapter service has preserved IDs and changed only idx while the GPU is absent.
    adapters = {amd};
    defaults.graphicsCardId.idx = -1;
    const auto missing = defaults.graphicsCardId;
    const auto beforeMissing = settings.saves;
    vm._RefreshGraphicsCardSettings();
    Check(vm.ShowGraphicsCardUnavailableWarning() && vm.GraphicsCards().Size() == 3, "missing request has a distinct option and warning");
    Selected(vm, 2, "missing request is not misrepresented as Auto");
    vm.GraphicsCard(vm.GraphicsCard());
    Check(Same(defaults.graphicsCardId, missing) && settings.saves == beforeMissing, "missing-option echo preserves IDs without saving");

    adapters.clear();
    oldItems = vm.GraphicsCards();
    vm._RefreshGraphicsCardSettings();
    Check(vm.IsNoGraphicsCard() && !vm.ShowGraphicsCardUnavailableWarning(), "zero candidates use only the CPU-attempt warning");
    Selected(vm, 0, "zero-card missing request still has a placeholder");
    vm.GraphicsCard(oldItems.GetAt(0));
    vm.GraphicsCard(vm.GraphicsCard());
    Check(Same(defaults.graphicsCardId, missing), "zero-card list preserves the absent request");

    // Existing service rematches the returning card to its new DXGI index.
    adapters = {amd, {7, nvidia.vendorId, nvidia.deviceId, nvidia.description}};
    defaults.graphicsCardId.idx = 7;
    vm._RefreshGraphicsCardSettings();
    Check(!vm.ShowGraphicsCardUnavailableWarning() && vm.GraphicsCards().Size() == 3, "restored device removes missing state");
    Selected(vm, 2, "restored device is selected at its new index");
    vm.GraphicsCard(vm.GraphicsCards().GetAt(0));
    Check(Same(defaults.graphicsCardId, {}), "deliberate Auto clears all saved device identity");
    vm._RefreshGraphicsCardSettings();
    Selected(vm, 0, "automatic policy remains automatic after later device changes");

    defaults.graphicsCardId = {-1, nvidia.vendorId, nvidia.deviceId};
    adapters = {amd};
    vm._RefreshGraphicsCardSettings();
    vm.GraphicsCard(vm.GraphicsCards().GetAt(1));
    Check(Same(defaults.graphicsCardId, {0, amd.vendorId, amd.deviceId}), "choosing another GPU replaces the missing request");
    Check(!vm.ShowGraphicsCardUnavailableWarning() && vm.GraphicsCards().Size() == 2, "new explicit request removes missing option");

    ProfileViewModel appVm{&app, nullptr, {}, false, {}};
    appVm._RefreshGraphicsCardSettings();
    const auto savedDefault = defaults.graphicsCardId;
    appVm.GraphicsCard(appVm.GraphicsCards().GetAt(1));
    Check(Same(defaults.graphicsCardId, savedDefault) && app.graphicsCardId.vendorId == amd.vendorId, "application edits do not affect default profile");
    vm.notify = [](std::wstring_view) { throw std::runtime_error("notification failed"); };
    try { vm._RefreshGraphicsCardSettings(); } catch (const std::runtime_error&) {}
    Check(!vm._isHandlingAdapterChanged, "notification exception restores binding-write guard");
    vm.notify = {};

    // Repeated list changes must not turn stable automatic or pinned intent into a user edit.
    for (unsigned iteration = 0; iteration < 50; ++iteration) {
        const bool explicitChoice = iteration % 2 != 0;
        defaults.graphicsCardId = explicitChoice ? pinned : GraphicsCardId{};
        adapters = {nvidia};
        vm._RefreshGraphicsCardSettings();
        const auto staleAuto = vm.GraphicsCards().GetAt(0);
        const auto saved = defaults.graphicsCardId;
        const auto saveCount = settings.saves;
        adapters = {amd, nvidia};
        vm._RefreshGraphicsCardSettings();
        vm.GraphicsCard(staleAuto);
        Check(Same(defaults.graphicsCardId, saved) && settings.saves == saveCount, "repeated hotplug preserves intent and avoids extra saves");
    }
    std::printf("Rendering GPU selection: %u checks passed; real WinRT identity, zero/single/multi/missing/restored, stale writes and profile isolation.\n", checks);
}
