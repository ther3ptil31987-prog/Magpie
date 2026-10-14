#Requires -Version 7.0
param([Parameter(Mandatory)][string]$SdkDirectory)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$build = Get-Content -LiteralPath (Join-Path $repoRoot 'scripts/Build-Release.ps1') -Raw
$start = $build.IndexOf('$featureOptions = [ordered]@{}')
$end = $build.IndexOf('function Find-MSBuild', $start)
if ($start -lt 0 -or $end -lt 0) { throw 'Feature preflight boundary not found' }
# Execute only the production feature resolution block; no build/staging entry.
$resolve = [scriptblock]::Create($build.Substring($start, $end - $start) + "`nreturn ,`$featureOptions")
$tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$fixtureRoot = Join-Path $tempBase ('Magpie-FG-preflight-' + [guid]::NewGuid())
$sourceRoot = $fixtureRoot
$Platform = 'x64'
$checks = 0
try {
    New-Item -ItemType Directory -Path (Join-Path $fixtureRoot 'src') -Force | Out-Null
    foreach ($case in @(
        @{ fg='false'; sr='false'; amd=$null; expected='false' },
        @{ fg='false'; sr='true'; amd=$null; expected='true' },
        @{ fg='true'; sr='false'; amd=$null; expected='true' },
        @{ fg='true'; sr='true'; amd=$null; expected='true' },
        @{ fg='true'; sr='false'; amd='true'; expected='true' },
        @{ fg='false'; sr='true'; amd='false'; expected='false' },
        @{ fg='true'; sr='true'; amd='false'; expected='error' }
    )) {
        [xml]$props = Get-Content -LiteralPath (Join-Path $repoRoot 'src/BuildOptions.props') -Raw
        $props.Project.PropertyGroup.ChildNodes | Where-Object Name -eq 'EnableFSR3FrameGeneration' | ForEach-Object { $_.InnerText = $case.fg }
        $props.Project.PropertyGroup.ChildNodes | Where-Object Name -eq 'EnableFSR3ZeroMV' | ForEach-Object { $_.InnerText = $case.sr }
        $props.Save((Join-Path $fixtureRoot 'src/BuildOptions.props'))
        $override = if ($null -eq $case.amd) { '' } else { "<EnableAmdOpticalFlow>$($case.amd)</EnableAmdOpticalFlow>" }
        [IO.File]::WriteAllText((Join-Path $fixtureRoot 'src/BuildOptions.props.user'), "<Project><PropertyGroup>$override</PropertyGroup></Project>")
        $result = $null
        try { $result = & $resolve }
        catch {
            if ($case.expected -ne 'error' -or $_ -notmatch 'FSR3 FG requires EnableAmdOpticalFlow=true') { throw }
        }
        if ($case.expected -eq 'error') {
            if ($null -ne $result) { throw 'Explicit AMDOF disable was accepted for FSR3 FG' }
        } elseif ($result['EnableAmdOpticalFlow'] -ne $case.expected) { throw "Wrong feature resolution: $($case | ConvertTo-Json -Compress)" }
        ++$checks
    }
    $package = Join-Path $fixtureRoot 'package'
    New-Item -ItemType Directory -Path (Join-Path $package 'effects/FSR3FG') -Force | Out-Null
    foreach ($name in @('amd_fidelityfx_loader_dx12.dll', 'amd_fidelityfx_framegeneration_dx12.dll')) {
        Copy-Item -LiteralPath (Join-Path $SdkDirectory "Kits/FidelityFX/signedbin/$name") -Destination (Join-Path $package $name)
    }
    foreach ($name in @('AMD-FSR-SDK-LICENSE.md','AMD-FSR-SDK-THIRD-PARTY.md','FSR3FG-README.md')) {
        [IO.File]::WriteAllText((Join-Path $package $name), 'Synthetic fixture, not a distributable package.')
    }
    $markerPath = Join-Path $package 'effects/FSR3FG/FSR3_FrameGeneration.hlsl'
    Copy-Item -LiteralPath (Join-Path $repoRoot 'src/Effects/FSR3FG/FSR3_FrameGeneration.hlsl') -Destination $markerPath
    $validate = Join-Path $repoRoot 'scripts/Test-FSR3FGPackage.ps1'
    $rejected = $false
    try { & $validate -PackageDirectory $package | Out-Null } catch { $rejected = $_ -match 'no compiled AMDOF capability marker' }
    if (!$rejected) { throw 'Package without provider capability was accepted' }; ++$checks
    # This is data for testing the capability scan; it is never executed.
    [IO.File]::WriteAllText((Join-Path $package 'Magpie.exe'), 'Synthetic AMDOF capability v1: FSR3-FG required input')
    & $validate -PackageDirectory $package | Out-Null
    ++$checks
    [IO.File]::AppendAllText($markerPath, "`n//!OPTION 0 None`n")
    $rejected = $false
    try { & $validate -PackageDirectory $package | Out-Null } catch { $rejected = $_ -match 'marker does not require' }
    if (!$rejected) { throw 'Package with retired None option was accepted' }; ++$checks
    "FG feature/package preflight: $checks fixture checks passed; no compilation, binary execution or deployment."
} finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        $resolved = (Resolve-Path -LiteralPath $fixtureRoot).Path
        if (![IO.Path]::GetFullPath($resolved).StartsWith((Join-Path $tempBase 'Magpie-FG-preflight-'), [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Refusing to remove fixture outside its expected temporary root'
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
