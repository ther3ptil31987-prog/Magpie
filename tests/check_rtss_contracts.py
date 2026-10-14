"""RTSS source/UI/resource contracts. No compilation or external profile writes."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

repo = Path(__file__).resolve().parents[1]
checks = 0


def check(condition, message):
    global checks
    checks += 1
    assert condition, message


def read(path):
    return (repo / path).read_text(encoding="utf-8-sig")


x = "{http://schemas.microsoft.com/winfx/2006/xaml}"
root = ET.fromstring(read("src/Magpie/ProfilePage.xaml"))
nodes = list(root.iter())
uids = {n.get(x + "Uid"): n for n in nodes if n.get(x + "Uid")}
group = next(n for n in nodes if n.get(x+"Name") == "FrameRefreshGroup")
rtss = uids["Rtss_Takeover"]
check([n.get(x+"Uid") for n in uids["Rtss_Mode_Input"]] == ["Rtss_Scope_None", "Rtss_Rate_Sync", "Rtss_Rate_Custom"], "combined management/rate selector")
check([n.get(x+"Uid") for n in uids["Rtss_Scope_Input"]] == ["Rtss_Scope_Global", "Rtss_Scope_Target"], "scope only contains actual scopes")
check([n.get(x+"Uid") for n in uids["Rtss_ExitPolicy_Input"]] == ["Rtss_Exit_Keep", "Rtss_Exit_Restore"], "exit semantics preserved")
rate_grid = next(n for n in nodes if uids["Rtss_CustomRate_Input"] in list(n))
check(rate_grid.get("Visibility") == "{x:Bind ViewModel.ShowRtssCustomRate, Mode=OneWay}", "custom number and unit dependency")
check("FrameRefresh_Advanced" not in uids and "Rtss_Advanced" not in uids, "no nested advanced group")
for uid in ("Rtss_Scope_Input", "Rtss_ExitPolicy_Input"):
    row = next(n for n in nodes if uids[uid] in list(n))
    check(row.tag.endswith("SettingsCard") and row.get("Visibility") == "{x:Bind ViewModel.ShowRtssOptions, Mode=OneWay}", "dependent options use separate shared rows")
    check(row.get("SizeChanged") == "ProfileCard_SizeChanged", "responsive page layout")
    check(uids[uid].get("Style") == "{StaticResource ProfileComboBoxStyle}", "same native option style")
status = next(n for n in nodes if n.get(x+"Name") == "RtssStatusCard")
check("Visibility" not in status.attrib and "IsEnabled" not in status.attrib, "shared status in both modes")
check(uids["Rtss_Retry"].get("Click") == "{x:Bind ViewModel.RetryRtss}", "retry current session")
check(uids["Rtss_Download"].get("NavigateUri").startswith("https://www.guru3d.com/"), "explicit external download link")
manage = next(n for n in status.iter() if n.get(x+"Name") == "RtssManagementToggle")
check(manage.get("Checked") == "RtssManagement_Changed" and manage.get("Unchecked") == "RtssManagement_Changed", "native toggle exposes inline management")
details = [next(n for n in nodes if n.get(x+"Name") == name) for name in ("RtssProgramCard", "RtssSelfCard", "RtssAfterburnerCard", "RtssResultCard")]
items = next(n for n in rtss if n.tag.endswith("SettingsExpander.Items"))
check(status in list(items) and all(n in list(items) for n in details), "status and details share direct native settings rows")
check(all(n.get("Style") == "{StaticResource ProfileItemStyle}" for n in [status]+details), "continuous group surface")
check(sum(n.tag.endswith("Button") for row in details for n in row.iter()) == 4, "recheck/self init/tool entries remain available")
check(rtss.tag.endswith("SettingsExpander") and "Visibility" not in rtss.attrib, "fixed RTSS feature group available in both modes")
check(status.get(x+"Uid") is None, "status does not repeat the fixed feature title")
check(uids["Rtss_TargetLimit"] in list(items) and uids["Rtss_SimpleSync"] in list(items), "simple and advanced target rules belong to RTSS")
presentation = status
texts = {n.get("Text") for n in presentation.iter()}
check("{x:Bind ViewModel.RtssStatusTitle, Mode=OneWay}" in texts and
      status.get("Description") == "{x:Bind ViewModel.RtssStatusDescription, Mode=OneWay}", "independent status title and detail")
check(any(n.get("AutomationProperties.LiveSetting") == "Polite" for n in presentation.iter()), "accessible status updates")

idl = read("src/Magpie/ProfileViewModel.idl")
header = read("src/Magpie/ProfileViewModel.h")
vm = read("src/Magpie/ProfileViewModel.cpp")
sync_getter = vm.split("double ProfileViewModel::RtssSyncedRate() const noexcept {", 1)[1].split("\n}", 1)[0]
check("ContentSyncRate(_data->runtimeIdentity)" in sync_getter, "profile-isolated current session display")
check("quiet_NaN()" in sync_getter and "IsRtssSyncAvailable()" in sync_getter, "unknown or source-follow sync has no number")
check("_SaveRtss" not in sync_getter and "currentRate" not in sync_getter, "display neither saves nor uses readback rate")
manual_setter = vm.split("void ProfileViewModel::RtssCustomRate(double value) {", 1)[1].split("\n}", 1)[0]
check("if (!IsRtssManaged() || !ShowRtssCustomRate()) return;" in manual_setter, "ignore hidden or disabled editor callbacks")
check('Double RtssSyncedRate { get; };' in idl, "sync property is read only")
notify = vm.split("void ProfileViewModel::_NotifyRtssChanged() {", 1)[1].split("\n}", 1)[0]
check('L"RtssSyncedRate"' in notify and 'L"ShowRtssSyncedRate"' in notify, "display changes notify bindings")
check('L"ShowRtssInitialize"' in notify and 'Boolean ShowRtssInitialize { get; };' in idl and 'bool ShowRtssInitialize() const;' in header, "initialization visibility binding is declared and refreshed")
for name in ("RtssStatusSegments", "RtssSyncText", "RtssLaunchText", "AfterburnerLaunchText", "RtssInitializeText"):
    getter = vm.split(f"ProfileViewModel::{name}() const {{", 1)[1].split("\n}", 1)[0]
    check("ResourceLoader::GetForCurrentView(CommonSharedConstants::APP_RESOURCE_MAP_ID)" in getter,
          "native profile resource map: " + name)
    check("ResourceLoader::GetForCurrentView()" not in getter, "no missing default resource map: " + name)
sync_text = vm.split("hstring ProfileViewModel::RtssSyncText() const {", 1)[1].split("\n}", 1)[0]
check("desiredRate" not in sync_text and "FPS" not in sync_text, "sync description does not duplicate rate")
members = set(re.findall(r"ViewModel\.(\w+)", ET.tostring(rtss, encoding="unicode")))
for member in members:
    check(re.search(r"\b" + member + r"\b", idl) is not None, "IDL: " + member)
    check(re.search(r"\b" + member + r"\(", header) is not None, "header: " + member)
    check("ProfileViewModel::" + member + "(" in vm, "implementation: " + member)

resource_maps = []
for path in sorted((repo / "src/Magpie").glob("Resources.language-*.resw")):
    data = ET.parse(path).getroot().findall("data")
    resources = {n.get("name"): n.findtext("value") for n in data}
    check(len(resources) == len(data), "duplicate resources in " + path.name)
    rtss_resources = {key: value for key, value in resources.items() if key.startswith("Rtss_")}
    check(len(rtss_resources) >= 59 and all(rtss_resources.values()), "RTSS resources in " + path.name)
    check(not any(k in resources for k in ("Rtss_Advanced.Header", "Rtss_ExitPolicy.Description", "Rtss_ContentConstrained")),
          "obsolete RTSS resources removed: " + path.name)
    resource_maps.append(rtss_resources)
check(all(set(r) == set(resource_maps[0]) for r in resource_maps), "same resource keys in all languages")
for language, expected in {
    "zh-Hans": {"Rtss_Rate_Sync.Content": "同步内容帧率", "Rtss_Status_Written": "RTSS 已就绪",
                "Rtss_GlobalNotice.Text": "使用全局配置的应用共用此上限。",
                "Rtss_TargetOverridesGlobal": "目标使用独立配置。"},
    "en-US": {"Rtss_Rate_Sync.Content": "Sync content frame rate", "Rtss_Status_Written": "RTSS is ready"}
}.items():
    resources = {n.get("name"): n.findtext("value") for n in
                 ET.parse(repo / f"src/Magpie/Resources.language-{language}.resw").getroot().findall("data")}
    for key, value in expected.items():
        check(resources[key] == value, "concise wording: " + language + " " + key)
for resources in resource_maps:
    check(not any(phrase in value for value in resources.values() for phrase in
                  ("效果未确认", "缩放可以继续", "修改可能保留", "清理已结束", "本次不再调用",
                   "unconfirmed", "Scaling can continue", "changes may remain", "Cleanup has ended", "no longer called")),
          "no defensive RTSS UI wording")
for uid in (n.get(x + "Uid") for n in rtss.iter() if n.get(x + "Uid", "").startswith("Rtss_")):
    check(any(key.startswith(uid + ".") for key in resource_maps[0]), "resource for " + uid)
for key in set(re.findall(r'L"(Rtss_[A-Za-z0-9_]+)"', vm + read("src/Magpie/RtssService.cpp"))):
    check(key in resource_maps[0], "dynamic resource " + key)

settings = read("src/Magpie/AppSettings.cpp")
profile = read("src/Magpie/Profile.h")
service = read("src/Magpie/RtssService.cpp")
access = read("src/Magpie/RtssProfileAccess.cpp")
initialization = access.split("bool RtssProfileAccess::InitializeUnlimited(", 1)[1].split("\n}", 1)[0]
check('_stricmp(profile.c_str(), _selfProfile.c_str())' in initialization, "initialization binds trusted process filename")
check('before.exists && before.rateKnown[0] && before.rateKnown[2]' in initialization and initialization.index('alreadyUnlimited = true') < initialization.index('_CheckWriteAccess'), "already unlimited requires no write permission or setters")
check('_version !=' not in access and '_version ==' not in access, "version metadata never gates operations")
check('if (!_load || !_save || !_delete || !_get || !_set || !_update)' not in access, "no aggregate export gate")
prepare = service.split("bool RtssService::_Prepare(", 1)[1].split("\n}", 1)[0]
check('if (!status.rtss.running)' not in prepare, "configuration access can prepare offline")
check('return s.initializeReady &&' in vm and 's.accessReady' not in vm, "initialization has its own availability")
check('ReplaceFileW(path.c_str(), tempName, backupName, 0' in access and 'identity != before.fileIdentity' in access, "commit compares actual replaced bytes and identity")
check('actual == contents' in access and 'FILE_SHARE_READ,' in access and 'temporary.retainBackup = true' in access, "rollback is guarded and retains unresolved backup")
check('MOVEFILE_REPLACE_EXISTING' not in access and 'sourceLock' in access, "new profile never overwrites and Global source is locked")
worker_initialization = service.split("void RtssService::_InitializeMagpie() {", 1)[1].split("\n}", 1)[0]
check('session->original =' not in worker_initialization and 'session->lastWritten =' not in worker_initialization and 'selfInitialization = true' in worker_initialization and 'GetExePath().filename()' in worker_initialization, "worker initializes actual self name without session mutation")
startup = read("src/Magpie/App.cpp")
check('bool _isAlwaysRunAsAdmin = true' in read('src/Magpie/AppSettings.h'), "fresh settings request admin")
check('ReadAdminStartupSettings(root' in settings and 'adminStartupDefaultsVersion' in settings, "versioned admin defaults migration")
check(startup.index('if (settings.IsAlwaysRunAsAdmin()') < startup.index('settings.UpdateAdminAutoStart()'), "auto-start migration follows elevation")
check('if (_isAlwaysRunAsAdmin && !Win32Helper::IsProcessElevated()) return true;' in settings, "startup persistence waits for elevation")
check('adminAutoStartDefaultsVersion' in settings and 'AutoStartHelper::EnableAutoStart(_isAlwaysRunAsAdmin)' in settings, "existing auto-start reconciled once")
scaling = read("src/Magpie/ScalingService.cpp")
renderer = read("src/Magpie.Core/Renderer.cpp")
check(settings.count("ReadProfileRtss(") == 2, "default and application load")
check("WriteProfileRtss(writer, profile.rtss)" in settings, "save preferences")
check("rtss = other.rtss" in profile, "copy preferences")
check("ContentTargetChanged(rtssSession, source, fixed, baseRate, constrained)" in scaling, "generation-tagged content target")
check("options.contentTargetChanged(ScalingWindow::Get().SrcTracker().Handle(), fixedContent, _baseFrameRateLimit, constrained)" in renderer, "actual resolved base target")
check("rebound->SameProcess(*session->initialTarget)" in service and "std::atomic<std::shared_ptr<TargetIdentity>>" in service, "same-process HWND rebuild preserves the session baseline")
check("RestoreOriginal" in read("src/Magpie/RtssSettings.h"), "restore default")
check("stillValid()" in access and access.index("if (!stillValid())") < access.index("_CallProfile(_save, profile.c_str())"), "cancel check before save")
check("saveAttempted = true" in access and "else if (saveAttempted &&" in service, "failed readback retains cleanup")
check("readbackComplete && RtssProfileAccess::SameRate(before, written)" in service, "verified unchanged failed writes do not fabricate ownership")
check("current.contents != lastWritten.contents" in access, "new profile deletion ownership")
check("disk.contents != current.contents" in access, "restore concurrent edit guard")
check("_SetIfChanged(\"AppDetectionLevel\", 1, error," in access, "new profile Low")
check("before.exists && !NonRateEqual" in access, "existing non-rate preservation check")
check("std::chrono::milliseconds(750)" in service and "_thread.detach()" in service, "bounded final shutdown")
check("Recheck(false)" in service and "Status().state == RtssState::Conflict" in service, "launch readiness does not retry failures")
check("session->cancelled = true" in service and "session->finished = true" in service, "stop cancellation and one cleanup")
sync_rate = service.split("uint32_t RtssService::ContentSyncRate(", 1)[1].split("\n}", 1)[0]
check("_uiSession" in sync_rate and "identity.lock()" in sync_rate and "session->cancelled" in sync_rate,
      "sync display uses live matching session")
check("session->settings.rateMode != RtssRateMode::ContentSync" in sync_rate, "custom session is not a sync target")
check("session->desiredRate.load()" in sync_rate and "_access" not in sync_rate, "display reads resolved rate without RTSS I/O")
end = service.split("void RtssService::End() {", 1)[1].split("\n}", 1)[0]
check("StatusChanged.Invoke()" in end, "stopping refreshes display even without a write")
check("session->desiredRate.exchange(rate)" in service, "resolved target change refreshes display")
check("WritePrivateProfile" not in access, "no silent file write fallback")
check('GetProcAddress(_module, "EnumProfiles")' not in access and "GetFileAttributesW(path.c_str())" in access, "reliable existence check without unsafe enumeration")
check("GuardProfile" in access and "GuardProperty" in access and "GuardReload" in access and "_faulted" in access, "native faults quarantined")
check("catch (...) { error = L\"WriteException\"; }" in service, "write exceptions retain save-attempt cleanup")
check("original.fields.contains(key) != after.fields.contains(key)" in access, "restore does not claim a changed expression is identical")
check("timeout && !selectedRunning" in service and "Rtss_Failure_StartTimeout" in service, "launch timeout is a reported failure")

setter = access.split("bool RtssProfileAccess::_SetIfChanged(", 1)[1].split("\n}", 1)[0]
check("_failure.observed != value" in setter and "_Get(property" in setter and "_Set(property, value)" in setter,
      "shared setter only changes different values and checks readback")
for method in ("Apply", "Restore"):
    body = access.split("bool RtssProfileAccess::" + method + "(", 1)[1].split("\n}", 1)[0]
    check('_SetIfChanged("FrametimeLimit"' in body and '_Set("FrametimeLimit"' not in body, method + " skips unchanged time")
check("RtssFailureStage::Setting" in access and "RtssFailureStage::Verify" in access and "RtssFailureStage::Reload" in access,
      "typed transaction failures")
check("TimeModeUnsupported" in access and "Rtss_Failure_TimeMode" in service, "unsupported time mode remains explicit")
check("LogProfileFailure" in service and "observedKnown={} " in service and "nativeFault={} fileError={}" in service,
      "transaction failure logs preserve values and native/file distinction")
check("GetLastError()" not in setter, "SDK BOOL rejection does not use stale last error")
check('_CheckWriteAccess(profile, before.exists, error)' in access and '_CheckWriteAccess(profile, true, error)' in access,
      "Apply/Restore check the caller's file write rights")
probe = access.split("bool RtssProfileAccess::_CheckWriteAccess(", 1)[1].split("\n}", 1)[0]
check("FILE_ADD_FILE" in probe and "FILE_FLAG_BACKUP_SEMANTICS" in probe and "CREATE_NEW" not in probe,
      "new-profile permission probe opens only the existing directory")
check("failure.fileError == ERROR_ACCESS_DENIED" in service and "Rtss_Failure_Permission" in service,
      "permission errors have a specific UI result")

ns = {"msb": "http://schemas.microsoft.com/developer/msbuild/2003"}
project = ET.fromstring(read("src/Magpie/Magpie.vcxproj"))
filters = ET.fromstring(read("src/Magpie/Magpie.vcxproj.filters"))
for tree in (project, filters):
    registered = [n.get("Include") for n in tree.findall(".//msb:ClCompile", ns) + tree.findall(".//msb:ClInclude", ns)]
    for name in ("RtssSettings.h", "ProfileRtss.h", "RtssProfileAccess.h", "RtssProfileAccess.cpp", "RtssService.h", "RtssService.cpp"):
        check(registered.count(name) == 1, "project registration: " + name)

print(f"RTSS source/UI/resource contracts: {checks} checks passed across {len(resource_maps)} languages.")
print("This checks structure only; C++ compilation, API writes, native UI and target FPS remain separate acceptance.")
