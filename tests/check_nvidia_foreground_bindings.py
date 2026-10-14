"""Source boundary checks; no compilation, API hook, UI or G-SYNC proof."""
from pathlib import Path
import ast
import re
import xml.etree.ElementTree as ET

repo = Path(__file__).resolve().parents[1]
def read(path):
    return (repo / path).read_text(encoding="utf-8-sig")

for folder in ("src/Magpie", "src/Magpie.Core"):
    for path in (repo / folder).rglob("*.*"):
        if path.suffix not in (".cpp", ".h") or path.name == "NvidiaForegroundCompatibility.cpp":
            continue
        assert not re.search(r"\bGetForegroundWindow\s*\(", path.read_text(encoding="utf-8-sig")), path

model = read("src/Magpie.Core/include/FrameRefreshSettings.h")
assert "nvidiaForegroundCompatibilityEnabled" not in model
assert "MERGE_REFRESH(vrrEnabled)" in model
serialization = read("src/Magpie/ProfileFrameRefresh.h")
assert 'boolean("vrrEnabled", result.vrrEnabled)' in serialization
assert 'writer.Key("nvidiaForegroundCompatibilityEnabled")' not in serialization
for path in ("src/Magpie/ProfilePage.xaml", "src/Magpie/ProfileViewModel.h",
             "src/Magpie/ProfileViewModel.idl", "src/Magpie/ProfileViewModel.cpp"):
    assert "IsNvidiaForegroundCompatibilityEnabled" not in read(path), path
x = "{http://schemas.microsoft.com/winfx/2006/xaml}"
page = ET.fromstring(read("src/Magpie/ProfilePage.xaml"))
assert not any(n.get(x + "Uid", "").startswith("FrameRefresh_NvidiaForeground") for n in page.iter())
for language in ("en-US", "zh-Hans", "zh-Hant"):
    entries = ET.fromstring(read(f"src/Magpie/Resources.language-{language}.resw")).findall("data")
    resources = {entry.get("name"): entry.findtext("value") for entry in entries}
    assert len(entries) == len(resources)
    assert not any(key.startswith("FrameRefresh_NvidiaForeground") for key in resources)
    assert resources["FrameRefresh_VRR.Header"].startswith("VRR ")

controller = read("src/Magpie.Core/NvidiaForegroundCompatibility.cpp")
assert 'MH_CreateHookApiEx(L"user32.dll", "GetForegroundWindow"' in controller
assert "MH_EnableHook(hookTarget)" in controller and "MH_DisableHook(hookTarget)" in controller
assert "MH_ALL_HOOKS" not in controller and "MH_RemoveHook(" not in controller
assert "published.Current(version)" in controller and "realQueryDepth" in controller
assert "QueryWindowThreadProcessId" in controller and "GET_MODULE_HANDLE_EX_FLAG_PIN" in controller
assert "active=unknown" in controller
assert "SetCursorPos" not in controller and "AttachThreadInput" not in controller
assert "10000" in controller and "MAGPIE_GSYNC_DIAGNOSTIC" in controller
window = read("src/Magpie.Core/ScalingWindow.cpp")
begin = window[window.index("void ScalingWindow::_BeginDestroy("):window.index("bool ScalingWindow::CompletePendingDestroy(")]
assert begin.index("NvidiaForegroundCompatibility::Retire()") < begin.index("BeginShutdown()")
assert begin.index("TrySetScalingWindowOwner") < begin.index("ReleaseReferenceFocus()")
finish = window[window.index("void ScalingWindow::_FinishDestroy("):window.index("LRESULT ScalingWindow::_RendererWndProc(")]
assert finish.index("_renderer.reset()") < finish.index("FinishRetire()")
assert window.index("NvidiaForegroundCompatibility::Prepare(") < window.index("_renderer->Initialize(")
assert "FrontendSuspended(_renderer &&" in window
assert "ReferenceOwnsForeground()" in window
assert "NvidiaForegroundCompatibility::PresentationState" in read("src/Magpie.Core/PresenterBase.cpp")
renderer = read("src/Magpie.Core/Renderer.cpp")
for path in ("OrdinaryDXGI", "DLSSFrontendDXGI", "XeSSProxy", "Composition"):
    assert "ForegroundPresentationPath::" + path in renderer
