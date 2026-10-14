"""Extract production foreground decisions; substitute only native endpoints.

Generates a CPU harness. Does not install a hook or change native foreground.
"""
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
source = (repo / "src/Magpie.Core/NvidiaForegroundCompatibility.cpp").read_text(encoding="utf-8-sig")
markers = ("ForegroundWindowIdentity Identity(", "bool Matches(", "bool MatchesPair(", "bool SourceForeground(", "bool OutputUsable(",
           "const char* ForegroundRole(", "QuerySlot* Record(", "bool RefreshModules(", "void Publish(",
           "HWND WINAPI DetourForegroundWindow(", "bool InstallHook(",
           "HWND NvidiaForegroundCompatibility::RealForegroundWindow(", "void NvidiaForegroundCompatibility::Prepare(",
           "void NvidiaForegroundCompatibility::DeviceInitialized(", "bool NvidiaForegroundCompatibility::BindPresentationWindow(",
           "void NvidiaForegroundCompatibility::PresentationState(", "void NvidiaForegroundCompatibility::Reject(",
           "void NvidiaForegroundCompatibility::MainUIChanged(",
           "void NvidiaForegroundCompatibility::FrontendSuspended(", "void NvidiaForegroundCompatibility::Phase(",
           "void NvidiaForegroundCompatibility::Retire(", "void NvidiaForegroundCompatibility::FinishRetire(")
blocks = []
for marker in markers:
    start = source.index(marker)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    blocks.append(source[start:end].replace("QuerySlot* Record(", "QuerySlot* RecordCandidate("))
template = (repo / "tests/NvidiaForegroundDecisionTests.cpp").read_text(encoding="utf-8-sig")
assert template.count("/* PRODUCTION_FOREGROUND_DECISIONS */") == 1
start = source.index("struct Control {")
end = source.index("} control;", start) + len("} control;")
assert template.count("/* PRODUCTION_FOREGROUND_CONTROL */") == 1
template = template.replace("/* PRODUCTION_FOREGROUND_CONTROL */", source[start:end])
start = source.index("struct UnknownQueryObservation {")
end = source.index("// Frontend-only control.", start)
assert template.count("/* PRODUCTION_QUERY_RECORDS */") == 1
template = template.replace("/* PRODUCTION_QUERY_RECORDS */", source[start:end])
(out / "NvidiaForegroundDecisionTests.cpp").write_text(
    template.replace("/* PRODUCTION_FOREGROUND_DECISIONS */", "\n".join(blocks)), encoding="utf-8")
print("Generated production foreground decision fixture (not compiled or executed).")
