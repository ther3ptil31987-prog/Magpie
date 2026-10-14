"""Compile the production global preference and startup policy in an isolated host."""
from pathlib import Path
import re
import sys

repo = Path(__file__).resolve().parents[1]
output = Path(sys.argv[1])

def read(path):
    return (repo / path).read_text(encoding="utf-8-sig")

def block(source, signature):
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]

def match(pattern, source):
    result = re.search(pattern, source, re.S)
    assert result, pattern
    return result[0]

header = read("src/Magpie/AppSettings.h")
source = read("src/Magpie/AppSettings.cpp")
service = read("src/Magpie/ScalingService.cpp")
declaration = match(r"bool _isSimpleFrameRefresh = .*?;", header)
accessors = block(header, "bool IsSimpleFrameRefresh()") + "\n" + block(header, "void IsSimpleFrameRefresh(bool")
load = match(r'_isSimpleFrameRefresh = true;\s*JsonHelper::ReadBool\(root, "simpleFrameRefresh", _isSimpleFrameRefresh\);', source)
save = match(r'writer.Key\("simpleFrameRefresh"\);\s*writer.Bool\(data._isSimpleFrameRefresh\);', source)
startup = "\n".join(match(pattern, service) for pattern in (
    r"options.isSimpleFrameRefresh = AppSettings::Get\(\).IsSimpleFrameRefresh\(\);",
    r"ApplyFrameRefreshSettings\(options, profile.frameRefresh, options.isSimpleFrameRefresh\);",
    r"options.duplicateFrameDetectionMode = options.isSimpleFrameRefresh \?.*?;",
    r"options.IsStatisticsForDynamicDetectionEnabled\(!options.isSimpleFrameRefresh.*?;",
    r"const auto rtssSession = RtssService::Get\(\).Begin\(hWnd, profile.runtimeIdentity,.*?;",
))
duplicate = block(read("src/Magpie.Core/include/ScalingOptions.h"), "enum class DuplicateFrameDetectionMode") + ";"
json_read = block(read("src/Magpie/JsonHelper.cpp"), "bool JsonHelper::ReadBool(")
prefix = r'''
#include "FrameRefreshSettings.h"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <cassert>
#include <iostream>
#include <utility>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <string>
#include "EffectParameterPersistence.h"
#include "FrameGenerationParameters.h"
using namespace Magpie;
struct JsonHelper {
    static bool ReadBool(const rapidjson::GenericObject<true, rapidjson::Value>&, const char*, bool&, bool = false) noexcept;
};
'''
prefix += '#include "' + (repo / "src/Magpie/RtssSettings.h").as_posix() + '"\n' + json_read + "\n" + duplicate
prefix += "\nstruct AppSettings {\n" + declaration + r'''
    unsigned saves=0;
    struct Event { unsigned count=0; void Invoke() { ++count; } } FrameRefreshModeChanged;
    void SaveAsync() { ++saves; }
    static AppSettings& Get() { static AppSettings settings; return settings; }
    ::DuplicateFrameDetectionMode DuplicateFrameDetectionMode() const { return ::DuplicateFrameDetectionMode::Always; }
    bool IsStatisticsForDynamicDetectionEnabled() const { return true; }
'''
prefix += accessors + '\nvoid Load(const rapidjson::GenericObject<true, rapidjson::Value>& root) {\n' + load + '}\n};\n'
prefix += 'std::string Save(const AppSettings& data) { rapidjson::StringBuffer b; rapidjson::Writer<rapidjson::StringBuffer> writer(b); writer.StartObject();\n' + save + '\nwriter.EndObject(); return b.GetString(); }\n'
duplicate_load = match(r"uint32_t duplicateFrameDetectionMode = .*?_duplicateFrameDetectionMode = .*?;", source)
read_uint = block(read("src/Magpie/JsonHelper.cpp"), "bool JsonHelper::ReadUInt(")
prefix = prefix.replace("static bool ReadBool", "static bool ReadUInt(const rapidjson::GenericObject<true, rapidjson::Value>&, const char*, uint32_t&, bool = false) noexcept;\n    static bool ReadBool")
prefix += read_uint + "\nnamespace Magpie { using ::DuplicateFrameDetectionMode; }\n"
prefix += "DuplicateFrameDetectionMode ReadDuplicate(const rapidjson::GenericObject<true, rapidjson::Value>& root) { DuplicateFrameDetectionMode _duplicateFrameDetectionMode;\n" + duplicate_load + "\nreturn _duplicateFrameDetectionMode; }\n"
prefix += r'''
struct Options {
    FrameRefreshSettings frameRefresh;
    bool isSimpleFrameRefresh=false, isVRREnabled=false, isFrontEdgeSyncEnabled=false, stats=false;
    FrameSyncMode frameSyncMode{};
    float minFrameRate=0, frontEdgeSyncFrameRate=0;
    std::optional<float> maxFrameRate;
    CursorRefreshSettings cursorRefresh;
    DuplicateFrameDetectionMode duplicateFrameDetectionMode{};
    void IsStatisticsForDynamicDetectionEnabled(bool value) { stats=value; }
};
struct Profile { FrameRefreshSettings frameRefresh; RtssSettings rtss; int runtimeIdentity=1; };
struct RtssService {
    RtssSettings captured;
    static RtssService& Get() { static RtssService s; return s; }
    unsigned Begin(int, int, RtssSettings settings) { captured=settings; return 1; }
};
Options Start(const Profile& profile) {
    Options options;
    auto& settings = AppSettings::Get();
    const int hWnd=1;
'''
prefix += startup + '\n(void)rtssSession; return options; }\n'
prefix += r'''
struct EffectOption { std::unordered_map<std::string, float> parameters; std::string name; };
'''
prefix += block(read("src/Magpie.Core/include/ScalingOptions.h"), "struct EffectParameterSessionState") + ";\n"
prefix += r'''
struct ScalingWindow {
    ::Options options;
    static ScalingWindow& Get() { static ScalingWindow window; return window; }
    const ::Options& Options() const { return options; }
};
uint32_t Pending(const FrameRefreshSettings& _draftFrameRefresh,
    const EffectParameterSessionState::Snapshot& _parameterSessionSnapshot) {
'''
prefix += block(read("src/Magpie.Core/OverlayDrawer.cpp"), "auto frameSyncChangeCount =") + ';\nreturn frameSyncChangeCount(); }\n'
suffix = r'''
int main() {
    auto& settings=AppSettings::Get();
    assert(settings.IsSimpleFrameRefresh());
    settings.IsSimpleFrameRefresh(false); settings.IsSimpleFrameRefresh(false);
    assert(settings.saves==1 && settings.FrameRefreshModeChanged.count==1);
    for (bool simple : {false,true}) {
        settings.IsSimpleFrameRefresh(simple);
        auto json=Save(settings);
        rapidjson::Document doc; doc.Parse(json.c_str());
        AppSettings loaded; loaded.Load(std::as_const(doc).GetObj());
        assert(loaded.IsSimpleFrameRefresh()==simple);
    }
    for (const char* json : {"{}", "{\"simpleFrameRefresh\":null}", "{\"simpleFrameRefresh\":0}", "{\"simpleFrameRefresh\":\"false\"}"}) {
        rapidjson::Document doc; doc.Parse(json);
        settings.Load(std::as_const(doc).GetObj());
        assert(settings.IsSimpleFrameRefresh());
    }
    Profile profile;
    profile.rtss.scope=RtssScope::Target; profile.rtss.simpleContentSync=true;
    profile.frameRefresh.vrrEnabled=true;
    profile.frameRefresh.pacing=FrameSyncMode::Reflex;
    profile.frameRefresh.contentRate=90;
    profile.frameRefresh.cursorRate=144;
    const auto saved=profile.frameRefresh;
    for (const char* json : {"{}", R"({"duplicateFrameDetectionMode":null})", R"({"duplicateFrameDetectionMode":9})"}) {
        rapidjson::Document d; d.Parse(json); assert(ReadDuplicate(std::as_const(d).GetObj())==DuplicateFrameDetectionMode::Never);
    }
    for (unsigned i=0; i<3; ++i) {
        rapidjson::Document d; d.Parse(("{\"duplicateFrameDetectionMode\":"+std::to_string(i)+"}").c_str());
        assert(ReadDuplicate(std::as_const(d).GetObj())==DuplicateFrameDetectionMode(i));
    }
    const auto savedRtss=profile.rtss;
    for (bool simple : {true,false,true,false}) {
        settings.IsSimpleFrameRefresh(simple);
        const auto options=Start(profile);
        assert(options.isSimpleFrameRefresh==simple);
        assert(options.isVRREnabled && options.stats==!simple);
        assert(options.duplicateFrameDetectionMode==(simple ? DuplicateFrameDetectionMode::Never : DuplicateFrameDetectionMode::Always));
        assert(RtssService::Get().captured.scope==RtssScope::Target);
        assert(options.cursorRefresh.minimumRefreshRate==(simple ? 90 : 144));
        assert(options.frameSyncMode==(simple ? FrameSyncMode::FrontEdge : FrameSyncMode::Reflex));
        settings.IsSimpleFrameRefresh(!simple);
        assert(options.isSimpleFrameRefresh==simple); // Running snapshot remains immutable.
        assert(profile.frameRefresh==saved && profile.rtss==savedRtss);
    }
    // A mode-only change must enable Save and restart without adding a mode
    // selector to the toolbar. Ordinary frame edits must not erase that change.
    settings.IsSimpleFrameRefresh(false);
    ScalingWindow::Get().options=Start(profile);
    EffectParameterSessionState session({}, profile.frameRefresh, false);
    EffectParameterSessionState::Snapshot snapshot;
    assert(session.ReadIfChanged(snapshot) && !snapshot.simpleFrameRefresh);
    assert(Pending(profile.frameRefresh, snapshot)==0);
    session.DesiredFrameRefresh(profile.frameRefresh, true);
    assert(session.ReadIfChanged(snapshot) && snapshot.simpleFrameRefresh);
    assert(Pending(profile.frameRefresh, snapshot)==1);
    auto sharedEdit=profile.frameRefresh; sharedEdit.contentRate=72;
    session.DesiredFrameRefresh(sharedEdit);
    assert(session.ReadIfChanged(snapshot) && snapshot.simpleFrameRefresh && snapshot.frameRefresh.contentRate==72);
    session.DesiredFrameRefresh(profile.frameRefresh, false);
    assert(session.ReadIfChanged(snapshot) && Pending(profile.frameRefresh, snapshot)==0);
    assert(!session.ReadIfChanged(snapshot));
    session.DesiredFrameRefresh(snapshot.frameRefresh, std::nullopt, true);
    assert(session.ReadIfChanged(snapshot) && snapshot.refreshPolicyChanged);
    assert(Pending(snapshot.frameRefresh, snapshot)==1);
    std::cout << "Production global mode persistence, notifications, runtime/RTSS policy and mode-only restart pending passed\n";
}
'''
(output / "frame_refresh_mode.cpp").write_text(prefix + suffix, encoding="utf-8")