assert "adapterDesc.AdapterLuid.LowPart" in renderer
assert "SupportsForegroundQuery(control.path) && control.potential" in controller
assert "frameGeneration" not in controller and "outside-prototype" not in controller
assert '_wcsicmp(diagnostic, L"standard")' in controller
assert "Matches(value.outputRoot)" in controller and "value.outputShown" in controller
assert "FrontendWindows(" in window and "_options.mainUI" in window
assert "RegisterSessionWindow(App::Get().MainWindow().Handle())" in read("src/Magpie/ScalingService.cpp")
main_window = read("src/Magpie/MainWindow.cpp")
assert "MainUIChanged(Handle())" in main_window and "MainUIChanged(nullptr)" in main_window
runtime = read("src/Magpie.Core/ScalingRuntime.cpp")
update = runtime[runtime.index("void ScalingRuntime::UpdateMainUI("):runtime.index("void ScalingRuntime::UpdateFrameRefreshSettings(")]
assert "_commandGeneration.load(std::memory_order_acquire) == generation" in update
assert "_Dispatcher().TryEnqueue" in update and "UpdateMainUI(identity)" in update
assert "UnknownQueryObservation, 16" in controller
assert 'L"d3d12.dll"' not in controller and 'L"D3D12Core.dll"' not in controller
assert 'L"nvspcap64.dll"' not in controller
for role in ("source", "output", "parameter-host", "magpie-main-ui", "source-popup", "external", "none"):
    assert 'return "' + role + '"' in controller
base = read("src/Magpie.Core/PresenterBase.cpp")
assert "swapChain->GetHwnd(&window)" in base and "window == _vrrWindow" in base
assert "_vrr.Created(desc.Flags)" in base
assert "_BindVRRSwapChain(_dxgiSwapChain.get())" in read("src/Magpie.Core/AdaptivePresenter.cpp")
assert "_BindVRRSwapChain(impl->swapChain.get())" in read("src/Magpie.Core/XeSSFGPresenter.cpp")
recipe_source = read("src/Magpie/conanfile.py")
assert "minhook/1.3.4" in recipe_source
assert "minhook/1.3.4#1ae65f6b5793b097a606940bb1774119" in read("conan-locks/Magpie.lock")
assert 'Path.Combine(projectDir, "conanfile.py")' in read("src/_ConanDeps/_ConanDeps.vcxproj")
# Run the actual conditional recipe method with a dependency collector.
recipe_ast = ast.parse(recipe_source)
recipe_class = next(node for node in recipe_ast.body if isinstance(node, ast.ClassDef))
requirements = next(node for node in recipe_class.body if isinstance(node, ast.FunctionDef) and node.name == "requirements")
requirements.decorator_list = []
module = ast.fix_missing_locations(ast.Module(body=[requirements], type_ignores=[]))
namespace = {}
exec(compile(module, "MagpieDependencies.requirements", "exec"), namespace)
for arch in ("x86", "x86_64", "armv8"):
    dependencies = []
    from types import SimpleNamespace
    namespace["requirements"](SimpleNamespace(settings=SimpleNamespace(arch=arch), requires=dependencies.append))
    assert ("minhook/1.3.4" in dependencies) == (arch != "armv8")
    assert len(dependencies) == (10 if arch != "armv8" else 9)
assert "CopyMinHookLicense" in read("src/Magpie/Magpie.vcxproj")

ns = "{http://schemas.microsoft.com/developer/msbuild/2003}"
for suffix in ("vcxproj", "vcxproj.filters"):
    project = ET.fromstring(read("src/Magpie.Core/Magpie.Core." + suffix))
    for tag, name in (("ClInclude", "ForegroundCompatibilityState.h"), ("ClInclude", "NvidiaForegroundCompatibility.h"),
                      ("ClCompile", "NvidiaForegroundCompatibility.cpp")):
        assert sum(node.get("Include") == name for node in project.iter(ns + tag)) == 1, (suffix, name)
print("Foreground compatibility source/model/UI/lifecycle boundaries passed (no compilation or hardware proof).")
