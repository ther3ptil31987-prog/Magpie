"""Check diagnostic/build boundaries. Does not compile or execute C++/GPU code."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

repo = Path(__file__).resolve().parents[1]
def read(path):
    return (repo / path).read_text(encoding="utf-8-sig")

controller = read("src/Magpie.Core/NvidiaForegroundCompatibility.cpp")
state = read("src/Magpie.Core/ForegroundCompatibilityState.h")
performance = read("src/Magpie.Core/VRRPerformance.h")
renderer = read("src/Magpie.Core/Renderer.cpp")
base = read("src/Magpie.Core/PresenterBase.cpp")

# Instrumentation is an explicit session option, also usable with VRR disabled.
assert 'L"MAGPIE_VRR_PERFORMANCE"' in controller and "performance[0] == L'1'" in controller
assert '_wcsicmp(diagnostic, L"profile")' in controller
assert "bool diagnostics = false;" in state
assert "value.diagnostics ? Record(" in controller
assert "value.diagnostics && target" in controller and "value.diagnostics && result != real" in controller
assert "gpuTiming=unmeasured displayFrames=unmeasured" in controller
assert "LogPerformance(now);" in controller and "VRRPerformance::Stop();" in controller

# Diagnostic code must not introduce another pacer, GPU query/wait or foreground
# manipulation. CPU Present wall time is labeled separately from submissions.
assert not re.search(r"\b(?:Sleep|WaitFor\w*|SetForegroundWindow|CreateQuery|Flush|Signal)\s*\(", performance)
assert "PresentCpuNs" in performance and "PresentScope performance(flags != 0)" in base
assert "VRRPerformance::Event::CaptureAccepted" in renderer
assert "VRRPerformance::Event::GeneratedPublication" in renderer
assert "VRRPerformance::Event::GeneratedSubmit" in renderer
assert "VRRPerformance::Event::AuxiliarySubmit" in renderer
assert "VRRPerformance::Event::IndependentOverlaySubmit" in renderer

# Retain immutable swap-chain/pacing contracts; drop only a fully overwritten
# colour clear. Regional copies retain the border clear.
copy = renderer[renderer.index("void Renderer::_CopySceneToTarget("):renderer.index("bool Renderer::_FrontendRender(")]
assert copy.index("context->CopyResource(") < copy.index("} else {") < copy.index("context->ClearRenderTargetView(")
assert "drawOffset.x == 0 && drawOffset.y == 0" in copy
assert "context->CopySubresourceRegion(" in copy
assert "CreateToolhelp32Snapshot" not in controller
assert "GET_MODULE_HANDLE_EX_FLAG_PIN" in controller and "K32GetModuleInformation" in controller
assert "WM_DISPLAYCHANGE" in renderer and "OutputEnvironmentChanged(true)" in renderer
assert "_presenter->OutputEnvironmentChanged();" in renderer
assert "if (value == _lastPublished) return;" in state
assert "std::atomic<ForegroundCompatibilityMode> _mode" in state
assert "ForegroundQuerySnapshot" in state and "ReadQuery(" in state

ns = "{http://schemas.microsoft.com/developer/msbuild/2003}"
for suffix in ("vcxproj", "vcxproj.filters"):
    project = ET.fromstring(read("src/Magpie.Core/Magpie.Core." + suffix))
    for name in ("VRRPerformance.h", "VRROutputProbe.h"):
        assert sum(node.get("Include") == name for node in project.iter(ns + "ClInclude")) == 1
print("VRR performance diagnostic/project/source boundaries passed (no C++ or hardware proof).")
