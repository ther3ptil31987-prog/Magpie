#Requires -Version 7.0
param([string]$OutputDirectory = (Join-Path $env:TEMP ('Magpie-VRRPerformance-' + [guid]::NewGuid())))
$ErrorActionPreference = 'Stop'
$vrrPerformanceRepo = Split-Path $PSScriptRoot -Parent
$vrrPerformanceOutput = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $vrrPerformanceOutput -Force | Out-Null
$vrrPerformanceVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vrrPerformanceVs = & $vrrPerformanceVswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $vrrPerformanceVs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vrrPerformanceVs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
& python "$PSScriptRoot/prepare_vrr_performance_test.py" $vrrPerformanceOutput
if ($LASTEXITCODE) { throw 'VRR performance fixture generation failed.' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX "/I$vrrPerformanceRepo/src/Magpie.Core" `
    "$vrrPerformanceOutput/VRRPerformanceTests.cpp" "/Fe:$vrrPerformanceOutput/VRRPerformanceTests.exe" "/Fo:$vrrPerformanceOutput/VRRPerformanceTests.obj"
if ($LASTEXITCODE) { throw 'VRR performance fixture compilation failed.' }
& "$vrrPerformanceOutput/VRRPerformanceTests.exe"
if ($LASTEXITCODE) { throw 'VRR performance CPU regression failed.' }
& python "$PSScriptRoot/check_vrr_performance_bindings.py"
if ($LASTEXITCODE) { throw 'VRR performance source contracts failed.' }
