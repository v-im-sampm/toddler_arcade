# Prepare the reusable WSL containers image or build the current v2 device image.
# Usage: pwsh tools/idf_wslc_build.ps1 [-Prepare] [-RebuildImage]
param(
  [switch]$Prepare,
  [switch]$RebuildImage
)
$ErrorActionPreference = 'Stop'
$Image = 'little-arcade/idf:5.5.6'
$Repo = (Resolve-Path "$PSScriptRoot/..").Path

if (-not (Get-Command wslc -ErrorAction SilentlyContinue)) {
  throw 'wslc (WSL containers CLI) not found. Install/update WSL; Docker is not supported for this project.'
}

$ImageId = & wslc images --filter "reference=$Image" --quiet
if ($LASTEXITCODE -ne 0) { throw 'Could not query local WSL container images' }
if ($RebuildImage -or -not $ImageId) {
  & wslc build -f "$PSScriptRoot/Containerfile.idf" -t $Image "$PSScriptRoot"
  if ($LASTEXITCODE -ne 0) { throw "Failed to prepare build image $Image" }
} else {
  Write-Host "Using cached build image $Image"
}
if ($Prepare) { return }

& pwsh "$Repo/v2/native/bubble_aquarium/device/build.ps1"
if ($LASTEXITCODE -ne 0) { throw "v2 device build failed ($LASTEXITCODE)" }
