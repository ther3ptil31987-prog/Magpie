#Requires -Version 7.0
param([string]$BaselineRef)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$workspace=Split-Path $repo -Parent
$output=Join-Path $workspace '.tools/067-beta4/tests'
if ($BaselineRef) { $output=Join-Path $output 'baseline' }
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$arguments=@((Join-Path $PSScriptRoot 'prepare_scaling_mode_identity_test.py'),$output)
if ($BaselineRef) { $arguments+=$BaselineRef }
& python @arguments
if ($LASTEXITCODE) { throw 'Group identity fixture extraction failed' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /DNOMINMAX "/I$repo/src/Magpie" "/I$repo/src/Magpie.Core/include" "$output/group_identity.cpp" "/Fe:$output/group_identity.exe" "/Fo:$output/group_identity.obj"
if ($LASTEXITCODE) { throw 'Group identity test compilation failed' }
& "$output/group_identity.exe"
if ($BaselineRef) {
    if ($LASTEXITCODE -eq 0) { throw 'Expected the baseline deletion regression to reproduce' }
    Write-Output 'Baseline regression reproduced.'
} elseif ($LASTEXITCODE) { throw 'Group identity regression tests failed' }
exit 0
