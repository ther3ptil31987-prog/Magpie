#Requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$SdkDirectory,
    [string]$OutputDirectory = (Join-Path $env:TEMP ('Magpie-FSR3FG-review-' + [guid]::NewGuid()))
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& python "$PSScriptRoot/prepare_fsr3fg_review_tests.py" $OutputDirectory --sdk-directory $SdkDirectory
if ($LASTEXITCODE) { throw 'FSR3FG review path extraction failed' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /W4 /WX /DNOMINMAX `
    "/I$root/src/Magpie.Core" "/I$root/src/Magpie.Core/include" `
    "/I$SdkDirectory/Kits/FidelityFX/api/include" "/I$SdkDirectory/Kits/FidelityFX/framegeneration/include" `
    "$OutputDirectory/FSR3FGReviewTests.cpp" "/Fe:$OutputDirectory/FSR3FGReviewTests.exe" "/Fo:$OutputDirectory/FSR3FGReviewTests.obj"
if ($LASTEXITCODE) { throw 'FSR3FG review test compilation failed' }
& "$OutputDirectory/FSR3FGReviewTests.exe"
if ($LASTEXITCODE) { throw 'FSR3FG review tests failed' }
