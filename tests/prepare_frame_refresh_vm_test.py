"""Extract production view-model mode/number/visibility/default decisions."""
from pathlib import Path
import re
import sys

repo = Path(__file__).resolve().parents[1]
output = Path(sys.argv[1])
source = (repo / "src/Magpie/ProfileViewModel.cpp").read_text(encoding="utf-8-sig")
names = ("IsSimpleFrameRefresh", "IsAdvancedFrameRefresh", "FrameRefreshModeIndex", "ContentFrameRateModeIndex", "ContentFrameRate", "FrameSyncModeIndex", "IsContentPacingEnabled",
         "ShowContentFrameRate", "CursorRefreshModeIndex", "CursorSupplementModeIndex", "CursorSupplementRate",
         "ShowCursorSupplement", "ShowCursorSupplementRate", "IdleRedrawModeIndex", "IdleRedrawRate",
         "ShowIdleRedrawRate", "IsVRREnabled", "ResetFrameRefresh", "_SaveFrameRefresh", "SimpleRtssSync", "_SaveRtss")
declarations, definitions = [], []
for name in names:
    for match in re.finditer(r"^(?:bool|double|int32_t|void) ProfileViewModel::" + name + r"\([^\n]+?\{", source, re.M):
        signature = match[0][:-1].strip()
        declarations.append(signature.replace("ProfileViewModel::", "") + ";")
        start, end, depth = match.start(), match.end(), 1
        while depth:
            depth += (source[end] == "{") - (source[end] == "}")
            end += 1
        definitions.append(source[start:end])
