"""Extract production RTSS text getters for a native PRI regression test."""
from pathlib import Path
import argparse
import re
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser()
parser.add_argument("output", type=Path)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[1]
vm = (repo / "src/Magpie/ProfileViewModel.cpp").read_text(encoding="utf-8-sig")
service = (repo / "src/Magpie/RtssService.h").read_text(encoding="utf-8-sig")
getters = []
start = vm.index("IVector<hstring> ProfileViewModel::RtssStatusSegments() const {")
end = vm.index("\n}", start) + 2
getters.append(vm[start:end])
for name in ("RtssStatusText", "RtssSyncText", "RtssLaunchText", "AfterburnerLaunchText", "RtssInitializeText",
             "RtssStatusTitle", "RtssStatusDescription", "RtssProgramStatus", "RtssSelfStatus", "RtssManagementResult"):
    start = vm.index(f"hstring ProfileViewModel::{name}() const {{")
    end = vm.index("\n}", start) + 2
    getters.append(vm[start:end])
for signature in ("bool RtssFailed(",):
    start = vm.index(signature)
    end = vm.index("\n}", start) + 2
    getters.insert(0, vm[start:end])

enum = re.search(r"enum class RtssState \{.*?\};", service, re.S).group()
status = re.search(r"struct RtssStatus \{.*?\n\};", service, re.S).group()
states = re.findall(r"\b\w+\b", enum.split("{", 1)[1].split("}", 1)[0])
resources = ET.parse(repo / "src/Magpie/Resources.language-en-US.resw").getroot()
keys = [n.get("name") for n in resources.findall("data")
        if n.get("name").startswith("Rtss_") and "." not in n.get("name")]
args.output.mkdir(parents=True, exist_ok=True)
(args.output / "RtssResourceGetters.g.h").write_text("\n".join(getters), encoding="utf-8")
(args.output / "RtssResourceCases.g.h").write_text(
    enum + "\n" + status + "\ninline const RtssState States[] = {" +
    ",".join("RtssState::" + name for name in states) + "};\n" +
    "inline const wchar_t* ResourceKeys[] = {" + ",".join('L"' + key + '"' for key in keys) + "};\n",
    encoding="utf-8")
print(f"Extracted {len(getters)} production getters, {len(states)} states and {len(keys)} dynamic resource keys.")
