#Requires -Version 7.0
param([string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
if (!$OutputDirectory) { $OutputDirectory = Join-Path $repo 'obj/tests/hdr-overlay' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (!$vs) { throw 'Visual Studio C++ tools not found.' }
Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$imgui = Get-ChildItem -LiteralPath (Join-Path $env:USERPROFILE '.conan2/p/b') -Directory -Filter 'imgui*' |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'p/lib/imgui.lib') } | Select-Object -First 1
if (!$imgui) { throw 'Existing ImGui dependency not found; no packages are restored by this test.' }
$shaderOutput = Join-Path $OutputDirectory 'shaders'
New-Item -ItemType Directory -Path $shaderOutput -Force | Out-Null
$fxc = Join-Path $env:WindowsSdkDir "bin/$($env:WindowsSDKVersion.TrimEnd('\'))/x64/fxc.exe"
foreach ($shader in @('ImGuiImplVS','ImGuiImplPS')) {
    $profile = if ($shader.EndsWith('VS')) { 'vs_5_0' } else { 'ps_5_0' }
    & $fxc /nologo /WX /T $profile /E main /Vn $shader /Fh "$shaderOutput/$shader.h" "$repo/src/Magpie.Core/shaders/$shader.hlsl"
    if ($LASTEXITCODE) { throw "$shader compilation failed" }
}
& python "$PSScriptRoot/prepare_hdr_overlay_tests.py" $OutputDirectory
if ($LASTEXITCODE) { throw 'Production extraction failed' }
$common = @('/nologo','/std:c++20','/EHsc','/utf-8','/MT','/DNOMINMAX',
    "/I$repo/src/Magpie.Core", "/I$repo/src/Magpie.Core/include", "/I$OutputDirectory", "/I$($imgui.FullName)/p/include")
foreach ($test in @('HdrComponentsTests','HdrOverlaySubmitTests','HdrOverlayWarpTests')) {
    $libs = if ($test -eq 'HdrOverlayWarpTests') { @("$($imgui.FullName)/p/lib/imgui.lib",'d3d11.lib','windowsapp.lib','imm32.lib') } else { @() }
    # Preserve the existing component fixture's warning level (integer map literals).
    [string[]]$warnings = if ($test -eq 'HdrComponentsTests') { @('/W1') } else { @('/W4','/WX') }
    & cl.exe @common @warnings "$PSScriptRoot/$test.cpp" @libs "/Fe:$OutputDirectory/$test.exe" "/Fo:$OutputDirectory/$test.obj"
    if ($LASTEXITCODE) { throw "$test compilation failed" }
    & "$OutputDirectory/$test.exe"
    if ($LASTEXITCODE) { throw "$test failed" }
}

# Check changed production translation units with /Zs only; no product objects,
# libraries, EXE/DLL or release package are built. Reuse installed dependencies.
$commonGit = (& git -C $repo rev-parse --git-common-dir).Trim()
if (![IO.Path]::IsPathRooted($commonGit)) { $commonGit = Join-Path $repo $commonGit }
$packageRoot = Join-Path (Split-Path ([IO.Path]::GetFullPath($commonGit)) -Parent) 'packages'
$wil = Get-ChildItem -LiteralPath $packageRoot -Directory -Filter 'Microsoft.Windows.ImplementationLibrary.*' |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'include/wil/resource.h') } | Select-Object -First 1
if (!$wil) { throw 'Existing WIL headers not found.' }
$includes = @("$repo/src/Magpie.Core", "$repo/src/Magpie.Core/include", "$repo/src/Shared", $OutputDirectory, "$($wil.FullName)/include")
foreach ($cache in @((Join-Path $env:USERPROFILE '.conan2/p'),(Join-Path $env:USERPROFILE '.conan2/p/b'))) {
    foreach ($package in Get-ChildItem -LiteralPath $cache -Directory) {
        $include = Join-Path $package.FullName 'p/include'
        if (Test-Path -LiteralPath $include) { $includes += $include }
    }
}
$flags = @('/nologo','/Zs','/std:c++20','/EHsc','/utf-8','/bigobj','/W4','/WX',
    '/DNOMINMAX','/DNDEBUG','/DMUPARSER_STATIC','/DUNICODE','/D_UNICODE','/DWIN32_LEAN_AND_MEAN',
    '/DWINRT_NO_MODULE_LOCK','/DWIL_USE_STL=1','/DWIL_SUPPRESS_EXCEPTIONS','/DSPDLOG_FMT_EXTERNAL','/DFMT_HEADER_ONLY')
$flags += $includes | Select-Object -Unique | ForEach-Object { "/I$_" }
foreach ($source in @('Renderer.cpp','OverlayDrawer.cpp','ImGuiImpl.cpp','ImGuiBackend.cpp')) {
    & cl.exe @flags "$repo/src/Magpie.Core/$source"
    if ($LASTEXITCODE) { throw "$source syntax validation failed" }
    Write-Output "PASS production /Zs /W4 /WX: $source"
}
Write-Output 'PASS HDR overlay isolated checks; no application build, release or deployment.'
