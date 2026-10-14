"""Source/resource integration checks only; does not build or run Magpie.

Behavioral state/classification tests live in DlssNrStatusTests.cpp and the
existing Multi Pass fixtures. Run those in the separate build-validation session.
"""
from pathlib import Path
import re
import subprocess
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
def read(path):
    return (ROOT / path).read_text(encoding="utf-8-sig")

class DlssNrGuidanceIntegration(unittest.TestCase):
    def test_codes_are_append_only(self):
        path = "src/Magpie.Core/include/ScalingOptions.h"
        old = subprocess.check_output(["git", "show", "cbe2c2cb:" + path], cwd=ROOT).decode("utf-8-sig")
        def codes(source):
            body = source.split("enum class ScalingError {")[1].split("};")[0]
            return re.findall(r"^\s*(\w+)\s*,?\s*$", re.sub(r"//[^\n]*", "", body), re.M)
        before, after = codes(old), codes(read(path))
        self.assertEqual(before, after[:len(before)])
        self.assertIn("WindowIntegrityQueryFailed", after[len(before):])
        self.assertIn("DlssNrStopped", after[len(before):])

    def test_localized_contracts(self):
        header = read("src/Magpie.Core/include/DlssNrStatus.h")
        required = set(re.findall(r'L"(DlssNr_[\w]+)"', header)) | {
            "Message_DlssNrStopped", "Message_WindowIntegrityQueryFailed",
            "Home_DlssNrStatus.Title", "DlssNr_Parameter_HdrRestricted",
            "Overlay_EffectParameters_RestrictedBadge", "ErrorDetails_EffectsChanged",
            "ErrorDetails_EffectsLocated", "DlssNr_OpenGuide", "DlssNr_OpenDirectory",
            "DlssNr_OpenHelp", "DlssNr_OpenFailed", "ErrorDetails_OpenEffectsButton.Content"}
        resources = {}
        for path in (ROOT / "src/Magpie").glob("Resources.language-*.resw"):
            entries = ET.parse(path).getroot().findall("data")
            names = [item.attrib["name"] for item in entries]
            self.assertEqual(len(names), len(set(names)), path.name)
            values = {item.attrib["name"]: item.findtext("value") for item in entries}
            for key in required:
                self.assertTrue(values.get(key), (path.name, key))
            resources[path.stem.removeprefix("Resources.language-")] = values
        for key in required:
            expected = re.findall(r"\{[^}]*\}", resources["en-US"][key])
            for lang, values in resources.items():
                self.assertEqual(expected, re.findall(r"\{[^}]*\}", values[key]), (lang, key))
        self.assertIn("未确认目标程序权限更高", resources["zh-Hans"]["Message_WindowIntegrityQueryFailed"])

    def test_restriction_is_not_backend_failure(self):
        nr = read("src/Magpie.Core/DLSSNRFilter.cpp")
        mode = nr.split("EffectParameterApplyMode DLSSNRFilter::GetParameterApplyMode(", 1)[1].split("EffectParameterRestartReason", 1)[0]
        self.assertIn("_impl->disabled", mode)
        self.assertIn("EffectParameterApplyMode::Restricted", mode)
        overlay = read("src/Magpie.Core/OverlayDrawer.cpp")
        self.assertIn("DlssNr_Parameter_HdrRestricted", overlay)
        self.assertIn("!backendUnavailable && !restricted", overlay)
        self.assertIn("nrStatus->Failed()", overlay)
        for file in ("NvidiaVfxImageBackend.cpp", "RTXVideoHdr.h", "RTXVideoDenoiser.cpp"):
            self.assertIn("EffectParameterApplyMode::Restricted", read("src/Magpie.Core/" + file))

    def test_session_and_history_are_separate(self):
        service = read("src/Magpie/ErrorService.cpp")
        persist = service.split("void ErrorService::_Persist()", 1)[1].split("void ErrorService::ConfigurationSaved", 1)[0]
        self.assertNotIn("_dlssNrSession", persist)
        self.assertIn("context.dlssNrSession != _dlssNrSession", service)
        self.assertIn("current.runId != context.runId", service)
        self.assertIn("issueContext.runId != snapshot.runId", service)
        renderer = read("src/Magpie.Core/Renderer.cpp")
        self.assertIn("_dlssNrSession->End(_dlssNrRunId)", renderer)
        self.assertIn("nativeDrawSucceeded && _nativeEffectBackends[i]->IsDlssNrProcessing()", renderer)
        self.assertIn("bool valid = !nrFailed", renderer)
        self.assertIn("options.reportErrorDetails && !nrFailed", renderer)
        state = read("src/Magpie.Core/include/DlssNrStatus.h")
        self.assertIn("_ended || runId != _snapshot.runId", state)

    def test_failure_boundaries_keep_typed_causes(self):
        nr = read("src/Magpie.Core/DLSSNRFilter.cpp")
        self.assertLess(nr.index("GetFileAttributesW(dllPath.c_str())"), nr.index("LoadLibraryExW(\n\t\tdllPath.c_str()"))
        load = nr.split("if (!impl.snippetModule)", 1)[1].split("impl.snippetInitExt", 1)[0]
        self.assertIn("LibraryLoadFailed", load)
        self.assertNotIn("LibraryMissing", load)
        self.assertIn("ngxCore.LastFailure()", nr)
        self.assertIn("impl.failure = session->failure", nr)
        self.assertIn("if (_failure.operation.empty())", nr)
        tracker = read("src/Magpie.Core/SrcTracker.cpp")
        query = tracker.split("if (!Win32Helper::GetWindowIntegrityLevel", 1)[1].split("if (windowIL", 1)[0]
        self.assertIn("WindowIntegrityQueryFailed", query)
        self.assertNotIn("LowIntegrityLevel", query)

    def test_ui_bindings_and_target_fallback(self):
        for file in ("HomePage.xaml", "ScalingModesPage.xaml"):
            ET.fromstring(read("src/Magpie/" + file))
        for name in ("ShowDlssNrStatus", "DlssNrStatusSummary", "OpenDlssNrEffects"):
            for file in ("HomeViewModel.idl", "HomeViewModel.h", "HomeViewModel.cpp", "HomePage.xaml"):
                self.assertIn(name, read("src/Magpie/" + file))
        navigation = read("src/Magpie/ScalingModesPage.cpp")
        self.assertIn("ambiguous", navigation)
        self.assertIn("modes[target].effects[i].name == _issueEffectNames[i]", navigation)
        self.assertIn("ErrorDetails_EffectsChanged", navigation)
        self.assertIn("control.StartBringIntoView()", navigation)
        self.assertIn("get_weak()", navigation)
        self.assertIn("_issueTarget.scalingModeIdentity.lock()", navigation)
        self.assertIn("_issueTarget.effectIdentities[i].lock()", navigation)

    def test_recovery_document_is_packaged(self):
        self.assertTrue((ROOT / "docs/DLSSNR-TROUBLESHOOTING.md").is_file())
        script = read("scripts/Build-Release.ps1")
        self.assertIn("source = 'docs/DLSSNR-TROUBLESHOOTING.md'; target = 'DLSSNR-TROUBLESHOOTING.md'", script)
        self.assertIn('directory / L"DLSSNR-TROUBLESHOOTING.md"', read("src/Magpie/HomeViewModel.cpp"))
        project = ET.fromstring(read("src/Magpie.Core/Magpie.Core.vcxproj"))
        self.assertTrue(any(x.attrib.get("Include") == "include\\DlssNrStatus.h" for x in project.iter()))

if __name__ == "__main__":
    unittest.main(verbosity=2)
