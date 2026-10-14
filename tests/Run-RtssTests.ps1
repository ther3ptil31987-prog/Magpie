#Requires -Version 7.0
param(
    [switch]$Compile,
    [string]$OutputDirectory = (Join-Path $env:TEMP ('Magpie-RtssTests-' + [guid]::NewGuid()))
)
$ErrorActionPreference = 'Stop'
$rtssRepo = Split-Path $PSScriptRoot -Parent
# Default is source-only. Compilation is an explicit step for the follow-up session.
& python (Join-Path $PSScriptRoot 'check_rtss_contracts.py')
if ($LASTEXITCODE) { throw 'RTSS source contracts failed.' }
& python (Join-Path $PSScriptRoot 'check_frame_refresh_bindings.py')
if ($LASTEXITCODE) { throw 'Existing frame refresh contracts failed.' }
& python (Join-Path $PSScriptRoot 'check_rendering_gpu_selection_bindings.py')
if ($LASTEXITCODE) { throw 'Existing GPU placement contracts failed.' }
if (!$Compile) { return }

$rtssOutput = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $rtssOutput -Force | Out-Null
$rtssVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$rtssVs = & $rtssVsWhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
if (!$rtssVs) { throw 'Visual Studio with C++ tools is required.' }
Import-Module (Join-Path $rtssVs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $rtssVs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
& python (Join-Path $PSScriptRoot 'prepare_rtss_presentation_tests.py') $rtssOutput
if ($LASTEXITCODE) { throw 'RTSS presentation extraction failed.' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX /DNOMINMAX `
    "/I$rtssOutput" "/I$rtssRepo/src/Magpie" (Join-Path $PSScriptRoot 'RtssPresentationTests.cpp') `
    "/Fe:$rtssOutput/RtssPresentationTests.exe" "/Fo:$rtssOutput/RtssPresentationTests.obj"
if ($LASTEXITCODE) { throw 'RTSS presentation test compilation failed.' }
& (Join-Path $rtssOutput 'RtssPresentationTests.exe')
if ($LASTEXITCODE) { throw 'RTSS presentation tests failed.' }
$rtssRapid = Get-ChildItem -LiteralPath (Join-Path $env:USERPROFILE '.conan2/p') -Directory -Filter 'rapid*' |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'p/include/rapidjson/document.h') } | Select-Object -First 1
if (!$rtssRapid) { throw 'Restore rapidjson before compiling the RTSS settings tests.' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /DNOMINMAX "/I$($rtssRapid.FullName)/p/include" `
    (Join-Path $PSScriptRoot 'RtssSettingsTests.cpp') "/Fe:$rtssOutput/RtssSettingsTests.exe" "/Fo:$rtssOutput/RtssSettingsTests.obj"
if ($LASTEXITCODE) { throw 'RTSS settings test compilation failed.' }
& (Join-Path $rtssOutput 'RtssSettingsTests.exe')
if ($LASTEXITCODE) { throw 'RTSS settings tests failed.' }
& python (Join-Path $PSScriptRoot 'prepare_rtss_rate_tests.py') $rtssOutput
if ($LASTEXITCODE) { throw 'Production rate code extraction failed.' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX /DNOMINMAX `
    "/I$rtssOutput" "/I$rtssRepo/src/Magpie" "/I$rtssRepo/src/Magpie.Core/include" `
    (Join-Path $PSScriptRoot 'RtssRateDisplayTests.cpp') "/Fe:$rtssOutput/RtssRateDisplayTests.exe" "/Fo:$rtssOutput/RtssRateDisplayTests.obj"
if ($LASTEXITCODE) { throw 'RTSS rate display test compilation failed.' }
& (Join-Path $rtssOutput 'RtssRateDisplayTests.exe')
if ($LASTEXITCODE) { throw 'RTSS rate display tests failed.' }
& python (Join-Path $PSScriptRoot 'prepare_rtss_profile_tests.py') $rtssOutput
if ($LASTEXITCODE) { throw 'Production profile code extraction failed.' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX /DNOMINMAX `
    "/I$rtssOutput" (Join-Path $PSScriptRoot 'RtssProfileTransactionTests.cpp') `
    (Join-Path $rtssOutput 'RtssProfileAccess.g.cpp') "/Fe:$rtssOutput/RtssProfileTransactionTests.exe" "/Fo:$rtssOutput/"
if ($LASTEXITCODE) { throw 'RTSS transaction test compilation failed.' }
& (Join-Path $rtssOutput 'RtssProfileTransactionTests.exe') $rtssOutput
if ($LASTEXITCODE) { throw 'RTSS transaction tests failed.' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX /DNOMINMAX `
    "/I$rtssOutput" "/I$rtssRepo/src/Magpie" "/I$rtssRepo/src/Magpie.Core/include" `
    (Join-Path $PSScriptRoot 'RtssSessionTransactionTests.cpp') `
    (Join-Path $rtssOutput 'RtssProfileAccess.g.cpp') "/Fe:$rtssOutput/RtssSessionTransactionTests.exe" "/Fo:$rtssOutput/"
if ($LASTEXITCODE) { throw 'RTSS session transaction test compilation failed.' }
& (Join-Path $rtssOutput 'RtssSessionTransactionTests.exe') $rtssOutput
if ($LASTEXITCODE) { throw 'RTSS session transaction tests failed.' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX /DNOMINMAX /DRTSS_TEST_FILE_COMMIT `
    "/I$rtssOutput" "/I$rtssRepo/src/Magpie" "/I$($rtssRapid.FullName)/p/include" `
    (Join-Path $PSScriptRoot 'RtssInitializationTests.cpp') (Join-Path $rtssOutput 'RtssProfileAccess.g.cpp') `
    "/Fe:$rtssOutput/RtssInitializationTests.exe" "/Fo:$rtssOutput/"
if ($LASTEXITCODE) { throw 'RTSS initialization test compilation failed.' }
& (Join-Path $rtssOutput 'RtssInitializationTests.exe') $rtssOutput
if ($LASTEXITCODE) { throw 'RTSS initialization tests failed.' }
& python (Join-Path $PSScriptRoot 'prepare_rtss_layout_startup_tests.py') $rtssOutput
if ($LASTEXITCODE) { throw 'Production layout/startup extraction failed.' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX /DNOMINMAX /DUNICODE /D_UNICODE `
    "/I$rtssOutput" (Join-Path $PSScriptRoot 'RtssLayoutStartupTests.cpp') `
    "/Fe:$rtssOutput/RtssLayoutStartupTests.exe" "/Fo:$rtssOutput/RtssLayoutStartupTests.obj"
if ($LASTEXITCODE) { throw 'RTSS layout/startup test compilation failed.' }
& (Join-Path $rtssOutput 'RtssLayoutStartupTests.exe')
if ($LASTEXITCODE) { throw 'RTSS layout/startup tests failed.' }
