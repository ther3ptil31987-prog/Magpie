#Requires -Version 7.0
param([string]$OutputDirectory = (Join-Path $env:TEMP ('Magpie-Foreground-' + [guid]::NewGuid())))
$ErrorActionPreference = 'Stop'
$foregroundRepo = Split-Path $PSScriptRoot -Parent
$foregroundOutput = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $foregroundOutput -Force | Out-Null
$foregroundVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$foregroundVs = & $foregroundVswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $foregroundVs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $foregroundVs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
& python "$PSScriptRoot/prepare_nvidia_foreground_test.py" $foregroundOutput
if ($LASTEXITCODE) { throw 'Foreground production fixture generation failed.' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX "/I$foregroundRepo/src/Magpie.Core" `
    "$foregroundOutput/NvidiaForegroundDecisionTests.cpp" "/Fe:$foregroundOutput/NvidiaForegroundDecisionTests.exe" "/Fo:$foregroundOutput/NvidiaForegroundDecisionTests.obj"
if ($LASTEXITCODE) { throw 'Foreground fixture compilation failed.' }
& "$foregroundOutput/NvidiaForegroundDecisionTests.exe"
if ($LASTEXITCODE) { throw 'Foreground CPU regression failed.' }
& python "$PSScriptRoot/check_nvidia_foreground_bindings.py"
if ($LASTEXITCODE) { throw 'Foreground source contracts failed.' }
