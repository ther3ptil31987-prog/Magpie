#Requires -Version 7.0
param([string]$OutputDirectory = (Join-Path $env:TEMP ('Magpie-FrameRefresh-' + [guid]::NewGuid())))
$ErrorActionPreference = 'Stop'
$refreshRepo = Split-Path $PSScriptRoot -Parent
$refreshOutput = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $refreshOutput -Force | Out-Null
$refreshVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$refreshVs = & $refreshVswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $refreshVs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $refreshVs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$refreshRapid = Get-ChildItem -LiteralPath (Join-Path $env:USERPROFILE '.conan2/p') -Directory -Filter 'rapid*' |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'p/include/rapidjson/document.h') } | Select-Object -First 1
if (!$refreshRapid) { throw 'Existing rapidjson dependency not found.' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX /DNOMINMAX "/I$refreshRepo/src/Magpie.Core/include" "/I$($refreshRapid.FullName)/p/include" `
    "$PSScriptRoot/FrameRefreshTests.cpp" "/Fe:$refreshOutput/FrameRefreshTests.exe" "/Fo:$refreshOutput/FrameRefreshTests.obj"
if ($LASTEXITCODE) { throw 'Unified refresh test compilation failed.' }
& "$refreshOutput/FrameRefreshTests.exe"
if ($LASTEXITCODE) { throw 'Unified refresh tests failed.' }
& python "$PSScriptRoot/prepare_frame_refresh_vm_test.py" $refreshOutput
if ($LASTEXITCODE) { throw 'Production refresh view-model extraction failed.' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX "/I$refreshRepo/src/Magpie.Core/include" "/I$refreshRepo/src/Magpie" `
    "$refreshOutput/frame_refresh_vm.cpp" "/Fe:$refreshOutput/FrameRefreshViewModelTests.exe" "/Fo:$refreshOutput/FrameRefreshViewModelTests.obj"
if ($LASTEXITCODE) { throw 'Production refresh view-model compilation failed.' }
& "$refreshOutput/FrameRefreshViewModelTests.exe"
if ($LASTEXITCODE) { throw 'Production refresh view-model tests failed.' }
& python "$PSScriptRoot/prepare_frame_refresh_mode_test.py" $refreshOutput
if ($LASTEXITCODE) { throw 'Production global mode extraction failed.' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX /DNOMINMAX "/I$refreshRepo/src/Magpie.Core/include" "/I$($refreshRapid.FullName)/p/include" `
    "$refreshOutput/frame_refresh_mode.cpp" "/Fe:$refreshOutput/FrameRefreshModeTests.exe" "/Fo:$refreshOutput/FrameRefreshModeTests.obj"
if ($LASTEXITCODE) { throw 'Production global mode compilation failed.' }
& "$refreshOutput/FrameRefreshModeTests.exe"
if ($LASTEXITCODE) { throw 'Production global mode tests failed.' }
& python "$PSScriptRoot/check_frame_refresh_bindings.py"
if ($LASTEXITCODE) { throw 'Unified refresh bindings failed.' }
