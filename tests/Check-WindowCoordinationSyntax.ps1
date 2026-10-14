#Requires -Version 7.0
param([string]$PackageRoot, [string]$OutputDirectory = (Join-Path $env:TEMP 'Magpie-Coordination-Syntax'))
$ErrorActionPreference = 'Stop'
$syntaxRepo = Split-Path $PSScriptRoot -Parent
if (!$PackageRoot) {
    $syntaxCommon = (& git -C $syntaxRepo rev-parse --git-common-dir).Trim()
    if (![IO.Path]::IsPathRooted($syntaxCommon)) { $syntaxCommon = Join-Path $syntaxRepo $syntaxCommon }
    $PackageRoot = Join-Path (Split-Path ([IO.Path]::GetFullPath($syntaxCommon)) -Parent) 'packages'
}
$syntaxVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$syntaxVs = & $syntaxVswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (!$syntaxVs) { throw 'Visual Studio C++ tools not found.' }
Import-Module (Join-Path $syntaxVs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $syntaxVs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$syntaxIncludes = @("$syntaxRepo/src/Magpie.Core", "$syntaxRepo/src/Magpie.Core/include", "$syntaxRepo/src/Shared")
$syntaxWil = Get-ChildItem -LiteralPath $PackageRoot -Directory -Filter 'Microsoft.Windows.ImplementationLibrary.*' |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'include/wil/resource.h') } | Select-Object -First 1
if (!$syntaxWil) { throw 'Existing WIL headers not found; this check does not restore packages.' }
$syntaxIncludes += Join-Path $syntaxWil.FullName 'include'
foreach ($syntaxCache in @((Join-Path $env:USERPROFILE '.conan2/p'), (Join-Path $env:USERPROFILE '.conan2/p/b'))) {
    foreach ($syntaxPackage in (Get-ChildItem -LiteralPath $syntaxCache -Directory)) {
        $syntaxInclude = Join-Path $syntaxPackage.FullName 'p/include'
        if (Test-Path -LiteralPath $syntaxInclude) { $syntaxIncludes += $syntaxInclude }
    }
}
$syntaxFlags = @('/nologo', '/Zs', '/std:c++20', '/EHsc', '/utf-8', '/bigobj', '/W4', '/WX',
    '/DNOMINMAX', '/DNDEBUG', '/DMUPARSER_STATIC', '/DUNICODE', '/D_UNICODE', '/DWIN32_LEAN_AND_MEAN', '/DWINRT_NO_MODULE_LOCK',
    '/DWIL_USE_STL=1', '/DWIL_SUPPRESS_EXCEPTIONS', '/DSPDLOG_FMT_EXTERNAL', '/DFMT_HEADER_ONLY')
$syntaxFlags += $syntaxIncludes | Select-Object -Unique | ForEach-Object { "/I$_" }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$syntaxLog = Join-Path $OutputDirectory 'syntax.log'
Set-Content -LiteralPath $syntaxLog -Value 'MSVC /Zs only; no object, library, executable, package or deployment.'
foreach ($syntaxSource in @('Win32Helper.cpp', 'NvidiaForegroundCompatibility.cpp', 'ScalingWindow.cpp', 'ScalingRuntime.cpp', 'Renderer.cpp', 'GraphicsCaptureFrameSource.cpp', 'SrcTracker.cpp', 'ParameterInputHost.cpp', 'CursorManager.cpp')) {
    Write-Host "Checking $syntaxSource"
    $syntaxResult = & cl.exe @syntaxFlags "$syntaxRepo/src/Magpie.Core/$syntaxSource" 2>&1
    $syntaxCode = $LASTEXITCODE
    $syntaxResult | Tee-Object -FilePath $syntaxLog -Append
    if ($syntaxCode) { throw "Production syntax check failed: $syntaxSource" }
}
Write-Host 'All nine production translation units passed /Zs /W4 /WX. No product built.'
