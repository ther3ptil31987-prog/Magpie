"""Extract production status routing, projection and read-only recheck behavior.

The executable replaces dispatch, resource lookup and external I/O. It does not
build the application, open RTSS or claim native visual acceptance.
"""
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[1]
output = Path(sys.argv[1])
output.mkdir(parents=True, exist_ok=True)


def body(source, signature):
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


header = (repo / "src/Magpie/RtssService.h").read_text(encoding="utf-8-sig")
start = header.index("enum class RtssState {")
end = header.index("\n};", header.index("struct RtssStatus {")) + 3
(output / "RtssPresentationTypes.g.h").write_text(header[start:end], encoding="utf-8")

service = (repo / "src/Magpie/RtssService.cpp").read_text(encoding="utf-8-sig")
vm = (repo / "src/Magpie/ProfileViewModel.cpp").read_text(encoding="utf-8-sig")
methods = [body(service, signature) for signature in (
    "RtssStatus RtssService::Status()", "RtssStatus RtssService::InventoryStatus()", "RtssStatus RtssService::SessionStatus()",
    "RtssStatus RtssService::ManagementStatus()", "RtssStatus RtssService::CurrentSessionStatus(",
    "bool RtssService::HasSession(", "bool RtssService::IsCurrentStatus(", "bool RtssService::IsLatestStatus(",
    "void RtssService::_Publish(", "void RtssService::_PublishSession(", "void RtssService::Recheck(",
)]
methods += [body(vm, signature) for signature in (
    "RtssStatus ProfileViewModel::_RtssStatus()", "bool RtssFailed(", "bool RtssBusy(",
    "hstring ProfileViewModel::RtssStatusTitle()", "hstring ProfileViewModel::RtssStatusDescription()",
    "hstring ProfileViewModel::RtssStatusGlyph()", "bool ProfileViewModel::IsRtssBusy()",
    "bool ProfileViewModel::ShowRtssManagement()", "bool ProfileViewModel::CanRecheckRtss()",
    "bool ProfileViewModel::CanRetryRtss()", "bool ProfileViewModel::ShowRtssDownload()",
    "hstring ProfileViewModel::RtssProgramStatus()", "hstring ProfileViewModel::RtssSelfStatus()",
    "hstring ProfileViewModel::RtssManagementResult()", "bool ProfileViewModel::ShowRtssManagementResult()",
    "void ProfileViewModel::RefreshRtss()",
)]
(output / "RtssPresentationCode.g.h").write_text("\n\n".join(methods), encoding="utf-8")
print("Extracted production RTSS status routing, UI projection and read-only recheck.")
