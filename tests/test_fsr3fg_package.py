"""Package validator fixtures only; no DLL is loaded, compiled or deployed."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--sdk-directory', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
sdk = args.sdk_directory / 'Kits/FidelityFX'
with tempfile.TemporaryDirectory(prefix='magpie-fsr3fg-payload-') as temporary:
    output = Path(temporary).resolve()
    # Cleanup is limited to the freshly created fixture directory. These are
    # copies, never hardlinks: corrupting a fixture must not alter the SDK.
    assert output.parent == Path(tempfile.gettempdir()).resolve()
    assert output.name.startswith('magpie-fsr3fg-payload-')
    files = {
        sdk / 'signedbin/amd_fidelityfx_loader_dx12.dll': 'amd_fidelityfx_loader_dx12.dll',
        sdk / 'signedbin/amd_fidelityfx_framegeneration_dx12.dll': 'amd_fidelityfx_framegeneration_dx12.dll',
        sdk / 'docs/license.md': 'AMD-FSR-SDK-LICENSE.md',
        args.sdk_directory / '3rdpartynotice.md': 'AMD-FSR-SDK-THIRD-PARTY.md',
        root / 'src/Effects/FSR3FG/FSR3_FrameGeneration.hlsl': 'effects/FSR3FG/FSR3_FrameGeneration.hlsl',
        root / 'docs/experimental/effects/FSR3FG.md': 'FSR3FG-README.md',
    }
    for source, name in files.items():
        target = output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)

    def run(expected_success, expected_text):
        result = subprocess.run(['pwsh', '-NoProfile', '-File', str(root / 'scripts/Test-FSR3FGPackage.ps1'),
                                 '-PackageDirectory', str(output)], capture_output=True, text=True, encoding='utf-8')
        assert (result.returncode == 0) == expected_success, result.stdout + result.stderr
        assert expected_text in result.stdout + result.stderr, result.stdout + result.stderr

    run(False, 'no compiled AMDOF capability marker')
    # Data-only fixture for the capability scanner; never execute this file.
    (output / 'Magpie.exe').write_bytes(b'Synthetic AMDOF capability v1: FSR3-FG required input')
    run(True, 'FSR3FG payload matches')
    provider = output / 'amd_fidelityfx_framegeneration_dx12.dll'
    provider.unlink()
    run(False, 'package is missing')
    provider.write_bytes(b'wrong-runtime-fixture')
    run(False, 'runtime differs from SDK 2.3.0')
print('FSR3FG package fixtures: complete pair with capability accepted; missing capability, missing and mismatched providers rejected. No runtime execution.')
