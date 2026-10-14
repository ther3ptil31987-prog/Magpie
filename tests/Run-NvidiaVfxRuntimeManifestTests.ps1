param([Parameter(Mandatory = $true)][string]$SdkDirectory,
      [Parameter(Mandatory = $true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = [IO.Path]::GetFullPath($OutputDirectory)
$null = [IO.Directory]::CreateDirectory($taskRoot)
$taskFixture = Join-Path $taskRoot ('runtime-fixture-' + [guid]::NewGuid().ToString('N'))
$null = [IO.Directory]::CreateDirectory($taskFixture)
$taskSdk = [IO.Path]::GetFullPath($SdkDirectory)
$taskValidate = Join-Path $PSScriptRoot '..\scripts\Validate-NvidiaVfxRuntime.ps1'
$taskPackage = Join-Path $PSScriptRoot '..\scripts\Test-NvidiaVfxPackage.ps1'
[xml]$taskXml = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\src\NvidiaVfxRuntime.props') -Raw
$taskNs = New-Object Xml.XmlNamespaceManager($taskXml.NameTable)
$taskNs.AddNamespace('m','http://schemas.microsoft.com/developer/msbuild/2003')
foreach($taskItem in @($taskXml.SelectNodes('//m:NvidiaVfxRuntimeFile',$taskNs))) {
    $taskRelative=$taskItem.SelectSingleNode('m:SdkRelativePath',$taskNs).InnerText
    Copy-Item -LiteralPath (Join-Path $taskSdk $taskRelative) -Destination (Join-Path $taskFixture ([IO.Path]::GetFileName($taskRelative)))
}
$taskLicenseDir=Join-Path $taskFixture 'NVIDIA-VFX-Licenses'
$null=[IO.Directory]::CreateDirectory($taskLicenseDir)
Get-ChildItem -LiteralPath (Join-Path $taskSdk 'license') -File | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $taskLicenseDir }
Copy-Item -LiteralPath (Join-Path $taskSdk 'bin\ThirdPartyLicenses.txt') -Destination $taskLicenseDir
$taskReceipt=Join-Path $taskFixture 'nvidia-vfx-runtime-manifest.json'
$taskResults=@()
function Expect-Rejection([string]$Name,[string]$Message,[scriptblock]$Action) {
    try { & $Action; throw "Expected rejection did not occur: $Name" }
    catch { if($_.Exception.Message -notlike "*$Message*") {throw} }
    $script:taskResults += [pscustomobject]@{case=$Name;status='pass'}
}
& $taskValidate -SdkDirectory $taskSdk -Layout Minimal -OutputDirectory $taskFixture -RequireOutput -Report $taskReceipt
& $taskPackage -PackageDirectory $taskFixture -Layout Minimal
$taskResults += [pscustomobject]@{case='matching minimal components and four license files';status='pass'}
$taskStale=Join-Path $taskFixture 'cublasLt64_12.dll'
try {
    [IO.File]::WriteAllText($taskStale,'Test sentinel, not a DLL')
    Expect-Rejection 'stale full-Core build output' 'Stale full-runtime DLL' { & $taskValidate -SdkDirectory $taskSdk -OutputDirectory $taskFixture -RequireOutput }
    Expect-Rejection 'stale full-Core package' 'Stale full-runtime file' { & $taskPackage -PackageDirectory $taskFixture }
} finally { if(Test-Path -LiteralPath $taskStale){Remove-Item -LiteralPath $taskStale} }
$taskTarget=Join-Path $taskFixture 'nvngxruntime.dll'
try {
    Remove-Item -LiteralPath $taskTarget
    Expect-Rejection 'missing required component' 'missing or mismatched' { & $taskPackage -PackageDirectory $taskFixture }
    Copy-Item -LiteralPath (Join-Path $taskSdk 'bin\nvngxruntime.dll') -Destination $taskTarget
    [IO.File]::WriteAllText($taskTarget,'Test corruption')
    Expect-Rejection 'corrupted required component' 'missing or mismatched' { & $taskPackage -PackageDirectory $taskFixture }
} finally { Copy-Item -LiteralPath (Join-Path $taskSdk 'bin\nvngxruntime.dll') -Destination $taskTarget -Force }
$taskLicense=Join-Path $taskLicenseDir 'ThirdPartyLicenses.txt'
try {
    Remove-Item -LiteralPath $taskLicense
    Expect-Rejection 'missing third-party notices' 'missing or mismatched' { & $taskPackage -PackageDirectory $taskFixture }
} finally { Copy-Item -LiteralPath (Join-Path $taskSdk 'bin\ThirdPartyLicenses.txt') -Destination $taskLicense }
& $taskPackage -PackageDirectory $taskFixture
$taskResults += [pscustomobject]@{case='fixture restored to valid state after failures';status='pass'}
$taskResults | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $taskRoot 'runtime-manifest-tests.json') -Encoding UTF8
Write-Output "PASS $($taskResults.Count) runtime/package integrity cases; SDK and release files unchanged; no compilation or DLL execution."
