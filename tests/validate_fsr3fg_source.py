"""Read-only integration/SDK contract audit. Does not invoke a compiler or GPU.

Native policy and full build tests are deliberately separate. This audit checks
cross-file registration, public API vocabulary, packaging and resource metadata;
it is not C++ type checking or a replacement for D3D12/runtime acceptance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--sdk-directory", type=Path)
args = parser.parse_args()
checks = 0


def check(value, message):
    global checks
    checks += 1
    if not value:
        raise AssertionError(message)


def read(name):
    return (ROOT / name).read_text(encoding="utf-8-sig")


ID = r"FSR3FG\FSR3_FrameGeneration"
cpp = read("src/Magpie.Core/FSR3FGPresenter.cpp")
policy = read("src/Magpie.Core/FSR3FGPolicy.h")
marker = read("src/Effects/FSR3FG/FSR3_FrameGeneration.hlsl")
review = json.loads(read("docs/experimental/reviews/20261011-fsr3fg-effect-description-review.json"))["effects"][0]
check(review["id"] == ID, "review identity")
check(hashlib.sha256(marker.encode()).hexdigest() == review["source"]["sha256"], "reviewed marker hash")
parameters = re.findall(r"//!PARAMETER\s+(.*?)(?:int|float) (\w+);", marker, re.S)
check({p[1] for p in parameters} == {p["name"] for p in review["source"]["parameters"]}, "marker/review parameter identity")
for block, name in parameters:
    entry = next(p for p in review["source"]["parameters"] if p["name"] == name)
    check(re.search(r"//!DEFAULT (\S+)", block)[1] == entry["default"], f"default {name}")
check("int multiplier;" not in marker, "fixed 2x has no adjustable multiplier")
check("MP_ENABLE_FSR3_FRAME_GENERATION" in cpp and "#else" in cpp, "no-SDK implementation exists")

ns = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
for project in ("src/BuildOptions.props", "src/Common.Post.props", "src/Magpie.Core/Magpie.Core.vcxproj",
                "src/Magpie/Magpie.vcxproj", "src/Effects/Effects.vcxproj"):
    tree = ET.fromstring(read(project))
    check(tree.tag.endswith("Project"), f"valid XML {project}")
core = ET.fromstring(read("src/Magpie.Core/Magpie.Core.vcxproj"))
check(any(n.get("Include") == "FSR3FGPresenter.cpp" for n in core.findall(".//m:ClCompile", ns)), "presenter compilation unit registered")
effects = ET.fromstring(read("src/Effects/Effects.vcxproj"))
check(any(n.get("Include") == ID + ".hlsl" for n in effects.findall(".//m:CopyFileToFolders", ns)), "marker distribution registered")
app = ET.fromstring(read("src/Magpie/Magpie.vcxproj"))
copy_target = next(n for n in app.findall("m:Target", ns) if n.get("Name") == "CopyFSR3FrameGenerationRuntime")
check("EnableFSR3FrameGeneration" in copy_target.get("Condition") and "EnableFSR3ZeroMV" not in copy_target.get("Condition"), "FG runtime independent of SR flag")
copied = " ".join(n.get("SourceFiles", "") for n in copy_target)
for file in ("amd_fidelityfx_loader_dx12.dll", "amd_fidelityfx_framegeneration_dx12.dll", "license.md", "3rdpartynotice.md", "FSR3FG.md"):
    check(file in copied, f"payload copied: {file}")
check("Test-FSR3FGPackage.ps1" in read("scripts/Build-Release.ps1"), "release payload check registered")
check((ROOT / "docs/experimental/effects/FSR3FG.md").is_file(), "distributed instructions exist")

source_ids = {str(p.relative_to(ROOT / 'src/Effects').with_suffix('')).replace('/', '\\')
              for p in (ROOT / 'src/Effects').rglob('*.hlsl') if '//!MAGPIE EFFECT' in p.read_text(encoding='utf-8-sig')}
for language in ("zh-Hans", "en-US"):
    catalog = json.loads(read(f"src/Magpie/EffectCatalog/{language}.json"))
    entries = [e for e in catalog["effects"] if e["id"] == ID]
    check(len(entries) == 1, f"unique {language} FSR entry")
    check({e['id'] for e in catalog['effects']} == source_ids, f"{language} catalog covers installed effects")
    check(entries[0]["level"] == "advanced" and "SDR" in entries[0]["summary"], f"{language} experimental SDR scope")
    resource = ET.fromstring(read(f"src/Magpie/Resources.language-{language}.resw"))
    keys = [n.get("name") for n in resource.findall("data")]
    check(len(keys) == len(set(keys)), f"{language} resource keys unique")
    for name in ("opticalFlowMethod", "amdOpticalFlowMode", "nvidiaOpticalFlowQuality"):
        check(f"EffectParam_FSR3FG_FSR3_FrameGeneration_{name}_Label" in keys, f"localized {name}")

scaling = read("src/Magpie.Core/include/ScalingOptions.h")
check(scaling.count('FSR3FG\\\\FSR3_FrameGeneration') == 2, "narrow/wide classifiers agree")
renderer = read("src/Magpie.Core/Renderer.cpp")
check("_proxyMotionRequest" in renderer and "_xessMotionRequest" not in renderer, "shared guidance collection/publication routing")
check("_HasSoftwareFrameGenerator() || _HasProxyFrameGenerator()" in renderer, "stale capture guard covers proxy FG")
check("_ReportPresenterFailure();" in renderer and "HasFatalError()" in renderer, "fatal presenter errors stop scaling")
stable = read("src/Magpie.Core/FrontendMotionTexture.h")
# Wiring only; the native resource test exercises these production helpers on
# real D3D resources. Passing this audit is NOT evidence that the SRV runs.
check("EnsureFrontendMotionTexture(_frontendResources.GetD3DDevice(),\n\t\t\t\tmotionSource, _isFSR3FrameGenerationActive" in renderer,
      "FSR renderer requests a shader-readable stable copy")
check("shaderReadable ? D3D11_BIND_SHADER_RESOURCE : 0" in stable and
      "current.BindFlags == required.BindFlags" in stable, "stable texture binding participates in recreation")
check("sourceMotion.Prepare(" in cpp and "RemapFSR3FGMotion(" in cpp and "FSR3FG_MOTION_SHADER" in cpp,
      "presenter uses resource-tested motion helpers")
check("failure.context, failure.code" in renderer and "failure = std::move(failure)" in renderer,
      "runtime report owns a copy of the original error")
check(cpp.count("FSR3FGPresenter::GetFailure()") == 2, "diagnostics exist with and without SDK")
for header in ("FSR3FGMotion.h", "FrontendMotionTexture.h", "PresenterFailure.h"):
    check(any(n.get("Include") == header for n in core.findall(".//m:ClInclude", ns)), f"resource helper registered: {header}")
resource_test = read("tests/FSR3FGMotionResourceTests.cpp")
for helper in ("EnsureFrontendMotionTexture", "FSR3FGMotionSource", "FSR3FG_MOTION_SHADER", "RemapFSR3FGMotion"):
    check(helper in resource_test, f"native resource regression uses production helper: {helper}")
check("FSR 3 FG requires Graphics Capture" not in renderer, "capture selection is not restricted to WGC")
check("SetCaptureAdmissionTime(sourceMetadata.captureAdmissionTime100ns)" in renderer and
      "_lastCapturedFrameTime.time_since_epoch()" in renderer, "capture admission clock crosses the shared frame metadata")
check("CaptureAdmissionEstimate" in policy and "TimingSource" in policy, "timestamp fallback has explicit provenance and transition reset")
check("FFX_FRAMEGENERATION_ENABLE_ASYNC_WORKLOAD_SUPPORT" not in cpp, "async compute not enabled before acceptance")
check("_lastPresentedFrameCount = std::nullopt" in cpp and "_lastSubmittedFrameCount = 1u + *impl.callbackGeneratedCount" in cpp,
      "submission counter is distinct from unknown presented count")
check("LastContentFrameCount()" in renderer and "sample.source" in renderer, "content provenance reaches statistics")
check("xell" not in cpp.lower() and "XeSSFGCompatibility" not in cpp, "AMD runtime independent of Intel patches and XeLL")

if args.sdk_directory:
    sdk = args.sdk_directory / "Kits/FidelityFX"
    headers = "\n".join(p.read_text(encoding="utf-8-sig") for folder in (sdk / 'api/include', sdk / 'framegeneration/include') for p in folder.rglob('*.h'))
    for symbol in sorted(set(re.findall(r"\bFFX_[A-Z0-9_]+\b", cpp))):
        check(re.search(r"\b" + symbol + r"\b", headers), f"public SDK symbol {symbol}")
    # Verify descriptor member assignments against the pinned header declarations.
    for function in ("CreateFGContext", "_Initialize", "EndFrame"):
        start = cpp.index(function + "(")
        text = cpp[start:cpp.find("\n}\n", start)]
        for typ, name in re.findall(r"\b(ffx\w+Desc\w*|ffxOverrideVersion)\s+(\w+)\{\}", text):
            struct = re.search(r"struct\s+" + typ + r"\s*\{(.*?)\};", headers, re.S)
            check(struct is not None, f"descriptor exists: {typ}")
            for member in set(re.findall(r"\b" + name + r"\.(\w+)\s*(?:[=;.]|\[)", text)):
                check(re.search(r"\b" + member + r"\b", struct[1]), f"{typ}.{member} exists")
    package_script = read("scripts/Test-FSR3FGPackage.ps1")
    for name in ("amd_fidelityfx_loader_dx12.dll", "amd_fidelityfx_framegeneration_dx12.dll"):
        digest = hashlib.sha256((sdk / 'signedbin' / name).read_bytes()).hexdigest().upper()
        check(digest in package_script, f"pinned SHA256 matches local SDK {name}")

print(f"FSR3FG source audit: {checks} checks passed; no C++/HLSL compilation or GPU/runtime validation.")
