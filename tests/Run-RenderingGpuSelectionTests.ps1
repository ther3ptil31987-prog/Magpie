#Requires -Version 7.0
param([string]$OutputDirectory = (Join-Path $env:TEMP ('Magpie-RenderingGpu-' + [guid]::NewGuid())),
      [switch]$NativeProbe)
$ErrorActionPreference = 'Stop'
$gpuOutput = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $gpuOutput -Force | Out-Null
# The existing suite supplies current configuration extraction and adapter regression.
& "$PSScriptRoot/Run-DisplayOnlyAdapterTests.ps1" -OutputDirectory $gpuOutput -NativeProbe:$NativeProbe
& python "$PSScriptRoot/prepare_rendering_gpu_selection_test.py" $gpuOutput
if ($LASTEXITCODE) { throw 'GPU view-model extraction failed.' }
$gpuCommon = & git -C $PSScriptRoot rev-parse --git-common-dir
if (![IO.Path]::IsPathRooted($gpuCommon)) { $gpuCommon = Join-Path $PSScriptRoot $gpuCommon }
$gpuCachedRepo = Split-Path ([IO.Path]::GetFullPath($gpuCommon)) -Parent
$gpuWil = Join-Path $gpuCachedRepo 'packages/Microsoft.Windows.ImplementationLibrary.1.0.260126.7/include'
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /W4 /WX /DNOMINMAX "/I$gpuOutput" "/I$gpuWil" `
    "$PSScriptRoot/RenderingGpuSelectionTests.cpp" "/Fe:$gpuOutput/RenderingGpuSelectionTests.exe" "/Fo:$gpuOutput/RenderingGpuSelectionTests.obj" /link WindowsApp.lib
if ($LASTEXITCODE) { throw 'GPU selection test compilation failed.' }
& "$gpuOutput/RenderingGpuSelectionTests.exe"
if ($LASTEXITCODE) { throw 'GPU selection tests failed.' }
& python "$PSScriptRoot/check_rendering_gpu_selection_bindings.py"
if ($LASTEXITCODE) { throw 'GPU selection UI/resource contracts failed.' }
