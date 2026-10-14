"""Test production focus-setting migration, profile persistence and runtime selection."""
from pathlib import Path
import re
import sys

root=Path(__file__).resolve().parents[2]
def read(path): return (root/path).read_text(encoding='utf-8-sig')
settings=read('src/Magpie/AppSettings.cpp')
profile=read('src/Magpie/Profile.h')
helper=read('src/Magpie/JsonHelper.cpp')
reader=helper[helper.index('bool JsonHelper::ReadBool('):helper.index('bool JsonHelper::ReadBoolFlag(')]
field=re.search(r'bool isParameterFocusSwitchingEnabled = [^;]+;',profile).group()
copy=profile.split('void Copy(const Profile& other) noexcept {',1)[1].split('scalingMode =',1)[0]
migration=settings[settings.index('\tbool legacyParameterFocusSwitching ='):settings.index('\t_isStopEffectsOnTaskSwitchEnabled = false;')]
profiles_end = settings.index('\tauto overlayNode = root.FindMember("overlay");')
if '\t_screenshotContent =' in settings:
    profiles_end = min(profiles_end, settings.index('\t_screenshotContent ='))
load=settings[settings.index('\tauto scaleProfilesNode = root.FindMember("profiles");'):profiles_end]
# Frame-sync migration is covered by ProfileFrameSyncTests; isolate this focus fixture.
load=re.sub(r'\t\t\tfor \(const auto& item : scaleProfilesArray\) \{.*?\n\t\t\t\}\n', '', load, flags=re.S)
load=load.replace(', legacyFrameSync, legacyIdle, migrateLegacyRefresh', '')
assert 'legacyFrameSync' not in load and 'migrateLegacyRefresh' not in load
per_profile=re.search(r'profile.isParameterFocusSwitchingEnabled = legacyParameterFocusSwitching;\s+JsonHelper::ReadBool\(profileObj, "parameterFocusSwitching", profile.isParameterFocusSwitchingEnabled\);',settings).group()
save=re.search(r'writer.Key\("parameterFocusSwitching"\);\s+writer.Bool\(profile.isParameterFocusSwitchingEnabled\);',settings).group()
select=re.search(r'options.isParameterFocusSwitchingEnabled = profile.isParameterFocusSwitchingEnabled;',read('src/Magpie/ScalingService.cpp')).group()
assert 'writer.Bool(data._isParameterFocusSwitchingEnabled)' not in settings
assert 'Home_ParameterFocusSwitching' not in read('src/Magpie/HomePage.xaml')
assert 'Profile_Advanced_ParameterFocusSwitching' in read('src/Magpie/ProfilePage.xaml')

harness=r'''
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
struct JsonHelper {
    static bool ReadBool(const rapidjson::GenericObject<true, rapidjson::Value>&,
        const char*, bool&, bool required=false) noexcept;
};
READER
struct Profile {
    FIELD
    void Copy(const Profile& other) { COPY }
};
struct Settings {
    Profile _defaultProfile;
    std::vector<Profile> _profiles;
    bool _isConfigMigrationNeeded=false;
    bool _LoadProfile(const rapidjson::GenericObject<true, rapidjson::Value>& profileObj,
        Profile& profile, bool isDefault=false, bool legacyParameterFocusSwitching=false) {
        // Unrelated identity and capture fields are omitted from this focused fixture.
        PROFILE_LOAD
        return true;
    }
    void Load(const char* text) {
        rapidjson::Document parsed; parsed.Parse(text); assert(!parsed.HasParseError());
        const auto& document=parsed; const auto root=document.GetObject();
        _profiles.clear(); _isConfigMigrationNeeded=false;
        MIGRATION
        LOAD
    }
    std::string Save() const {
        rapidjson::StringBuffer buffer; rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        writer.StartObject(); writer.Key("profiles"); writer.StartArray();
        auto write=[&](const Profile& profile) { writer.StartObject(); SAVE writer.EndObject(); };
        write(_defaultProfile); for(const auto& profile:_profiles) write(profile);
        writer.EndArray(); writer.EndObject(); return buffer.GetString();
    }
};
static bool Session(const Profile& profile) {
    struct { bool isParameterFocusSwitchingEnabled=false; } options;
    SELECT
    return options.isParameterFocusSwitchingEnabled;
}
int main() {
    Settings s;
    for(const char* text : {"{}", R"({"profiles":[{},{}]})", R"({"parameterFocusSwitching":false,"profiles":[{},{}]})",
        R"({"parameterFocusSwitching":"true","profiles":[{},{}]})"}) {
        s.Load(text); assert(!Session(s._defaultProfile));
        for(const auto& profile:s._profiles) assert(!Session(profile));
    }
    s.Load(R"({"parameterFocusSwitching":true,"profiles":[{},{},{}]})");
    assert(s._isConfigMigrationNeeded && Session(s._defaultProfile) && s._profiles.size()==2);
    assert(Session(s._profiles[0]) && Session(s._profiles[1]));
    Settings reloaded; reloaded.Load(s.Save().c_str());
    assert(!reloaded._isConfigMigrationNeeded && Session(reloaded._defaultProfile) && Session(reloaded._profiles[1]));
    // Individual choices override the old global value and remain independent across reloads.
    s.Load(R"({"parameterFocusSwitching":true,"profiles":[{"parameterFocusSwitching":false},{"parameterFocusSwitching":false},{}]})");
    assert(!Session(s._defaultProfile) && !Session(s._profiles[0]) && Session(s._profiles[1]));
    s._profiles[0].isParameterFocusSwitchingEnabled=true;
    s._profiles[1].isParameterFocusSwitchingEnabled=false;
    reloaded.Load(s.Save().c_str());
    assert(!Session(reloaded._defaultProfile) && Session(reloaded._profiles[0]) && !Session(reloaded._profiles[1]));
    const std::string saved=s.Save(); rapidjson::Document document;document.Parse(saved.c_str());
    assert(!document.HasMember("parameterFocusSwitching"));
    // A new profile remains off even when copied from an enabled default/application profile.
    Profile fresh; fresh.Copy(reloaded._profiles[0]); assert(!Session(fresh));
    s.Load(R"({"parameterFocusSwitching":true})"); assert(Session(s._defaultProfile));
    s.Load(R"({"parameterFocusSwitching":true,"scalingProfiles":[{},{}]})");
    assert(Session(s._defaultProfile) && Session(s._profiles[0]));
    s.Load(R"({"profiles":[{"parameterFocusSwitching":true},{"parameterFocusSwitching":null}]})");
    assert(Session(s._defaultProfile) && !Session(s._profiles[0]));
    std::cout<<"PASS profile focus setting: legacy migration, profile overrides, independent round-trip, default/application selection, no global serialization, new/copy defaults off, old array key and invalid values\n";
}
'''
for token,value in [('READER',reader),('FIELD',field),('COPY',copy),('PROFILE_LOAD',per_profile),('MIGRATION',migration),('LOAD',load),('SAVE',save),('SELECT',select)]:
    harness=harness.replace(token,value)
output=Path(sys.argv[1])/'parameter_focus_settings.cpp'
output.write_text(harness,encoding='utf-8')
print(output)
