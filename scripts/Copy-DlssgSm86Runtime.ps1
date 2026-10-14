param(
    [Parameter(Mandatory)][string]$RuntimeDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'DlssgSm86Package.ps1')
$contract = Assert-DlssgSm86Files $RuntimeDirectory
$destination = Join-Path $OutputDirectory 'runtimes/dlssg_sm86'
New-Item -ItemType Directory -Path $destination -Force | Out-Null
foreach ($record in $contract.Files) {
    Copy-Item -LiteralPath (Join-Path $RuntimeDirectory $record.Name) -Destination (Join-Path $destination $record.Name) -Force
}
# Remove only the obsolete FG output owned by the old copy target.
# Its replacement is the verified, explicitly loaded bundle registered by
# DlssgSm86Runtime/NgxFeatureDiscovery, not a DLL discovered at the package root.
$legacy = Join-Path $OutputDirectory 'nvngx_dlssg.dll'
if (Test-Path -LiteralPath $legacy) { Remove-Item -LiteralPath $legacy }
& (Join-Path $PSScriptRoot 'Test-DlssgSm86Package.ps1') -PackageDirectory $OutputDirectory
