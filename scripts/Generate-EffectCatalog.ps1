#Requires -Version 7.0
param([switch]$Check)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
# Current bilingual review is authoritative; the 20260908 audits remain historical.
$review = & (Join-Path $repoRoot 'scripts/Read-EffectCatalogReview.ps1')
$labels = @{
    'zh-Hans' = @{ first_try = '优先尝试'; situational = '特定场景推荐'; advanced = '进阶／实验'; diagnostic = '调试用途' }
    'en-US' = @{ first_try = 'Try first'; situational = 'Situational'; advanced = 'Advanced / experimental'; diagnostic = 'Diagnostic' }
}
$families = @{}
foreach ($family in $review.families) { $families[$family.id] = $family }
$subfamilies = @{}
foreach ($family in $review.subfamilies) { $subfamilies[$family.id] = $family }
foreach ($language in @('zh-Hans', 'en-US')) {
    $categories = @($review.categories | ForEach-Object {
        $text = $_.$language
        [ordered]@{ id = $_.id; name = $text.name; description = $text.description; subcategories = @($text.subcategories) }
    })
    $effects = @($review.effects | ForEach-Object {
        $entry = $_
        $text = $entry.$language
        if (!$text.summary -or !$text.details -or !$labels[$language].ContainsKey($entry.level)) { throw "Incomplete reviewed entry: $language $($entry.id)" }
        $family = if ($entry.family) { $families[$entry.family] } else { $null }
        $subfamily = if ($entry.subfamily) { $subfamilies[$entry.subfamily] } else { $null }
        if (($entry.family -and !$family) -or ($entry.subfamily -and !$subfamily)) { throw "Unknown reviewed family: $($entry.id)" }
        [ordered]@{
            id = $entry.id; name = $text.name; category = $entry.category; subcategory = $text.subcategory
            purposes = @($entry.purposes); summary = $text.summary; details = $text.details
            level = $entry.level; recommendation = $labels[$language][$entry.level]; search = $text.search
            family = if ($family) { [ordered]@{ id = $family.id; name = $family.$language.name; summary = $family.$language.summary } } else { $null }
            subfamily = if ($subfamily) { [ordered]@{ id = $subfamily.id; name = $subfamily.$language.name; summary = $subfamily.$language.summary } } else { $null }
        }
    })
    $catalog = [ordered]@{ schemaVersion = 1; language = $language; categories = $categories; effects = $effects }
    $result = $catalog | ConvertTo-Json -Depth 8
    $output = Join-Path $repoRoot "src/Magpie/EffectCatalog/$language.json"
    if ($Check) {
        if (!(Test-Path -LiteralPath $output) -or (Get-Content -LiteralPath $output -Raw).TrimEnd() -cne $result.TrimEnd()) { throw "Effect catalog is stale: $language. Run scripts/Generate-EffectCatalog.ps1." }
    } else {
        [IO.File]::WriteAllText($output, $result + "`n", [Text.UTF8Encoding]::new($false))
    }
    if ($language -eq 'zh-Hans') {
        # Keep the existing HDR component artifact synchronized with the same source.
        $hdr = [ordered]@{ category = $categories | Where-Object id -eq 'hdr-components'; effects = @($effects | Where-Object category -eq 'hdr-components') } | ConvertTo-Json -Depth 8
        $hdrPath = Join-Path $repoRoot 'src/Magpie/EffectCatalog/hdr-components.json'
        if ($Check) {
            if ((Get-Content -LiteralPath $hdrPath -Raw).TrimEnd() -cne $hdr.TrimEnd()) { throw 'HDR components are stale. Run scripts/Generate-EffectCatalog.ps1.' }
        } else {
            [IO.File]::WriteAllText($hdrPath, $hdr + "`n", [Text.UTF8Encoding]::new($false))
        }
    }
}
if ($Check) { 'Both effect catalogs and HDR components match the current bilingual review.' }
