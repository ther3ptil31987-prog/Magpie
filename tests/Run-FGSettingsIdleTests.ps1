#Requires -Version 7.0
param([string]$OutputDirectory = (Join-Path $env:TEMP ('Magpie-FG-settings-idle-' + [guid]::NewGuid())))
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
# Deferred to the separate build session. Requires an x64 VS developer shell.
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /W4 /WX /DNOMINMAX "/I$root/src/Magpie.Core" "/I$root/src/Magpie.Core/include" `
    "$PSScriptRoot/FGSettingsIdleTests.cpp" "/Fe:$OutputDirectory/FGSettingsIdleTests.exe" "/Fo:$OutputDirectory/FGSettingsIdleTests.obj"
if ($LASTEXITCODE) { throw 'FG settings/idle policy test compilation failed' }
& "$OutputDirectory/FGSettingsIdleTests.exe"
if ($LASTEXITCODE) { throw 'FG settings/idle policy tests failed' }
& python "$root/scripts/tests/test_fg_duplicate_filter.py" $OutputDirectory
if ($LASTEXITCODE) { throw 'Production capture policy extraction failed' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /W4 /WX /DNOMINMAX `
    "$OutputDirectory/fg_duplicate.cpp" "/Fe:$OutputDirectory/fg_duplicate.exe" "/Fo:$OutputDirectory/fg_duplicate.obj"
if ($LASTEXITCODE) { throw 'Common duplicate capture test compilation failed' }
& "$OutputDirectory/fg_duplicate.exe"
if ($LASTEXITCODE) { throw 'Common duplicate capture tests failed' }
