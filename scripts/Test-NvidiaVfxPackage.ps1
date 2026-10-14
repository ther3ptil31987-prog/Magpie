param(
    [Parameter(Mandatory = $true)][string]$PackageDirectory,
    [ValidateSet('Minimal', 'Full')][string]$Layout = 'Minimal'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = [IO.Path]::GetFullPath($PackageDirectory)
[xml]$taskXml = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\src\NvidiaVfxRuntime.props') -Raw
$taskNs = New-Object Xml.XmlNamespaceManager($taskXml.NameTable)
$taskNs.AddNamespace('m', 'http://schemas.microsoft.com/developer/msbuild/2003')
$taskManifestPath = Join-Path $taskRoot 'nvidia-vfx-runtime-manifest.json'
if (!(Test-Path -LiteralPath $taskManifestPath -PathType Leaf)) { throw 'Missing VFX runtime manifest; use a validated complete candidate build.' }
$taskManifest = Get-Content -LiteralPath $taskManifestPath -Raw | ConvertFrom-Json
$taskVersion = $taskXml.SelectSingleNode('//m:NvidiaVfxRuntimePackageVersion', $taskNs).InnerText
if ($taskManifest.schemaVersion -ne 1 -or $taskManifest.packageVersion -ne $taskVersion -or
    $taskManifest.platform -ne 'windows-x64' -or $taskManifest.layout -ne $Layout -or !$taskManifest.outputVerified) {
    throw 'VFX runtime manifest version/layout/output verification mismatch'
}
function Assert-Hash([string]$Name, [string]$Relative, [string]$Hash) {
    $taskPath = Join-Path $taskRoot $Relative
    if (!(Test-Path -LiteralPath $taskPath -PathType Leaf) -or
        (Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash -ne $Hash) { throw "Package missing or mismatched: $Name" }
}
$taskExpectedNames = @()
foreach ($taskItem in @($taskXml.SelectNodes('//m:NvidiaVfxRuntimeFile', $taskNs))) {
    $taskName = [IO.Path]::GetFileName($taskItem.SelectSingleNode('m:SdkRelativePath', $taskNs).InnerText)
    $taskHash = $taskItem.SelectSingleNode('m:SHA256', $taskNs).InnerText
    Assert-Hash $taskName $taskName $taskHash
    $taskReceipt = @($taskManifest.files | Where-Object { $_.name -eq $taskName })
    if ($taskReceipt.Count -ne 1 -or $taskReceipt[0].sha256 -ne $taskHash) { throw "VFX component receipt mismatch: $taskName" }
    $taskExpectedNames += $taskName
}
$taskExcluded = $taskXml.SelectSingleNode('//m:NvidiaVfxExcludedCoreName', $taskNs).GetAttribute('Include').Split(';')
if ($Layout -eq 'Minimal') {
    if (@($taskManifest.files).Count -ne 10) { throw 'Minimal VFX manifest must contain exactly ten DLLs' }
    foreach ($taskName in $taskExcluded) {
        if (Test-Path -LiteralPath (Join-Path $taskRoot $taskName)) { throw "Stale full-runtime file in Minimal package: $taskName" }
    }
} else {
    # Full is a private validation package. Its extra hashes come from the
    # pre-copy SDK validation receipt, while the ten required hashes are pinned.
    foreach ($taskFile in @($taskManifest.files)) {
        if ([IO.Path]::GetFileName($taskFile.name) -ne $taskFile.name) { throw 'Invalid VFX receipt filename' }
        Assert-Hash $taskFile.name $taskFile.name $taskFile.sha256
    }
}
foreach ($taskItem in @($taskXml.SelectNodes('//m:NvidiaVfxLicenseFile', $taskNs))) {
    $taskName = $taskItem.GetAttribute('Include')
    Assert-Hash $taskName "NVIDIA-VFX-Licenses\$taskName" $taskItem.SelectSingleNode('m:SHA256', $taskNs).InnerText
}
Write-Output "VFX $taskVersion $Layout package files and licenses verified. No file changed or DLL executed."
