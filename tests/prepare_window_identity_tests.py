"""Extract real process/admission/popup/RTSS identity paths for isolated tests."""
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)


def read(path):
    return (repo / path).read_text(encoding="utf-8-sig")


def block(text, marker):
    start = text.index(marker)
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


helper = read("src/Magpie.Core/Win32Helper.cpp")
tracker = read("src/Magpie.Core/SrcTracker.cpp")
service = read("src/Magpie/ScalingService.cpp")
rtss = read("src/Magpie/RtssService.cpp")
functions = [block(helper, marker) for marker in (
    "wil::unique_process_handle Win32Helper::GetWindowProcessHandle(",
    "bool Win32Helper::GetProcessIntegrityLevel(",
    "DWORD Win32Helper::GetCurrentProcessIntegrityLevel(",
    "bool Win32Helper::GetWindowIntegrityLevel(",
)]
gate = tracker[tracker.index("// 检查 integrity level"):tracker.index("// 已在 ScalingService")]
functions += ["static ScalingError CheckSourceIntegrity(HWND hWnd) {\n" + gate +
              "return ScalingError::NoError;\n}",
              block(service, "static bool IsPopupWindow("),
              block(rtss, "struct TargetIdentity {") + ";",
              block(rtss, "std::shared_ptr<TargetIdentity> ResolveTarget(")]
(out / "WindowIdentityIntegration.inc").write_text("\n".join(functions), encoding="utf-8")
print("Extracted production process/token/admission, popup and RTSS identity paths.")
