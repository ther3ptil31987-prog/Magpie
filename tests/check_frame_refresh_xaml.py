"""Run only the SDK XAML task with current IDL metadata in isolated output.

No repository build, product linking, PRI generation or deployment is invoked.
First run check_cursor_refresh_native_syntax.py to generate App.winmd.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys
from xml.sax.saxutils import escape

repo = Path(__file__).resolve().parents[1]
build, output = (Path(arg).resolve() for arg in sys.argv[1:3])
obj = build / "obj/Magpie"
if not obj.exists():
    obj = build / "obj/x64/Release/Magpie"
response = (obj / "Magpie.vcxproj.midlrt.rsp").read_text(encoding="utf-8-sig")
refs = re.findall(r'/reference\s+"([^"]+)"', response)
sdk = re.search(r'([A-Za-z]:\\[^"\r\n]+?\\Windows Kits\\10)\\References\\([^\\]+)', response)
sdk_root, sdk_version = sdk.groups()
compiler = Path(sdk_root) / "bin" / sdk_version / "XamlCompiler/Microsoft.Windows.UI.Xaml.Build.Tasks.dll"
metadata = output / "App.winmd"
if not metadata.exists():
    raise RuntimeError("Generate current App.winmd before checking XAML.")
generated = output / "xaml"
generated.mkdir(parents=True, exist_ok=True)
refs_xml = "\n".join(f'<Reference Include="{escape(ref, {chr(34): "&quot;"})}" />' for ref in refs)
paths_xml = "\n".join(f'<ReferenceDirectory Include="{escape(str(parent))}" />'
                      for parent in sorted({Path(ref).parent for ref in refs}))
project = output / "refresh-xaml.proj"
project.write_text(f'''<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <UsingTask TaskName="Microsoft.Windows.UI.Xaml.Build.Tasks.CompileXaml" AssemblyFile="{compiler}" />
  <ItemGroup>
    <Page Include="{repo / 'src/Magpie/HomePage.xaml'}"><Link>HomePage.xaml</Link></Page>
    <Page Include="{repo / 'src/Magpie/ProfilePage.xaml'}"><Link>ProfilePage.xaml</Link></Page>
    <Page Include="{repo / 'src/Magpie/SettingsExpander.Resource.xaml'}"><Link>SettingsExpander.Resource.xaml</Link></Page>
    {refs_xml}
    <ReferenceDirectory Include="{Path(os.environ['WINDIR']) / 'Microsoft.NET/Framework/v4.0.30319'}" />
    {paths_xml}
    <Local Include="{metadata}" />
  </ItemGroup>
  <Target Name="Check">
    <CompileXaml Language="CppWinRT" LanguageSourceExtension=".cpp" RootNamespace="Magpie"
        ProjectName="Magpie" ProjectPath="{repo / 'src/Magpie/Magpie.vcxproj'}" XamlPages="@(Page)"
        OutputPath="{generated}/" OutputType="WinExe" IsPass1="True" CompileMode="RealBuildPass1"
        ReferenceAssemblies="@(Reference)" ReferenceAssemblyPaths="@(ReferenceDirectory)" CppWinRTLocalAssembly="@(Local)"
        WindowsSdkPath="{sdk_root}\\" TargetPlatformMinVersion="10.0.19041.0"
        SavedStateFile="{generated / 'state.xml'}" EnableTypeInfoReflection="False">
      <Output TaskParameter="GeneratedCodeFiles" ItemName="Generated" />
    </CompileXaml>
    <Message Text="Generated XAML binding code: @(Generated)" Importance="high" />
  </Target>
</Project>''', encoding="utf-8")
result = subprocess.run([shutil.which("msbuild.exe"), str(project), "/t:Check", "/nologo", "/v:minimal"],
                        cwd=repo / "src/Magpie", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
log = result.stdout.decode("utf-8", errors="replace")
(output / "xaml-tools.log").write_text(log, encoding="utf-8")
print(log[-6000:])
sys.exit(result.returncode)
