"""Extract production profile/config functions; OS/encoding surfaces are fixtures.

This checks serialization and decisions, not native UI or GPU behavior.
"""
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

repo = Path(__file__).resolve().parents[1]
output = Path(sys.argv[1]).resolve()
output.mkdir(parents=True, exist_ok=True)

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

options = read("src/Magpie.Core/include/ScalingOptions.h")
toolbar = read("src/Magpie.Core/include/ToolbarPlacement.h")
types = [block(options, signature) + ";" for signature in (
    "enum class CaptureMethod", "enum class MultiMonitorUsage", "enum class CursorInterpolationMode",
    "struct Cropping", "struct GraphicsCardId", "enum class DestAlignment", "struct ScalingFlags",
    "enum class DuplicateFrameDetectionMode")]
types += [block(toolbar, signature) + (";" if signature.startswith(("enum", "struct")) else "")
          for signature in ("enum class ToolbarDock", "struct ToolbarDockSettings", "constexpr ToolbarDock SanitizeToolbarDock")]
profile = read("src/Magpie/Profile.h").replace('#include "ScalingOptions.h"', "")
(output / "CursorProfileProduction.inc").write_text(
    "namespace Magpie {\n" + "\n".join(types) + "\n}\n" + profile, encoding="utf-8")
settings = read("src/Magpie/AppSettings.cpp")
helper = read("src/Magpie/JsonHelper.cpp")
parts = [block(helper, "bool JsonHelper::" + name + "(") for name in (
    "ReadBool", "ReadBoolFlag", "ReadUInt", "ReadInt", "ReadFloat", "ReadString")]
parts += [block(settings, signature) for signature in (
    "static void WriteProfile(", "bool AppSettings::_LoadProfile(", "void AppSettings::IsDeveloperMode(")]
(output / "CursorConfigProduction.inc").write_text("\n\n".join(parts), encoding="utf-8")
cursor = read("src/Magpie.Core/CursorDrawer.cpp")
(output / "CursorInputProduction.inc").write_text("\n\n".join(block(cursor, signature) for signature in (
    "CursorVisualState CursorDrawer::_SampleCursorState(", "bool CursorDrawer::NeedRedraw(",
    "bool CursorDrawer::IsMinimumRefreshDue(", "bool CursorDrawer::HasVisibilityTransition(",
    "void CursorDrawer::OnPresent(")), encoding="utf-8")

# Binding/resource contracts complement compiled config/policy tests.
x = "{http://schemas.microsoft.com/winfx/2006/xaml}"
ns = "{using:Magpie}"
home = ET.fromstring(read("src/Magpie/HomePage.xaml"))
dev = next(n for n in home.iter() if n.get(x + "Name") == "DeveloperModeExpander")
assert x + "Load" not in dev.attrib
assert "ViewModel.IsDeveloperMode" in dev.get("IsExpanded")
assert all("ViewModel.IsDeveloperMode" in n.get("IsEnabled", "") for n in dev.iter(ns + "SettingsCard"))
uid = "Home_Advanced_DeveloperOptions_DuplicateFrameDetection"
assert not any(n.get(x + "Uid") == uid for n in home.iter())
profile_ui = ET.fromstring(read("src/Magpie/ProfilePage.xaml"))
advanced = next(n for n in profile_ui.iter() if n.get(x + "Name") == "FrameRefreshGroup")
cards = [n for n in advanced.iter() if n.get(x + "Uid") == uid]
assert len(cards) == 1
assert cards[0].get("Visibility") == "{x:Bind ViewModel.IsAdvancedFrameRefresh, Mode=OneWay}"
new_uids = {n.get(x + "Uid") for n in profile_ui.iter() if n.get(x + "Uid", "").startswith("FrameRefresh") and n.tag in (ns+"SettingsCard", ns+"SettingsExpander", ns+"SettingsGroup")}
rate = next(n for n in profile_ui.iter() if "ViewModel.CursorSupplementRate" in n.get("Value", ""))
assert rate.get("Minimum") == "15" and rate.get("Maximum") == "360"
for lang in ("en-US", "zh-Hans", "zh-Hant"):
    root = ET.fromstring(read(f"src/Magpie/Resources.language-{lang}.resw"))
    names = [n.get("name") for n in root.findall("data")]
    assert len(names) == len(set(names)), f"Duplicate resource in {lang}"
    for prefix in new_uids:
        assert prefix + ".Header" in names, (lang, prefix)
        # The cursor strategy is a label-only child of the described cursor group.
        # Keep description coverage for the existing groups and settings.
        if prefix != "FrameRefresh_CursorStrategy":
            assert prefix + ".Description" in names, (lang, prefix)
    assert uid + ".Description" in names
assert '<DefaultLanguage>en-US</DefaultLanguage>' in read("src/Common.Pre.props")
idl = read("src/Magpie/ProfileViewModel.idl")
header = read("src/Magpie/ProfileViewModel.h")
vm = read("src/Magpie/ProfileViewModel.cpp")
for name in ("CursorRefreshModeIndex", "CursorSupplementModeIndex", "CursorSupplementRate"):
    assert name in idl and name in header and f"ProfileViewModel::{name}(" in vm
    assert f'RaisePropertyChanged(L"{name}")' in vm
print("Production config extracted; XAML bindings, permanent entries and three language resources checked.")
import re
events = block(read("src/Magpie.Core/FrameTrace.h"), "enum class Event")
events = [part.strip() for part in events[events.index("{") + 1:-1].split(",") if part.strip() != "Count"]
trace = read("src/Magpie.Core/FrameTrace.cpp")
names = re.findall(r'"([A-Za-z]+)"', trace[trace.index("NAMES{"):trace.index("};", trace.index("NAMES{"))])
assert events == names, "FrameTrace event/name correspondence broken"
print(f"FrameTrace: {len(events)} event names match enum order, including CursorPublish.")
