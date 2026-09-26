# Baut das Download-Paket fuer tool8.online: frischer Release-Bau, dann
# dist\VocalEQQ-<Version>-win64.zip mit VST3, Standalone, Anleitung, Lizenz.
#
#   powershell -ExecutionPolicy Bypass -File tools\package.ps1
#
# Die Version kommt aus CMakeLists.txt (project(... VERSION x.y.z)), damit
# Paketname, Anleitung und das Plugin selbst nie auseinanderlaufen.

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$cmakeText = Get-Content -Raw 'CMakeLists.txt'
if ($cmakeText -notmatch 'project\(VocalEQQ VERSION (\d+\.\d+\.\d+)') { throw 'Version in CMakeLists.txt nicht gefunden' }
$version = $Matches[1]

# JUCE aus dem Voxx-Cache, falls vorhanden - spart den Download.
$juceArgs = @()
if (Test-Path '..\Voxx\build\_deps\juce-src') { $juceArgs = @('-DFETCHCONTENT_SOURCE_DIR_JUCE=../Voxx/build/_deps/juce-src') }

cmake -B build-release -DVOCALEQQ_BUILD_TESTS=OFF -DVOCALEQQ_BUILD_CLI=OFF -DVOCALEQQ_COPY_PLUGIN=OFF @juceArgs
if ($LASTEXITCODE -ne 0) { throw 'Konfigurieren fehlgeschlagen' }
cmake --build build-release --config Release
if ($LASTEXITCODE -ne 0) { throw 'Bauen fehlgeschlagen' }

$artefacts = 'build-release\VocalEQQ_artefacts\Release'
$name = "VocalEQQ-$version-win64"
$stage = Join-Path 'dist' $name

if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null

Copy-Item -Recurse "$artefacts\VST3\Vocal EQQ.vst3" $stage
Copy-Item "$artefacts\Standalone\Vocal EQQ.exe" $stage
Copy-Item 'LICENSE' (Join-Path $stage 'LICENSE.txt')

# Anleitung mit Windows-Zeilenenden, damit sie auch im alten Editor lesbar ist.
$readme = (Get-Content -Raw 'packaging\README.txt').Replace('@VERSION@', $version) -replace "`r?`n", "`r`n"
[System.IO.File]::WriteAllText((Join-Path $root "$stage\README.txt"), $readme, (New-Object System.Text.UTF8Encoding $false))

$zip = Join-Path 'dist' "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $stage -DestinationPath $zip

$hash = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLower()
"$hash  $name.zip" | Set-Content -Encoding ascii "$zip.sha256"

Write-Output "Paket: $zip"
Write-Output "SHA256: $hash"