prefix = r'''
#include "FrameRefreshSettings.h"
#include "RtssSettings.h"
#include <cassert>
#include <iostream>
#include <limits>
namespace Magpie {
enum class DuplicateFrameDetectionMode { Always, Dynamic, Never };
struct Profile { FrameRefreshSettings frameRefresh; RtssSettings rtss; };
struct ProfileService {
    struct Changed { unsigned count=0; void Invoke(Profile&) { ++count; } } FrameRefreshChanged, RtssSettingsChanged;
    static ProfileService& Get() { static ProfileService s; return s; }
};
struct AppSettings { unsigned saves=0; bool simple=false; void SaveAsync() { ++saves; }
    bool IsSimpleFrameRefresh() const { return simple; }
    void IsSimpleFrameRefresh(bool value) { if (simple != value) { simple=value; SaveAsync(); } }
    static AppSettings& Get() { static AppSettings s; return s; }
};
struct ProfileViewModel {
    Profile* _data;
    int32_t duplicateMode = 1;
    void DuplicateFrameDetectionMode(int32_t value) { duplicateMode = value; }
    void _NotifyRtssChanged() {}
    void _LoadRtssPreview() {}
'''
suffix = r'''
}
int main() {
    using namespace Magpie;
    Profile defaults, app;
    ProfileViewModel vm{&defaults};
    vm.IsVRREnabled(true);
    assert(vm.IsVRREnabled() && !app.frameRefresh.vrrEnabled);
    vm.ContentFrameRate(37); vm.CursorSupplementRate(144); vm.IdleRedrawRate(17);
    for (int content=0; content<2; ++content) for (int pacing=0; pacing<3; ++pacing) for (int cursor=0; cursor<3; ++cursor)
    for (int supplement=0; supplement<2; ++supplement) for (int idle=0; idle<2; ++idle) {
        vm.ContentFrameRateModeIndex(content); vm.FrameSyncModeIndex(pacing); vm.CursorRefreshModeIndex(cursor);
        vm.CursorSupplementModeIndex(supplement); vm.IdleRedrawModeIndex(idle);
        assert(vm.ContentFrameRate()==37 && vm.CursorSupplementRate()==144 && vm.IdleRedrawRate()==17);
        assert(vm.ShowContentFrameRate()==(content==1) && vm.IsContentPacingEnabled()==(content!=0));
        assert(vm.ShowCursorSupplement()==(cursor==2) && vm.ShowCursorSupplementRate()==(cursor==2 && supplement==1));
        assert(vm.ShowIdleRedrawRate()==bool(idle));
        assert(app.frameRefresh==FrameRefreshSettings{});
    }
    auto saved=defaults.frameRefresh; const auto previousSaves=AppSettings::Get().saves;
    vm.ContentFrameRateModeIndex(-1); vm.ContentFrameRateModeIndex(2); vm.CursorRefreshModeIndex(3); vm.CursorSupplementModeIndex(2);
    vm.IdleRedrawModeIndex(2); vm.FrameSyncModeIndex(9);
    assert(defaults.frameRefresh==saved && AppSettings::Get().saves==previousSaves);
    defaults.frameRefresh.legacyContentLimit=20; vm.ContentFrameRate(38);
    assert(defaults.frameRefresh.legacyContentLimit==0);
    defaults.frameRefresh.legacyResponsiveMinimum=true; vm.CursorRefreshModeIndex(0);
    assert(!defaults.frameRefresh.legacyResponsiveMinimum);
    vm.ContentFrameRate(std::numeric_limits<double>::infinity());
    vm.CursorSupplementRate(std::numeric_limits<double>::quiet_NaN()); vm.IdleRedrawRate(0);
    assert(vm.ContentFrameRate()==60 && vm.CursorSupplementRate()==60 && vm.IdleRedrawRate()==30);
    // Explicit edits quantize, without quantizing saved legacy values on read.
    vm.ContentFrameRate(59.6); vm.CursorSupplementRate(143.4); vm.IdleRedrawRate(29.5);
    assert(vm.ContentFrameRate()==60 && vm.CursorSupplementRate()==143 && vm.IdleRedrawRate()==30);
    vm.ContentFrameRate(1); vm.CursorSupplementRate(1000); vm.IdleRedrawRate(10.4);
    assert(vm.ContentFrameRate()==15 && vm.CursorSupplementRate()==360 && vm.IdleRedrawRate()==15);
    defaults.frameRefresh.contentRate=59.94f;
    assert(vm.ContentFrameRate()==double(59.94f));
    vm.FrameSyncModeIndex(2); vm.ContentFrameRateModeIndex(0);
    assert(vm.FrameSyncModeIndex()==2 && !vm.IsContentPacingEnabled());
    vm.ResetFrameRefresh();
    assert(!vm.IsVRREnabled());
    assert(defaults.frameRefresh==FrameRefreshSettings{} && app.frameRefresh==FrameRefreshSettings{});
    assert(vm.duplicateMode==int32_t(DuplicateFrameDetectionMode::Never));
    assert(AppSettings::Get().saves==ProfileService::Get().FrameRefreshChanged.count);
    defaults.frameRefresh.pacing=FrameSyncMode::Reflex;
    defaults.frameRefresh.cursorRate=144;
    defaults.frameRefresh.vrrEnabled=true;
    vm.FrameRefreshModeIndex(0);
    assert(vm.IsSimpleFrameRefresh() && !vm.IsAdvancedFrameRefresh() && vm.FrameRefreshModeIndex()==0);
    vm.FrameSyncModeIndex(0); vm.CursorSupplementRate(30); vm.IsVRREnabled(false); vm.ResetFrameRefresh();
    assert(defaults.frameRefresh.pacing==FrameSyncMode::Reflex && defaults.frameRefresh.cursorRate==144 && !defaults.frameRefresh.vrrEnabled);
    defaults.frameRefresh.cursorMode=CursorRefreshMode::OriginalOnly;
    defaults.frameRefresh.cursorSupplement=CursorSupplementMode::Auto;
    defaults.frameRefresh.legacyResponsiveMinimum=true;
    defaults.rtss={RtssScope::Global, RtssRateMode::Custom, 143, RtssExitPolicy::KeepChanges, true};
    auto expectedRtss=defaults.rtss; expectedRtss.simpleContentSync=false;
    vm.ContentFrameRate(90); vm.ContentFrameRateModeIndex(0);
    vm.IdleRedrawRate(45); vm.IdleRedrawModeIndex(0); vm.IsVRREnabled(true);
    defaults.frameRefresh.legacyContentLimit=20;
    defaults.frameRefresh.legacySourceTarget=48;
    vm.duplicateMode=int32_t(DuplicateFrameDetectionMode::Dynamic);
    const auto appBefore=app;
    const auto frameEvents=ProfileService::Get().FrameRefreshChanged.count;
    const auto rtssEvents=ProfileService::Get().RtssSettingsChanged.count;
    vm.ResetFrameRefresh();
    assert(vm.IsSimpleFrameRefresh() && vm.ContentFrameRateModeIndex()==1 && vm.ContentFrameRate()==60);
    assert(vm.IdleRedrawModeIndex()==1 && vm.IdleRedrawRate()==30 && !vm.IsVRREnabled());
    assert(defaults.rtss==expectedRtss && !vm.SimpleRtssSync());
    assert(defaults.frameRefresh.pacing==FrameSyncMode::Reflex && defaults.frameRefresh.cursorRate==144);
    assert(defaults.frameRefresh.cursorMode==CursorRefreshMode::OriginalOnly && defaults.frameRefresh.cursorSupplement==CursorSupplementMode::Auto);
    assert(defaults.frameRefresh.legacyResponsiveMinimum && vm.duplicateMode==int32_t(DuplicateFrameDetectionMode::Dynamic));
    assert(defaults.frameRefresh.legacyContentLimit==0 && defaults.frameRefresh.legacySourceTarget==-1 && !defaults.frameRefresh.legacyLimiterOnly);
    assert(app.frameRefresh==appBefore.frameRefresh && app.rtss==appBefore.rtss);
    assert(ProfileService::Get().FrameRefreshChanged.count==frameEvents+1 && ProfileService::Get().RtssSettingsChanged.count==rtssEvents+1);
    const auto once=defaults;
    vm.ResetFrameRefresh();
    assert(defaults.frameRefresh==once.frameRefresh && defaults.rtss==once.rtss);
    assert(ProfileService::Get().RtssSettingsChanged.count==rtssEvents+1);
    vm.ContentFrameRateModeIndex(1); vm.ContentFrameRate(90); vm.IdleRedrawRate(45);
    vm.FrameRefreshModeIndex(1);
    assert(vm.IsAdvancedFrameRefresh() && defaults.frameRefresh.contentRate==90 && defaults.frameRefresh.idleRate==45);
    assert(vm.CursorSupplementRate()==144 && !vm.IsVRREnabled());
    ProfileViewModel other{&app};
    other.FrameRefreshModeIndex(0);
    assert(vm.IsSimpleFrameRefresh());
    other.FrameRefreshModeIndex(-1); other.FrameRefreshModeIndex(2);
    assert(vm.IsSimpleFrameRefresh());
    std::cout << "Production refresh view model: 72 mode combinations, hidden numeric retention, invalid edits, compatibility release, profile isolation and simple/advanced reset boundaries passed\n";
}
'''
(output / "frame_refresh_vm.cpp").write_text(prefix + "\n".join(declarations) + "\n};\n" +
                                             "\n".join(definitions) + suffix, encoding="utf-8")
