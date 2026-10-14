#Requires -Version 7.0
param([string]$OutputDirectory = (Join-Path $env:TEMP ('Magpie-FSR3FG-motion-' + [guid]::NewGuid())))
$ErrorActionPreference = 'Stop'
# Compile/run only in the later build session, from an x64 VS developer shell.
# WARP validates the resource contract/remap, not the FSR SDK or real displays.
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /W4 /WX /DNOMINMAX `
    "$PSScriptRoot/FSR3FGMotionResourceTests.cpp" "/Fe:$OutputDirectory/FSR3FGMotionResourceTests.exe" `
    "/Fo:$OutputDirectory/FSR3FGMotionResourceTests.obj" /link d3d11.lib d3dcompiler.lib dxgi.lib WindowsApp.lib
if ($LASTEXITCODE) { throw 'FSR3FG motion resource test compilation failed' }
& "$OutputDirectory/FSR3FGMotionResourceTests.exe"
if ($LASTEXITCODE) { throw 'FSR3FG motion resource tests failed' }
