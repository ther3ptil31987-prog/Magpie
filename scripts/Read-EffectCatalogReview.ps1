#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$review = Get-Content -LiteralPath (Join-Path $repoRoot 'docs/experimental/reviews/20261002-effect-description-review.json') -Raw | ConvertFrom-Json
$supplement = Get-Content -LiteralPath (Join-Path $repoRoot 'docs/experimental/reviews/20261009-nvidia-vfx-suite-effect-description-review.json') -Raw | ConvertFrom-Json
$fgSupplement = Get-Content -LiteralPath (Join-Path $repoRoot 'docs/experimental/reviews/20261011-fsr3fg-effect-description-review.json') -Raw | ConvertFrom-Json
$supplement.effects = @($supplement.effects) + @($fgSupplement.effects)
$replacements = @{}
foreach ($entry in $supplement.effects) {
    if ($replacements.ContainsKey($entry.id)) { throw "Duplicate reviewed supplement: $($entry.id)" }
    $replacements[$entry.id] = $entry
}
$merged = @($review.effects | ForEach-Object {
    if ($replacements.ContainsKey($_.id)) {
        $entry = $replacements[$_.id]
        $replacements.Remove($_.id)
        $entry
    } else { $_ }
})
# Append new IDs in supplement order, keeping all historical entries stable.
$merged += @($supplement.effects | Where-Object { $replacements.ContainsKey($_.id) })
$review.effects = $merged
$review
