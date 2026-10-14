"""Non-compiling integration checks. Does not load a DLL or execute GPU code."""
from pathlib import Path
import argparse, hashlib, json, re, subprocess, xml.etree.ElementTree as ET

parser=argparse.ArgumentParser()
parser.add_argument('--sdk',type=Path,required=True,help='Official core/VideoFX directory')
parser.add_argument('--report',type=Path)
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
sdk=args.sdk
results=[]
def check(name,condition):
    if not condition: raise AssertionError(name)
    results.append(name)

core=root/'src/Magpie.Core'
include=core/'include'
ns={'m':'http://schemas.microsoft.com/developer/msbuild/2003'}
for path in ['src/BuildOptions.props','src/Common.Post.props','src/Magpie.Core/Magpie.Core.vcxproj','src/Magpie/Magpie.vcxproj']:
    ET.parse(root/path)
    check('XML parsed: '+path,True)
project=ET.parse(core/'Magpie.Core.vcxproj')
registered={item.attrib['Include'].replace('\\','/') for tag in ['ClInclude','ClCompile'] for item in project.findall('.//m:'+tag,ns) if 'Include' in item.attrib}
for name in ['NvidiaVfxRuntime.h','NvidiaVfxRuntime.cpp','NvidiaVfxImageBackend.h','NvidiaVfxImageBackend.cpp','NvidiaVfxFrameGenerator.h','NvidiaVfxFrameGenerator.cpp','include/NvidiaVfxParameters.h','include/NvidiaVfxParameterPolicy.h','include/NvidiaVfxExecutionPlan.h']:
    check('Project registration: '+name,name in registered and (core/name).is_file())

version=(sdk/'videofx_version.h').read_text()
check('Official SDK version is 1.3.0.0','NVIDIA_VIDEOEFFECTS_SDK_VERSION_STRING "1.3.0.0"' in version)
headers='\n'.join(p.read_text(encoding='utf-8-sig') for parent in [sdk/'nvvfx/include',sdk/'features'] for p in parent.rglob('*.h'))
source='\n'.join((core/name).read_text(encoding='utf-8-sig') for name in ['NvidiaVfxRuntime.cpp','NvidiaVfxRuntime.h','NvidiaVfxImageBackend.cpp','NvidiaVfxFrameGenerator.cpp'])
symbols=set(re.findall(r'\b(?:NVVFX[A-Z0-9_]*|NVCV_[A-Z0-9_]+)\b',source))
functions=set(re.findall(r'\b(?:NvVFX|NvCVImage|NvCV)_[A-Za-z0-9]+(?=\s*\()',source))
for symbol in sorted(symbols|functions):
    check('Official SDK declaration: '+symbol,bool(re.search(r'\b'+re.escape(symbol)+r'\b',headers)))
for name in ['NvidiaVfxImageBackend.cpp','NvidiaVfxFrameGenerator.cpp']:
    text=(core/name).read_text(encoding='utf-8-sig')
    check('Pin modules before SDK image constructors: '+name,text.index('if (!LoadNvidiaVfxRuntime(')<text.index('std::make_unique<Impl>()'))
renderer=(core/'Renderer.h').read_text(encoding='utf-8-sig')
check('Publication and reference-image rings share capacity','MAX_SHARED_TEXTURE_SLOTS = PassThroughFrames::MAX_SLOTS' in renderer)

manifestPath=sdk.parents[1]/'feature-runtime-manifest.json'
manifest=json.loads(manifestPath.read_text(encoding='utf-8-sig'))
files=manifest if isinstance(manifest,list) else manifest.get('files',[])
check('Six requested feature DLL receipts',len(files)==6)
for item in files:
    # File paths are relative to the official core/VideoFX layout.
    p=sdk/item['path']
    check('Feature DLL hash: '+p.name,p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest().upper()==item['sha256'].upper())

def parameters(path):
    text=path.read_text(encoding='utf-8-sig')
    result={}
    for block in text.split('//!PARAMETER')[1:]:
        m=re.search(r'^(?:float|int) (\w+);',block,re.M)
        if m: result[m[1]]={k.lower():v for k,v in re.findall(r'^//!(MIN|MAX|DEFAULT|STEP) (.+)$',block,re.M)}
    return result
hdr=parameters(root/'src/Effects/RTXVideo/RTXVideo_HDR.hlsl')
for key,expected in {'contrast':('0','200','100'),'saturation':('0','200','100'),'middleGray':('10','100','50'),'peakNits':('400','2000','1000')}.items():
    check('TrueHDR saved range/default: '+key,tuple(hdr[key][x] for x in ['min','max','default'])==expected)
check('TrueHDR debanding default ON',hdr['debanding']['default']=='1')
baseline=subprocess.check_output(['git','show','49de3f25478008c962e24015cf81d7e9772b9a2b:src/Magpie.Core/include/HdrParameterPolicy.h'],cwd=root).decode('utf-8-sig').replace('\r\n','\n')
policy=(include/'HdrParameterPolicy.h').read_text(encoding='utf-8-sig')
check('Beta4 exact-input/ordinary/coarse policy preserved',policy.replace(' || parameter.name == "debanding"','')==baseline)

vfg=(root/'src/Effects/NvidiaVFX/VFG.hlsl').read_text(encoding='utf-8-sig')
check('VFG exposes Performance/Balanced with stable 0/1 ABI',
      re.findall(r'^//!OPTION (\d+) (.+)$',vfg,re.M)==[('0','Performance'),('1','Balanced')])
check('VFG Balanced remains default',parameters(root/'src/Effects/NvidiaVFX/VFG.hlsl')['model']['default']=='1')
for language in ['en-US','zh-Hans','zh-Hant']:
    resources=ET.parse(root/f'src/Magpie/Resources.language-{language}.resw')
    names=[item.attrib['name'] for item in resources.findall('data')]
    check('Unique resource keys: '+language,len(names)==len(set(names)))
    keys=['EffectParam_NvidiaVFX_VFG_model_Option_0','EffectParam_NvidiaVFX_VFG_model_Option_1']
    if language!='zh-Hant':
        keys+=['NvidiaVfx_Upscale_Process','NvidiaVfx_Upscale_Auto','EffectParam_NvidiaVFX_VFG_model_Label','EffectParam_RTXVideo_RTXVideo_VSR_category_Label','EffectParam_RTXVideo_RTXVideo_HDR_debanding_Label']
    for key in keys:
        check('Localized resource: '+language+'/'+key,key in names)

receipt={'scope':'source-only; no compilation, deployment or GPU execution', 'passed':len(results),'checks':results}
if args.report:
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(f'PASS {len(results)} non-compiling SDK/project/resource/compatibility checks')
