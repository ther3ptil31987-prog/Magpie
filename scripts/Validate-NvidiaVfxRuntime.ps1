param(
    [Parameter(Mandatory = $true)][string]$SdkDirectory,
    [string]$RuntimeDirectory,
    [ValidateSet('Minimal', 'Full')][string]$Layout = 'Minimal',
    [string]$OutputDirectory,
    [switch]$RequireOutput,
    [string]$Report
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
# MSBuild may invoke Windows PowerShell with another host's module search path.
# Load the matching built-in hash/signature commands without relying on discovery.
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Security\Microsoft.PowerShell.Security.psd1') -ErrorAction Stop
$taskSdk = [IO.Path]::GetFullPath($SdkDirectory)
$taskCore = if ($RuntimeDirectory) { [IO.Path]::GetFullPath($RuntimeDirectory) } else { Join-Path $taskSdk 'bin' }
$taskManifest = Join-Path $PSScriptRoot '..\src\NvidiaVfxRuntime.props'
[xml]$taskXml = Get-Content -LiteralPath $taskManifest -Raw
$taskNs = New-Object Xml.XmlNamespaceManager($taskXml.NameTable)
$taskNs.AddNamespace('m', 'http://schemas.microsoft.com/developer/msbuild/2003')
$taskVersion = $taskXml.SelectSingleNode('//m:NvidiaVfxRuntimePackageVersion', $taskNs).InnerText
$taskItems = @($taskXml.SelectNodes('//m:NvidiaVfxRuntimeFile', $taskNs))
$taskExcluded = @($taskXml.SelectSingleNode('//m:NvidiaVfxExcludedCoreName', $taskNs).GetAttribute('Include').Split(';'))
if ($taskItems.Count -ne 10 -or $taskExcluded.Count -ne 17) { throw 'Incomplete VFX runtime manifest' }

function Assert-X64([string]$Path) {
    $taskReader = New-Object IO.BinaryReader([IO.File]::OpenRead($Path))
    try {
        if ($taskReader.ReadUInt16() -ne 0x5a4d) { throw "Not a PE file: $Path" }
        $null = $taskReader.BaseStream.Seek(60, [IO.SeekOrigin]::Begin)
        $taskOffset = $taskReader.ReadUInt32()
        $null = $taskReader.BaseStream.Seek($taskOffset, [IO.SeekOrigin]::Begin)
        if ($taskReader.ReadUInt32() -ne 0x4550 -or $taskReader.ReadUInt16() -ne 0x8664) {
            throw "Not a Windows x64 PE: $Path"
        }
    } finally { $taskReader.Dispose() }
}

$taskDlls = @()
foreach ($taskItem in $taskItems) {
    $taskRelative = $taskItem.SelectSingleNode('m:SdkRelativePath', $taskNs).InnerText
    $taskRole = $taskItem.SelectSingleNode('m:Role', $taskNs).InnerText
    $taskExpected = $taskItem.SelectSingleNode('m:SHA256', $taskNs).InnerText
    $taskName = [IO.Path]::GetFileName($taskRelative)
    $taskSource = if ($taskRole -eq 'Core') { Join-Path $taskCore $taskName } else { Join-Path $taskSdk $taskRelative }
    if (!(Test-Path -LiteralPath $taskSource -PathType Leaf)) { throw "Missing VFX component: $taskSource" }
    Assert-X64 $taskSource
    $taskHash = (Get-FileHash -LiteralPath $taskSource -Algorithm SHA256).Hash
    if ($taskHash -ne $taskExpected) { throw "VFX $taskVersion hash mismatch: $taskName" }
    $taskSignature = Get-AuthenticodeSignature -LiteralPath $taskSource
    if ($taskSignature.Status -ne 'Valid' -or $taskSignature.SignerCertificate.Subject -notlike '*CN=NVIDIA Corporation*') {
        throw "VFX signature validation failed: $taskName"
    }
    $taskDlls += [pscustomobject]@{ name = $taskName; role = $taskRole; bytes = (Get-Item -LiteralPath $taskSource).Length; sha256 = $taskHash }
}
if ($Layout -eq 'Full') {
    foreach ($taskFile in @(Get-ChildItem -LiteralPath $taskCore -Filter '*.dll' -File)) {
        if ($taskFile.Name -in $taskDlls.name) { continue }
        Assert-X64 $taskFile.FullName
        $taskDlls += [pscustomobject]@{ name = $taskFile.Name; role = 'PrivateCoreExtra'; bytes = $taskFile.Length; sha256 = (Get-FileHash -LiteralPath $taskFile.FullName -Algorithm SHA256).Hash }
    }
}

# Equal license names must have equal content before the build flattens them.
$taskLicenses = @{}
$taskLicenseDirectories = @((Join-Path $taskSdk 'license'))
foreach ($taskFeature in @('nvvfxupscale', 'nvvfxvideosuperres', 'nvvfxvideoframegeneration', 'nvvfxtruehdr')) {
    $taskLicenseDirectories += Join-Path $taskSdk "features\$taskFeature\license"
}
foreach ($taskDirectory in $taskLicenseDirectories) {
    $taskFiles = @(Get-ChildItem -LiteralPath $taskDirectory -File)
    if ($taskFiles.Count -eq 0) { throw "Empty VFX license directory: $taskDirectory" }
    foreach ($taskFile in $taskFiles) {
        $taskHash = (Get-FileHash -LiteralPath $taskFile.FullName -Algorithm SHA256).Hash
        if ($taskLicenses.ContainsKey($taskFile.Name) -and $taskLicenses[$taskFile.Name].sha256 -ne $taskHash) {
            throw "VFX license name collision: $($taskFile.Name)"
        }
        $taskLicenses[$taskFile.Name] = [pscustomobject]@{ name = $taskFile.Name; sha256 = $taskHash; bytes = $taskFile.Length }
    }
}
$taskThirdParty = Join-Path $taskCore 'ThirdPartyLicenses.txt'
if (!(Test-Path -LiteralPath $taskThirdParty -PathType Leaf)) { throw 'Missing VFX ThirdPartyLicenses.txt' }
$taskLicenses['ThirdPartyLicenses.txt'] = [pscustomobject]@{ name = 'ThirdPartyLicenses.txt'; sha256 = (Get-FileHash -LiteralPath $taskThirdParty -Algorithm SHA256).Hash; bytes = (Get-Item -LiteralPath $taskThirdParty).Length }

if ($OutputDirectory) {
    $taskOutput = [IO.Path]::GetFullPath($OutputDirectory)
    if ($Layout -eq 'Minimal') {
        foreach ($taskName in $taskExcluded) {
            if (Test-Path -LiteralPath (Join-Path $taskOutput $taskName)) {
                throw "Stale full-runtime DLL in Minimal output: $taskName. Use a clean candidate output directory; files were not deleted."
            }
        }
    }
    if ($RequireOutput) {
        foreach ($taskFile in $taskDlls) {
            $taskPath = Join-Path $taskOutput $taskFile.name
            if (!(Test-Path -LiteralPath $taskPath -PathType Leaf) -or
                (Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash -ne $taskFile.sha256) {
                throw "Output VFX component missing or mismatched: $($taskFile.name)"
            }
        }
        foreach ($taskFile in $taskLicenses.Values) {
            $taskPath = Join-Path $taskOutput "NVIDIA-VFX-Licenses\$($taskFile.name)"
            if (!(Test-Path -LiteralPath $taskPath -PathType Leaf) -or
                (Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash -ne $taskFile.sha256) {
                throw "Output VFX license missing or mismatched: $($taskFile.name)"
            }
        }
    }
} elseif ($RequireOutput) { throw '-RequireOutput requires -OutputDirectory' }
$taskResult = [ordered]@{ schemaVersion = 1; packageVersion = $taskVersion; platform = 'windows-x64'; layout = $Layout;
    files = @($taskDlls | Sort-Object name); licenses = @($taskLicenses.Values | Sort-Object name);
    runtimeBytes = ($taskDlls | Measure-Object bytes -Sum).Sum; outputVerified = [bool]$RequireOutput }
if ($Report) {
    $taskReport = [IO.Path]::GetFullPath($Report)
    $null = [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($taskReport))
    $taskResult | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $taskReport -Encoding UTF8
}
Write-Output "VFX $taskVersion $Layout validated: $($taskDlls.Count) DLLs, $($taskResult.runtimeBytes) bytes. No DLL executed or copied."
