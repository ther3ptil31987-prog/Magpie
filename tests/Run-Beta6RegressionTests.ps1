#Requires -Version 7.0
param([string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$beta6Repo = Split-Path $PSScriptRoot -Parent
$beta6Workspace = Split-Path $beta6Repo -Parent
$beta6Output = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $beta6Workspace '.tools/067-beta6/tests' }
New-Item -ItemType Directory -Path $beta6Output -Force | Out-Null
$beta6Vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$beta6Vs = & $beta6Vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $beta6Vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $beta6Vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
& python "$beta6Repo/scripts/tests/test_fullscreen_parameter_transition.py" $beta6Output
if ($LASTEXITCODE) { throw 'Extract source transition paths failed' }
& python "$PSScriptRoot/prepare_beta6_presentation_test.py" $beta6Output
if ($LASTEXITCODE) { throw 'Extract FIFO renderer failed' }
foreach ($beta6Test in @('source_state_paths', 'beta6_presentation')) {
    & cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 "/I$beta6Repo/src/Magpie.Core" `
        "/I$beta6Repo/src/Magpie.Core/include" `
        "$beta6Output/$beta6Test.cpp" "/Fe:$beta6Output/$beta6Test.exe" "/Fo:$beta6Output/$beta6Test.obj" user32.lib
    if ($LASTEXITCODE) { throw "Compile failed: $beta6Test" }
    & "$beta6Output/$beta6Test.exe"
    if ($LASTEXITCODE) { throw "Test failed: $beta6Test" }
}
