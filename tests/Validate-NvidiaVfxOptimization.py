"""Structural checks only: no C++/HLSL compilation or GPU execution."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser()
parser.add_argument('--sdk', type=Path, required=True)
parser.add_argument('--report', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
checks = []
def check(label, condition):
    if not condition: raise AssertionError(label)
    checks.append(label)
def text(name): return (root/name).read_text(encoding='utf-8-sig')
ns = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
for name in ['src/NvidiaVfxRuntime.props', 'src/BuildOptions.props', 'src/Common.Post.props',
             'src/Magpie.Core/Magpie.Core.vcxproj', 'src/Magpie/Magpie.vcxproj']:
    ET.parse(root/name); check('XML: '+name, True)
manifest = ET.parse(root/'src/NvidiaVfxRuntime.props')
files = manifest.findall('.//m:NvidiaVfxRuntimeFile', ns)
excluded = manifest.find('.//m:NvidiaVfxExcludedCoreName', ns).attrib['Include'].split(';')
check('Ten unique required DLLs', len(files)==10 and len({x.find('m:SdkRelativePath',ns).text for x in files})==10)
check('Seventeen unique excluded Core DLLs', len(excluded)==17 and len(set(excluded))==17)
for item in files:
    relative = item.find('m:SdkRelativePath',ns).text
    expected = item.find('m:SHA256',ns).text
    check('Pinned SDK SHA256: '+relative, hashlib.sha256((args.sdk/relative).read_bytes()).hexdigest().upper()==expected)
for item in manifest.findall('.//m:NvidiaVfxLicenseFile',ns):
    name = item.attrib['Include']; source=args.sdk/('bin' if name=='ThirdPartyLicenses.txt' else 'license')/name
    check('Pinned license SHA256: '+name, hashlib.sha256(source.read_bytes()).hexdigest().upper()==item.find('m:SHA256',ns).text)
options=ET.parse(root/'src/BuildOptions.props')
for name,value in [('EnableNvidiaVfxAsync','false'),('EnableNvidiaVfxCudaHandoff','false'),('NvidiaVfxRuntimeLayout','Minimal')]:
    check('Conservative default: '+name, options.find('.//m:'+name,ns).text==value)
project=text('src/Magpie/Magpie.vcxproj')
check('Canonical runtime manifest imported','<Import Project="..\\NvidiaVfxRuntime.props"' in project)
check('Minimal path copies the allowlist','Include="@(NvidiaVfxRuntimeFile)" Condition="$(EnableNvidiaVfx)"' in project)
check('Whole Core is limited to private Full or legacy builds',
      'Condition="!$(EnableNvidiaVfx) Or \'$(NvidiaVfxRuntimeLayout)\' == \'Full\'"' in project)
check('Before/after copy validation exists','BeforeTargets="CopyRTXVideoDenoiseRuntime"' in project and 'AfterTargets="CopyRTXVideoDenoiseRuntime"' in project)
check('Third-party licenses retained','ThirdPartyLicenses.txt' in project)
check('Empty legacy license directory cannot glob a drive root',"'$(VFXLicenseDir)' != ''" in project)
package=text('scripts/Build-Release.ps1')
check('Packaging validates runtime even when build is skipped',
      package.index("Test-NvidiaVfxPackage.ps1") > package.index('$featureOptions =') and '/p:NvidiaVfxRuntimeLayout=' in package)
fg=text('src/Magpie.Core/NvidiaVfxFrameGenerator.cpp')
shader=re.search(r'constexpr char compare\[\] = R"\((.*?)\)";',fg,re.S).group(1)
check('Two unconditional comparison group barriers',shader.count('GroupMemoryBarrierWithGroupSync();')==2 and 'return;' not in shader)
check('Only the group leader updates the global flag','localIndex == 0 && groupDifferent != 0' in shader and shader.count('difference.InterlockedOr')==1)
check('Exact comparison includes all four channels','any(generated.Load(int3(id.xy,0)) != current.Load(int3(id.xy,0)))' in shader)
check('SRV cache retains its source texture','currentViewTexture.get() != current' in fg and 'currentViewTexture.copy_from(current)' in fg)
check('B-to-history GPU copy removed','VFG retain B' not in fg and 'NvCVImage_Transfer(&s.input, &s.previous' not in fg)
check('Roles advance after real output consumption',fg.index('if (!publishOutput(input, current') < fg.index('_impl->currentUsesInput = !_impl->currentUsesInput'))
check('Owning descriptors never swapped',not re.search(r'std::swap\([^;]*(?:s\.input|s\.previous)',fg))
check('Source encoding matched','NVCV_BGRA : NVCV_RGBA' in fg)
check('A/B rebound to stable role descriptors','NVVFX_INPUT_IMAGE_0, &previous' in fg and 'NVVFX_INPUT_IMAGE_1, &current' in fg)
check('CUDA publication keeps the first comparison export','!publishCuda || (index == 1 && _settings.automaticShotDetection)' in fg)
runtime=text('src/Magpie.Core/NvidiaVfxRuntime.cpp')
check('Opaque timing owner constructor is out of line','NvidiaVfxSession::NvidiaVfxSession() = default;' in runtime)
check('Async mode remains compile-time opt-in','#ifdef MP_ENABLE_NVIDIA_VFX_ASYNC' in runtime and 'VfxAsyncEnabled ? 1 : 0' in runtime)
check('CPU synchronization kept by default','VfxAsyncEnabled || Synchronize()' in runtime)
check('Rebind drains pending transfers','if (retained.get() != texture) {\n\t\tif (!Synchronize())' in runtime)
check('Session teardown drains before effect destruction',runtime.index('if (stream) Synchronize();') < runtime.index('if (effect) { NvVFX_DestroyEffect'))
check('Shared-context stream dependencies checked','_context.Handle() != producer._context.Handle()' in runtime)
check('Ready events record and wait on exact producer/consumer streams','record(producer._readyEvent, producer.stream)' in runtime and 'wait(stream, producer._readyEvent, 0)' in runtime)
check('Timing polling does not block','if (ready == 600) continue;' in runtime and 'cuEventSynchronize' not in runtime)
image=text('src/Magpie.Core/NvidiaVfxImageBackend.cpp')
check('CUDA handoff disabled without explicit build flag','defined(MP_ENABLE_NVIDIA_VFX_CUDA_HANDOFF)' in image)
check('Handoff excludes bounded conversion and encoding changes','_hdrBoundary.plan.requiresBoundedMapping' in image and 'image.pixelFormat != session.input.pixelFormat' in image)
check('Handoff fences both directions and restores owned input',
      'session.WaitFor(*frame.producer)' in image and 'frame.producer->WaitFor(session)' in image and 'Restore owned TrueHDR input' in image)
renderer=text('src/Magpie.Core/Renderer.cpp')
check('Handoff capability checks actual SDR source','SupportsNvidiaVfxCudaInput(_vfxSourceTexture.get())' in renderer)
check('Per-frame HDR completion retained','if (!drawn || !drawer.CompleteHdrOutput())' in renderer)
check('Presentation waits reported separately','ringWaitCpuMs' in renderer and 'mutexWaitCpuMs' in renderer and 'publicationCpuTotalMs' in renderer)
check('Diagnostic present counters reject zero-present results','*_presenter->LastPresentedFrameCount() > 0' in renderer)
timings=text('src/Magpie.Core/NvidiaVfxMeasurements.h')
check('Runtime measurement field name matches declaration',
      'NvidiaVfxStageMeasurements importImage,' in timings and not re.search(r'\.import\b',runtime+renderer))
check('D3D timing queries never flush or block','D3D11_ASYNC_GETDATA_DONOTFLUSH' in timings and 'a == S_FALSE || b == S_FALSE || c == S_FALSE' in timings)
check('Disjoint/invalid timing samples rejected','disjoint.Disjoint || !disjoint.Frequency || end < start' in timings)
check('Timing is opt-in and pools bounded','NativeBackendTiming::Enabled' in timings and 'std::array<Slot, 16>' in timings and 'std::array<Slot, 32>' in runtime)
check('Four-slot publication/reference bound preserved','MAX_SHARED_TEXTURE_SLOTS = PassThroughFrames::MAX_SLOTS' in text('src/Magpie.Core/Renderer.h'))
receipt={'scope':'Structural/source/dependency checks only; no compilation or GPU execution','passed':len(checks),'checks':checks}
if args.report:
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print(f'PASS {len(checks)} VFX optimization and distribution structural checks')
