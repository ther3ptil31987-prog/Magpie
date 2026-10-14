"""Cross-file source audit only. Does not compile, execute C++, or test a GPU."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
checks = 0


def read(path):
    return (ROOT / path).read_text(encoding="utf-8-sig")


def check(value, message):
    global checks
    checks += 1
    if not value:
        raise AssertionError(message)


def body(text, signature):
    """Balanced C++ braces; targeted functions contain no brace string literals."""
    begin = text.index("{", text.index(signature))
    depth = 1
    pos = begin + 1
    while depth:
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
        pos += 1
    return text[begin + 1:pos - 1]


ids = ("DLSSFG/DLSS_FrameGeneration", "XeSSFG/XeSS_FrameGeneration",
       "FSR3FG/FSR3_FrameGeneration", "NvidiaVFX/VFG")
for effect in ids:
    marker = read(f"src/Effects/{effect}.hlsl")
    check("duplicateFrameFiltering" not in marker, f"retired UI field: {effect}")
    if effect.startswith("FSR3FG"):
        method = next(p for p in marker.split("//!PARAMETER") if "int opticalFlowMethod;" in p)
        check(re.findall(r"//!OPTION (\d+) (\w+)", method) == [("1", "AMDOF"), ("2", "NVOF")], "stable nonzero FSR choices")
        check("//!DEFAULT 1" in method, "FSR default AMD")
    elif not effect.startswith("NvidiaVFX"):
        check("//!OPTION 0 None" in marker, "other FG None option is unchanged")
for lang in ("en-US", "zh-Hans", "zh-Hant"):
    tree = ET.fromstring(read(f"src/Magpie/Resources.language-{lang}.resw"))
    keys = [n.get("name") for n in tree.findall("data")]
    check(len(keys) == len(set(keys)), f"unique resource keys {lang}")
    for effect in ids:
        check(f"EffectParam_{effect.replace('/', '_')}_duplicateFrameFiltering_Label" not in keys, f"retired label {lang}/{effect}")
    check("EffectParam_FSR3FG_FSR3_FrameGeneration_opticalFlowMethod_Option_0" not in keys, f"retired None label {lang}")
    for key in ("Overlay_FG_Waiting", "Overlay_FG_IdleSubmissions", "Overlay_FG_FrameRateHelp", "Overlay_FG_GeneratedRate"):
        check(key in keys, f"idle explanation {lang}/{key}")

renderer = read("src/Magpie.Core/Renderer.cpp")
capture = read("src/Magpie.Core/FrameSourceBase.cpp") + read("src/Magpie.Core/FrameSourceBase.h")
check("DuplicateFrameDetectionOverride" not in capture + renderer, "no hidden FG capture override")
check("options.duplicateFrameDetectionMode" in capture and "options.Is3DGameMode()" in capture, "common capture policy retained")
for path in ("src/Magpie/ScalingModesService.cpp", "src/Magpie/ScalingMode.cpp", "src/Magpie/ScalingModeItem.cpp",
             "src/Magpie/ScalingService.cpp", "src/Magpie.Core/ScalingWindow.cpp", "src/Magpie.Core/include/ScalingOptions.h"):
    check("NormalizeFrameGenerationParameters" in read(path), f"shared normalization entrance {path}")
service = read("src/Magpie/ScalingModesService.cpp")
check("if (loadingSettings) AppSettings::Get().MarkConfigMigrationNeeded();" in service, "migration participates in original-file backup")
check("NormalizeFrameGenerationParameters" in body(service, "static void WriteScalingMode("), "export drops obsolete keys")
check("Select Optical Flow Method: None" not in renderer and "incompatible->name" in renderer, "named error has no unsupported None bypass")
idle = body(renderer, "Renderer::TimeUntilIdleRedraw()")
for token in ("IsStopping", "HasFatalError", "IsResizingOrMoving", "HasPendingContent", "_backendContentInFlight", "_captureAllowsIdle", "_frontendPresentedBaseValid"):
    check(token in idle, f"idle admission guard {token}")
check("TimeUntilIdleRedraw()" in body(renderer, "bool Renderer::_TryIdleRedraw()"), "render and wake share one admission decision")
draw = body(renderer, "bool Renderer::_FrontendRender(")
guidance = body(draw, "if (!stableBaseOnly && !idleRedraw) {\n\t\t_presenter->SetSourceTiming")
check("SetFrameGuidance" in guidance, "cached output does not submit source timing or guidance")
check("idleRedraw ? _presenter->BeginIdleFrame" in draw, "replay uses presenter contract")
submit = body(renderer, "bool Renderer::_SubmitFrontendFrame()")
check("if (idleRedraw) _presentationRate.RecordIdleSubmission();" in submit, "idle statistics have a separate channel")
check("submitted && contentFrame && _hasFrameGeneration" in submit, "content statistics remain content-only")
check("_stepTimer.Initialize(useFrameGeneration ? 0.0f : minFrameRate" in renderer and "_idleRedrawRate.store(useFrameGeneration" in renderer, "idle rate reaches frontend, not capture forcing")
check("TimeUntilIdleRedraw" in read("src/Magpie.Core/ScalingRuntime.cpp"), "idle deadline wakes outer message pump")
check("(_hasFrameGeneration && !HasPendingContent()) || !_presenter->WaitForFrameCapacity(ms)" in renderer, "signalled capacity cannot spin idle loop")
clock = read("src/Magpie.Core/IdlePresentationClock.h")
check("milliseconds(500)" in clock, "ordinary 24/30 FPS gaps do not disable proxy FG")
for name in ("FSR3FG", "XeSSFG"):
    cpp = read(f"src/Magpie.Core/{name}Presenter.cpp")
    check(cpp.count(f"{name}Presenter::BeginIdleFrame(") == 2, f"SDK and disabled-build replay implementations {name}")
    end = body(cpp, f"bool {name}Presenter::EndFrame(")
    if name == "FSR3FG":
        guarded = body(end, "if (!impl.idlePresentation)")
        check("impl.dispatch(&impl.fgContext, &prepare.header)" in guarded, "idle never dispatches FSR Prepare")
        check("config.frameGenerationEnabled = !impl.idlePresentation" in end, "FSR disables interpolation for cached output")
        replay = body(end, "if (impl.idlePresentation)")
        check("history.ReplaySubmitted()" in replay and "history.Submitted" not in replay and "return true" in replay, "FSR replay advances only SDK ID")
    else:
        guarded = body(end, "if (impl.frameGenerationEnabled && !impl.idlePresentation)")
        check("impl.timing.Submit" in guarded and "xefgSwapChainTagFrameConstants" in guarded, "XeSS replay skips input history/tagging")
        begin = body(cpp, "bool XeSSFGPresenter::_BeginFrame(")
        check(begin.index('impl.Fail("XeLL resume failed")') < begin.index("xefgSwapChainSetEnabled") < begin.index('impl.Fail("XeLL idle configuration failed")'), "XeLL/FG transition ordering")
        check("xellSleep(impl.xell, impl.frameId)" in begin and "if (!idle) xellSleep" not in begin, "XeLL lifecycle survives disabled pacing")
for f in ("src/BuildOptions.props", "src/Magpie.Core/Magpie.Core.vcxproj"):
    ET.fromstring(read(f))
    check("EnableAmdOpticalFlow" in read(f) and "EnableFSR3FrameGeneration" in read(f), f"build dependency {f}")
package = read("scripts/Test-FSR3FGPackage.ps1")
check("AMDOF capability v1: FSR3-FG required input" in package and "AMDOF capability v1: FSR3-FG required input" in read("src/Magpie.Core/AmdOpticalFlowProvider.cpp"), "package validates compiled provider capability")
build = read("scripts/Build-Release.ps1")
check(build.index("FSR3 FG requires EnableAmdOpticalFlow=true") < build.index("function Find-MSBuild"), "invalid feature dependency fails before build/staging")
print(f"FG settings/idle source audit: {checks} checks passed; C++/SDK/runtime acceptance deferred.")
