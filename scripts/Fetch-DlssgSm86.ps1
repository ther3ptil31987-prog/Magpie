param([string]$OutputDirectory = (Join-Path $PSScriptRoot '../../dependencies/dlssg-sm86'))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'DlssgSm86Package.ps1')
$contract = Get-DlssgSm86Contract
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
foreach ($record in $contract.Files) {
    $target = Join-Path $OutputDirectory $record.Name
    if (Test-DlssgSm86File $target $record) { continue }
    $temporary = $target + ".download.$PID"
    try {
        if ($record.Name.EndsWith('.ini')) {
            Copy-Item -LiteralPath (Join-Path $PSScriptRoot "../src/DlssgSm86/$($record.Name)") -Destination $temporary
        } else {
            Invoke-WebRequest -Uri "https://raw.githubusercontent.com/sdli1995/dlssg_for_sm86/$($contract.Revision)/$($record.Name)" -OutFile $temporary
        }
        if (!(Test-DlssgSm86File $temporary $record)) { throw "Downloaded DLSSG file failed verification: $($record.Name)" }
        Move-Item -LiteralPath $temporary -Destination $target -Force
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
    }
}
$null = Assert-DlssgSm86Files $OutputDirectory
Write-Output "Verified DLSSG community development dependency $($contract.Revision): $OutputDirectory (not executed; redistribution review still required)."
