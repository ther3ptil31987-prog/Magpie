#Requires -Version 7.0
param([string]$OutputDirectory = (Join-Path $env:TEMP ('Magpie-WindowOwner-' + [guid]::NewGuid())), [string[]]$TestNames = @())
$ErrorActionPreference = 'Stop'
$TestNames = @($TestNames | ForEach-Object { $_ -split ',' })
$ownerRepo = Split-Path $PSScriptRoot -Parent
$ownerOutput = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $ownerOutput -Force | Out-Null
$ownerVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$ownerVs = & $ownerVswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (!$ownerVs) { throw 'Visual Studio C++ tools not found.' }
Import-Module (Join-Path $ownerVs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $ownerVs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$ownerFmt = $null
foreach ($ownerCache in @((Join-Path $env:USERPROFILE '.conan2/p'), (Join-Path $env:USERPROFILE '.conan2/p/b'))) {
    if (!(Test-Path -LiteralPath $ownerCache)) { continue }
    foreach ($ownerPackage in (Get-ChildItem -LiteralPath $ownerCache -Directory -Filter 'fmt*')) {
        $ownerInclude = Join-Path $ownerPackage.FullName 'p/include'
        if (Test-Path -LiteralPath (Join-Path $ownerInclude 'fmt/format.h')) { $ownerFmt = $ownerInclude; break }
    }
    if ($ownerFmt) { break }
}
if (!$ownerFmt) { throw 'Existing fmt headers not found.' }
$ownerWilFlags = @()
if (!$TestNames.Count -or 'WindowIdentityIntegrationTests' -in $TestNames) {
    $ownerCommon = (& git -C $ownerRepo rev-parse --git-common-dir).Trim()
    if (![IO.Path]::IsPathRooted($ownerCommon)) { $ownerCommon = Join-Path $ownerRepo $ownerCommon }
    $ownerPackages = Join-Path (Split-Path ([IO.Path]::GetFullPath($ownerCommon)) -Parent) 'packages'
    $ownerWil = Get-ChildItem -LiteralPath $ownerPackages -Directory -Filter 'Microsoft.Windows.ImplementationLibrary.*' |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'include/wil/token_helpers.h') } | Select-Object -First 1
    if (!$ownerWil) { throw 'Existing WIL headers not found; this check does not restore packages.' }
    $ownerWilFlags += "/I$($ownerWil.FullName)/include"
}
& python "$ownerRepo/tests/prepare_window_identity_tests.py" $ownerOutput
if ($LASTEXITCODE) { throw 'Window identity integration extraction failed.' }
& python "$ownerRepo/scripts/tests/test_scaling_window_owner.py" $ownerOutput
if ($LASTEXITCODE) { throw 'Production startup extraction failed.' }
& python "$ownerRepo/scripts/tests/test_capture_shutdown.py" $ownerOutput
if ($LASTEXITCODE) { throw 'Capture shutdown extraction failed.' }
& python "$ownerRepo/scripts/tests/test_scaling_prerequisites.py" $ownerOutput
if ($LASTEXITCODE) { throw 'Scaling prerequisites extraction failed.' }
$ownerFlags = @('/nologo', '/std:c++20', '/EHsc', '/utf-8', '/MT', '/W4', '/WX', '/DNOMINMAX', '/DUNICODE', '/D_UNICODE',
    '/DFMT_HEADER_ONLY', "/I$ownerFmt", "/I$ownerOutput", "/I$ownerRepo/src/Magpie.Core/include", "/I$ownerRepo/src/Magpie.Core") + $ownerWilFlags
$ownerSelectedCount = 0
foreach ($ownerSource in @("$ownerRepo/tests/WindowIdentityTests.cpp", "$ownerRepo/tests/WindowIdentityIntegrationTests.cpp", "$ownerRepo/scripts/tests/scaling_window_owner_policy.cpp",
    "$ownerRepo/scripts/tests/scaling_window_owner.cpp", "$ownerRepo/scripts/tests/scaling_window_lifecycle.cpp", "$ownerRepo/scripts/tests/ngx_runtime_guard.cpp",
    "$ownerOutput/capture_shutdown.cpp", "$ownerOutput/scaling_prerequisites.cpp")) {
    $ownerName = [IO.Path]::GetFileNameWithoutExtension($ownerSource)
    if ($TestNames.Count -and $ownerName -notin $TestNames) { continue }
    ++$ownerSelectedCount
    & cl.exe @ownerFlags $ownerSource "/Fe:$ownerOutput/$ownerName.exe" "/Fo:$ownerOutput/$ownerName.obj" /link User32.lib Advapi32.lib Winmm.lib
    if ($LASTEXITCODE) { throw "Test compilation failed: $ownerName" }
    & "$ownerOutput/$ownerName.exe"
    if ($LASTEXITCODE) { throw "Test failed: $ownerName" }
    if ($ownerName -eq 'ngx_runtime_guard') {
        & "$ownerOutput/$ownerName.exe" shutdown-failure
        if ($LASTEXITCODE) { throw 'NGX final shutdown failure test failed.' }
    }
}
if (!$ownerSelectedCount) { throw 'No tests selected; check TestNames.' }
Write-Host "Selected owner/coordination/lifecycle checks passed. Count=$ownerSelectedCount TestNames=$($TestNames -join ',') Output: $ownerOutput"
