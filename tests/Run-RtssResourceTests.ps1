#Requires -Version 7.0
[CmdletBinding(DefaultParameterSetName = 'Runtime')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Runtime')][string]$RuntimeDirectory,
    [Parameter(Mandatory, ParameterSetName = 'Source')][switch]$SourceResources,
    [Parameter(Mandatory)][string]$GeneratedDirectory,
    [string]$OutputDirectory = (Join-Path $env:TEMP ('Magpie-RtssResourceTests-' + [guid]::NewGuid()))
)
$ErrorActionPreference = 'Stop'
$rtssRepo = Split-Path $PSScriptRoot -Parent
$rtssOutput = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $rtssOutput -Force | Out-Null
& python (Join-Path $PSScriptRoot 'prepare_rtss_resource_tests.py') $rtssOutput
if ($LASTEXITCODE) { throw 'Production getter extraction failed.' }
$rtssVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$rtssVs = & $rtssVsWhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
if (!$rtssVs) { throw 'Visual Studio with C++ tools is required.' }
Import-Module (Join-Path $rtssVs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $rtssVs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
if ($SourceResources) {
    # Compile only the current language files into an isolated test PRI, not the product.
    $rtssResourceRoot = Join-Path $rtssOutput 'source-resource-index'
    foreach ($rtssFile in Get-ChildItem -LiteralPath (Join-Path $rtssRepo 'src/Magpie') -Filter 'Resources.language-*.resw') {
        $rtssLanguage = $rtssFile.BaseName.Substring('Resources.language-'.Length)
        $rtssLanguageRoot = Join-Path $rtssResourceRoot $rtssLanguage
        New-Item -ItemType Directory -Path $rtssLanguageRoot -Force | Out-Null
        Copy-Item -LiteralPath $rtssFile.FullName -Destination (Join-Path $rtssLanguageRoot 'Resources.resw') -Force
    }
    $rtssMakePri = Join-Path $env:WindowsSdkDir "bin/$($env:WindowsSDKVersion.TrimEnd('\'))/x64/makepri.exe"
    if (!(Test-Path -LiteralPath $rtssMakePri)) { throw 'Windows SDK makepri.exe is required for source resources.' }
    $rtssPriConfig = Join-Path $rtssOutput 'pri.config.xml'
    & $rtssMakePri createconfig /cf $rtssPriConfig /dq en-US /o > (Join-Path $rtssOutput 'makepri-config.log')
    if ($LASTEXITCODE) { throw 'Test PRI configuration failed.' }
    # The resw indexer ignores ordinary folders; mirror the product's explicit prefix.
    [xml]$rtssConfig = Get-Content -LiteralPath $rtssPriConfig -Raw
    $rtssConfig.SelectSingleNode('//indexer-config[@type="resw"]').SetAttribute('initialPath', 'Magpie')
    # Keep every language in one index, as in the desktop product.
    $rtssConfig.SelectSingleNode('/resources/packaging').RemoveAll()
    $rtssConfig.Save($rtssPriConfig)
    & $rtssMakePri new /pr $rtssResourceRoot /cf $rtssPriConfig /in Magpie /of (Join-Path $rtssOutput 'resources.pri') /o > (Join-Path $rtssOutput 'makepri.log')
    if ($LASTEXITCODE) { throw 'Source resource test PRI failed.' }
} else {
    $rtssRuntime = [IO.Path]::GetFullPath($RuntimeDirectory)
    if (!(Test-Path -LiteralPath (Join-Path $rtssRuntime 'resources.pri'))) { throw 'Compiled product resources.pri is required.' }
    Copy-Item -LiteralPath (Join-Path $rtssRuntime 'resources.pri') -Destination $rtssOutput -Force
}
$rtssConanRoots = @((Join-Path $env:USERPROFILE '.conan2/p'), (Join-Path $env:USERPROFILE '.conan2/p/b'))
$rtssFmt = $rtssConanRoots | ForEach-Object { Get-ChildItem -LiteralPath $_ -Directory -Filter 'fmt*' } |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'p/include/fmt/xchar.h') } | Select-Object -First 1
if (!$rtssFmt) { throw 'Restore fmt before compiling the resource test.' }
$rtssExe = Join-Path $rtssOutput 'RtssResourceTextTests.exe'
$rtssResourceDefines = @(if ($SourceResources) { '/DRTSS_SOURCE_RESOURCES' })
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX /DNOMINMAX /DWIN32_LEAN_AND_MEAN /DWINRT_NO_MODULE_LOCK /DFMT_HEADER_ONLY `
    @rtssResourceDefines `
    "/I$GeneratedDirectory" "/I$rtssOutput" "/I$rtssRepo/src/Magpie" "/I$rtssRepo/src/Shared" "/I$($rtssFmt.FullName)/p/include" `
    (Join-Path $PSScriptRoot 'RtssResourceTextTests.cpp') "/Fe:$rtssExe" "/Fo:$rtssOutput/RtssResourceTextTests.obj" `
    /link WindowsApp.lib /MANIFEST:EMBED "/MANIFESTINPUT:$PSScriptRoot/EffectChoiceItemsTests.manifest"
if ($LASTEXITCODE) { throw 'Native resource test compilation failed.' }
Push-Location $rtssOutput
try { & $rtssExe; if ($LASTEXITCODE) { throw 'Native resource text test failed.' } }
finally { Pop-Location }

# No product PRI/App.xbf beside this host: it uses fixture text but the exact
# production getter, so real XAML can verify collection binding independently.
$rtssXamlOutput = Join-Path $rtssOutput 'xaml'
New-Item -ItemType Directory -Path $rtssXamlOutput -Force | Out-Null
$rtssXamlExe = Join-Path $rtssXamlOutput 'RtssStatusItemsSourceTests.exe'
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX /DNOMINMAX /DWIN32_LEAN_AND_MEAN /DWINRT_NO_MODULE_LOCK /DFMT_HEADER_ONLY `
    "/I$GeneratedDirectory" "/I$rtssOutput" "/I$rtssRepo/src/Magpie" "/I$rtssRepo/src/Shared" "/I$($rtssFmt.FullName)/p/include" `
    (Join-Path $PSScriptRoot 'RtssStatusItemsSourceTests.cpp') "/Fe:$rtssXamlExe" "/Fo:$rtssOutput/RtssStatusItemsSourceTests.obj" `
    /link WindowsApp.lib /MANIFEST:EMBED "/MANIFESTINPUT:$PSScriptRoot/EffectChoiceItemsTests.manifest"
if ($LASTEXITCODE) { throw 'Native status ItemsSource test compilation failed.' }
Push-Location $rtssXamlOutput
try { & $rtssXamlExe; if ($LASTEXITCODE) { throw 'Native status ItemsSource test failed.' } }
finally { Pop-Location }
