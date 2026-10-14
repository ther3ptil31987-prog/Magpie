"""Extract production UI/session rate code without RTSS, XAML or disk writes."""
from pathlib import Path
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("output", type=Path)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[1]


def extract(path, signatures):
    source = (repo / path).read_text(encoding="utf-8-sig")
    methods = []
    for signature in signatures:
        start = source.index(signature)
        brace = source.index("{", start)
        # The selected one-line methods have no nested braces.
        if "\n" not in source[brace:source.index("}", brace)]:
            end = source.index("}", brace) + 1
        else:
            end = source.index("\n}", brace) + 2
        methods.append(source[start:end])
    return "\n".join(methods)


vm = extract("src/Magpie/ProfileViewModel.cpp", [
    "double ProfileViewModel::RtssCustomRate() const noexcept",
    "void ProfileViewModel::RtssCustomRate(double value)",
    "double ProfileViewModel::RtssSyncedRate() const noexcept",
    "bool ProfileViewModel::IsRtssManaged() const noexcept",
    "bool ProfileViewModel::ShowRtssOptions() const noexcept",
    "bool ProfileViewModel::ShowRtssCustomRate() const noexcept",
    "bool ProfileViewModel::ShowRtssSyncedRate() const noexcept",
    "bool ProfileViewModel::IsRtssSyncAvailable() const noexcept",
])
service = extract("src/Magpie/RtssService.cpp", [
    "uint64_t RtssService::Begin(",
    "void RtssService::ContentTargetChanged(",
    "void RtssService::End()",
    "uint32_t RtssService::ContentSyncRate(",
])
args.output.mkdir(parents=True, exist_ok=True)
(args.output / "RtssRateCode.g.h").write_text(service + "\n" + vm, encoding="utf-8")
print("Extracted 7 production rate getters/setters and 4 session methods.")
