#Requires -Version 7.0
param([string]$OutputDirectory = (Join-Path $env:TEMP ('Magpie-FSR3FG-policy-' + [guid]::NewGuid())))
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
# This runner compiles a standalone CPU test. It is supplied for the later
# build session; the source-development session deliberately does not run it.
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /W4 /WX /DNOMINMAX "/I$root/src/Magpie.Core" "/I$root/src/Magpie.Core/include" `
    "$PSScriptRoot/FSR3FGPolicyTests.cpp" "/Fe:$OutputDirectory/FSR3FGPolicyTests.exe" "/Fo:$OutputDirectory/FSR3FGPolicyTests.obj"
if ($LASTEXITCODE) { throw 'FSR3FG policy test compilation failed' }
& "$OutputDirectory/FSR3FGPolicyTests.exe"
if ($LASTEXITCODE) { throw 'FSR3FG policy tests failed' }
