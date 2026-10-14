#Requires -Version 7.0
param([string]$OutputDirectory = (Join-Path $env:TEMP ('Magpie-VRR-' + [guid]::NewGuid())))
$ErrorActionPreference = 'Stop'
$vrrRepo = Split-Path $PSScriptRoot -Parent
$vrrOutput = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $vrrOutput -Force | Out-Null
$vrrVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vrrVs = & $vrrVswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $vrrVs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vrrVs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX /DNOMINMAX "/I$vrrRepo/src/Magpie.Core" `
    "$PSScriptRoot/VRRPresentationPolicyTests.cpp" "/Fe:$vrrOutput/VRRPresentationPolicyTests.exe" "/Fo:$vrrOutput/VRRPresentationPolicyTests.obj"
if ($LASTEXITCODE) { throw 'VRR policy test compilation failed.' }
& "$vrrOutput/VRRPresentationPolicyTests.exe"
if ($LASTEXITCODE) { throw 'VRR policy tests failed.' }
& python "$PSScriptRoot/prepare_vrr_present_test.py" $vrrOutput
if ($LASTEXITCODE) { throw 'VRR Present helper extraction failed.' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX /DNOMINMAX "/I$vrrRepo/src/Magpie.Core" `
    "$vrrOutput/VRRPresentTests.cpp" "/Fe:$vrrOutput/VRRPresentTests.exe" "/Fo:$vrrOutput/VRRPresentTests.obj"
if ($LASTEXITCODE) { throw 'VRR Present test compilation failed.' }
& "$vrrOutput/VRRPresentTests.exe"
if ($LASTEXITCODE) { throw 'VRR Present tests failed.' }
& python "$PSScriptRoot/check_vrr_bindings.py"
if ($LASTEXITCODE) { throw 'VRR source contracts failed.' }
