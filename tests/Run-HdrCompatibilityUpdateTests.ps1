param([string]$OutputDirectory, [string]$RapidJsonIncludeDirectory, [string[]]$TrialConfigurations)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$TrialConfigurations = @($TrialConfigurations | Where-Object { $_ } | ForEach-Object { (Resolve-Path -LiteralPath $_).Path })
if (!$OutputDirectory) { $OutputDirectory = Join-Path $repo 'obj/tests/hdr-compatibility-update' }
if (!(Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
    Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
if (!$RapidJsonIncludeDirectory) {
    $header = & rg --files --no-ignore (Join-Path $env:USERPROFILE '.conan2/p') -g document.h | Where-Object { $_ -match '[\\/]rapidjson[\\/]document.h$' } | Select-Object -First 1
    if (!$header) { throw 'Provide -RapidJsonIncludeDirectory for the existing RapidJSON dependency.' }
    $RapidJsonIncludeDirectory = Split-Path (Split-Path $header -Parent) -Parent
}
& python (Join-Path $PSScriptRoot 'prepare_hdr_manual_migration_tests.py') $OutputDirectory
if ($LASTEXITCODE) { throw 'Manual migration extraction failed' }
& python (Join-Path $PSScriptRoot 'prepare_hdr_manual_editing_tests.py') $OutputDirectory
if ($LASTEXITCODE) { throw 'Manual editing extraction failed' }
& python (Join-Path $PSScriptRoot 'prepare_hdr_cleanup_tests.py') $OutputDirectory
if ($LASTEXITCODE) { throw 'Cleanup extraction failed' }
& python (Join-Path $PSScriptRoot 'prepare_hdr_export_tests.py') $OutputDirectory
if ($LASTEXITCODE) { throw 'Export extraction failed' }
$common = @('/nologo', '/std:c++20', '/EHsc', '/utf-8', '/MT', '/DNOMINMAX', "/I$repo/src/Magpie.Core/include", "/I$repo/src/Magpie.Core", "/I$repo/src/Magpie", "/I$repo/src/Shared", "/I$RapidJsonIncludeDirectory")
foreach ($test in @('HdrCompatibilityUpdateTests', 'HdrComponentsTests', 'HdrManualMigrationTests', 'HdrManualEditingTests', 'HdrCleanupTests')) {
    $source = if ($test -in @('HdrCleanupTests','HdrManualMigrationTests','HdrManualEditingTests')) { Join-Path $OutputDirectory "$test.cpp" } else { Join-Path $PSScriptRoot "$test.cpp" }
    & cl.exe @common $source "/Fe:$OutputDirectory/$test.exe" "/Fo:$OutputDirectory/$test.obj"
    if ($LASTEXITCODE) { throw "$test test compilation failed" }
    $testArguments = @()
    if ($test -eq 'HdrManualMigrationTests') { $testArguments = @($TrialConfigurations) }
    & (Join-Path $OutputDirectory "$test.exe") @testArguments
    if ($LASTEXITCODE) { throw "$test failed" }
}
& cl.exe @common (Join-Path $OutputDirectory 'HdrExportTests.cpp') windowscodecs.lib windowsapp.lib ole32.lib "/Fe:$OutputDirectory/HdrExportTests.exe" "/Fo:$OutputDirectory/HdrExportTests.obj"
if ($LASTEXITCODE) { throw 'HDR PNG export test compilation failed' }
& (Join-Path $OutputDirectory 'HdrExportTests.exe') $OutputDirectory
if ($LASTEXITCODE) { throw 'HDR PNG encode/decode failed' }
